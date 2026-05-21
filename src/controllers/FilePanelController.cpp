#include "FilePanelController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <winioctl.h>
#endif

#include "../core/LocalFileProvider.h"
#include "../core/MetadataExtractor.h"
#include "../core/DriveUtils.h"


FilePanelController::FilePanelController(QObject *parent)
    : QObject(parent)
    , m_fileProvider(std::make_unique<LocalFileProvider>())
{
    connect(&m_directoryModel, &DirectoryModel::currentPathChanged, this, &FilePanelController::currentPathChanged);
    connect(&m_directoryModel, &DirectoryModel::directoryUnavailable, this, &FilePanelController::recoverFromMissingPath);
}

bool FilePanelController::isDeviceRoot() const
{
    return m_isDeviceRoot;
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

QString FilePanelController::statusMessage() const
{
    return m_statusMessage;
}

bool FilePanelController::scrolling() const
{
    return m_scrolling;
}

void FilePanelController::setHoveredPath(const QString &path)
{
    if (m_hoveredPath == path) {
        return;
    }
    m_hoveredPath = path;
    emit hoveredPathChanged();
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
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
}

bool FilePanelController::openPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    if (path == QString(DEVICE_ROOT)) {
        return openPathInternal(path, true);
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
    if (m_directoryModel.isDirectoryAt(row)) {
        openPath(m_directoryModel.pathAt(row));
        return;
    }
    const QString path = m_directoryModel.pathAt(row);
    if (!path.isEmpty()) {
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

    const QString nativePath = QDir::toNativeSeparators(path);

#if defined(Q_OS_WIN)
    const QString arg = QStringLiteral("/select,\"%1\"").arg(nativePath);
    QProcess::startDetached(QStringLiteral("explorer.exe"), {arg});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), path});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_fileProvider->parentPath(path)));
#endif
}

void FilePanelController::openInTerminal()
{
    if (m_isDeviceRoot) return;
#if defined(Q_OS_WIN)
    const QString path = QDir::toNativeSeparators(currentPath());
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
    openPathInternal(previous, false);
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
    const QString parent = m_fileProvider->parentPath(cp);
    // If parent == current, we are at the drive root — go to devices://
    if (parent.isEmpty() || parent == cp) {
        openPath(QString(DEVICE_ROOT));
    } else {
        openPath(parent);
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

    return renamePath(oldPath, newName);
}

bool FilePanelController::renamePath(const QString &oldPath, const QString &newName)
{
    if (m_isDeviceRoot) {
        return false;
    }
    if (oldPath.isEmpty()) {
        return false;
    }

    if (m_fileProvider->renamePath(oldPath, newName)) {
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
    QString path;
    if (m_fileProvider->createFolder(currentPath(), name, &path)) {
        if (!m_directoryModel.insertPath(path)) {
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
        }
        emit entryCreated(path);
        emit contentsChanged(currentPath());
        return true;
    }
    return false;
}

bool FilePanelController::createFile(const QString &name)
{
    if (m_isDeviceRoot) {
        return false;
    }
    QString path;
    if (m_fileProvider->createFile(currentPath(), name, &path)) {
        if (!m_directoryModel.insertPath(path)) {
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
        }
        emit entryCreated(path);
        emit contentsChanged(currentPath());
        return true;
    }
    return false;
}

QString FilePanelController::fileNameForPath(const QString &path) const
{
    return m_fileProvider->fileName(path);
}

QString FilePanelController::parentPathForPath(const QString &path) const
{
    return m_fileProvider->parentPath(path);
}

QString FilePanelController::childPathForCurrent(const QString &name) const
{
    return m_fileProvider->childPath(currentPath(), name);
}

QString FilePanelController::childPathForPath(const QString &parentPath, const QString &name) const
{
    return m_fileProvider->childPath(parentPath, name);
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
    QtConcurrent::run([this, path]() {
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
    setStatusMessage({});
    m_directoryModel.refresh();
    emit contentsChanged(currentPath());
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
    QtConcurrent::run([this, path]() {
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

bool FilePanelController::openPathInternal(const QString &path, bool addToHistory)
{
    const bool targetIsDeviceRoot = (path == DEVICE_ROOT);
    const bool wasDeviceRoot = m_isDeviceRoot;

    QString newPath;
    if (targetIsDeviceRoot) {
        newPath = DEVICE_ROOT;
    } else {
        newPath = m_fileProvider->normalizedPath(path);
    }

    const QString oldPath = currentPath();

    if (!newPath.isEmpty() && newPath == oldPath) {
        return true;
    }

    if (targetIsDeviceRoot) {
        m_directoryModel.setFilterText({});
        setStatusMessage({});
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
        m_directoryModel.setFilterText({});
        setStatusMessage({});
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
    emit viewModeChanged();
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

    // Determine the directory to search in, and the prefix we are matching.
    QString searchDir;
    QString prefix;

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

    if (searchDir.isEmpty() || !m_fileProvider->pathExists(searchDir) || !m_fileProvider->isDirectory(searchDir)) {
        return suggestions;
    }

    // Query children of searchDir.
    QStringList childPathsList = m_fileProvider->childPaths(searchDir, false);
    for (const QString &child : childPathsList) {
        if (m_fileProvider->isDirectory(child)) {
            QString name = m_fileProvider->fileName(child);
            if (name.startsWith(prefix, Qt::CaseInsensitive)) {
                QString path = QDir::toNativeSeparators(child);
                if (!path.endsWith(QDir::separator())) {
                    path += QDir::separator();
                }
                suggestions.append(path);
            }
        }
    }

    suggestions.sort(Qt::CaseInsensitive);
    return suggestions;
}

