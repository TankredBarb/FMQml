#pragma once

#include <QObject>
#include <QVariantMap>

#include "FilePanelController.h"
#include "../models/TreeModel.h"
#include "../models/PlacesModel.h"
#include "../core/OperationQueue.h"
#include "../core/HistoryManager.h"
#include "../core/IsoMountManager.h"

class WorkspaceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(FilePanelController *leftPanel READ leftPanel CONSTANT)
    Q_PROPERTY(FilePanelController *rightPanel READ rightPanel CONSTANT)
    Q_PROPERTY(PlacesModel *placesModel READ placesModel CONSTANT)
    Q_PROPERTY(TreeModel *treeModel READ treeModel CONSTANT)
    Q_PROPERTY(OperationQueue *operationQueue READ operationQueue CONSTANT)
    Q_PROPERTY(HistoryManager *historyManager READ historyManager CONSTANT)
    Q_PROPERTY(IsoMountManager *isoMountManager READ isoMountManager CONSTANT)
    Q_PROPERTY(bool splitEnabled READ splitEnabled WRITE setSplitEnabled NOTIFY splitEnabledChanged)
    Q_PROPERTY(int activePanel READ activePanel WRITE setActivePanel NOTIFY activePanelChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(int clipboardCount READ clipboardCount NOTIFY clipboardChanged)
    Q_PROPERTY(bool clipboardCut READ clipboardCut NOTIFY clipboardChanged)
    Q_PROPERTY(QString clipboardSummary READ clipboardSummary NOTIFY clipboardChanged)

public:
    explicit WorkspaceController(QObject *parent = nullptr);

    FilePanelController *leftPanel();
    FilePanelController *rightPanel();
    PlacesModel *placesModel();
    TreeModel *treeModel();
    OperationQueue *operationQueue();
    HistoryManager *historyManager();
    IsoMountManager *isoMountManager();

    bool splitEnabled() const;
    void setSplitEnabled(bool enabled);

    int activePanel() const;
    void setActivePanel(int panel);

    bool hasClipboard() const;
    int clipboardCount() const;
    bool clipboardCut() const;
    QString clipboardSummary() const;

    Q_INVOKABLE void toggleSplit();
    Q_INVOKABLE void activateLeft();
    Q_INVOKABLE void activateRight();
    Q_INVOKABLE void focusActivePanel();
    Q_INVOKABLE void mirrorActivePanelToOpposite();
    Q_INVOKABLE void copyActiveSelectionToOpposite();
    Q_INVOKABLE void duplicateActiveSelection();
    Q_INVOKABLE void compressActiveSelection(const QString &format = QStringLiteral("7z"));
    Q_INVOKABLE void moveActiveSelectionToOpposite();
    Q_INVOKABLE void deleteActiveSelection();
    Q_INVOKABLE void requestDelete(const QStringList &paths, const QString &label);
    Q_INVOKABLE bool confirmDelete(const QStringList &paths);
    Q_INVOKABLE QVariantMap deleteRequestDetails(const QStringList &paths, const QString &label) const;
    Q_INVOKABLE void triggerRename();

    Q_INVOKABLE void copyToClipboard();
    Q_INVOKABLE void cutToClipboard();
    Q_INVOKABLE void copyTextToClipboard(const QString &text);
    Q_INVOKABLE QString displayPath(const QString &path) const;
    Q_INVOKABLE void pasteFromClipboard();
    Q_INVOKABLE void extractArchiveTo(const QString &archivePath, const QString &destination);
    Q_INVOKABLE bool canExtractArchivePath(const QString &archivePath) const;
    Q_INVOKABLE void extractArchiveHerePath(const QString &archivePath, const QString &currentFolder);
    Q_INVOKABLE void extractArchiveToNamedFolderPath(const QString &archivePath, const QString &currentFolder);
    Q_INVOKABLE bool canMountIsoPath(const QString &path) const;
    Q_INVOKABLE void requestMountIso(const QString &path);
    Q_INVOKABLE void mountIsoToLetter(const QString &path, const QString &letter);
    Q_INVOKABLE void mountIsoAutomatically(const QString &path);
    Q_INVOKABLE bool isManagedIsoMountRoot(const QString &rootPath) const;
    Q_INVOKABLE bool isInsideManagedIsoMount(const QString &path) const;
    Q_INVOKABLE void unmountIsoRoot(const QString &rootPath);

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void splitEnabledChanged();
    void activePanelChanged();
    void clipboardChanged();
    void renameRequested();
    void deleteRequested(const QStringList &paths, const QString &label);
    void mountIsoRequested(const QString &path);
    void focusActivePanelRequested();

private:
    FilePanelController *panelForPath(const QString &path);
    void recordOperationHistory(OperationQueue::Type type, const QStringList &sources, const QString &destination);
    void recordRenameHistory(const QString &oldPath, const QString &newPath);
    void finishHistoryReplay();

    FilePanelController m_leftPanel;
    FilePanelController m_rightPanel;
    PlacesModel m_placesModel;
    TreeModel m_treeModel;
    OperationQueue m_operationQueue;
    HistoryManager m_historyManager;
    IsoMountManager m_isoMountManager;
    bool m_splitEnabled = false;
    int m_activePanel = 0;
    QStringList m_clipboard;
    bool m_isCut = false;
    bool m_replayingHistory = false;
};
