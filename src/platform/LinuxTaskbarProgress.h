#pragma once

#include <QObject>

class IsoMountManager;
class OperationQueue;
class QWindow;

class LinuxTaskbarProgress final : public QObject {
    Q_OBJECT

public:
    explicit LinuxTaskbarProgress(QObject *parent = nullptr);

    void attachWindow(QWindow *window);
    void setOperationQueue(OperationQueue *queue);
    void setIsoMountManager(IsoMountManager *manager);

private:
    enum class State {
        NoProgress,
        Indeterminate,
        Normal,
        Error
    };

    void refresh();
    void setNoProgress();
    void setIndeterminate();
    void setNormalProgress(double progress);
    void setError();
    void sendProgress(bool visible, double progress, bool urgent);

    OperationQueue *m_queue = nullptr;
    IsoMountManager *m_isoMountManager = nullptr;
    bool m_isoActivity = false;
    State m_state = State::NoProgress;
    double m_progress = -1.0;
    bool m_urgent = false;
};
