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

void WorkspaceController::copyActiveSelectionToOpposite()
{
    if (!m_splitEnabled) {
        return;
    }
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    copyDroppedSelectionToPanel(m_activePanel, panelBySide(m_activePanel)->selectedPaths(),
                                m_activePanel == 0 ? 1 : 0, destination->currentPath());
}

QVariantMap WorkspaceController::oppositePanelDropCapabilities(int sourcePanel,
                                                               const QStringList &sources,
                                                               int destinationPanel)
{
    QVariantMap result;
    result.insert(QStringLiteral("canCopy"), false);
    result.insert(QStringLiteral("canMove"), false);
    result.insert(QStringLiteral("reason"), QString());
    result.insert(QStringLiteral("copyReason"), QString());
    result.insert(QStringLiteral("moveReason"), QString());
    result.insert(QStringLiteral("destinationPath"), QString());

    FilePanelController *source = panelBySide(sourcePanel);
    FilePanelController *destination = panelBySide(destinationPanel);
    if (!m_splitEnabled) {
        result[QStringLiteral("reason")] = QStringLiteral("Split view is required.");
        return result;
    }
    if (!source || !destination || source == destination || (sourcePanel + destinationPanel) != 1) {
        result[QStringLiteral("reason")] = QStringLiteral("Drop target must be the opposite panel.");
        return result;
    }
    result[QStringLiteral("destinationPath")] = destination->currentPath();
    if (sources.isEmpty()) {
        result[QStringLiteral("reason")] = QStringLiteral("No selected items to drop.");
        return result;
    }
    if (m_operationQueue.busy()) {
        result[QStringLiteral("reason")] = QStringLiteral("Another file operation is already running.");
        return result;
    }
    if (source->isVirtualRoot() || destination->isVirtualRoot()) {
        result[QStringLiteral("reason")] = QStringLiteral("Cannot drop from or to a virtual root.");
        return result;
    }

    QString copyReason;
    if (!source->canCopyPaths(sources)) {
        copyReason = QStringLiteral("One or more selected items cannot be copied from this location.");
    } else if (!destination->canCreateInCurrentPath()) {
        copyReason = QStringLiteral("You do not have permission to write items to this location.");
    } else {
        bool allSourcesInDestination = true;
        for (const QString &sourcePath : sources) {
            if (ArchiveSupport::isArchivePath(sourcePath)) {
                allSourcesInDestination = false;
                break;
            }
            const QString sourceParent = destination->parentPathForPath(sourcePath);
            if (normalizedLocalPath(sourceParent) != normalizedLocalPath(destination->currentPath())) {
                allSourcesInDestination = false;
                break;
            }
        }
        if (allSourcesInDestination) {
            copyReason = QStringLiteral("Source and destination are the same folder.");
        }
    }

    const bool canCopy = copyReason.isEmpty();
    result[QStringLiteral("canCopy")] = canCopy;
    result[QStringLiteral("copyReason")] = copyReason;

    QString moveReason;
    if (!canCopy) {
        moveReason = copyReason;
    } else if (isProviderUriPath(source->currentPath()) || isProviderUriPath(destination->currentPath())) {
        moveReason = QStringLiteral("Move is not supported for remote providers. Use copy instead.");
    } else if (!source->canDeletePaths(sources)) {
        moveReason = QStringLiteral("You do not have permission to move the selected items from this location.");
    }
    result[QStringLiteral("canMove")] = moveReason.isEmpty();
    result[QStringLiteral("moveReason")] = moveReason;
    if (!canCopy) {
        result[QStringLiteral("reason")] = copyReason;
    }
    return result;
}

QVariantMap WorkspaceController::externalDropCapabilities(const QVariantList &urls,
                                                          int destinationPanel,
                                                          const QString &destinationPath)
{
    QVariantMap result;
    result.insert(QStringLiteral("canCopy"), false);
    result.insert(QStringLiteral("reason"), QString());
    result.insert(QStringLiteral("destinationPath"), QString());
    result.insert(QStringLiteral("acceptedPaths"), QStringList());
    result.insert(QStringLiteral("rejectedPaths"), QStringList());
    result.insert(QStringLiteral("conflictCount"), 0);
    result.insert(QStringLiteral("invalidCount"), 0);

    FilePanelController *destination = panelBySide(destinationPanel);
    if (!destination) {
        result[QStringLiteral("reason")] = QStringLiteral("Invalid drop destination.");
        return result;
    }

    const QString currentDestinationPath = destination->currentPath();
    result[QStringLiteral("destinationPath")] = currentDestinationPath;
    if (currentDestinationPath.isEmpty()
        || !pathsReferToSameDropDestination(destinationPath, currentDestinationPath)) {
        result[QStringLiteral("reason")] = QStringLiteral("Destination changed before drop completed.");
        return result;
    }
    if (m_operationQueue.busy()) {
        result[QStringLiteral("reason")] = QStringLiteral("Another file operation is already running.");
        return result;
    }
    if (destination->isVirtualRoot()) {
        result[QStringLiteral("reason")] = QStringLiteral("Cannot drop files into a virtual root.");
        return result;
    }
    if (!isLocalFilesystemPath(currentDestinationPath)
        || ArchiveSupport::isArchivePath(currentDestinationPath)
        || m_isoMountManager.isInsideManagedMount(currentDestinationPath)
        || isProviderUriPath(currentDestinationPath)) {
        result[QStringLiteral("reason")] = QStringLiteral("External drops are supported for local folders only.");
        return result;
    }
    if (!destination->canCreateInCurrentPath()) {
        result[QStringLiteral("reason")] = QStringLiteral("You do not have permission to write items to this location.");
        return result;
    }
    if (urls.isEmpty()) {
        result[QStringLiteral("reason")] = QStringLiteral("Drop local files only.");
        return result;
    }

    QStringList acceptedPaths;
    QStringList rejectedPaths;
    QStringList acceptedNames;
    int conflictCount = 0;
    int invalidCount = 0;
    const QDir destinationDir(currentDestinationPath);

    for (const QVariant &urlValue : urls) {
        const QString sourcePath = localPathFromUrlVariant(urlValue);
        const QFileInfo sourceInfo(sourcePath);
        bool valid = !sourcePath.isEmpty()
            && !ArchiveSupport::isArchivePath(sourcePath)
            && isLocalFilesystemPath(sourcePath)
            && sourceInfo.exists();
        if (valid) {
            FileAccessResolver::invalidate(sourcePath);
            const FileCapabilityInfo sourceCapabilities = FileAccessResolver::resolve(sourcePath);
            valid = sourceCapabilities.exists
                && (sourceCapabilities.isDirectory
                    ? sourceCapabilities.access.canBrowse
                    : sourceCapabilities.access.canRead);
        }
        if (!valid) {
            if (!sourcePath.isEmpty()) {
                rejectedPaths.append(sourcePath);
            }
            ++invalidCount;
            continue;
        }

        const QString fileName = sourceInfo.fileName();
        const QString destinationChildPath = destinationDir.filePath(fileName);
        const QString normalizedName = normalizedLocalPath(fileName);
        if (fileName.isEmpty()
            || QFileInfo::exists(destinationChildPath)
            || acceptedNames.contains(normalizedName)) {
            rejectedPaths.append(sourcePath);
            ++conflictCount;
            continue;
        }

        acceptedNames.append(normalizedName);
        acceptedPaths.append(sourcePath);
    }

    result[QStringLiteral("acceptedPaths")] = acceptedPaths;
    result[QStringLiteral("rejectedPaths")] = rejectedPaths;
    result[QStringLiteral("conflictCount")] = conflictCount;
    result[QStringLiteral("invalidCount")] = invalidCount;

    if (acceptedPaths.isEmpty()) {
        if (conflictCount > 0 && invalidCount == 0) {
            result[QStringLiteral("reason")] = QStringLiteral("All dropped items already exist in the destination.");
        } else if (conflictCount > 0) {
            result[QStringLiteral("reason")] = QStringLiteral("No dropped local files can be copied; some already exist.");
        } else {
            result[QStringLiteral("reason")] = QStringLiteral("Drop local files only.");
        }
        return result;
    }

    result[QStringLiteral("canCopy")] = true;
    if (conflictCount > 0 || invalidCount > 0) {
        result[QStringLiteral("reason")] = externalDropStatusMessage(acceptedPaths.size(), conflictCount, invalidCount);
    }
    return result;
}

bool WorkspaceController::copyDroppedSelectionToPanel(int sourcePanel,
                                                      const QStringList &sources,
                                                      int destinationPanel,
                                                      const QString &destinationPath)
{
    const QVariantMap capabilities = oppositePanelDropCapabilities(sourcePanel, sources, destinationPanel);
    if (!capabilities.value(QStringLiteral("canCopy")).toBool()) {
        QString reason = capabilities.value(QStringLiteral("copyReason")).toString();
        if (reason.isEmpty()) {
            reason = capabilities.value(QStringLiteral("reason")).toString();
        }
        m_operationQueue.setStatusMessage(reason);
        return false;
    }
    FilePanelController *destination = panelBySide(destinationPanel);
    if (!destination || !pathsReferToSameDropDestination(destinationPath, destination->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Destination changed before drop completed."));
        return false;
    }
    return copyPathsToPanel(sources, destination);
}

bool WorkspaceController::copyExternalUrlsToPanel(const QVariantList &urls,
                                                  int destinationPanel,
                                                  const QString &destinationPath)
{
    const QVariantMap capabilities = externalDropCapabilities(urls, destinationPanel, destinationPath);
    if (!capabilities.value(QStringLiteral("canCopy")).toBool()) {
        const QString reason = capabilities.value(QStringLiteral("reason")).toString();
        m_operationQueue.setStatusMessage(reason.isEmpty()
                                              ? QStringLiteral("Drop local files only.")
                                              : reason);
        return false;
    }

    FilePanelController *destination = panelBySide(destinationPanel);
    if (!destination || !pathsReferToSameDropDestination(destinationPath, destination->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Destination changed before drop completed."));
        return false;
    }

    const QStringList acceptedPaths = capabilities.value(QStringLiteral("acceptedPaths")).toStringList();
    if (acceptedPaths.isEmpty()) {
        m_operationQueue.setStatusMessage(QStringLiteral("Drop local files only."));
        return false;
    }

    m_operationQueue.copyTo(acceptedPaths, destination->currentPath());
    const int conflictCount = capabilities.value(QStringLiteral("conflictCount")).toInt();
    const int invalidCount = capabilities.value(QStringLiteral("invalidCount")).toInt();
    if (conflictCount > 0 || invalidCount > 0) {
        m_operationQueue.setStatusMessage(externalDropStatusMessage(acceptedPaths.size(), conflictCount, invalidCount));
    }
    return true;
}

void WorkspaceController::duplicateActiveSelection()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canDuplicateSelection()) {
        m_operationQueue.reportError(QStringLiteral("You do not have permission to write items to this location."),
                                     active->currentPath(),
                                     QStringLiteral("copy"));
        return;
    }

    const QStringList selected = active->selectedPaths();
    if (selected.size() != 1) {
        return;
    }
    const QString path = selected.constFirst();
    if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path)) {
        m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
        return;
    }

    m_operationQueue.duplicateInPlace(selected, active->currentPath());
}

void WorkspaceController::compressActiveSelection(const QString &format)
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canCompressSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("Cannot create a 7z archive in this location."));
        return;
    }
    if (m_isoMountManager.isInsideManagedMount(active->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
        return;
    }

    const QStringList selected = active->selectedPaths();
    if (selected.isEmpty()) {
        return;
    }
    const QString normalizedFormat = normalizedArchiveFormat(format);
    if (archiveFormatRequiresSingleFile(normalizedFormat)) {
        if (selected.size() != 1 || !QFileInfo(selected.constFirst()).isFile()) {
            m_operationQueue.setStatusMessage(QStringLiteral("This format can compress one file only."));
            return;
        }
    }

    const QString archivePath = uniqueArchivePath(active->currentPath(), selected, normalizedFormat);
    m_operationQueue.compressToArchive(selected, archivePath);
}

void WorkspaceController::moveActiveSelectionToOpposite()
{
    if (!m_splitEnabled) {
        return;
    }
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    moveDroppedSelectionToPanel(m_activePanel, panelBySide(m_activePanel)->selectedPaths(),
                                m_activePanel == 0 ? 1 : 0, destination->currentPath());
}

bool WorkspaceController::moveDroppedSelectionToPanel(int sourcePanel,
                                                      const QStringList &sources,
                                                      int destinationPanel,
                                                      const QString &destinationPath)
{
    const QVariantMap capabilities = oppositePanelDropCapabilities(sourcePanel, sources, destinationPanel);
    if (!capabilities.value(QStringLiteral("canMove")).toBool()) {
        QString reason = capabilities.value(QStringLiteral("moveReason")).toString();
        if (reason.isEmpty()) {
            reason = capabilities.value(QStringLiteral("reason")).toString();
        }
        m_operationQueue.setStatusMessage(reason);
        return false;
    }
    FilePanelController *destination = panelBySide(destinationPanel);
    if (!destination || !pathsReferToSameDropDestination(destinationPath, destination->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Destination changed before drop completed."));
        return false;
    }
    m_operationQueue.moveTo(sources, destination->currentPath());
    return true;
}
