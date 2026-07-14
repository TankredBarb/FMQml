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

void WorkspaceController::requestEjectVolume(const QString &rootPath)
{
    const QString root = QDir::cleanPath(QDir::fromNativeSeparators(rootPath.trimmed()));
    if (root.isEmpty()) {
        emit deviceEjectFailed(rootPath, {}, QStringLiteral("Invalid device path."));
        return;
    }

    const QString managedIsoRoot = m_isoMountManager.managedMountRootForPath(root);
    if (qEnvironmentVariableIntValue("FM_ISO_TRACE") > 0) {
        qInfo().noquote() << "[IsoTrace] eject-route"
                          << "inputRoot=" << rootPath
                          << "normalizedRoot=" << root
                          << "managedRoot=" << managedIsoRoot;
    }
    if (!managedIsoRoot.isEmpty()) {
        unmountIsoRoot(managedIsoRoot);
        return;
    }

    const QString displayName = m_volumeMonitor.displayNameForRoot(root);
    if (!m_volumeMonitor.isKnownUnmountableRoot(root)) {
        const QString message = QStringLiteral("This device cannot be unmounted from FM.");
        m_operationQueue.setStatusMessage(message);
        emit deviceEjectFailed(root, displayName, message);
        return;
    }

    if (m_operationQueue.busy()) {
        const QString message = QStringLiteral("Wait for the current file operation to finish before unmounting this device.");
        m_operationQueue.setStatusMessage(message);
        emit deviceEjectFailed(root, displayName, message);
        return;
    }

    const QStringList affectedRoots = m_volumeMonitor.relatedMountedRoots(root);
    for (const QString &affectedRoot : affectedRoots) {
        for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
            if (m_volumeMonitor.pathBelongsToRoot(panel->currentPath(), affectedRoot)
                || m_volumeMonitor.pathBelongsToRoot(panel->directoryModel()->currentPath(), affectedRoot)) {
                panel->handleDeviceRemoved(affectedRoot, displayName);
            }
        }
        emit deviceEjectStarted(affectedRoot, displayName);
    }
    m_volumeMonitor.requestEject(root);
}

void WorkspaceController::requestMountVolume(const QString &stableDeviceId)
{
    if (m_operationQueue.busy()) {
        m_operationQueue.setStatusMessage(QStringLiteral("Wait for the current file operation to finish before mounting this device."));
        return;
    }
    m_volumeMonitor.requestMount(stableDeviceId);
}

void WorkspaceController::handleVolumeRemoved(const QString &rootPath, const QString &displayName)
{
    bool affectedPanel = false;
    for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
        if (m_volumeMonitor.pathBelongsToRoot(panel->currentPath(), rootPath)
            || m_volumeMonitor.pathBelongsToRoot(panel->directoryModel()->currentPath(), rootPath)) {
            panel->handleDeviceRemoved(rootPath, displayName);
            affectedPanel = true;
        }
    }

    if (affectedPanel) {
        emit deviceRemoved(rootPath, displayName);
    }
}

void WorkspaceController::handleProviderPlaceRemoved(const QString &rootPath,
                                                     const QString &displayName,
                                                     const QString &section)
{
    if (section != QLatin1String("portable") || !isPortablePlaceRoot(rootPath)) {
        return;
    }

    bool affectedPanel = false;
    for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
        if (pathBelongsToProviderPlaceRoot(panel->currentPath(), rootPath)
            || pathBelongsToProviderPlaceRoot(panel->directoryModel()->currentPath(), rootPath)) {
            panel->handleDeviceRemoved(rootPath, displayName);
            affectedPanel = true;
        }
    }

    if (affectedPanel) {
        emit deviceRemoved(rootPath, displayName);
    }
}

void WorkspaceController::handleVolumeEjectFinished(const QString &rootPath, bool success, const QString &message)
{
    const QString displayName = m_volumeMonitor.displayNameForRoot(rootPath);
    if (success) {
        m_operationQueue.setStatusMessage(QStringLiteral("Device disconnected safely"));
        emit deviceEjectSucceeded(rootPath, displayName);
    } else {
        const QString failure = message.isEmpty()
            ? QStringLiteral("Cannot eject device.")
            : QStringLiteral("Cannot eject device: %1").arg(message);
        m_operationQueue.setStatusMessage(failure);
        emit deviceEjectFailed(rootPath, displayName, failure);
    }
    m_placesModel.refresh();
    m_treeModel.refresh();
}

bool WorkspaceController::pathBelongsToVolumeRoot(const QString &path, const QString &rootPath) const
{
    return m_volumeMonitor.pathBelongsToRoot(path, rootPath);
}
