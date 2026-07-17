#include "FilePanelController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <functional>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <winioctl.h>
#endif

#ifdef Q_OS_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include "../core/ArchiveSupport.h"
#include "../core/ArchiveFileProvider.h"
#include "../core/FileAccessResolver.h"
#include "FilePanelLoadMorePolicy.h"
#include "../core/FileEntryPresentationResolver.h"
#include "../core/IsoSupport.h"
#include "../core/LaunchService.h"
#include "../core/OpenWithService.h"
#include "../core/LinuxAdminBroker.h"
#include "../core/LocalFileProvider.h"
#include "../core/MetadataExtractor.h"
#include "../core/TerminalLauncher.h"
#include "../core/WallpaperSetter.h"
#include "../core/DriveUtils.h"
#include "../core/CleanupSubsystem.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileError.h"
#include "../core/VolumeMonitor.h"
#include "FavoritesController.h"
#include "../platform/openwith/LinuxOpenWithBackend.h"

#include "FilePanelControllerInternal.h"

using namespace FilePanelControllerInternal;


bool FilePanelController::openPath(const QString &path)
{
    const QString current = currentPath();
    const QString preprocessed = FileProviderFactory::preprocessPath(expandHomeShortcutPath(path));
    const QString normalized = FileProviderFactory::normalizePath(preprocessed.isEmpty() ? path : preprocessed);
    const bool loadMoreForCurrentPath = isLoadMorePathForCurrentProviderPath(current, normalized);
    return requestOpenPath(path, true, loadMoreForCurrentPath,
                           loadMoreForCurrentPath ? LoadMoreNavigation : DirectNavigation);
}

bool FilePanelController::openPathPreservingScroll(const QString &path)
{
    return requestOpenPath(path, true, true, PreserveScrollNavigation);
}

bool FilePanelController::openStartupRestoredFolder(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()
        || !normalizedVirtualRoot(trimmedPath).isEmpty()
        || ArchiveSupport::isArchivePath(trimmedPath)
        || ArchiveSupport::isArchiveFilePath(trimmedPath)
        || IsoSupport::isIsoImagePath(trimmedPath)) {
        return requestOpenPath(path, true, false, StartupRestoreNavigation);
    }

    const std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(trimmedPath);
    if (!provider || !provider->pathExists(trimmedPath) || !provider->isDirectory(trimmedPath)) {
        return requestOpenPath(path, true, false, StartupRestoreNavigation);
    }

    ++m_navigationRequestId;
    setNavigationPending(false);
    return openPathInternal(trimmedPath, true, false, StartupRestoreNavigation);
}

bool FilePanelController::requestOpenPath(const QString &path, bool addToHistory, bool preserveScroll,
                                          NavigationReason reason)
{
    cancelDirectorySuggestions();
    QElapsedTimer totalTimer;
    totalTimer.start();
    if (filePanelNavTraceEnabled()) {
        traceFilePanelNav("openPath-begin", path,
                          QStringLiteral("current=%1").arg(QDir::toNativeSeparators(currentPath())));
    }

    if (path.isEmpty()) {
        ++m_navigationRequestId;
        setNavigationPending(false);
        traceFilePanelNav("openPath-end", path, QStringLiteral("result=false reason=empty elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }

    const QString preprocessedPath = FileProviderFactory::preprocessPath(expandHomeShortcutPath(path));
    if (preprocessedPath.isEmpty()) {
        ++m_navigationRequestId;
        setNavigationPending(false);
        traceFilePanelNav("openPath-end", path, QStringLiteral("result=false reason=blank elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }
    const QString virtualRoot = normalizedVirtualRoot(preprocessedPath);
    if (!virtualRoot.isEmpty()) {
        ++m_navigationRequestId;
        setNavigationPending(false);
        const bool result = openPathInternal(virtualRoot, addToHistory, preserveScroll, reason);
        traceFilePanelNav("openPath-end", virtualRoot,
                          QStringLiteral("result=%1 type=virtual elapsedMs=%2").arg(result).arg(totalTimer.elapsed()));
        return result;
    }

    const QString approvalTarget = nestedArchiveApprovalTarget(preprocessedPath);
    const QString approvalScope = nestedArchiveScopeKeyForPath(approvalTarget);
    if (!approvalScope.isEmpty()
        && !m_approvedNestedArchiveScopeKeys.contains(approvalScope)
        && !ArchiveFileProvider::hasCachedContainerForPath(approvalTarget)) {
        ++m_navigationRequestId;
        setNavigationPending(false);
        emit nestedArchiveOpenRequested(approvalTarget,
                                        nestedArchiveDisplayNameForPath(approvalTarget),
                                        nestedArchiveSizeTextForPath(approvalTarget));
        traceFilePanelNav("openPath-end", approvalTarget,
                          QStringLiteral("result=true reason=nested-approval elapsedMs=%1").arg(totalTimer.elapsed()));
        return true;
    }
    if (!approvalScope.isEmpty()) {
        m_approvedNestedArchiveScopeKeys.insert(approvalScope);
        if (!ArchiveFileProvider::hasCachedContainerForPath(approvalTarget)) {
            setStatusMessage(nestedArchivePreparationStatusForPath(approvalTarget));
        }
    }

    const int requestId = ++m_navigationRequestId;
    setNavigationPending(true, preprocessedPath);
    QPointer<FilePanelController> self(this);
    (void)QtConcurrent::run([self, preprocessedPath, requestId, addToHistory, preserveScroll, reason]() {
        QElapsedTimer resolverTimer;
        resolverTimer.start();
        const NavigationResolution resolution = resolveNavigationPath(preprocessedPath);
        traceFilePanelNav("openPath-resolver-finished", preprocessedPath,
                          QStringLiteral("requestId=%1 type=%2 elapsedMs=%3")
                              .arg(requestId)
                              .arg(resolution.traceType)
                              .arg(resolverTimer.elapsed()));
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(),
                                  [self, preprocessedPath, requestId, addToHistory, preserveScroll, reason, resolution]() {
            if (!self || requestId != self->m_navigationRequestId) {
                return;
            }

            switch (resolution.type) {
            case NavigationResolution::Type::Invalid:
                self->setNavigationPending(false);
                self->setOperationError(resolution.error.isEmpty()
                                             ? QStringLiteral("Path is invalid, unavailable, or not a folder.")
                                             : resolution.error,
                                         preprocessedPath,
                                         QStringLiteral("open"));
                emit self->pathNavigationFailed(preprocessedPath);
                return;
            case NavigationResolution::Type::MountIso:
                self->setNavigationPending(false);
                emit self->isoMountRequested(resolution.path);
                return;
            case NavigationResolution::Type::OpenPath:
                break;
            }

            const bool result = self->openPathInternal(resolution.path, addToHistory, preserveScroll, reason);
            self->setNavigationPending(false);
            traceFilePanelNav("openPath-end", resolution.path,
                              QStringLiteral("result=%1 type=%2 requestId=%3")
                                  .arg(result)
                                  .arg(resolution.traceType)
                                  .arg(requestId));
        }, Qt::QueuedConnection);
    });

    traceFilePanelNav("openPath-end", preprocessedPath,
                      QStringLiteral("result=true reason=queued requestId=%1 elapsedMs=%2")
                          .arg(requestId)
                          .arg(totalTimer.elapsed()));
    return true;
}

bool FilePanelController::canOpenPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        return false;
    }
    if (!normalizedVirtualRoot(trimmedPath).isEmpty()) {
        return true;
    }

    if (ArchiveSupport::isArchivePath(trimmedPath)) {
        return true;
    }

    return true;
}

bool FilePanelController::openSearchResult(const QString &path, bool isDirectory)
{
    if (isVirtualRoot() || path.trimmed().isEmpty()) {
        return false;
    }
    if (isDirectory) {
        return openPath(path);
    }

    const QString parentPath = parentPathForPath(path);
    if (parentPath.isEmpty()) {
        return false;
    }
    emit pathRevealRequested(path);
    return openPath(parentPath);
}

bool FilePanelController::openInPanelTarget(const QString &path, bool isDirectory)
{
    if (path.trimmed().isEmpty()) {
        return false;
    }
    if (isDirectory) {
        return openPath(path);
    }

    const QString parentPath = parentPathForPath(path);
    if (parentPath.isEmpty()) {
        return false;
    }
    emit pathRevealRequested(path);
    return openPath(parentPath);
}

bool FilePanelController::openNestedArchivePath(const QString &path)
{
    if (isVirtualRoot() || !ArchiveSupport::isArchivePath(path)) {
        return false;
    }

    const QString targetPath = nestedArchiveApprovalTarget(path);
    const QString approvalScope = nestedArchiveScopeKeyForPath(targetPath);
    if (approvalScope.isEmpty()) {
        return false;
    }

    const QString archiveName = ArchiveSupport::archiveFileName(targetPath);
    const QString archiveSuffix = QFileInfo(archiveName).suffix().toLower();
    if (!ArchiveSupport::isArchiveExtension(archiveSuffix)) {
        return false;
    }

    m_approvedNestedArchiveScopeKeys.insert(approvalScope);
    setStatusMessage(nestedArchivePreparationStatusForPath(targetPath));
    return requestOpenPath(targetPath, true);
}

void FilePanelController::submitArchivePassword(const QString &path, const QString &password)
{
    const QString trimmedPath = path.trimmed();
    if (!ArchiveSupport::isArchivePath(trimmedPath)) {
        return;
    }

    ArchiveFileProvider::setPasswordForPath(trimmedPath, password);
    const QString approvalScope = nestedArchiveScopeKeyForPath(trimmedPath);
    if (!approvalScope.isEmpty()) {
        m_approvedNestedArchiveScopeKeys.insert(approvalScope);
    }
    setStatusMessage(QStringLiteral("Opening archive..."));
    requestOpenPath(trimmedPath, false);
}

void FilePanelController::cancelArchivePassword(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (!ArchiveSupport::isArchivePath(trimmedPath)) {
        return;
    }

    ArchiveFileProvider::clearPasswordForPath(trimmedPath);
    setOperationError(QStringLiteral("Archive password required"), trimmedPath, QStringLiteral("open"));
    emit pathNavigationFailed(trimmedPath);
}

void FilePanelController::cancelCurrentLoad()
{
    if (!m_directoryModel.loading()) {
        if (m_navigationPending) {
            const QString cancelledPath = m_pendingNavigationPath;
            const QString cancelledScope = nestedArchiveScopeKeyForPath(cancelledPath);
            if (!cancelledScope.isEmpty()) {
                m_approvedNestedArchiveScopeKeys.remove(cancelledScope);
                ArchiveFileProvider::invalidateCacheForPath(cancelledPath);
            }
            ++m_navigationRequestId;
            setStatusMessage(QStringLiteral("Archive preparation was cancelled"));
            setNavigationPending(false);
        }
        return;
    }

    const QString cancelledPath = currentPath();
    const QString cancelledScope = nestedArchiveScopeKeyForPath(cancelledPath);
    if (!cancelledScope.isEmpty()) {
        m_approvedNestedArchiveScopeKeys.remove(cancelledScope);
        ArchiveFileProvider::invalidateCacheForPath(cancelledPath);
    }

    m_directoryModel.cancelLoading();
    setStatusMessage(QStringLiteral("Archive preparation was cancelled"));
    setNavigationPending(false);

    if (!m_backStack.isEmpty()) {
        const QString previous = m_backStack.takeLast();
        requestOpenPath(previous, false, true, RecoveryNavigation);
        emit historyChanged();
    }
}

void FilePanelController::openItem(int row)
{
    if (isVirtualRoot()) return;
    if (m_directoryModel.specialActionAt(row) == static_cast<int>(FileEntrySpecialAction::LoadMore)) {
        if (!m_directoryModel.loading()) openPathPreservingScroll(m_directoryModel.pathAt(row));
        return;
    }
    const bool shortcut = m_directoryModel.isShortcutAt(row);
    const QString itemPath = m_directoryModel.pathAt(row);
    QString shortcutTargetPath = shortcut ? m_directoryModel.shortcutOpenPathAt(row) : QString{};
    if (shortcutTargetPath.isEmpty() && shortcut) {
        shortcutTargetPath = m_directoryModel.shortcutTargetPathAt(row);
    }
    const bool shortcutTargetIsDirectory = shortcut && m_directoryModel.shortcutTargetIsDirectoryAt(row);
    QString path = shortcutTargetIsDirectory
        ? shortcutTargetPath
        : itemPath;
    if (!path.isEmpty()) {
        if (shortcutTargetIsDirectory || (!shortcut && m_directoryModel.isDirectoryAt(row))) {
            openPath(path);
            return;
        }

        if (shortcut && !shortcutTargetPath.isEmpty()) {
            const QUrl shortcutUrl(shortcutTargetPath);
            if ((shortcutUrl.scheme() == QLatin1String("http") || shortcutUrl.scheme() == QLatin1String("https"))
                && QDesktopServices::openUrl(shortcutUrl)) {
                return;
            }
        }

#ifdef Q_OS_LINUX
        if (!QFileInfo(path).isReadable() && !LinuxAdminBroker::activeSessionNonce().isEmpty()) {
            QString materializeError;
            const QString readOnlyCopy = materializeAdminReadOnlyLaunchFile(path, &materializeError);
            if (readOnlyCopy.isEmpty()) {
                setStatusMessage(materializeError.isEmpty()
                                     ? QStringLiteral("Could not prepare administrator read-only copy.")
                                     : materializeError);
                return;
            }
            path = readOnlyCopy;
        }
#endif

        const QString suffix = QFileInfo(path).suffix().toLower();
        if (IsoSupport::isIsoImageExtension(suffix)) {
            emit isoMountRequested(path);
            return;
        }

        if (ArchiveSupport::isArchivePath(path)) {
            const QString archiveSuffix = QFileInfo(ArchiveSupport::archiveFileName(path)).suffix().toLower();
            if (ArchiveSupport::isArchiveExtension(archiveSuffix)) {
                const QString targetPath = nestedArchiveApprovalTarget(path);
                const QString approvalScope = nestedArchiveScopeKeyForPath(targetPath);
                if (!approvalScope.isEmpty()
                    && (m_approvedNestedArchiveScopeKeys.contains(approvalScope)
                        || ArchiveFileProvider::hasCachedContainerForPath(targetPath))) {
                    m_approvedNestedArchiveScopeKeys.insert(approvalScope);
                    openPath(targetPath);
                    return;
                }
                emit nestedArchiveOpenRequested(targetPath,
                                                nestedArchiveDisplayNameForPath(targetPath),
                                                nestedArchiveSizeTextForPath(targetPath));
                return;
            }
        }

        if (ArchiveSupport::isArchiveExtension(suffix)) {
            openPath(path);
            return;
        }

        if (isProviderUriPath(path)) {
            setStatusMessage(QStringLiteral("This provider does not support direct file launch."));
            return;
        }

        const auto preferredCandidate = openWithService().effectiveCandidate(path);
        if (preferredCandidate && preferredCandidate->fmDefault) {
            const OpenWithResult openWithResult = openWithService().openWith(path, preferredCandidate->id);
            if (!openWithResult.ok) {
                setStatusMessage(openWithResult.message.isEmpty()
                                     ? QStringLiteral("Could not open file.")
                                     : openWithResult.message);
                setLastError(openWithErrorInfo(openWithResult, path));
            } else {
                setLastError({});
            }
            return;
        }

        const LaunchService::LaunchResult launchResult = LaunchService::openPath(path);
        if (!launchResult.ok) {
            setStatusMessage(launchResult.message.isEmpty()
                                 ? QStringLiteral("Could not open file.")
                                 : launchResult.message);
            setLastError(launchErrorInfo(launchResult, path));
        }
    }
}

bool FilePanelController::canLoadMore() const
{
    return !m_directoryModel.loading()
        && m_directoryModel.indexOfSpecialAction(static_cast<int>(FileEntrySpecialAction::LoadMore)) >= 0;
}

QString FilePanelController::loadMoreIconName() const
{
    const int row = m_directoryModel.indexOfSpecialAction(static_cast<int>(FileEntrySpecialAction::LoadMore));
    if (row < 0) return {};
    const QModelIndex index = m_directoryModel.index(row);
    const QString overlayIconName = m_directoryModel.data(
        index, DirectoryModel::OverlayIconNameRole).toString();
    return overlayIconName.isEmpty()
        ? m_directoryModel.data(index, DirectoryModel::IconNameRole).toString()
        : overlayIconName;
}

void FilePanelController::loadMore()
{
    const int row = m_directoryModel.indexOfSpecialAction(static_cast<int>(FileEntrySpecialAction::LoadMore));
    dispatchLoadMoreRequest(row, m_directoryModel.loading(), [this](int loadMoreRow) { openItem(loadMoreRow); });
}

void FilePanelController::goBack()
{
    if (m_backStack.isEmpty()) {
        return;
    }

    const QString previous = m_backStack.takeLast();
    if (!currentPath().isEmpty()) {
        m_forwardStack.append(currentPath());
    }
    requestOpenPath(previous, false, true, BackNavigation);
    emit historyChanged();
}

void FilePanelController::goForward()
{
    if (m_forwardStack.isEmpty()) {
        return;
    }

    const QString next = m_forwardStack.takeLast();
    if (!currentPath().isEmpty()) {
        m_backStack.append(currentPath());
    }
    requestOpenPath(next, false, true, ForwardNavigation);
    emit historyChanged();
}

void FilePanelController::goUp()
{
    if (isVirtualRoot()) {
        return; // Already at the top
    }
    const QString cp = currentPath();
    const QString parent = parentPathForPath(cp);
    // If parent == current, we are at the drive root — go to devices://
    if (parent.isEmpty() || parent == cp) {
        if (isProviderUriPath(cp)) {
            return;
        }
        openPath(QString(DEVICE_ROOT));
    } else {
        requestOpenPath(parent, true, true, UpNavigation);
    }
}

QString FilePanelController::fileNameForPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveFileName(path);
    }
    if (isProviderUriPath(path)) {
        if (std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path)) {
            return provider->fileName(path);
        }
    }
    return m_fileProvider->fileName(path);
}

QString FilePanelController::parentPathForPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveParentPath(path);
    }
    if (isProviderUriPath(path)) {
        if (std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path)) {
            return provider->parentPath(path);
        }
        return {};
    }
    return m_fileProvider->parentPath(path);
}

QString FilePanelController::childPathForPath(const QString &parentPath, const QString &name) const
{
    if (ArchiveSupport::isArchivePath(parentPath)) {
        return ArchiveSupport::archiveChildPath(parentPath, name);
    }
    if (isProviderUriPath(parentPath)) {
        if (std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(parentPath)) {
            return provider->childPath(parentPath, name);
        }
        return {};
    }
    return m_fileProvider->childPath(parentPath, name);
}

QStringList FilePanelController::breadcrumbPathsForPath(const QString &path) const
{
    QStringList result;
    if (path.isEmpty() || path == QString(DEVICE_ROOT) || path == QString(FAVORITES_ROOT)) {
        return result;
    }

    if (ArchiveSupport::isArchivePath(path)) {
        const QStringList tokens = ArchiveSupport::splitArchiveTokens(path);
        if (tokens.isEmpty()) {
            return result;
        }

        const QString physicalPath = QDir::fromNativeSeparators(tokens.first().trimmed());
        if (physicalPath.isEmpty()) {
            return result;
        }

        // Get breadcrumbs for the containing local folder
        const QString parentDir = QDir::fromNativeSeparators(QFileInfo(physicalPath).absoluteDir().absolutePath());
        result = breadcrumbPathsForPath(parentDir);

        // Append the outer archive root path
        result.append(ArchiveSupport::archiveRootPath(physicalPath));

        const int n = tokens.size();
        // Append intermediate nested archives if any
        for (int i = 1; i < n - 1; ++i) {
            QStringList subTokens = tokens.mid(0, i + 1);
            result.append(QStringLiteral("archive://") + subTokens.join(QLatin1Char('|')) + QStringLiteral("|/"));
        }

        // Append paths inside the innermost archive
        QString browse = QDir::fromNativeSeparators(tokens.last().trimmed());
        if (browse != QLatin1String("/") && !browse.isEmpty()) {
            if (browse.startsWith(QLatin1Char('/'))) {
                browse.remove(0, 1);
            }
            if (browse.endsWith(QLatin1Char('/'))) {
                browse.chop(1);
            }
            if (!browse.isEmpty()) {
                const QString innerArchiveRoot = QStringLiteral("archive://") + tokens.mid(0, n - 1).join(QLatin1Char('|')) + QStringLiteral("|/");
                const QStringList browseParts = browse.split(QLatin1Char('/'), Qt::SkipEmptyParts);
                QString rel;
                for (const QString &part : browseParts) {
                    if (!rel.isEmpty()) {
                        rel += QLatin1Char('/');
                    }
                    rel += part;
                    result.append(innerArchiveRoot + rel);
                }
            }
        }
        return result;
    }

    if (isProviderUriPath(path)) {
        std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
        const QString normalized = provider ? provider->normalizedPath(path) : FileProviderFactory::normalizePath(path);
        const QString providerPath = normalized.isEmpty() ? path.trimmed() : normalized;
        const QString scheme = provider ? provider->scheme() : uriSchemeForPath(providerPath);
        if (scheme.isEmpty()) {
            return result;
        }

        const QString schemeRoot = scheme + QStringLiteral("://");
        const QString normalizedRoot = provider ? provider->normalizedPath(schemeRoot) : QString{};
        const QString rootPath = normalizedRoot.isEmpty() ? schemeRoot : normalizedRoot;
        if (provider && !providerPath.isEmpty()) {
            QStringList chain;
            QSet<QString> seen;
            QString current = providerPath;
            for (int depth = 0; depth < 64 && !current.isEmpty(); ++depth) {
                const QString normalizedCurrent = provider->normalizedPath(current);
                current = normalizedCurrent.isEmpty() ? current.trimmed() : normalizedCurrent;
                if (current.isEmpty() || seen.contains(current)) {
                    break;
                }
                seen.insert(current);
                chain.prepend(current);
                if (current == rootPath) {
                    break;
                }
                const QString parent = provider->parentPath(current);
                if (parent.isEmpty() || parent == current) {
                    break;
                }
                current = parent;
            }
            if (!chain.isEmpty() && chain.constFirst() != rootPath) {
                chain.prepend(rootPath);
            }
            if (!chain.isEmpty()) {
                return chain;
            }
        }

        result.append(rootPath);
        if (providerPath != rootPath) {
            result.append(providerPath);
        }
        return result;
    }

    const QString normalized = QDir::fromNativeSeparators(path);
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        if (normalized == QLatin1String("/")) {
            result.append(normalized);
        }
        return result;
    }

    QString current;
    int startIndex = 0;
    if (normalized.size() >= 2 && normalized.at(1) == QLatin1Char(':')) {
        current = parts.first() + QStringLiteral("/");
        result.append(current);
        startIndex = 1;
    } else if (normalized.startsWith(QLatin1Char('/'))) {
        current = QStringLiteral("/");
    }

    for (int i = startIndex; i < parts.size(); ++i) {
        const QString part = parts.at(i);
        if (part.isEmpty()) {
            continue;
        }
        if (!current.isEmpty() && !current.endsWith(QLatin1Char('/'))) {
            current += QLatin1Char('/');
        }
        current += part;
        result.append(current);
    }

    return result;
}

QVariantList FilePanelController::breadcrumbEntriesForPath(const QString &path) const
{
    QVariantList result;
    const QString lowerPath = path.toLower();
    if (lowerPath.startsWith(QStringLiteral("instagram://"))) {
        const QString normalized = FileProviderFactory::normalizePath(path).trimmed();
        const QString tail = normalized.mid(QStringLiteral("instagram://").size());
        const QStringList parts = tail.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            const QString kind = parts.at(0).toLower();
            const QString id = parts.at(1);
            QVariantMap entry;
            entry[QStringLiteral("name")] = kind == QLatin1String("user")
                ? id
                : QStringLiteral("%1 %2").arg(kind, id);
            entry[QStringLiteral("path")] = QStringLiteral("instagram://%1/%2").arg(kind, id);
            entry[QStringLiteral("pathKind")] = QStringLiteral("instagram");
            entry[QStringLiteral("isDrive")] = false;
            entry[QStringLiteral("isArchive")] = false;
            entry[QStringLiteral("iconName")] = FileEntryPresentationResolver::breadcrumbIconNameForPath(entry.value(QStringLiteral("path")).toString());
            entry[QStringLiteral("iconRecolorAllowed")] = false;
            result.append(entry);
            if (kind == QLatin1String("user")
                && parts.size() >= 3
                && parts.at(2).compare(QStringLiteral("stories"), Qt::CaseInsensitive) == 0) {
                QVariantMap storiesEntry;
                storiesEntry[QStringLiteral("name")] = QStringLiteral("Stories");
                storiesEntry[QStringLiteral("path")] = QStringLiteral("instagram://user/%1/stories").arg(id);
                storiesEntry[QStringLiteral("pathKind")] = QStringLiteral("instagram");
                storiesEntry[QStringLiteral("isDrive")] = false;
                storiesEntry[QStringLiteral("isArchive")] = false;
                storiesEntry[QStringLiteral("iconName")] = FileEntryPresentationResolver::breadcrumbIconNameForPath(storiesEntry.value(QStringLiteral("path")).toString());
                storiesEntry[QStringLiteral("iconRecolorAllowed")] = false;
                result.append(storiesEntry);
            }
            return result;
        }
    }

    const QStringList paths = breadcrumbPathsForPath(path);
    auto appendEntry = [this, &result](const QString &name, const QString &entryPath, bool isDrive = false) {
        QVariantMap entry;
        entry[QStringLiteral("name")] = name;
        entry[QStringLiteral("path")] = entryPath;
        entry[QStringLiteral("pathKind")] = pathKindFor(entryPath);
        entry[QStringLiteral("isDrive")] = isDrive;
        entry[QStringLiteral("isArchive")] = ArchiveSupport::isArchivePath(entryPath)
            && entryPath.endsWith(QStringLiteral("|/"));
        entry[QStringLiteral("iconName")] = FileEntryPresentationResolver::breadcrumbIconNameForPath(entryPath);
        entry[QStringLiteral("iconRecolorAllowed")] = entry.value(QStringLiteral("iconName")).toString().isEmpty();
        result.append(entry);
    };

    for (int i = 0; i < paths.size(); ++i) {
        const QString &entryPath = paths.at(i);
        const bool isDrive = !ArchiveSupport::isArchivePath(entryPath)
                            && entryPath.size() >= 2
                            && entryPath.at(1) == QLatin1Char(':')
                            && entryPath.endsWith(QLatin1Char('/'));
        QString name;
        if (ArchiveSupport::isArchivePath(entryPath)) {
            if (i == 0) {
                name = ArchiveSupport::physicalArchivePath(entryPath);
            } else if (entryPath.endsWith(QStringLiteral("|/"))) {
                name = ArchiveSupport::archiveFileName(entryPath);
            } else {
                name = fileNameForPath(entryPath);
            }
        } else if (isProviderUriPath(entryPath) && uriSchemeForPath(entryPath) == QLatin1String("ftp")
                   && entryPath == QLatin1String("ftp://")) {
            name = QStringLiteral("FTP");
        } else {
            name = isDrive ? DriveUtils::rootDisplayName(entryPath) : fileNameForPath(entryPath);
        }
        appendEntry(name.isEmpty() ? entryPath : name, entryPath, isDrive);
    }
    return result;
}

void FilePanelController::handleDeviceRemoved(const QString &rootPath, const QString &displayName)
{
    Q_UNUSED(rootPath)

    cancelDirectorySuggestions();
    ++m_navigationRequestId;
    setNavigationPending(false);
    setCurrentItemPath({});
    m_directoryModel.clearSelection();
    m_directoryModel.suppressNextWatchRestart();
    if (m_directoryModel.loading()) {
        m_directoryModel.cancelLoading();
    }

    openPathInternal(QString(DEVICE_ROOT), false, false, RecoveryNavigation);
    const QString label = displayName.trimmed();
    setStatusMessage(label.isEmpty()
                         ? QStringLiteral("Device was removed")
                         : QStringLiteral("%1 was removed").arg(label));
}

void FilePanelController::syncStateFrom(FilePanelController *other)
{
    if (!other || other == this) {
        return;
    }

    const QString sourcePath = other->currentPath();
    if (!sourcePath.isEmpty() && sourcePath != currentPath()) {
        openPath(sourcePath);
    }

    setViewMode(other->viewMode());
    setPanelSortPolicy(int(other->panelSortRole()), int(other->panelSortOrder()));

    DirectoryModel *sourceModel = other->directoryModel();
    DirectoryModel *targetModel = directoryModel();
    if (!sourceModel || !targetModel) {
        return;
    }

    targetModel->setShowHidden(sourceModel->showHidden());
    targetModel->setMixFilesAndFolders(sourceModel->mixFilesAndFolders());
    targetModel->setSearchText(sourceModel->searchText());
}

bool FilePanelController::openPathInternal(const QString &path, bool addToHistory, bool preserveScroll,
                                           NavigationReason reason)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    if (filePanelNavTraceEnabled()) {
        traceFilePanelNav("openPathInternal-begin", path,
                          QStringLiteral("addToHistory=%1 preserveScroll=%2")
                              .arg(addToHistory)
                              .arg(preserveScroll));
    }

    const bool targetIsDeviceRoot = (path == DEVICE_ROOT);
    const bool targetIsFavoritesRoot = (path == FAVORITES_ROOT);
    const bool wasVirtualRoot = isVirtualRoot();

    QString newPath;
    if (targetIsDeviceRoot) {
        newPath = DEVICE_ROOT;
    } else if (targetIsFavoritesRoot) {
        newPath = FAVORITES_ROOT;
    } else if (ArchiveSupport::isArchivePath(path)) {
        newPath = ArchiveSupport::normalizeArchivePath(path);
    } else {
        newPath = FileProviderFactory::normalizePath(path);
    }

    const QString oldPath = currentPath();
    const QString oldOuterArchiveSession = outerArchiveSessionKeyForPath(oldPath);
    const bool loadMoreForCurrentPath = preserveScroll
        && isLoadMorePathForCurrentProviderPath(oldPath, newPath);
    const QString uiNavigationPath = loadMoreForCurrentPath ? oldPath : newPath;
    traceFilePanelNav("openPathInternal-normalized", newPath,
                      QStringLiteral("old=%1 ui=%2 preserveScroll=%3 loadMoreForCurrent=%4 elapsedMs=%5")
                          .arg(QDir::toNativeSeparators(oldPath))
                          .arg(QDir::toNativeSeparators(uiNavigationPath))
                          .arg(preserveScroll)
                          .arg(loadMoreForCurrentPath)
                          .arg(totalTimer.elapsed()));

    if (loadMoreForCurrentPath) {
        emit pathAboutToChange(oldPath, oldPath, true, LoadMoreNavigation);
        QElapsedTimer modelTimer;
        modelTimer.start();
        const bool modelOpened = m_directoryModel.openPath(newPath);
        traceFilePanelNav("openPathInternal-load-more", newPath,
                          QStringLiteral("result=%1 elapsedMs=%2 totalMs=%3")
                              .arg(modelOpened)
                              .arg(modelTimer.elapsed())
                              .arg(totalTimer.elapsed()));
        if (modelOpened) {
            setStatusMessage({});
            setLastError({});
            emit pathNavigated(oldPath);
            return true;
        }
        emit pathNavigationFailed(newPath);
        return false;
    }

    const bool sameLocalPath = !newPath.isEmpty()
        && !targetIsDeviceRoot
        && !targetIsFavoritesRoot
        && !isProviderUriPath(newPath)
        && !isProviderUriPath(oldPath)
        && !ArchiveSupport::isArchivePath(newPath)
        && !ArchiveSupport::isArchivePath(oldPath)
        && samePanelFilesystemPath(newPath, oldPath);
    if (!newPath.isEmpty() && (newPath == oldPath || sameLocalPath)) {
        emit pathNavigated(newPath);
        traceFilePanelNav("openPathInternal-end", newPath,
                          QStringLiteral("result=true reason=same elapsedMs=%1").arg(totalTimer.elapsed()));
        return true;
    }

    traceFilePanelNav("openPathInternal-before-pathAboutToChange", newPath,
                      QStringLiteral("from=%1 elapsedMs=%2")
                          .arg(QDir::toNativeSeparators(oldPath))
                          .arg(totalTimer.elapsed()));
    emit pathAboutToChange(oldPath, uiNavigationPath, preserveScroll, reason);
    setCurrentItemPath({});
    traceFilePanelNav("openPathInternal-after-pathAboutToChange", newPath,
                      QStringLiteral("elapsedMs=%1").arg(totalTimer.elapsed()));

    if (targetIsDeviceRoot || targetIsFavoritesRoot) {
        m_directoryModel.setSearchText({});
        clearCategoryFilterScope();
        if (!oldOuterArchiveSession.isEmpty()) {
            m_approvedNestedArchiveScopeKeys.clear();
            ArchiveFileProvider::invalidateCacheForPath(oldPath);
        }
        setStatusMessage({});
        setLastError({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        setIsDeviceRoot(targetIsDeviceRoot);
        setIsFavoritesRoot(targetIsFavoritesRoot);
        m_directoryModel.clear();
        emit pathNavigated(newPath);
        traceFilePanelNav("openPathInternal-before-currentPathChanged", newPath,
                          QStringLiteral("type=virtual elapsedMs=%1").arg(totalTimer.elapsed()));
        emit currentPathChanged();
        traceFilePanelNav("openPathInternal-after-currentPathChanged", newPath,
                          QStringLiteral("type=virtual elapsedMs=%1").arg(totalTimer.elapsed()));
        emit capabilitiesChanged();
        emit historyChanged();
        traceFilePanelNav("openPathInternal-end", newPath,
                          QStringLiteral("result=true type=virtual elapsedMs=%1").arg(totalTimer.elapsed()));
        return true;
    }

    QElapsedTimer modelTimer;
    modelTimer.start();
    const bool modelOpened = m_directoryModel.openPath(newPath);
    traceFilePanelNav("openPathInternal-directoryModel.openPath", newPath,
                      QStringLiteral("result=%1 elapsedMs=%2 totalMs=%3")
                          .arg(modelOpened)
                          .arg(modelTimer.elapsed())
                          .arg(totalTimer.elapsed()));

    if (modelOpened) {
        updateCategoryFilterForPath(newPath);
        const QString newOuterArchiveSession = outerArchiveSessionKeyForPath(newPath);
        if (!oldOuterArchiveSession.isEmpty() && oldOuterArchiveSession != newOuterArchiveSession) {
            m_approvedNestedArchiveScopeKeys.clear();
            ArchiveFileProvider::invalidateCacheForPath(oldPath);
        }
        m_directoryModel.setSearchText({});
        const bool keepNestedPreparationStatus = !nestedArchiveScopeKeyForPath(newPath).isEmpty()
            && !ArchiveFileProvider::hasCachedContainerForPath(newPath);
        if (!keepNestedPreparationStatus) {
            setStatusMessage({});
        }
        setLastError({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        setIsDeviceRoot(false);
        setIsFavoritesRoot(false);
        emit pathNavigated(uiNavigationPath);
        if (wasVirtualRoot) {
            traceFilePanelNav("openPathInternal-before-currentPathChanged", newPath,
                              QStringLiteral("type=from-virtual elapsedMs=%1").arg(totalTimer.elapsed()));
            emit currentPathChanged();
            traceFilePanelNav("openPathInternal-after-currentPathChanged", newPath,
                              QStringLiteral("type=from-virtual elapsedMs=%1").arg(totalTimer.elapsed()));
            emit capabilitiesChanged();
        }
        emit historyChanged();
        traceFilePanelNav("openPathInternal-end", newPath,
                          QStringLiteral("result=true elapsedMs=%1").arg(totalTimer.elapsed()));
        return true;
    }

    emit pathNavigationFailed(newPath);
    const QString failedScope = nestedArchiveScopeKeyForPath(newPath);
    if (!failedScope.isEmpty()) {
        m_approvedNestedArchiveScopeKeys.remove(failedScope);
    }
    traceFilePanelNav("openPathInternal-end", newPath,
                      QStringLiteral("result=false elapsedMs=%1").arg(totalTimer.elapsed()));
    return false;
}

void FilePanelController::pushHistory(const QString &path)
{
    m_backStack.append(path);
    constexpr qsizetype maxHistory = 64;
    while (m_backStack.size() > maxHistory) {
        m_backStack.removeFirst();
    }
}

bool FilePanelController::removeLastHistoryEntryIfPath(const QString &path)
{
    const QString normalizedPath = m_fileProvider->normalizedPath(path);
    if (normalizedPath.isEmpty() || m_backStack.isEmpty()) {
        return false;
    }

    const QString lastHistoryPath = m_fileProvider->normalizedPath(m_backStack.constLast());
    if (lastHistoryPath.isEmpty() || !samePanelFilesystemPath(lastHistoryPath, normalizedPath)) {
        return false;
    }

    m_backStack.removeLast();
    emit historyChanged();
    return true;
}

void FilePanelController::recoverFromMissingPath(const QString &path, const QString &error)
{
    const QString revealPath = failedNavigationRevealPath(path);
    if (!revealPath.isEmpty()) {
        emit pathRevealRequested(revealPath);
    }

    if (ArchiveSupport::isArchivePath(path)) {
        if (ArchiveFileProvider::errorNeedsPassword(error)) {
            ArchiveFileProvider::clearPasswordForPath(path);
            emit archivePasswordRequested(path,
                                          nestedArchiveDisplayNameForPath(path),
                                          error.isEmpty()
                                              ? QStringLiteral("Archive password required")
                                              : error);
            return;
        }
        setOperationError(error.isEmpty()
                              ? QStringLiteral("Cannot open archive.")
                              : error,
                          path,
                          QStringLiteral("open"));
        emit pathNavigationFailed(path);
        return;
    }

    if (isPortableUriPath(path)) {
        if (portableFailureIndicatesRemoval(error)) {
            handleDeviceRemoved({}, {});
            return;
        }
        setOperationError(error.isEmpty()
                              ? QStringLiteral("Cannot open portable device.")
                              : error,
                          path,
                          QStringLiteral("open"));
        emit pathNavigationFailed(path);
        return;
    }

    const QString normalizedCurrent = m_fileProvider->normalizedPath(currentPath());
    const QString normalizedMissing = m_fileProvider->normalizedPath(path);
    if (normalizedMissing.isEmpty()) {
        return;
    }

    if (m_volumeMonitor) {
        const QString unavailableRoot = m_volumeMonitor->unavailableRootForPath(path);
        if (!unavailableRoot.isEmpty()) {
            handleDeviceRemoved(unavailableRoot, m_volumeMonitor->displayNameForRoot(unavailableRoot));
            return;
        }
    }

    if (!normalizedCurrent.isEmpty() && !samePanelFilesystemPath(normalizedCurrent, normalizedMissing)) {
        removeLastHistoryEntryIfPath(normalizedCurrent);
        if (navigationFailureIndicatesMissingPath(error)) {
            m_directoryModel.refresh();
        }
        setOperationError(error.isEmpty()
                              ? QStringLiteral("Cannot open folder.")
                              : error,
                          path,
                          QStringLiteral("open"));
        emit pathNavigationFailed(path);
        return;
    }

    const int requestId = ++m_navigationRequestId;
    setNavigationPending(true, normalizedMissing);
    QPointer<FilePanelController> self(this);
    (void)QtConcurrent::run([self, normalizedMissing, requestId]() {
        const QString fallback = fallbackPathForMissing(normalizedMissing);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, normalizedMissing, fallback, requestId]() {
            if (!self || requestId != self->m_navigationRequestId) {
                return;
            }

            self->setNavigationPending(false);
            const QString currentNow = self->m_fileProvider->normalizedPath(self->currentPath());
            if (!currentNow.isEmpty() && !samePanelFilesystemPath(currentNow, normalizedMissing)) {
                return;
            }

            if (fallback.isEmpty() || samePanelFilesystemPath(fallback, currentNow)) {
                self->setStatusMessage(QStringLiteral("Folder is no longer available"));
                return;
            }

            self->removeLastHistoryEntryIfPath(fallback);
            self->m_directoryModel.suppressNextWatchRestart();
            if (!self->openPathInternal(fallback, false, false, RecoveryNavigation)) {
                self->setStatusMessage(QStringLiteral("Folder is no longer available"));
                return;
            }

            self->setStatusMessage(QStringLiteral("Folder was removed externally. Moved up to %1")
                                   .arg(self->m_fileProvider->fileName(fallback).isEmpty()
                                            ? fallback
                                            : self->m_fileProvider->fileName(fallback)));
        }, Qt::QueuedConnection);
    });
}
