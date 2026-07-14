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

void WorkspaceController::requestDelete(const QStringList &paths, const QString &label, const QVariantList &items)
{
    if (paths.isEmpty()) {
        return;
    }
    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path)) {
            m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
            return;
        }
        if (isProviderUriPath(path)) {
            continue;
        }
        FileAccessResolver::invalidate(path);
        const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
        if (!capabilities.exists || !capabilities.access.canDelete) {
            m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be deleted from this location."));
            return;
        }
    }
    const QVariantMap details = deleteRequestDetails(paths, label);
    if (details.value(QStringLiteral("blocked")).toBool()) {
        const QString message = details.value(QStringLiteral("subtitle")).toString();
        m_operationQueue.setStatusMessage(message.isEmpty()
                                              ? QStringLiteral("Deletion is blocked for this protected location.")
                                              : message);
        return;
    }
    emit deleteRequested(paths, label, items);
}

void WorkspaceController::requestDeleteAsAdministrator(const QStringList &paths, const QString &label, const QVariantList &items)
{
#ifdef Q_OS_LINUX
    if (paths.size() != 1) {
        m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator supports one item at a time."));
        return;
    }
    const QString path = paths.constFirst();
    if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path) || isProviderUriPath(path)) {
        m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator is available for local items only."));
        return;
    }

    const QVariantMap details = deleteRequestDetails(paths, label);
    if (details.value(QStringLiteral("blocked")).toBool()) {
        const QString message = details.value(QStringLiteral("subtitle")).toString();
        m_operationQueue.setStatusMessage(message.isEmpty()
                                              ? QStringLiteral("Deletion is blocked for this protected location.")
                                              : message);
        return;
    }
    emit deleteAsAdministratorRequested(paths, label, items);
#else
    Q_UNUSED(paths)
    Q_UNUSED(label)
    Q_UNUSED(items)
    m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator is available on Linux only."));
#endif
}

bool WorkspaceController::confirmDelete(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return false;
    }

    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path)) {
            m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
            return false;
        }
        if (isProviderUriPath(path)) {
            continue;
        }
        FileAccessResolver::invalidate(path);
        const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
        if (!capabilities.exists || !capabilities.access.canDelete) {
            m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be deleted from this location."));
            return false;
        }
    }

    const QVariantMap details = deleteRequestDetails(paths, {});
    if (details.value(QStringLiteral("blocked")).toBool()) {
        const QString message = details.value(QStringLiteral("subtitle")).toString();
        m_operationQueue.setStatusMessage(message.isEmpty()
                                              ? QStringLiteral("Deletion is blocked for this protected location.")
                                              : message);
        return false;
    }

    m_operationQueue.deletePaths(paths);
    return true;
}

bool WorkspaceController::confirmDeleteAsAdministrator(const QStringList &paths)
{
#ifdef Q_OS_LINUX
    if (paths.size() != 1) {
        m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator supports one item at a time."));
        return false;
    }
    const QString path = paths.constFirst();
    if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path) || isProviderUriPath(path)) {
        m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator is available for local items only."));
        return false;
    }
    const QVariantMap details = deleteRequestDetails(paths, {});
    if (details.value(QStringLiteral("blocked")).toBool()) {
        const QString message = details.value(QStringLiteral("subtitle")).toString();
        m_operationQueue.setStatusMessage(message.isEmpty()
                                              ? QStringLiteral("Deletion is blocked for this protected location.")
                                              : message);
        return false;
    }

    m_operationQueue.deletePathsAsAdministrator(paths);
    return true;
#else
    Q_UNUSED(paths)
    m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator is available on Linux only."));
    return false;
#endif
}

QVariantMap WorkspaceController::deleteRequestDetails(const QStringList &paths, const QString &label) const
{
    Q_UNUSED(label)

    const int itemCount = paths.size();
    bool allProviderPaths = itemCount > 0;
    QString providerScheme;
    for (const QString &path : paths) {
        if (!isProviderUriPath(path)) {
            allProviderPaths = false;
            break;
        }
        const QString scheme = uriSchemeForPath(path);
        if (providerScheme.isEmpty()) {
            providerScheme = scheme;
        } else if (providerScheme != scheme) {
            allProviderPaths = false;
            break;
        }
    }

    if (allProviderPaths) {
        const bool googleDrive = providerScheme == QLatin1String("gdrive");
        return makeDeleteDetails(false,
                                 false,
                                 false,
                                 itemCount == 1
                                     ? (googleDrive ? QStringLiteral("Move item to Trash?")
                                                    : QStringLiteral("Delete remote item?"))
                                     : (googleDrive ? QStringLiteral("Move %1 items to Trash?").arg(itemCount)
                                                    : QStringLiteral("Delete %1 remote items?").arg(itemCount)),
                                 googleDrive
                                     ? QStringLiteral("Google Drive items will be moved to Trash.")
                                     : QStringLiteral("The remote provider will handle deletion."),
                                 {},
                                 {},
                                 googleDrive ? QStringLiteral("Move to Trash") : QStringLiteral("Delete"));
    }

    int protectedWarningCount = 0;
    int readOnlyWarningCount = 0;
    int systemWarningCount = 0;
    QString firstProtectedWarningPath;
    QString firstBlockedPath;

    for (const QString &path : paths) {
        if (!path.isEmpty() && !ArchiveSupport::isArchivePath(path) && !isProviderUriPath(path)) {
            FileAccessResolver::invalidate(path);
        }
    }

#ifdef Q_OS_WIN
    const QString homePath = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    const QString windowsPath = QDir::cleanPath(qEnvironmentVariable("SystemRoot"));
    const QString programFilesPath = QDir::cleanPath(qEnvironmentVariable("ProgramFiles"));
    const QString programFilesX86Path = QDir::cleanPath(qEnvironmentVariable("ProgramFiles(x86)"));
#endif

    for (const QString &path : paths) {
        if (path.isEmpty() || ArchiveSupport::isArchivePath(path)
            || path.startsWith(QStringLiteral("devices://"), Qt::CaseInsensitive)
            || path.startsWith(QStringLiteral("favorites://"), Qt::CaseInsensitive)) {
            continue;
        }

        const QFileInfo info(path);
#ifdef Q_OS_WIN
        if (info.exists() && info.isRoot()) {
            firstBlockedPath = nativeDisplayPath(path);
            break;
        }

        if (deletePolicyPathEquals(path, windowsPath)
            || deletePolicyPathEquals(path, programFilesPath)
            || deletePolicyPathEquals(path, programFilesX86Path)
            || deletePolicyPathEquals(path, homePath)) {
            firstBlockedPath = nativeDisplayPath(path);
            break;
        }

        if (deletePolicyIsChildOfPath(path, windowsPath)
            || deletePolicyIsChildOfPath(path, programFilesPath)
            || deletePolicyIsChildOfPath(path, programFilesX86Path)) {
            ++protectedWarningCount;
            if (firstProtectedWarningPath.isEmpty()) {
                firstProtectedWarningPath = nativeDisplayPath(path);
            }
        }
#endif

        const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
        if (capabilities.attributes.readOnly) {
            ++readOnlyWarningCount;
        }
        if (capabilities.attributes.system) {
            ++systemWarningCount;
            if (firstProtectedWarningPath.isEmpty()) {
                firstProtectedWarningPath = nativeDisplayPath(path);
            }
        }
    }

    if (!firstBlockedPath.isEmpty()) {
        const QString title = itemCount == 1
            ? QStringLiteral("Deletion blocked")
            : QStringLiteral("Deletion blocked for protected items");
        const QString subtitle = QStringLiteral("This protected location cannot be permanently deleted from FM.");
        const QString details = QStringLiteral("Blocked path: %1").arg(firstBlockedPath);
        return makeDeleteDetails(true,
                                 false,
                                 false,
                                 title,
                                 subtitle,
                                 details,
                                 {},
                                 QStringLiteral("Close"));
    }

    const bool protectedWarning = protectedWarningCount > 0 || systemWarningCount > 0;
    const bool bulkWarning = itemCount >= 20;
    const bool readOnlyAttributeWarning = readOnlyWarningCount > 0;
    const bool requiresExplicitConfirmation = protectedWarning || bulkWarning;

    if (protectedWarning) {
        const QString title = itemCount == 1
            ? QStringLiteral("Delete from a protected location?")
            : QStringLiteral("Delete protected items?");
        const QString subtitle = QStringLiteral("These items are in a sensitive location and will be deleted permanently.");
        const QString details = firstProtectedWarningPath.isEmpty()
            ? QStringLiteral("Review this selection carefully before continuing.")
            : QStringLiteral("Protected location detected: %1").arg(firstProtectedWarningPath);
        return makeDeleteDetails(false,
                                 true,
                                 requiresExplicitConfirmation,
                                 title,
                                 subtitle,
                                 details,
                                 QStringLiteral("DELETE"),
                                 QStringLiteral("Delete Forever"));
    }

    if (bulkWarning || readOnlyAttributeWarning) {
        QString title = itemCount == 1 ? QStringLiteral("Delete item?") : QStringLiteral("Delete %1 items?").arg(itemCount);
        QString subtitle = QStringLiteral("This action cannot be undone.");
        QString details;
        if (bulkWarning) {
            details = QStringLiteral("This selection contains %1 items. Permanent deletion will start immediately.").arg(itemCount);
        }
        if (readOnlyAttributeWarning) {
            if (!details.isEmpty()) {
                details += QLatin1Char(' ');
            }
            details += readOnlyWarningCount == 1
                ? QStringLiteral("One selected item is marked read-only.")
                : QStringLiteral("%1 selected items are marked read-only.").arg(readOnlyWarningCount);
        }
        return makeDeleteDetails(false,
                                 bulkWarning || readOnlyAttributeWarning,
                                 requiresExplicitConfirmation,
                                 title,
                                 subtitle,
                                 details,
                                 requiresExplicitConfirmation ? QStringLiteral("DELETE") : QString(),
                                 QStringLiteral("Delete Forever"));
    }

    return makeDeleteDetails(false,
                             false,
                             false,
                             itemCount == 1 ? QStringLiteral("Delete item?") : QStringLiteral("Delete %1 items?").arg(itemCount),
                             QStringLiteral("This action cannot be undone."),
                             {},
                             {},
                             QStringLiteral("Delete Forever"));
}
