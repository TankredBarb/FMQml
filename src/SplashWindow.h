#pragma once

#include <QWindow>
#include <QBackingStore>
#include <QTimer>
#include <QElapsedTimer>
#include <QMetaObject>

class SplashWindow : public QWindow
{
    Q_OBJECT
    QBackingStore m_store;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    int m_frameCount = 0;
    QMetaObject::Connection m_frameConn;

public:
    int borderX = 0;
    int borderY = 0;
    int caption = 0;

    explicit SplashWindow();
    ~SplashWindow() override = default;

    void showSplash();
    void hideSplash();
    void resizeContent(int w, int h, int bx, int by, int cap);
    void onFrameSwapped(QWindow *mainWin);

    void setFrameConnection(const QMetaObject::Connection &conn) { m_frameConn = conn; }

protected:
    void exposeEvent(QExposeEvent *) override;
    bool event(QEvent *e) override;

private:
    void render();
};
