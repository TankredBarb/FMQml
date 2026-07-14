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

void WorkspaceController::triggerRename()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (!active->canRenameSelection()) {
#ifdef Q_OS_LINUX
        const QStringList selected = active->selectedPaths();
        if (selected.size() == 1
            && !active->isVirtualRoot()
            && !ArchiveSupport::isArchivePath(selected.constFirst())
            && !isProviderUriPath(selected.constFirst())) {
            emit renameRequested();
            return;
        }
#endif
        m_operationQueue.setStatusMessage(QStringLiteral("The current item cannot be renamed with the available permissions."));
        return;
    }
    emit renameRequested();
}

bool WorkspaceController::hasClipboard() const
{
    return !m_clipboard.isEmpty();
}

bool WorkspaceController::clipboardCut() const
{
    return m_isCut;
}

QString WorkspaceController::clipboardSummary() const
{
    if (m_clipboard.isEmpty()) {
        return {};
    }

    return QStringLiteral("Clipboard: %1 %2 %3")
        .arg(m_clipboard.size())
        .arg(m_clipboard.size() == 1 ? "file" : "files")
        .arg(m_isCut ? "cut" : "copied");
}

void WorkspaceController::copyToClipboard()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canCopySelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be copied from this location."));
        return;
    }
    m_clipboard = active->selectedPaths();
    m_isCut = false;
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        clipboardSummary());
    focusActivePanel();
}

void WorkspaceController::cutToClipboard()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canDeleteSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be moved from this location."));
        return;
    }
    m_clipboard = active->selectedPaths();
    m_isCut = true;
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        clipboardSummary());
    focusActivePanel();
}

void WorkspaceController::pasteFromClipboard()
{
    if (m_clipboard.isEmpty()) {
        return;
    }
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canPasteIntoCurrentPath()) {
        m_operationQueue.reportError(QStringLiteral("You do not have permission to write items to this location."),
                                     active->currentPath(),
                                     m_isCut ? QStringLiteral("move") : QStringLiteral("copy"));
        return;
    }
    if (m_isCut) {
        m_operationQueue.moveTo(m_clipboard, active->currentPath());
        m_clipboard.clear();
        m_isCut = false;
        emit clipboardChanged();
    } else {
        copyPathsToPanel(m_clipboard, active);
    }
}

void WorkspaceController::pasteFromClipboardAsAdministrator()
{
#ifdef Q_OS_LINUX
    if (m_clipboard.isEmpty()) {
        return;
    }
    if (m_isCut) {
        m_operationQueue.setStatusMessage(QStringLiteral("Paste as Administrator currently supports copied items only."));
        return;
    }
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()
        || isProviderUriPath(active->currentPath())
        || ArchiveSupport::isArchivePath(active->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Paste as Administrator is available for local folders only."));
        return;
    }
    m_operationQueue.copyToAsAdministrator(m_clipboard, active->currentPath());
#else
    m_operationQueue.setStatusMessage(QStringLiteral("Paste as Administrator is available on Linux only."));
#endif
}

void WorkspaceController::createFolderInActivePanelAsAdministrator()
{
#ifdef Q_OS_LINUX
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()
        || isProviderUriPath(active->currentPath())
        || ArchiveSupport::isArchivePath(active->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Create Folder as Administrator is available for local folders only."));
        return;
    }
    m_operationQueue.createFolderAsAdministrator(active->currentPath(), QStringLiteral("New Folder"));
#else
    m_operationQueue.setStatusMessage(QStringLiteral("Create Folder as Administrator is available on Linux only."));
#endif
}

bool WorkspaceController::copyPathsToPanel(const QStringList &sources, FilePanelController *destination)
{
    if (sources.isEmpty() || !destination) {
        return false;
    }
    if (!destination->canCreateInCurrentPath()) {
        m_operationQueue.reportError(QStringLiteral("You do not have permission to write items to this location."),
                                     destination->currentPath(),
                                     QStringLiteral("copy"));
        return false;
    }

    bool allSourcesInDestination = true;
    for (const QString &source : sources) {
        if (ArchiveSupport::isArchivePath(source)) {
            allSourcesInDestination = false;
            break;
        }
        const QString sourceParent = destination->parentPathForPath(source);
        if (normalizedLocalPath(sourceParent) != normalizedLocalPath(destination->currentPath())) {
            allSourcesInDestination = false;
            break;
        }
    }
    if (allSourcesInDestination) {
        m_operationQueue.setStatusMessage(QStringLiteral("Source and destination are the same folder."));
        return false;
    }

    m_operationQueue.copyTo(sources, destination->currentPath());
    return true;
}
