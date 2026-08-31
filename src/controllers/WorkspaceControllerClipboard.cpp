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
#include <QMimeData>
#include <QScopedValueRollback>
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

namespace {
constexpr auto GnomeCopiedFiles = "x-special/gnome-copied-files";
constexpr auto KdeCutSelection = "application/x-kde-cutselection";
constexpr auto WindowsPreferredDropEffect = "application/x-qt-windows-mime;value=\"Preferred DropEffect\"";

QString localClipboardPath(const QString &path)
{
    if (ArchiveSupport::isArchivePath(path) || isProviderUriPath(path)) return {};
    const QUrl url(path);
    const QString local = url.isLocalFile() ? url.toLocalFile() : path;
    if (local.trimmed().isEmpty()) return {};
    const QFileInfo info(local);
    return info.exists() ? QDir::cleanPath(info.absoluteFilePath()) : QString();
}

bool clipboardMimeIsCut(const QMimeData *mime)
{
    if (!mime) return false;
    const QByteArray gnome = mime->data(GnomeCopiedFiles).trimmed();
    if (!gnome.isEmpty()) {
        return gnome.split('\n').constFirst().trimmed().compare("cut", Qt::CaseInsensitive) == 0;
    }
    const QByteArray kde = mime->data(KdeCutSelection).trimmed();
    if (!kde.isEmpty()) return kde != "0";

    const QByteArray effect = mime->data(WindowsPreferredDropEffect);
    return effect.size() >= 4
        && static_cast<unsigned char>(effect.at(0)) == 2
        && effect.at(1) == 0 && effect.at(2) == 0 && effect.at(3) == 0;
}
}

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
    m_cutPastePending = false;
    ++m_clipboardGeneration;
    publishFileClipboard();
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        clipboardSummary());
    focusActivePanel();
}

bool WorkspaceController::copyPathsToClipboard(const QStringList &paths, int sourcePanel)
{
    FilePanelController *source = sourcePanel == 0 ? &m_leftPanel
                                                   : (sourcePanel == 1 ? &m_rightPanel : nullptr);
    if (!source || !source->canCopyPaths(paths)) {
        m_operationQueue.setStatusMessage(
            QStringLiteral("One or more selected items cannot be copied from this location."));
        return false;
    }

    m_clipboard = paths;
    m_isCut = false;
    m_cutPastePending = false;
    ++m_clipboardGeneration;
    publishFileClipboard();
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        QStringLiteral("%1 %2 copied to clipboard")
            .arg(paths.size())
            .arg(paths.size() == 1 ? QStringLiteral("item") : QStringLiteral("items")));
    return true;
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
    m_cutPastePending = false;
    ++m_clipboardGeneration;
    publishFileClipboard();
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        clipboardSummary());
    focusActivePanel();
}

void WorkspaceController::pasteFromClipboard()
{
    syncFileClipboardFromSystem();
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
        if (m_cutPastePending) {
            m_operationQueue.setStatusMessage(QStringLiteral("This cut operation is already in progress."));
            return;
        }
        m_pendingCutPasteSources = m_clipboard;
        m_pendingCutPasteGeneration = m_clipboardGeneration;
        m_cutPastePending = true;
        m_operationQueue.moveTo(m_pendingCutPasteSources, active->currentPath());
    } else {
        copyPathsToPanel(m_clipboard, active);
    }
}

void WorkspaceController::publishFileClipboard()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (!clipboard) return;
    QScopedValueRollback publishing(m_publishingFileClipboard, true);
    if (m_clipboard.isEmpty()) {
        clipboard->clear();
        return;
    }

    QList<QUrl> urls;
    urls.reserve(m_clipboard.size());
    for (const QString &path : std::as_const(m_clipboard)) {
        const QString local = localClipboardPath(path);
        if (local.isEmpty()) {
            clipboard->clear();
            return;
        }
        urls.append(QUrl::fromLocalFile(local));
    }

    auto *mime = new QMimeData;
    mime->setUrls(urls);
    QByteArray gnome(m_isCut ? "cut\n" : "copy\n");
    for (const QUrl &url : urls) {
        gnome += url.toEncoded();
        gnome += '\n';
    }
    mime->setData(GnomeCopiedFiles, gnome);
    mime->setData(KdeCutSelection, m_isCut ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
    QByteArray effect(4, '\0');
    effect[0] = m_isCut ? 2 : 1;
    mime->setData(WindowsPreferredDropEffect, effect);
    clipboard->setMimeData(mime);
}

void WorkspaceController::syncFileClipboardFromSystem()
{
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QMimeData *mime = clipboard ? clipboard->mimeData() : nullptr;
    if (m_publishingFileClipboard) return;
    if ((!mime || mime->formats().isEmpty())
        && std::any_of(m_clipboard.cbegin(), m_clipboard.cend(), [](const QString &path) {
            return ArchiveSupport::isArchivePath(path) || isProviderUriPath(path);
        })) {
        return;
    }

    QStringList paths;
    if (mime && mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            if (!url.isLocalFile()) continue;
            const QString path = localClipboardPath(url.toLocalFile());
            if (!path.isEmpty() && !paths.contains(path)) paths.append(path);
        }
    }
    if (paths.isEmpty() && mime) {
        const QList<QByteArray> lines = mime->data(GnomeCopiedFiles).split('\n');
        for (qsizetype i = 1; i < lines.size(); ++i) {
            const QUrl url = QUrl::fromEncoded(lines.at(i).trimmed());
            if (!url.isLocalFile()) continue;
            const QString path = localClipboardPath(url.toLocalFile());
            if (!path.isEmpty() && !paths.contains(path)) paths.append(path);
        }
    }
    const bool cut = !paths.isEmpty() && clipboardMimeIsCut(mime);
    if (m_clipboard == paths && m_isCut == cut) return;

    m_clipboard = paths;
    m_isCut = cut;
    m_cutPastePending = false;
    m_pendingCutPasteSources.clear();
    ++m_clipboardGeneration;
    emit clipboardChanged();
}

void WorkspaceController::handleClipboardOperationCompleted(const QVariantMap &completion)
{
    if (!m_cutPastePending
        || static_cast<OperationQueue::Type>(completion.value(QStringLiteral("type")).toInt()) != OperationQueue::Type::Move
        || completion.value(QStringLiteral("sources")).toStringList() != m_pendingCutPasteSources) {
        return;
    }

    const QStringList pendingSources = m_pendingCutPasteSources;
    const quint64 pendingGeneration = m_pendingCutPasteGeneration;
    m_cutPastePending = false;
    m_pendingCutPasteSources.clear();
    if (m_clipboardGeneration != pendingGeneration || m_clipboard != pendingSources || !m_isCut) return;

    QStringList remaining = pendingSources;
    const QVariantList outcomes = completion.value(QStringLiteral("itemOutcomes")).toList();
    for (const QVariant &value : outcomes) {
        const QVariantMap outcome = value.toMap();
        if (outcome.value(QStringLiteral("disposition")).toString() == QLatin1String("Succeeded")) {
            remaining.removeAll(outcome.value(QStringLiteral("sourcePath")).toString());
        }
    }
    m_clipboard = remaining;
    m_isCut = !remaining.isEmpty();
    ++m_clipboardGeneration;
    publishFileClipboard();
    emit clipboardChanged();
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
