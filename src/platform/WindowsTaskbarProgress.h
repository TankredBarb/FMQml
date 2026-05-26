#pragma once

#include <QObject>

class IsoMountManager;
class OperationQueue;
class QWindow;

#ifdef Q_OS_WIN
struct ITaskbarList3;
#endif

class WindowsTaskbarProgress final : public QObject {
    Q_OBJECT

public:
    explicit WindowsTaskbarProgress(QObject *parent = nullptr);
    ~WindowsTaskbarProgress() override;

    void attachWindow(QWindow *window);
    void setOperationQueue(OperationQueue *queue);
    void setIsoMountManager(IsoMountManager *manager);

private:
    void refresh();
    void setNoProgress();
    void setIndeterminate();
    void setNormalProgress(double progress);
    void setError();

#ifdef Q_OS_WIN
    ITaskbarList3 *m_taskbar = nullptr;
    void *m_hwnd = nullptr;
    bool m_comInitialized = false;
#endif
    OperationQueue *m_queue = nullptr;
    IsoMountManager *m_isoMountManager = nullptr;
    bool m_isoActivity = false;
};
