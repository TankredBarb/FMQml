#include "FolderPeekController.h"
#include "FolderPreviewWarmupRegistry.h"

#include "FilePanelController.h"
#include "../core/FileProvider.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileEntrySortPolicy.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QUrl>
#include <QThread>

#include <algorithm>
#include <utility>

namespace {
constexpr int MaxPeekEntries = 1000;

bool localPath(const QString &path)
{
    const QUrl url(path);
    return !url.isValid() || url.scheme().isEmpty() || url.isLocalFile();
}

QVariantMap peekPresentationEntry(const FileEntry &entry)
{
    return {{QStringLiteral("name"), entry.name.left(1024)},
            {QStringLiteral("path"), entry.path.left(1024)},
            {QStringLiteral("isDirectory"), entry.isDirectory},
            {QStringLiteral("suffix"), entry.suffix.left(1024)},
            {QStringLiteral("mimeType"), entry.mimeType.left(1024)},
            {QStringLiteral("hasThumbnail"), entry.hasThumbnail},
            {QStringLiteral("iconName"), entry.iconName.left(1024)}};
}
}

FolderPeekController::FolderPeekController(QObject *parent) : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
    m_pool.setExpiryTimeout(30000);
    m_warmPool.setMaxThreadCount(1);
    m_warmPool.setExpiryTimeout(30000);
}

FolderPeekController::~FolderPeekController()
{
    ++m_generation;
    m_pool.clear();
    m_warmPool.clear();
    m_pool.waitForDone();
    m_warmPool.waitForDone();
}

void FolderPeekController::setSourcePanel(FilePanelController *panel) { m_sourcePanel = panel; }
bool FolderPeekController::isOpen() const { return m_open; }
QString FolderPeekController::currentPath() const { return m_currentPath; }
QString FolderPeekController::state() const { return m_state; }
QVariantList FolderPeekController::entries() const { return m_entries; }
bool FolderPeekController::canGoBack() const { return !m_backStack.isEmpty(); }
QVariantList FolderPeekController::breadcrumbs() const
{
    return m_sourcePanel ? m_sourcePanel->breadcrumbEntriesForPath(m_currentPath) : QVariantList{};
}

void FolderPeekController::openPath(const QString &path, bool showHidden)
{
    ++m_openCount;
    emit statisticsChanged();
    m_showHidden = showHidden;
    m_backStack.clear();
    emit historyChanged();
    if (!m_open) {
        m_open = true;
        emit openChanged();
    }
    load(path, false);
}

void FolderPeekController::navigate(const QString &path) { load(path, true); }

void FolderPeekController::goBack()
{
    if (m_backStack.isEmpty()) return;
    load(m_backStack.constLast(), false, true);
}

void FolderPeekController::goUp()
{
    if (!m_sourcePanel) return;
    const QString parent = m_sourcePanel->parentPathForPath(m_currentPath);
    if (!parent.isEmpty() && parent != m_currentPath) load(parent, true);
}

void FolderPeekController::close()
{
    if (m_loading) {
        ++m_cancellationCount;
        emit statisticsChanged();
    }
    ++m_generation;
    m_pool.clear();
    m_warmPool.clear();
    m_open = false;
    m_currentPath.clear();
    m_entries.clear();
    m_hasMore = false;
    m_backStack.clear();
    m_state = QStringLiteral("idle");
    m_pendingPath.clear();
    if (m_loading) {
        m_loading = false;
        emit loadingChanged();
    }
    emit openChanged();
    emit currentPathChanged();
    emit entriesChanged();
    emit stateChanged();
    emit historyChanged();
}

bool FolderPeekController::openInSourcePanel()
{
    if (!m_sourcePanel || m_currentPath.isEmpty()) return false;
    const bool opened = m_sourcePanel->openPath(m_currentPath);
    if (opened) close();
    return opened;
}

void FolderPeekController::openEntry(const QString &path, bool isDirectory)
{
    if (path.isEmpty()) return;
    if (isDirectory) {
        navigate(path);
        return;
    }
    if (!m_sourcePanel) return;
    m_sourcePanel->openFilePath(path);
}

void FolderPeekController::load(const QString &path, bool addToHistory, bool popBackOnSuccess)
{
    if (!m_open || path.isEmpty()) return;
    if (m_loading) {
        ++m_cancellationCount;
        emit statisticsChanged();
    }
    m_pendingPath = path;
    m_pendingAddToHistory = addToHistory;
    m_pendingPopBack = popBackOnSuccess;
    if (!m_loading) {
        m_loading = true;
        emit loadingChanged();
    }
    const bool initialLoad = m_currentPath.isEmpty();
    if (initialLoad) {
        m_currentPath = path;
        m_entries.clear();
        m_hasMore = false;
        m_state = QStringLiteral("loading");
        emit currentPathChanged();
        emit entriesChanged();
        emit stateChanged();
    }
    const quint64 generation = ++m_generation;

    const bool local = localPath(path);
    const QString resolvedPath = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    const DirectoryModel *directoryModel = m_sourcePanel ? m_sourcePanel->directoryModel() : nullptr;
    const int sortRole = directoryModel ? int(directoryModel->sortRole()) : 0;
    const Qt::SortOrder sortOrder = directoryModel ? directoryModel->sortOrder() : Qt::AscendingOrder;
    const bool mixFilesAndFolders = directoryModel ? directoryModel->mixFilesAndFolders() : false;
    QPointer<FolderPeekController> self(this);
    m_pool.start([self, generation, path, local, resolvedPath, showHidden = m_showHidden,
                  sortRole, sortOrder, mixFilesAndFolders]() {
        QVariantList entries;
        bool hasMore = false;
        QString resultState;
        if (!local) {
            const std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
            if (!provider) {
                resultState = QStringLiteral("unavailable");
            } else {
                const auto cancelled = [self, generation]() {
                    return !self || self->m_generation.load() != generation;
                };
                const BoundedFolderPreviewResult result = provider->boundedFolderPreview(
                    path, showHidden, MaxPeekEntries, cancelled);
                if (cancelled()) return;
                hasMore = result.hasMore;
                if (result.status == BoundedFolderPreviewResult::Status::Ready) {
                    QList<FileEntry> sortedEntries = result.entries;
                    std::stable_sort(sortedEntries.begin(), sortedEntries.end(),
                                     [mixFilesAndFolders, sortRole, sortOrder](const FileEntry &a, const FileEntry &b) {
                        return FileEntrySortPolicy::lessThan(a, b, mixFilesAndFolders, sortRole, sortOrder);
                    });
                    for (const FileEntry &entry : std::as_const(sortedEntries)) {
                        entries.push_back(peekPresentationEntry(entry));
                    }
                    resultState = entries.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
                } else {
                    resultState = QStringLiteral("unavailable");
                }
            }
            if (!self) return;
            const bool quickReady = resultState == QLatin1String("ready")
                || resultState == QLatin1String("empty");
            if (quickReady) {
                QMetaObject::invokeMethod(self, [self, generation, path, resultState, entries, hasMore]() {
                    if (self) self->publish(generation, path, resultState, entries, hasMore);
                }, Qt::QueuedConnection);
            }
            if (hasMore || !quickReady) {
                QMetaObject::invokeMethod(self, [self, generation, path, showHidden,
                                                 sortRole, sortOrder, mixFilesAndFolders,
                                                 commitPending = !quickReady]() {
                    if (self) self->startRemoteWarmup(generation, path, showHidden, sortRole,
                                                      sortOrder, mixFilesAndFolders, commitPending);
                }, Qt::QueuedConnection);
            }
            return;
        }

        const QFileInfo root(resolvedPath);
        if (!root.exists() || !root.isDir() || !root.isReadable()) {
            resultState = QStringLiteral("error");
        } else {
            const QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot
                                          | (showHidden ? QDir::Hidden : QDir::Filter(0));
            QDirIterator iterator(resolvedPath, filters, QDirIterator::NoIteratorFlags);
            QMimeDatabase mimeDatabase;
            QList<FileEntry> scannedEntries;
            while (iterator.hasNext()) {
                if (!self || self->m_generation.load() != generation) return;
                iterator.next();
                if (scannedEntries.size() >= MaxPeekEntries) {
                    hasMore = true;
                    break;
                }
                const QFileInfo info = iterator.fileInfo();
                const bool directory = info.isDir();
                const QString mime = directory ? QStringLiteral("inode/directory")
                                               : mimeDatabase.mimeTypeForFile(info, QMimeDatabase::MatchExtension).name();
                FileEntry entry;
                entry.name = info.fileName();
                entry.path = info.absoluteFilePath();
                entry.suffix = info.suffix().toLower();
                entry.size = info.size();
                entry.modified = info.lastModified();
                entry.created = info.birthTime();
                entry.isDirectory = directory;
                entry.mimeType = mime;
                entry.hasThumbnail = !directory && mime.startsWith(QLatin1String("image/"));
                entry.iconName = directory ? QStringLiteral("folder.svg")
                                           : (entry.hasThumbnail ? QStringLiteral("image.svg")
                                                                 : QStringLiteral("document.svg"));
                scannedEntries.push_back(std::move(entry));
            }
            std::stable_sort(scannedEntries.begin(), scannedEntries.end(),
                             [mixFilesAndFolders, sortRole, sortOrder](const FileEntry &a, const FileEntry &b) {
                return FileEntrySortPolicy::lessThan(a, b, mixFilesAndFolders, sortRole, sortOrder);
            });
            for (const FileEntry &entry : std::as_const(scannedEntries)) {
                entries.push_back(peekPresentationEntry(entry));
            }
            resultState = scannedEntries.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
        }
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, generation, path, resultState, entries, hasMore]() {
            if (self) self->publish(generation, path, resultState, entries, hasMore);
        }, Qt::QueuedConnection);
    });
}

void FolderPeekController::startRemoteWarmup(quint64 generation, const QString &path,
                                             bool showHidden, int sortRole,
                                             Qt::SortOrder sortOrder, bool mixFilesAndFolders,
                                             bool commitPending)
{
    if (m_generation.load() != generation) return;
    QPointer<FolderPeekController> self(this);
    m_warmPool.start([self, generation, path, showHidden, sortRole, sortOrder,
                      mixFilesAndFolders, commitPending]() {
        const std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
        const auto cancelled = [self, generation]() {
            return !self || self->m_generation.load() != generation;
        };
        bool warmed = false;
        if (provider) {
            const bool owner = FolderPreviewWarmupRegistry::tryAcquire(path);
            if (owner) {
                warmed = provider->warmFolderPreviewCache(path, MaxPeekEntries, cancelled);
                FolderPreviewWarmupRegistry::release(path);
            } else {
                while (FolderPreviewWarmupRegistry::isRunning(path) && !cancelled()) QThread::msleep(100);
                warmed = !cancelled();
            }
        }
        if (cancelled()) return;
        QVariantList entries;
        bool hasMore = false;
        QString state = QStringLiteral("unavailable");
        if (warmed) {
            const BoundedFolderPreviewResult result = provider->boundedFolderPreview(
                path, showHidden, MaxPeekEntries, cancelled);
            if (cancelled()) return;
            if (result.status == BoundedFolderPreviewResult::Status::Ready) {
                QList<FileEntry> sortedEntries = result.entries;
                std::stable_sort(sortedEntries.begin(), sortedEntries.end(),
                                 [mixFilesAndFolders, sortRole, sortOrder](const FileEntry &a, const FileEntry &b) {
                    return FileEntrySortPolicy::lessThan(a, b, mixFilesAndFolders, sortRole, sortOrder);
                });
                for (const FileEntry &entry : std::as_const(sortedEntries)) {
                    entries.push_back(peekPresentationEntry(entry));
                }
                hasMore = result.hasMore;
                state = entries.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
            }
        }
        QMetaObject::invokeMethod(self, [self, generation, path, state, entries, hasMore, commitPending]() {
            if (!self || self->m_generation.load() != generation) return;
            if (commitPending) {
                self->publish(generation, path, state, entries, hasMore);
            } else if (self->m_currentPath == path && self->m_pendingPath.isEmpty()
                       && (state == QLatin1String("ready") || state == QLatin1String("empty"))) {
                self->m_state = state;
                self->m_entries = entries;
                self->m_hasMore = hasMore;
                emit self->stateChanged();
                emit self->entriesChanged();
            }
        }, Qt::QueuedConnection);
    });
}

void FolderPeekController::publish(quint64 generation, const QString &path,
                                   const QString &state, const QVariantList &entries, bool hasMore)
{
    if (m_generation.load() != generation || m_pendingPath != path) return;
    const bool success = state == QLatin1String("ready") || state == QLatin1String("empty");
    if (!success && !m_currentPath.isEmpty() && m_currentPath != path) {
        m_pendingPath.clear();
        m_loading = false;
        emit loadingChanged();
        if (state == QLatin1String("error")) {
            ++m_failureCount;
            emit statisticsChanged();
        }
        return;
    }
    if (success && m_currentPath != path) {
        if (m_pendingPopBack && !m_backStack.isEmpty()
            && m_backStack.constLast() == path) {
            m_backStack.removeLast();
            emit historyChanged();
        } else if (m_pendingAddToHistory && !m_currentPath.isEmpty()) {
            m_backStack.push_back(m_currentPath);
            emit historyChanged();
        }
        m_currentPath = path;
        emit currentPathChanged();
    }
    m_state = state;
    m_entries = entries;
    m_hasMore = hasMore;
    m_pendingPath.clear();
    if (m_loading) {
        m_loading = false;
        emit loadingChanged();
    }
    if (state == QLatin1String("error")) {
        ++m_failureCount;
        emit statisticsChanged();
    }
    emit stateChanged();
    emit entriesChanged();
}
