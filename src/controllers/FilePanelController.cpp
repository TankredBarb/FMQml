#include "FilePanelController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <winioctl.h>
#endif

#include "../core/ArchiveSupport.h"
#include "../core/IsoSupport.h"
#include "../core/LocalFileProvider.h"
#include "../core/MetadataExtractor.h"
#include "../core/DriveUtils.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileError.h"


FilePanelController::FilePanelController(QObject *parent)
    : QObject(parent)
    , m_fileProvider(std::make_unique<LocalFileProvider>())
{
    connect(&m_directoryModel, &DirectoryModel::currentPathChanged, this, &FilePanelController::currentPathChanged);
    connect(&m_directoryModel, &DirectoryModel::directoryUnavailable, this, &FilePanelController::recoverFromMissingPath);
    connect(&m_directoryModel, &DirectoryModel::currentPathChanged, this, &FilePanelController::capabilitiesChanged);
    connect(&m_directoryModel, &DirectoryModel::selectionChanged, this, &FilePanelController::capabilitiesChanged);
    connect(&m_directoryModel, &DirectoryModel::sortRoleChanged, this, [this]() {
        if (m_viewMode == 0 && m_detailsSortRole != m_directoryModel.sortRole()) {
            m_detailsSortRole = m_directoryModel.sortRole();
            emit detailsSortRoleChanged();
        }
    });
    connect(&m_directoryModel, &DirectoryModel::sortOrderChanged, this, [this]() {
        if (m_viewMode == 0 && m_detailsSortOrder != m_directoryModel.sortOrder()) {
            m_detailsSortOrder = m_directoryModel.sortOrder();
            emit detailsSortOrderChanged();
        }
    });
}

bool FilePanelController::isDeviceRoot() const
{
    return m_isDeviceRoot;
}

DirectoryModel::SortRole FilePanelController::detailsSortRole() const
{
    return m_detailsSortRole;
}

void FilePanelController::setDetailsSortRole(DirectoryModel::SortRole role)
{
    if (m_detailsSortRole == role) {
        return;
    }

    m_detailsSortRole = role;
    emit detailsSortRoleChanged();
    if (m_viewMode == 0) {
        m_directoryModel.setSortRole(role);
    }
}

Qt::SortOrder FilePanelController::detailsSortOrder() const
{
    return m_detailsSortOrder;
}

void FilePanelController::setDetailsSortOrder(Qt::SortOrder order)
{
    if (m_detailsSortOrder == order) {
        return;
    }

    m_detailsSortOrder = order;
    emit detailsSortOrderChanged();
    if (m_viewMode == 0) {
        m_directoryModel.setSortOrder(order);
    }
}

void FilePanelController::setIsDeviceRoot(bool value)
{
    if (m_isDeviceRoot == value) return;
    m_isDeviceRoot = value;
    emit isDeviceRootChanged();
}

DirectoryModel *FilePanelController::directoryModel()
{
    return &m_directoryModel;
}

QString FilePanelController::currentPath() const
{
    if (m_isDeviceRoot) {
        return QString(DEVICE_ROOT);
    }
    return m_directoryModel.currentPath();
}

QString FilePanelController::pathKindFor(const QString &path) const
{
    const QString lowerPath = path.toLower();
    if (lowerPath.startsWith(QStringLiteral("archive://"))) {
        return QStringLiteral("archive");
    }
    if (lowerPath.startsWith(QStringLiteral("devices://"))) {
        return QStringLiteral("devices");
    }
    return QStringLiteral("path");
}

QString FilePanelController::fileTypeLabelFor(const QString &suffix, bool isDirectory) const
{
    if (isDirectory) {
        return QStringLiteral("Folder");
    }
    if (suffix.isEmpty()) {
        return QStringLiteral("File");
    }

    const QString s = suffix.toLower();
    if (s == QStringLiteral("png") || s == QStringLiteral("jpg") || s == QStringLiteral("jpeg")
        || s == QStringLiteral("gif") || s == QStringLiteral("webp") || s == QStringLiteral("bmp")
        || s == QStringLiteral("ico") || s == QStringLiteral("svg") || s == QStringLiteral("avif")
        || s == QStringLiteral("heic")) {
        return s.toUpper() + QStringLiteral(" Image");
    }
    if (s == QStringLiteral("pdf")) return QStringLiteral("PDF Document");
    if (s == QStringLiteral("txt")) return QStringLiteral("Text File");
    if (s == QStringLiteral("md")) return QStringLiteral("Markdown");
    if (s == QStringLiteral("json")) return QStringLiteral("JSON");
    if (s == QStringLiteral("xml") || s == QStringLiteral("html") || s == QStringLiteral("htm")) {
        return s.toUpper();
    }
    if (s == QStringLiteral("css")) return QStringLiteral("CSS Stylesheet");
    if (s == QStringLiteral("js") || s == QStringLiteral("ts")) return s.toUpper() + QStringLiteral(" Script");
    if (s == QStringLiteral("cpp") || s == QStringLiteral("c") || s == QStringLiteral("h") || s == QStringLiteral("hpp")) {
        return QStringLiteral("C/C++ Source");
    }
    if (s == QStringLiteral("py")) return QStringLiteral("Python Script");
    if (s == QStringLiteral("rs")) return QStringLiteral("Rust Source");
    if (s == QStringLiteral("go")) return QStringLiteral("Go Source");
    if (s == QStringLiteral("java") || s == QStringLiteral("kt")) {
        return s == QStringLiteral("kt") ? QStringLiteral("Kotlin Source") : QStringLiteral("Java Source");
    }
    if (s == QStringLiteral("mp3") || s == QStringLiteral("flac") || s == QStringLiteral("ogg")
        || s == QStringLiteral("m4a") || s == QStringLiteral("wav") || s == QStringLiteral("wma")) {
        return s.toUpper() + QStringLiteral(" Audio");
    }
    if (s == QStringLiteral("mp4") || s == QStringLiteral("mkv") || s == QStringLiteral("avi")
        || s == QStringLiteral("mov") || s == QStringLiteral("wmv")) {
        return s.toUpper() + QStringLiteral(" Video");
    }
    if (s == QStringLiteral("zip") || s == QStringLiteral("rar") || s == QStringLiteral("7z")
        || s == QStringLiteral("tar") || s == QStringLiteral("gz") || s == QStringLiteral("xz")) {
        return s.toUpper() + QStringLiteral(" Archive");
    }
    if (s == QStringLiteral("exe") || s == QStringLiteral("msi")) {
        return s.toUpper() + QStringLiteral(" Application");
    }
    if (s == QStringLiteral("bat") || s == QStringLiteral("cmd") || s == QStringLiteral("ps1") || s == QStringLiteral("sh")) {
        return QStringLiteral("Script");
    }
    if (s == QStringLiteral("lnk")) return QStringLiteral("Shortcut");
    if (s == QStringLiteral("iso")) return QStringLiteral("Disk Image");
    if (s == QStringLiteral("ttf") || s == QStringLiteral("otf") || s == QStringLiteral("woff") || s == QStringLiteral("woff2")) {
        return QStringLiteral("Font");
    }
    return s.toUpper() + QStringLiteral(" File");
}

bool FilePanelController::isArchiveFilePath(const QString &path) const
{
    return ArchiveSupport::archiveBackendAvailable() && ArchiveSupport::isArchiveFilePath(path);
}

bool FilePanelController::isIsoImageFilePath(const QString &path) const
{
    return IsoSupport::isIsoImagePath(path);
}

QString FilePanelController::archiveExtractionFolderNameForPath(const QString &path) const
{
    if (!isArchiveFilePath(path)) {
        return {};
    }

    const QString fileName = fileNameForPath(path);
    if (fileName.isEmpty()) {
        return {};
    }

    const QString baseName = QFileInfo(fileName).completeBaseName();
    return baseName.isEmpty() ? fileName : baseName;
}

bool FilePanelController::canGoBack() const
{
    return !m_backStack.isEmpty();
}

bool FilePanelController::canGoForward() const
{
    return !m_forwardStack.isEmpty();
}

QString FilePanelController::hoveredPath() const
{
    return m_hoveredPath;
}

QString FilePanelController::currentItemPath() const
{
    return m_currentItemPath;
}

QString FilePanelController::statusMessage() const
{
    return m_statusMessage;
}

QVariantMap FilePanelController::lastError() const
{
    return m_lastError;
}

bool FilePanelController::scrolling() const
{
    return m_scrolling;
}

bool FilePanelController::isReadOnlyContainerPath(const QString &path) const
{
    return ArchiveSupport::isArchivePath(path) || IsoSupport::isIsoImagePath(path);
}

bool FilePanelController::pathCanCreateChildren(const QString &path) const
{
    if (path.isEmpty() || isReadOnlyContainerPath(path)) {
        return false;
    }
    const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
    return capabilities.exists && capabilities.isDirectory && capabilities.access.canCreateChildren;
}

bool FilePanelController::pathCanDelete(const QString &path) const
{
    if (path.isEmpty() || isReadOnlyContainerPath(path)) {
        return false;
    }
    const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
    return capabilities.exists && capabilities.access.canDelete;
}

bool FilePanelController::canCreateInCurrentPath() const
{
    if (m_isDeviceRoot) {
        return false;
    }
    return pathCanCreateChildren(currentPath());
}

bool FilePanelController::canRenameSelection() const
{
    if (m_isDeviceRoot || !pathCanCreateChildren(currentPath())) {
        return false;
    }
    const QStringList paths = selectedPaths();
    if (paths.size() != 1) {
        return false;
    }
    return pathCanDelete(paths.constFirst());
}

bool FilePanelController::canDeleteSelection() const
{
    if (m_isDeviceRoot) {
        return false;
    }
    const QStringList paths = selectedPaths();
    if (paths.isEmpty()) {
        return false;
    }
    for (const QString &path : paths) {
        if (!pathCanDelete(path)) {
            return false;
        }
    }
    return true;
}

bool FilePanelController::canPasteIntoCurrentPath() const
{
    if (m_isDeviceRoot) {
        return false;
    }
    return pathCanCreateChildren(currentPath());
}

void FilePanelController::setHoveredPath(const QString &path)
{
    if (m_hoveredPath == path) {
        return;
    }
    m_hoveredPath = path;
    emit hoveredPathChanged();
}

void FilePanelController::setCurrentItemPath(const QString &path)
{
    if (m_currentItemPath == path) {
        return;
    }
    m_currentItemPath = path;
    emit currentItemPathChanged();
}

void FilePanelController::setScrolling(bool scrolling)
{
    if (m_scrolling == scrolling) {
        return;
    }
    m_scrolling = scrolling;
    emit scrollingChanged();
}

void FilePanelController::setStatusMessage(const QString &message)
{
    m_statusMessage = message;
    emit statusMessageChanged();
}

void FilePanelController::setLastError(const QVariantMap &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void FilePanelController::setOperationError(const QString &message, const QString &path, const QString &operation)
{
    setStatusMessage(message);
    setLastError(FileError::classify(message, path, operation));
}

bool FilePanelController::openPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    if (path == QString(DEVICE_ROOT)) {
        return openPathInternal(path, true);
    }

    if (IsoSupport::isIsoImagePath(path)) {
        emit isoMountRequested(path);
        return true;
    }

    if (ArchiveSupport::archiveBackendAvailable() && ArchiveSupport::isArchiveFilePath(path)) {
        return openPathInternal(ArchiveSupport::archiveRootPath(path), true);
    }

    if (ArchiveSupport::archiveBackendAvailable() && ArchiveSupport::isArchivePath(path)) {
        const QString normalized = ArchiveSupport::normalizeArchivePath(path);
        const QString fileName = ArchiveSupport::archiveFileName(normalized);
        const QString suffix = QFileInfo(fileName).suffix().toLower();
        if (!normalized.endsWith(QStringLiteral("|/")) && ArchiveSupport::isArchiveExtension(suffix)) {
            return openPathInternal(ArchiveSupport::archiveRootPathForPath(normalized), true);
        }
        return openPathInternal(normalized, true);
    }

    if (!m_fileProvider->pathExists(path)) {
        return false;
    }

    return openPathInternal(path, true);
}

void FilePanelController::openRow(int row)
{
    if (m_isDeviceRoot) return;
    if (!m_directoryModel.isDirectoryAt(row)) {
        return;
    }
    openPath(m_directoryModel.pathAt(row));
}

void FilePanelController::openItem(int row)
{
    if (m_isDeviceRoot) return;
    const QString path = m_directoryModel.pathAt(row);
    if (!path.isEmpty()) {
        if (m_directoryModel.isDirectoryAt(row)) {
            openPath(path);
            return;
        }

        if (IsoSupport::isIsoImagePath(path)) {
            emit isoMountRequested(path);
            return;
        }

        if (ArchiveSupport::archiveBackendAvailable() && ArchiveSupport::isArchiveFilePath(path)) {
            openPath(path);
            return;
        }

        if (ArchiveSupport::isArchivePath(path)) {
            const QString suffix = QFileInfo(ArchiveSupport::archiveFileName(path)).suffix().toLower();
            if (ArchiveSupport::isArchiveExtension(suffix)) {
                openPath(path);
                return;
            }
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void FilePanelController::revealInFileManager(int row)
{
    if (m_isDeviceRoot) return;
    const QString path = m_directoryModel.pathAt(row);
    if (path.isEmpty()) {
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(
        ArchiveSupport::isArchivePath(path) ? ArchiveSupport::physicalArchivePath(path) : path);

#if defined(Q_OS_WIN)
    const QString arg = QStringLiteral("/select,\"%1\"").arg(nativePath);
    QProcess::startDetached(QStringLiteral("explorer.exe"), {arg});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), path});
#else
    const QString parent = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::archiveParentPath(path)
        : m_fileProvider->parentPath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(parent));
#endif
}

void FilePanelController::openInTerminal()
{
    if (m_isDeviceRoot) return;
#if defined(Q_OS_WIN)
    const QString path = QDir::toNativeSeparators(
        ArchiveSupport::isArchivePath(currentPath())
            ? ArchiveSupport::physicalArchivePath(currentPath())
            : currentPath());
    QProcess::startDetached(QStringLiteral("wt.exe"),
        {QStringLiteral("-d"), path, QStringLiteral("powershell.exe"),
         QStringLiteral("-NoExit"), QStringLiteral("-Command"),
         QStringLiteral("Set-Location '%1'").arg(path)});
#endif
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
    openPathInternal(previous, false, true);
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
    openPathInternal(next, false);
    emit historyChanged();
}

void FilePanelController::goUp()
{
    if (m_isDeviceRoot) {
        return; // Already at the top
    }
    const QString cp = currentPath();
    const QString parent = ArchiveSupport::isArchivePath(cp)
        ? ArchiveSupport::archiveParentPath(cp)
        : m_fileProvider->parentPath(cp);
    // If parent == current, we are at the drive root — go to devices://
    if (parent.isEmpty() || parent == cp) {
        openPath(QString(DEVICE_ROOT));
    } else {
        openPathInternal(parent, true, true);
    }
}

bool FilePanelController::rename(int row, const QString &newName)
{
    if (m_isDeviceRoot) {
        return false;
    }
    const QString oldPath = m_directoryModel.pathAt(row);
    if (oldPath.isEmpty()) {
        return false;
    }
    if (!pathCanCreateChildren(currentPath()) || !pathCanDelete(oldPath)) {
        setOperationError(QStringLiteral("You do not have permission to rename this item here."),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    return renamePath(oldPath, newName);
}

bool FilePanelController::renamePath(const QString &oldPath, const QString &newName)
{
    if (m_isDeviceRoot) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(oldPath)) {
        setOperationError(QStringLiteral("Archive contents are read-only"),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }
    if (oldPath.isEmpty()) {
        return false;
    }

    if (m_fileProvider->renamePath(oldPath, newName)) {
        setLastError({});
        const QString trimmedName = newName.trimmed();
        const QString newPath = m_fileProvider->childPath(m_fileProvider->parentPath(oldPath), trimmedName);
        if (!m_directoryModel.renamePath(oldPath, newPath)) {
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
        }
        emit entryRenamed(oldPath, newPath);
        emit contentsChanged(m_fileProvider->parentPath(oldPath));
        return true;
    }

    const QString renameMessage = m_fileProvider->lastErrorString().isEmpty()
        ? QStringLiteral("Cannot rename %1").arg(QDir::toNativeSeparators(oldPath))
        : m_fileProvider->lastErrorString();
    setOperationError(renameMessage,
                      oldPath,
                      QStringLiteral("rename"));
    return false;
}

QVariantList FilePanelController::previewBatchRename(const QStringList &paths, const QVariantList &rules)
{
    QList<BatchRenameEngine::RenamePreview> previews = m_renameEngine.generatePreview(paths, rules);
    QVariantList result;
    for (const auto &p : previews) {
        QVariantMap map;
        map["oldPath"] = p.oldPath;
        map["oldName"] = p.oldName;
        map["newName"] = p.newName;
        map["newPath"] = p.newPath;
        map["hasConflict"] = p.hasConflict;
        map["error"] = p.error;
        result.append(map);
    }
    return result;
}

QVariantList FilePanelController::applyBatchRename(const QStringList &paths, const QVariantList &rules)
{
    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path)) {
            setStatusMessage(QStringLiteral("Archive contents are read-only"));
            return {};
        }
    }

    QList<BatchRenameEngine::RenamePreview> previews = m_renameEngine.generatePreview(paths, rules);
    QVariantList results;
    
    // Check conflicts first
    bool hasAnyConflict = false;
    for (const auto &p : previews) {
        if (p.hasConflict) {
            hasAnyConflict = true;
            break;
        }
    }
    
    if (hasAnyConflict) {
        for (const auto &p : previews) {
            QVariantMap map;
            map["oldPath"] = p.oldPath;
            map["oldName"] = p.oldName;
            map["newName"] = p.newName;
            map["newPath"] = p.newPath;
            map["success"] = false;
            map["error"] = p.hasConflict ? p.error : QStringLiteral("Cancelled due to other conflicts");
            results.append(map);
        }
        return results;
    }

    bool allSuccess = true;
    for (const auto &p : previews) {
        QVariantMap map;
        map["oldPath"] = p.oldPath;
        map["oldName"] = p.oldName;
        map["newName"] = p.newName;
        map["newPath"] = p.newPath;

        if (p.newName == p.oldName) {
            map["success"] = true;
            map["error"] = QString();
        } else {
            if (m_fileProvider->renamePath(p.oldPath, p.newName)) {
                if (!m_directoryModel.renamePath(p.oldPath, p.newPath)) {
                    // refresh at the end
                }
                emit entryRenamed(p.oldPath, p.newPath);
                map["success"] = true;
                map["error"] = QString();
            } else {
                allSuccess = false;
                map["success"] = false;
                map["error"] = QStringLiteral("Rename failed (system error)");
            }
        }
        results.append(map);
    }
    
    if (!allSuccess) {
        setStatusMessage(QStringLiteral("Some files could not be renamed"));
    }
    
    refresh();
    return results;
}

bool FilePanelController::createFolder(const QString &name)
{
    if (m_isDeviceRoot) {
        return false;
    }
    if (!canCreateInCurrentPath()) {
        setOperationError(QStringLiteral("You do not have permission to create items in this location."),
                          currentPath(),
                          QStringLiteral("createFolder"));
        return false;
    }
    QString path;
    if (m_fileProvider->createFolder(currentPath(), name, &path)) {
        setLastError({});
        if (!m_directoryModel.insertPath(path)) {
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
        }
        setStatusMessage(QStringLiteral("\"%1\" created").arg(m_fileProvider->fileName(path)));
        emit entryCreated(path);
        emit contentsChanged(currentPath());
        return true;
    }
    const QString folderMessage = m_fileProvider->lastErrorString().isEmpty()
        ? QStringLiteral("Cannot create folder in %1").arg(QDir::toNativeSeparators(currentPath()))
        : m_fileProvider->lastErrorString();
    setOperationError(folderMessage,
                      currentPath(),
                      QStringLiteral("createFolder"));
    return false;
}

bool FilePanelController::createFile(const QString &name)
{
    if (m_isDeviceRoot) {
        return false;
    }
    if (!canCreateInCurrentPath()) {
        setOperationError(QStringLiteral("You do not have permission to create items in this location."),
                          currentPath(),
                          QStringLiteral("createFile"));
        return false;
    }
    QString path;
    if (m_fileProvider->createFile(currentPath(), name, &path)) {
        setLastError({});
        if (!m_directoryModel.insertPath(path)) {
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
        }
        setStatusMessage(QStringLiteral("\"%1\" created").arg(m_fileProvider->fileName(path)));
        emit entryCreated(path);
        emit contentsChanged(currentPath());
        return true;
    }
    const QString fileMessage = m_fileProvider->lastErrorString().isEmpty()
        ? QStringLiteral("Cannot create file in %1").arg(QDir::toNativeSeparators(currentPath()))
        : m_fileProvider->lastErrorString();
    setOperationError(fileMessage,
                      currentPath(),
                      QStringLiteral("createFile"));
    return false;
}

QString FilePanelController::fileNameForPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveFileName(path);
    }
    return m_fileProvider->fileName(path);
}

QString FilePanelController::parentPathForPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveParentPath(path);
    }
    return m_fileProvider->parentPath(path);
}

QString FilePanelController::childPathForCurrent(const QString &name) const
{
    if (ArchiveSupport::isArchivePath(currentPath())) {
        return ArchiveSupport::archiveChildPath(currentPath(), name);
    }
    return m_fileProvider->childPath(currentPath(), name);
}

QString FilePanelController::childPathForPath(const QString &parentPath, const QString &name) const
{
    if (ArchiveSupport::isArchivePath(parentPath)) {
        return ArchiveSupport::archiveChildPath(parentPath, name);
    }
    return m_fileProvider->childPath(parentPath, name);
}

QStringList FilePanelController::breadcrumbPathsForPath(const QString &path) const
{
    QStringList result;
    if (path.isEmpty() || path == QString(DEVICE_ROOT)) {
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

    const QString normalized = QDir::fromNativeSeparators(path);
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
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
    const QStringList paths = breadcrumbPathsForPath(path);
    auto appendEntry = [&result](const QString &name, const QString &entryPath, bool isDrive = false) {
        QVariantMap entry;
        entry[QStringLiteral("name")] = name;
        entry[QStringLiteral("path")] = entryPath;
        entry[QStringLiteral("isDrive")] = isDrive;
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
        } else {
            name = fileNameForPath(entryPath);
        }
        appendEntry(name.isEmpty() ? entryPath : name, entryPath, isDrive);
    }
    return result;
}

void FilePanelController::showProperties(int row)
{
    if (m_isDeviceRoot) return;
    QStringList selected = m_directoryModel.selectedPaths();
    if (selected.isEmpty()) {
        // Fallback: use the path at the given row
        const QString path = m_directoryModel.pathAt(row);
        if (!path.isEmpty()) {
            selected = { path };
        }
    }
    if (!selected.isEmpty()) {
        emit revealProperties(selected);
    }
}

void FilePanelController::fetchMetadataAsync(const QString &path)
{
    if (m_isDeviceRoot) return;
    // Run extraction on a worker thread; marshal result back to GUI thread via signal.
    QThreadPool::globalInstance()->start([this, path]() {
        const QVariantList props = MetadataExtractor::extract(path);
        // Convert the label/value list into a flat map for efficient QML access
        QVariantMap meta;
        for (const QVariant &v : props) {
            const QVariantMap pair = v.toMap();
            const QString label = pair.value(QStringLiteral("label")).toString();
            const QString value = pair.value(QStringLiteral("value")).toString();
            // Normalize keys to camelCase for QML
            if (label == QLatin1String("Dimensions"))  meta[QStringLiteral("resolution")] = value;
            if (label == QLatin1String("Duration"))    meta[QStringLiteral("duration")]   = value;
            if (label == QLatin1String("Artist"))      meta[QStringLiteral("artist")]     = value;
            if (label == QLatin1String("Album"))       meta[QStringLiteral("album")]      = value;
            if (label == QLatin1String("Bitrate"))     meta[QStringLiteral("bitrate")]    = value;
        }
        // Always emit even if empty so delegate knows loading is done
        QMetaObject::invokeMethod(this, [this, path, meta]() {
            emit metadataReady(path, meta);
        }, Qt::QueuedConnection);
    });
}

void FilePanelController::refresh()
{
    clearError();
    m_directoryModel.refresh();
    emit contentsChanged(currentPath());
}

void FilePanelController::clearError()
{
    setStatusMessage({});
    setLastError({});
    m_directoryModel.clearError();
}

QStringList FilePanelController::selectedPaths() const
{
    return m_directoryModel.selectedPaths();
}

QVariantMap FilePanelController::storageInfoForPath(const QString &rootPath) const
{
    const QStorageInfo storage(rootPath);
    if (!storage.isValid() || !storage.isReady()) {
        return {};
    }
    const qint64 total = storage.bytesTotal();
    const qint64 free  = storage.bytesFree();
    const qint64 used  = total - free;
    const double pct   = total > 0 ? static_cast<double>(used) / static_cast<double>(total) : 0.0;
    return {
        {QStringLiteral("total"),      total},
        {QStringLiteral("free"),       free},
        {QStringLiteral("used"),       used},
        {QStringLiteral("percent"),    pct},
        {QStringLiteral("totalStr"),   DriveUtils::formatSize(total)},
        {QStringLiteral("freeStr"),    DriveUtils::formatSize(free)},
        {QStringLiteral("fs"),         QString::fromLatin1(storage.fileSystemType())},
        {QStringLiteral("isCritical"), total > 0 && (static_cast<double>(free) / static_cast<double>(total)) < 0.10},
    };
}

void FilePanelController::ejectDrive(const QString &rootPath)
{
#ifdef Q_OS_WIN
    // Run eject asynchronously so we don't block the GUI thread
    const QString path = rootPath;
    QThreadPool::globalInstance()->start([this, path]() {
        // Build volume path like "\\.\C:"
        QString vol = path;
        if (vol.endsWith('/') || vol.endsWith('\\')) vol.chop(1);
        const QString devPath = QStringLiteral("\\\\.\\%1").arg(vol);
        const std::wstring wdev = devPath.toStdWString();

        HANDLE hDevice = ::CreateFileW(
            wdev.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        bool ok = false;
        if (hDevice != INVALID_HANDLE_VALUE) {
            DWORD bytesReturned = 0;
            ok = ::DeviceIoControl(
                hDevice,
                IOCTL_STORAGE_EJECT_MEDIA,
                nullptr, 0,
                nullptr, 0,
                &bytesReturned,
                nullptr) != 0;
            ::CloseHandle(hDevice);
        }

        QMetaObject::invokeMethod(this, [this, path, ok]() {
            emit ejectFinished(path, ok);
        }, Qt::QueuedConnection);
    });
#else
    Q_UNUSED(rootPath)
    emit ejectFinished(rootPath, false);
#endif
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
    setDetailsSortRole(other->detailsSortRole());
    setDetailsSortOrder(other->detailsSortOrder());

    DirectoryModel *sourceModel = other->directoryModel();
    DirectoryModel *targetModel = directoryModel();
    if (!sourceModel || !targetModel) {
        return;
    }

    targetModel->setShowHidden(sourceModel->showHidden());
    targetModel->setMixFilesAndFolders(sourceModel->mixFilesAndFolders());
    targetModel->setCategoryFilter(sourceModel->categoryFilter());
    targetModel->setSearchText(sourceModel->searchText());
}

bool FilePanelController::openPathInternal(const QString &path, bool addToHistory, bool preserveScroll)
{
    const bool targetIsDeviceRoot = (path == DEVICE_ROOT);
    const bool wasDeviceRoot = m_isDeviceRoot;

    QString newPath;
    if (targetIsDeviceRoot) {
        newPath = DEVICE_ROOT;
    } else if (ArchiveSupport::isArchivePath(path)) {
        newPath = ArchiveSupport::normalizeArchivePath(path);
    } else {
        newPath = m_fileProvider->normalizedPath(path);
    }

    const QString oldPath = currentPath();

    if (!newPath.isEmpty() && newPath == oldPath) {
        return true;
    }

    setCurrentItemPath({});
    emit pathAboutToChange(oldPath, newPath, preserveScroll);

    if (targetIsDeviceRoot) {
        m_directoryModel.setSearchText({});
        m_directoryModel.clearFilters();
        setStatusMessage({});
        setLastError({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        setIsDeviceRoot(true);
        emit pathNavigated(newPath);
        emit currentPathChanged();
        emit historyChanged();
        return true;
    }

    if (m_directoryModel.openPath(newPath)) {
        m_directoryModel.setSearchText({});
        m_directoryModel.clearFilters();
        setStatusMessage({});
        setLastError({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        setIsDeviceRoot(false);
        emit pathNavigated(newPath);
        if (wasDeviceRoot) {
            emit currentPathChanged();
        }
        emit historyChanged();
        return true;
    }

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

QString FilePanelController::fallbackPathForMissing(const QString &path) const
{
    QString candidate = m_fileProvider->normalizedPath(path);
    if (candidate.isEmpty()) {
        return {};
    }

    while (!candidate.isEmpty()) {
        if (m_fileProvider->pathExists(candidate) && m_fileProvider->isDirectory(candidate)) {
            return candidate;
        }

        const QString parent = m_fileProvider->parentPath(candidate);
        if (parent.isEmpty() || parent == candidate) {
            break;
        }
        candidate = parent;
    }

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty() && m_fileProvider->pathExists(home) && m_fileProvider->isDirectory(home)) {
        return m_fileProvider->normalizedPath(home);
    }

    return {};
}

void FilePanelController::recoverFromMissingPath(const QString &path, const QString &error)
{
    const QString normalizedCurrent = m_fileProvider->normalizedPath(currentPath());
    const QString normalizedMissing = m_fileProvider->normalizedPath(path);
    if (normalizedCurrent.isEmpty() || normalizedMissing.isEmpty()) {
        return;
    }

    if (normalizedCurrent != normalizedMissing) {
        return;
    }

    const QString fallback = fallbackPathForMissing(normalizedMissing);
    if (fallback.isEmpty() || fallback == normalizedCurrent) {
        setStatusMessage(QStringLiteral("Folder is no longer available"));
        return;
    }

    if (!openPathInternal(fallback, false)) {
        setStatusMessage(QStringLiteral("Folder is no longer available"));
        return;
    }

    setStatusMessage(QStringLiteral("Folder was removed externally. Moved up to %1")
                     .arg(m_fileProvider->fileName(fallback).isEmpty() ? fallback : m_fileProvider->fileName(fallback)));
    Q_UNUSED(error)
}
int FilePanelController::viewMode() const
{
    return m_viewMode;
}

void FilePanelController::setViewMode(int mode)
{
    if (m_viewMode == mode) return;
    m_viewMode = mode;
    applySortPolicyForCurrentView();
    emit viewModeChanged();
}

void FilePanelController::applySortPolicyForCurrentView()
{
    if (m_viewMode == 0) {
        m_directoryModel.setSortRole(m_detailsSortRole);
        m_directoryModel.setSortOrder(m_detailsSortOrder);
        return;
    }

    // Grid/Brief intentionally ignore the richer Details sorting state.
    // Those views do not expose the same sort dimensions, so we normalize
    // them to a simple name-ascending order when the view mode changes.
    m_directoryModel.setSortRole(DirectoryModel::SortByName);
    m_directoryModel.setSortOrder(Qt::AscendingOrder);
}

QStringList FilePanelController::getDirectorySuggestions(const QString &inputPath) const
{
    QStringList suggestions;
    QString cleanPath = inputPath.trimmed();
    if (cleanPath.isEmpty()) {
        return suggestions;
    }

    // Handle "devices://" virtual path
    if (cleanPath.startsWith(DEVICE_ROOT, Qt::CaseInsensitive)) {
        #ifdef Q_OS_WIN
        for (const QFileInfo &drive : QDir::drives()) {
            QString drivePath = drive.absoluteFilePath();
            suggestions.append(QDir::toNativeSeparators(drivePath));
        }
        #endif
        return suggestions;
    }

    bool isArchive = ArchiveSupport::isArchivePath(cleanPath);

    // Determine the directory to search in, and the prefix we are matching.
    QString searchDir;
    QString prefix;

    if (isArchive) {
        // Keep slashes as-is (forward slash '/') and do not use toNativeSeparators
        if (cleanPath.endsWith(QLatin1Char('|'))) {
            searchDir = cleanPath + QLatin1Char('/');
            prefix = "";
        } else if (cleanPath.endsWith(QLatin1Char('/'))) {
            searchDir = cleanPath;
            prefix = "";
        } else {
            int lastSlash = cleanPath.lastIndexOf(QLatin1Char('/'));
            int lastPipe = cleanPath.lastIndexOf(QLatin1Char('|'));
            int lastSeparator = qMax(lastSlash, lastPipe);
            if (lastSeparator != -1) {
                if (cleanPath.at(lastSeparator) == QLatin1Char('|')) {
                    searchDir = cleanPath.left(lastSeparator + 1) + QLatin1Char('/');
                    prefix = cleanPath.mid(lastSeparator + 1);
                } else {
                    searchDir = cleanPath.left(lastSeparator + 1);
                    prefix = cleanPath.mid(lastSeparator + 1);
                }
            } else {
                searchDir = cleanPath;
                prefix = "";
            }
        }
    } else {
        // Convert all slashes to native for consistency
        QString nativePath = QDir::toNativeSeparators(cleanPath);

        // If path ends with a separator, searchDir is the path itself and prefix is empty
        if (nativePath.endsWith(QDir::separator())) {
            searchDir = nativePath;
            prefix = "";
        } else {
            int lastSeparator = nativePath.lastIndexOf(QDir::separator());
            if (lastSeparator != -1) {
                searchDir = nativePath.left(lastSeparator + 1);
                prefix = nativePath.mid(lastSeparator + 1);
            } else {
                // No separator. E.g. "C:" or "SomeRelativeFolder" or "C"
                if (nativePath.length() == 2 && nativePath.endsWith(':')) {
                    searchDir = nativePath + QDir::separator();
                    prefix = "";
                } else if (nativePath.length() == 1 && nativePath[0].isLetter()) {
                    // List Windows drives if typing single letter prefix
                    #ifdef Q_OS_WIN
                    for (const QFileInfo &drive : QDir::drives()) {
                        QString drivePath = drive.absoluteFilePath();
                        if (drivePath.startsWith(nativePath, Qt::CaseInsensitive)) {
                            suggestions.append(QDir::toNativeSeparators(drivePath));
                        }
                    }
                    #endif
                    return suggestions;
                } else {
                    searchDir = currentPath() + QDir::separator();
                    prefix = nativePath;
                }
            }
        }
    }

    std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(searchDir);
    if (!provider || searchDir.isEmpty() || !provider->pathExists(searchDir) || !provider->isDirectory(searchDir)) {
        return suggestions;
    }

    // Query children of searchDir.
    QStringList childPathsList = provider->childPaths(searchDir, false);
    for (const QString &child : childPathsList) {
        if (provider->isDirectory(child)) {
            QString name = provider->fileName(child);
            if (name.startsWith(prefix, Qt::CaseInsensitive)) {
                QString path = child;
                if (!isArchive) {
                    path = QDir::toNativeSeparators(path);
                    if (!path.endsWith(QDir::separator())) {
                        path += QDir::separator();
                    }
                } else {
                    if (!path.endsWith(QLatin1Char('/'))) {
                        path += QLatin1Char('/');
                    }
                }
                suggestions.append(path);
            }
        }
    }

    suggestions.sort(Qt::CaseInsensitive);
    return suggestions;
}
