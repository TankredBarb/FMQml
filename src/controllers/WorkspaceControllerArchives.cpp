#include "WorkspaceController.h"
#include "../core/ArchiveSupport.h"
#include "../core/ArchiveFileProvider.h"
#include "../core/DriveUtils.h"
#include "../core/FileAccessResolver.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include "../core/FileProviderPluginRegistry.h"
#include <QSysInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <fstream>
#include <string>
#endif
#include "WorkspaceControllerInternal.h"

using namespace WorkspaceControllerInternal;

bool WorkspaceController::requestArchivePasswordForExtractIfNeeded(const QString &archivePath, const QString &destination)
{
    if (archivePath.isEmpty() || destination.isEmpty() || m_operationQueue.busy()) {
        return false;
    }
    if (!ArchiveFileProvider::needsPasswordForPath(archivePath)) {
        return false;
    }

    m_pendingPasswordArchivePath = archivePath;
    m_pendingPasswordExtractDestination = destination;
    emit archivePasswordRequested(
        archivePath,
        QFileInfo(archivePath).fileName(),
        QStringLiteral("Archive password required"));
    return true;
}

void WorkspaceController::extractArchiveTo(const QString &archivePath, const QString &destination)
{
    if (archivePath.isEmpty() || destination.isEmpty()) {
        return;
    }
    if (requestArchivePasswordForExtractIfNeeded(archivePath, destination)) {
        return;
    }
    m_operationQueue.extractTo(QStringList{archivePath}, destination);
}

bool WorkspaceController::canExtractArchivePath(const QString &archivePath) const
{
    return !archivePath.isEmpty() && m_leftPanel.isArchiveFilePath(archivePath);
}

void WorkspaceController::extractArchiveHerePath(const QString &archivePath, const QString &currentFolder)
{
    if (!canExtractArchivePath(archivePath) || currentFolder.isEmpty()) {
        return;
    }
    extractArchiveTo(archivePath, currentFolder);
}

void WorkspaceController::extractArchiveToNamedFolderPath(const QString &archivePath, const QString &currentFolder)
{
    if (!canExtractArchivePath(archivePath) || currentFolder.isEmpty()) {
        return;
    }

    const QFileInfo info(archivePath);
    const QString folderName = archiveExtractionBaseName(info.fileName());
    if (folderName.isEmpty()) {
        return;
    }

    QDir currentDir(currentFolder);
    QString destination = currentDir.filePath(folderName);
    if (QFileInfo::exists(destination)) {
        for (int i = 1; i < 10000; ++i) {
            const QString candidate = currentDir.filePath(QStringLiteral("%1 copy %2").arg(folderName).arg(i));
            if (!QFileInfo::exists(candidate)) {
                destination = candidate;
                break;
            }
        }
    }

    extractArchiveTo(archivePath, destination);
}

void WorkspaceController::submitArchivePassword(const QString &path, const QString &password)
{
    if (path.isEmpty() || password.isEmpty()) {
        return;
    }

    ArchiveFileProvider::setPasswordForPath(path, password);
    if (m_pendingPasswordArchivePath != path || m_pendingPasswordExtractDestination.isEmpty()) {
        return;
    }

    const QString archivePath = m_pendingPasswordArchivePath;
    const QString destination = m_pendingPasswordExtractDestination;
    m_pendingPasswordArchivePath.clear();
    m_pendingPasswordExtractDestination.clear();
    m_operationQueue.extractTo(QStringList{archivePath}, destination);
}

void WorkspaceController::cancelArchivePassword(const QString &path)
{
    if (!path.isEmpty()) {
        ArchiveFileProvider::clearPasswordForPath(path);
    }
    if (m_pendingPasswordArchivePath == path) {
        m_pendingPasswordArchivePath.clear();
        m_pendingPasswordExtractDestination.clear();
    }
    m_operationQueue.reportError(QStringLiteral("Archive password required"),
                                 path,
                                 QStringLiteral("extract"));
}

bool WorkspaceController::canMountIsoPath(const QString &path) const
{
    return m_isoMountManager.canMountIsoPath(path);
}

void WorkspaceController::requestMountIso(const QString &path)
{
    if (!canMountIsoPath(path)) {
        return;
    }
    const QString mountedRoot = m_isoMountManager.mountedRootForImage(path);
    if (!mountedRoot.isEmpty()) {
        (m_activePanel == 0 ? &m_leftPanel : &m_rightPanel)->openPath(mountedRoot);
        return;
    }
    emit mountIsoRequested(path);
}

void WorkspaceController::mountIsoToLetter(const QString &path, const QString &letter)
{
    m_isoMountManager.mountIsoToLetter(path, letter);
}

void WorkspaceController::mountIsoAutomatically(const QString &path)
{
    m_isoMountManager.mountIsoToLetter(path, {});
}

bool WorkspaceController::isManagedIsoMountRoot(const QString &rootPath) const
{
    return m_isoMountManager.isManagedMountRoot(rootPath);
}

bool WorkspaceController::isInsideManagedIsoMount(const QString &path) const
{
    return m_isoMountManager.isInsideManagedMount(path);
}

void WorkspaceController::unmountIsoRoot(const QString &rootPath)
{
    const QString managedRoot = m_isoMountManager.managedMountRootForPath(rootPath);
    if (managedRoot.isEmpty()) {
        m_isoMountManager.unmountIsoRoot(rootPath);
        return;
    }
    if (m_operationQueue.busy()) {
        m_operationQueue.setStatusMessage(
            QStringLiteral("Wait for the current file operation to finish before unmounting this ISO."));
        return;
    }

    const QString displayName = QStringLiteral("ISO image");
    for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
        const bool panelInsideMount =
            m_isoMountManager.managedMountRootForPath(panel->currentPath()) == managedRoot
            || m_isoMountManager.managedMountRootForPath(panel->directoryModel()->currentPath()) == managedRoot;
        if (panelInsideMount) {
            panel->handleDeviceRemoved(managedRoot, displayName);
        }
    }
    emit deviceEjectStarted(managedRoot, displayName);

    // Let panel models and QuickLook release their current paths before
    // udisksctl checks whether the mount is busy.
    QTimer::singleShot(150, this, [this, managedRoot]() {
        m_isoMountManager.unmountIsoRoot(managedRoot);
    });
}
