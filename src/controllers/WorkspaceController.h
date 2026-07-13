#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "FilePanelController.h"
#include "../models/TreeModel.h"
#include "../models/PlacesModel.h"
#include "../core/OperationQueue.h"
#include "../core/HistoryManager.h"
#include "../core/IsoMountManager.h"
#include "../core/VolumeMonitor.h"

class WorkspaceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(FilePanelController *leftPanel READ leftPanel CONSTANT)
    Q_PROPERTY(FilePanelController *rightPanel READ rightPanel CONSTANT)
    Q_PROPERTY(PlacesModel *placesModel READ placesModel CONSTANT)
    Q_PROPERTY(TreeModel *treeModel READ treeModel CONSTANT)
    Q_PROPERTY(OperationQueue *operationQueue READ operationQueue CONSTANT)
    Q_PROPERTY(HistoryManager *historyManager READ historyManager CONSTANT)
    Q_PROPERTY(IsoMountManager *isoMountManager READ isoMountManager CONSTANT)
    Q_PROPERTY(VolumeMonitor *volumeMonitor READ volumeMonitor CONSTANT)
    Q_PROPERTY(bool splitEnabled READ splitEnabled WRITE setSplitEnabled NOTIFY splitEnabledChanged)
    Q_PROPERTY(int activePanel READ activePanel WRITE setActivePanel NOTIFY activePanelChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(bool clipboardCut READ clipboardCut NOTIFY clipboardChanged)
    Q_PROPERTY(QString clipboardSummary READ clipboardSummary NOTIFY clipboardChanged)
    Q_PROPERTY(QString applicationDirectory READ applicationDirectory CONSTANT)

public:
    explicit WorkspaceController(QObject *parent = nullptr);
    ~WorkspaceController() override;

    FilePanelController *leftPanel();
    FilePanelController *rightPanel();
    PlacesModel *placesModel();
    TreeModel *treeModel();
    OperationQueue *operationQueue();
    HistoryManager *historyManager();
    IsoMountManager *isoMountManager();
    VolumeMonitor *volumeMonitor();

    bool splitEnabled() const;
    void setSplitEnabled(bool enabled);

    int activePanel() const;
    void setActivePanel(int panel);

    bool hasClipboard() const;
    bool clipboardCut() const;
    QString clipboardSummary() const;

    Q_INVOKABLE void toggleSplit();
    Q_INVOKABLE void activateLeft();
    Q_INVOKABLE void activateRight();
    Q_INVOKABLE void focusActivePanel();
    Q_INVOKABLE void setDragCursorShape(int shape);
    Q_INVOKABLE void clearDragCursorShape();
    Q_INVOKABLE void mirrorActivePanelToOpposite();
    Q_INVOKABLE void copyActiveSelectionToOpposite();
    Q_INVOKABLE QVariantMap oppositePanelDropCapabilities(int sourcePanel,
                                                          const QStringList &sources,
                                                          int destinationPanel);
    Q_INVOKABLE QVariantMap externalDropCapabilities(const QVariantList &urls,
                                                     int destinationPanel,
                                                     const QString &destinationPath);
    Q_INVOKABLE bool copyDroppedSelectionToPanel(int sourcePanel,
                                                 const QStringList &sources,
                                                 int destinationPanel,
                                                 const QString &destinationPath);
    Q_INVOKABLE bool copyExternalUrlsToPanel(const QVariantList &urls,
                                             int destinationPanel,
                                             const QString &destinationPath);
    Q_INVOKABLE void duplicateActiveSelection();
    Q_INVOKABLE void compressActiveSelection(const QString &format = QStringLiteral("7z"));
    Q_INVOKABLE void moveActiveSelectionToOpposite();
    Q_INVOKABLE bool moveDroppedSelectionToPanel(int sourcePanel,
                                                 const QStringList &sources,
                                                 int destinationPanel,
                                                 const QString &destinationPath);
    Q_INVOKABLE void requestDelete(const QStringList &paths, const QString &label, const QVariantList &items = {});
    Q_INVOKABLE void requestDeleteAsAdministrator(const QStringList &paths, const QString &label, const QVariantList &items = {});
    Q_INVOKABLE bool confirmDelete(const QStringList &paths);
    Q_INVOKABLE bool confirmDeleteAsAdministrator(const QStringList &paths);
    Q_INVOKABLE QVariantMap deleteRequestDetails(const QStringList &paths, const QString &label) const;
    Q_INVOKABLE void triggerRename();

    Q_INVOKABLE void copyToClipboard();
    Q_INVOKABLE void cutToClipboard();
    Q_INVOKABLE void copyTextToClipboard(const QString &text);
    Q_INVOKABLE QString applicationDirectory() const;
    Q_INVOKABLE QString displayPath(const QString &path) const;
    Q_INVOKABLE QStringList clipboardPaths() const;
    Q_INVOKABLE QVariantList loadedPlugins() const;
    Q_INVOKABLE qint64 processMemoryUsage() const;
    Q_INVOKABLE QString qtVersion() const;
    Q_INVOKABLE void pasteFromClipboard();
    Q_INVOKABLE void pasteFromClipboardAsAdministrator();
    Q_INVOKABLE void createFolderInActivePanelAsAdministrator();
    Q_INVOKABLE void extractArchiveTo(const QString &archivePath, const QString &destination);
    Q_INVOKABLE bool canExtractArchivePath(const QString &archivePath) const;
    Q_INVOKABLE void extractArchiveHerePath(const QString &archivePath, const QString &currentFolder);
    Q_INVOKABLE void extractArchiveToNamedFolderPath(const QString &archivePath, const QString &currentFolder);
    Q_INVOKABLE void submitArchivePassword(const QString &path, const QString &password);
    Q_INVOKABLE void cancelArchivePassword(const QString &path);
    Q_INVOKABLE bool canMountIsoPath(const QString &path) const;
    Q_INVOKABLE void requestMountIso(const QString &path);
    Q_INVOKABLE void mountIsoToLetter(const QString &path, const QString &letter);
    Q_INVOKABLE void mountIsoAutomatically(const QString &path);
    Q_INVOKABLE bool isManagedIsoMountRoot(const QString &rootPath) const;
    Q_INVOKABLE bool isInsideManagedIsoMount(const QString &path) const;
    Q_INVOKABLE void unmountIsoRoot(const QString &rootPath);
    Q_INVOKABLE void requestEjectVolume(const QString &rootPath);
    Q_INVOKABLE void requestMountVolume(const QString &stableDeviceId);
    Q_INVOKABLE bool pathBelongsToVolumeRoot(const QString &path, const QString &rootPath) const;

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void splitEnabledChanged();
    void activePanelChanged();
    void clipboardChanged();
    void renameRequested();
    void deleteRequested(const QStringList &paths, const QString &label, const QVariantList &items);
    void deleteAsAdministratorRequested(const QStringList &paths, const QString &label, const QVariantList &items);
    void mountIsoRequested(const QString &path);
    void archivePasswordRequested(const QString &path, const QString &displayName, const QString &message);
    void focusActivePanelRequested();
    void deviceRemoved(const QString &rootPath, const QString &displayName);
    void deviceEjectStarted(const QString &rootPath, const QString &displayName);
    void deviceEjectSucceeded(const QString &rootPath, const QString &displayName);
    void deviceEjectFailed(const QString &rootPath, const QString &displayName, const QString &message);

private:
    FilePanelController *panelBySide(int side);
    FilePanelController *panelForPath(const QString &path);
    void handleVolumeRemoved(const QString &rootPath, const QString &displayName);
    void handleProviderPlaceRemoved(const QString &rootPath, const QString &displayName, const QString &section);
    void handleVolumeEjectFinished(const QString &rootPath, bool success, const QString &message);
    bool requestArchivePasswordForExtractIfNeeded(const QString &archivePath, const QString &destination);
    bool copyPathsToPanel(const QStringList &sources, FilePanelController *destination);
    void recordOperationHistory(OperationQueue::Type type, const QStringList &sources, const QString &destination);
    void recordRenameHistory(const QString &oldPath, const QString &newPath);
    void finishHistoryReplay();

    FilePanelController m_leftPanel;
    FilePanelController m_rightPanel;
    PlacesModel m_placesModel;
    TreeModel m_treeModel;
    VolumeMonitor m_volumeMonitor;
    OperationQueue m_operationQueue;
    HistoryManager m_historyManager;
    IsoMountManager m_isoMountManager;
    bool m_splitEnabled = false;
    int m_activePanel = 0;
    QStringList m_clipboard;
    QString m_pendingPasswordArchivePath;
    QString m_pendingPasswordExtractDestination;
    bool m_isCut = false;
    bool m_replayingHistory = false;
    bool m_dragCursorOverridden = false;
    int m_dragCursorShape = -1;
};
