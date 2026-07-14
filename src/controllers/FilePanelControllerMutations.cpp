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

bool FilePanelController::rename(int row, const QString &newName)
{
    if (isVirtualRoot()) {
        return false;
    }
    const QString oldPath = m_directoryModel.pathAt(row);
    if (oldPath.isEmpty()) {
        return false;
    }
    if (!(m_fileProvider->capabilities() & FileProvider::Rename)
        || !pathCanCreateChildren(currentPath())
        || !pathCanDelete(oldPath)) {
        setOperationError(QStringLiteral("You do not have permission to rename this item here."),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    return renamePath(oldPath, newName);
}

bool FilePanelController::renamePath(const QString &oldPath, const QString &newName)
{
    if (isVirtualRoot()) {
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
    if (!(m_fileProvider->capabilities() & FileProvider::Rename)
        || !pathCanCreateChildren(m_fileProvider->parentPath(oldPath))
        || !pathCanDelete(oldPath)) {
        setOperationError(QStringLiteral("You do not have permission to rename this item here."),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    if (m_fileProvider->renamePath(oldPath, newName)) {
        setLastError({});
        const QString trimmedName = newName.trimmed();
        const QString newPath = m_fileProvider->childPath(m_fileProvider->parentPath(oldPath), trimmedName);
        FileAccessResolver::invalidate(oldPath);
        FileAccessResolver::invalidate(newPath);
        FileAccessResolver::invalidate(m_fileProvider->parentPath(oldPath));
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

bool FilePanelController::renameAsAdministrator(int row, const QString &newName)
{
#ifdef Q_OS_LINUX
    if (isVirtualRoot()) {
        return false;
    }

    const QString oldPath = m_directoryModel.pathAt(row);
    const QString trimmedName = newName.trimmed();
    if (oldPath.isEmpty() || trimmedName.isEmpty()) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(oldPath) || isProviderUriPath(oldPath)) {
        setOperationError(QStringLiteral("Administrator rename is available for local items only"),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }
    if (trimmedName.contains(QLatin1Char('/')) || trimmedName.contains(QLatin1Char('\\'))) {
        setOperationError(QStringLiteral("The new name is invalid"),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    const QString parentPath = m_fileProvider->parentPath(oldPath);
    const QString newPath = m_fileProvider->childPath(parentPath, trimmedName);
    if (samePanelFilesystemPath(oldPath, newPath)) {
        return true;
    }
    if (m_fileProvider->pathExists(newPath)) {
        setOperationError(QStringLiteral("Cannot rename %1: an item with the same name already exists")
                              .arg(QDir::toNativeSeparators(oldPath)),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    LinuxAdminBroker::Request request;
    request.operation = LinuxAdminBroker::Operation::RenamePath;
    request.sourcePath = oldPath;
    request.destinationPath = newPath;
    const LinuxAdminBroker::Result result = submitLinuxAdminRequest(request);
    if (!result.success) {
        setOperationError(result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage,
                          result.failedPath.isEmpty() ? oldPath : result.failedPath,
                          QStringLiteral("rename"));
        return false;
    }

    setLastError({});
    FileAccessResolver::invalidate(oldPath);
    FileAccessResolver::invalidate(newPath);
    FileAccessResolver::invalidate(parentPath);
    if (!m_directoryModel.renamePath(oldPath, newPath)) {
        refresh();
    } else {
        m_directoryModel.noteLocalMutation();
    }
    emit entryRenamed(oldPath, newPath);
    emit administratorOperationSucceeded();
    emit contentsChanged(parentPath);
    return true;
#else
    Q_UNUSED(row)
    Q_UNUSED(newName)
    return false;
#endif
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
    if (isVirtualRoot()
        || !(m_fileProvider->capabilities() & FileProvider::Rename)
        || !pathCanCreateChildren(currentPath())) {
        QVariantList results;
        for (const QString &path : paths) {
            const QString oldName = fileNameForPath(path);
            QVariantMap map;
            map["oldPath"] = path;
            map["oldName"] = oldName;
            map["newName"] = oldName;
            map["newPath"] = path;
            map["success"] = false;
            map["error"] = QStringLiteral("Cannot rename items in this location");
            results.append(map);
        }
        setOperationError(QStringLiteral("You do not have permission to rename items here."),
                          currentPath(),
                          QStringLiteral("rename"));
        return results;
    }

    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path)) {
            setStatusMessage(QStringLiteral("Archive contents are read-only"));
            return {};
        }
        if (!pathCanDelete(path)) {
            QVariantList results;
            QList<BatchRenameEngine::RenamePreview> previews = m_renameEngine.generatePreview(paths, rules);
            for (const auto &p : previews) {
                QVariantMap map;
                map["oldPath"] = p.oldPath;
                map["oldName"] = p.oldName;
                map["newName"] = p.newName;
                map["newPath"] = p.newPath;
                map["success"] = false;
                map["error"] = p.oldPath == path
                    ? QStringLiteral("Permission denied")
                    : QStringLiteral("Cancelled due to permission failure");
                results.append(map);
            }
            setOperationError(QStringLiteral("You do not have permission to rename one or more selected items."),
                              path,
                              QStringLiteral("rename"));
            return results;
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
                FileAccessResolver::invalidate(p.oldPath);
                FileAccessResolver::invalidate(p.newPath);
                FileAccessResolver::invalidate(m_fileProvider->parentPath(p.oldPath));
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
    if (isVirtualRoot()) {
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
        FileAccessResolver::invalidate(currentPath());
        FileAccessResolver::invalidate(path);
        const bool inserted = m_directoryModel.insertPath(path);
        if (!inserted) {
            scheduleCreatedEntryReveal(path);
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
            scheduleCreatedEntryReveal(path);
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

bool FilePanelController::createFolderAsAdministrator(const QString &name)
{
#ifdef Q_OS_LINUX
    if (isVirtualRoot()) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(currentPath()) || isProviderUriPath(currentPath())) {
        setOperationError(QStringLiteral("Administrator folder creation is available for local folders only"),
                          currentPath(),
                          QStringLiteral("createFolder"));
        return false;
    }

    const QString folderName = uniqueCreationName(m_fileProvider.get(), currentPath(), name, false);
    if (folderName.isEmpty()) {
        return false;
    }

    const QString path = m_fileProvider->childPath(currentPath(), folderName);
    LinuxAdminBroker::Request request;
    request.operation = LinuxAdminBroker::Operation::MakeDirectory;
    request.destinationPath = path;
    const LinuxAdminBroker::Result result = submitLinuxAdminRequest(request);
    if (!result.success) {
        setOperationError(result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage,
                          result.failedPath.isEmpty() ? path : result.failedPath,
                          QStringLiteral("createFolder"));
        return false;
    }

    setLastError({});
    FileAccessResolver::invalidate(currentPath());
    FileAccessResolver::invalidate(path);
    const bool inserted = m_directoryModel.insertPath(path);
    if (!inserted) {
        scheduleCreatedEntryReveal(path);
        refresh();
    } else {
        m_directoryModel.noteLocalMutation();
        scheduleCreatedEntryReveal(path);
    }
    setStatusMessage(QStringLiteral("\"%1\" created").arg(m_fileProvider->fileName(path)));
    emit entryCreated(path);
    emit administratorOperationSucceeded();
    emit contentsChanged(currentPath());
    return true;
#else
    Q_UNUSED(name)
    return false;
#endif
}

bool FilePanelController::createFile(const QString &name)
{
    if (isVirtualRoot()) {
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
        FileAccessResolver::invalidate(currentPath());
        FileAccessResolver::invalidate(path);
        const bool inserted = m_directoryModel.insertPath(path);
        if (!inserted) {
            scheduleCreatedEntryReveal(path);
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
            scheduleCreatedEntryReveal(path);
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

bool FilePanelController::createFileAsAdministrator(const QString &name)
{
#ifdef Q_OS_LINUX
    if (isVirtualRoot()) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(currentPath()) || isProviderUriPath(currentPath())) {
        setOperationError(QStringLiteral("Administrator file creation is available for local folders only"),
                          currentPath(),
                          QStringLiteral("createFile"));
        return false;
    }

    QString fileName = uniqueCreationName(m_fileProvider.get(), currentPath(), name, true);
    if (fileName.isEmpty()) {
        return false;
    }

    const QString path = m_fileProvider->childPath(currentPath(), fileName);
    LinuxAdminBroker::Request request;
    request.operation = LinuxAdminBroker::Operation::CreateFile;
    request.destinationPath = path;
    const LinuxAdminBroker::Result result = submitLinuxAdminRequest(request);
    if (!result.success) {
        setOperationError(result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage,
                          result.failedPath.isEmpty() ? path : result.failedPath,
                          QStringLiteral("createFile"));
        return false;
    }

    setLastError({});
    FileAccessResolver::invalidate(currentPath());
    FileAccessResolver::invalidate(path);
    const bool inserted = m_directoryModel.insertPath(path);
    if (!inserted) {
        scheduleCreatedEntryReveal(path);
        refresh();
    } else {
        m_directoryModel.noteLocalMutation();
        scheduleCreatedEntryReveal(path);
    }
    setStatusMessage(QStringLiteral("\"%1\" created").arg(m_fileProvider->fileName(path)));
    emit entryCreated(path);
    emit administratorOperationSucceeded();
    emit contentsChanged(currentPath());
    return true;
#else
    Q_UNUSED(name)
    return false;
#endif
}

void FilePanelController::scheduleCreatedEntryReveal(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    m_pendingCreatedEntryRevealPath = path;
    m_createdEntryRevealAttempts = 0;
    m_createdEntryRevealTimer.start();
}

