#pragma once

#include <QObject>

#include "FilePanelController.h"
#include "../models/PlacesModel.h"
#include "../core/OperationQueue.h"
#include "../core/HistoryManager.h"

class WorkspaceController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(FilePanelController *leftPanel READ leftPanel CONSTANT)
    Q_PROPERTY(FilePanelController *rightPanel READ rightPanel CONSTANT)
    Q_PROPERTY(PlacesModel *placesModel READ placesModel CONSTANT)
    Q_PROPERTY(OperationQueue *operationQueue READ operationQueue CONSTANT)
    Q_PROPERTY(HistoryManager *historyManager READ historyManager CONSTANT)
    Q_PROPERTY(bool splitEnabled READ splitEnabled WRITE setSplitEnabled NOTIFY splitEnabledChanged)
    Q_PROPERTY(int activePanel READ activePanel WRITE setActivePanel NOTIFY activePanelChanged)
    Q_PROPERTY(bool hasClipboard READ hasClipboard NOTIFY clipboardChanged)
    Q_PROPERTY(int viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)

public:
    explicit WorkspaceController(QObject *parent = nullptr);

    FilePanelController *leftPanel();
    FilePanelController *rightPanel();
    PlacesModel *placesModel();
    OperationQueue *operationQueue();
    HistoryManager *historyManager();

    bool splitEnabled() const;
    void setSplitEnabled(bool enabled);

    int activePanel() const;
    void setActivePanel(int panel);

    int viewMode() const;
    void setViewMode(int mode);

    bool hasClipboard() const;

    Q_INVOKABLE void toggleSplit();
    Q_INVOKABLE void activateLeft();
    Q_INVOKABLE void activateRight();
    Q_INVOKABLE void copyActiveSelectionToOpposite();
    Q_INVOKABLE void moveActiveSelectionToOpposite();
    Q_INVOKABLE void deleteActiveSelection();

    Q_INVOKABLE void copyToClipboard();
    Q_INVOKABLE void cutToClipboard();
    Q_INVOKABLE void pasteFromClipboard();

    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();

signals:
    void splitEnabledChanged();
    void activePanelChanged();
    void clipboardChanged();
    void viewModeChanged();

private:
    FilePanelController m_leftPanel;
    FilePanelController m_rightPanel;
    PlacesModel m_placesModel;
    OperationQueue m_operationQueue;
    HistoryManager m_historyManager;
    bool m_splitEnabled = false;
    int m_activePanel = 0;
    int m_viewMode = 0;
    QStringList m_clipboard;
    bool m_isCut = false;
};
