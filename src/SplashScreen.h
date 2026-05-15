#pragma once

#include <QSplashScreen>
#include <QElapsedTimer>
#include <QTimer>
#include <QMetaObject>
#include <QWindow>

class SplashScreen final : public QSplashScreen {
    Q_OBJECT
public:
    explicit SplashScreen(QWidget *parent = nullptr);

    void showSplash();
    void hideSplash();
    void onMainFrameSwapped(QWindow *mainWindow);

private:
    void renderSplash();

    QPixmap m_basePixmap;
    QElapsedTimer m_elapsed;
    QTimer m_timer;
    int m_frameCount = 0;
    QMetaObject::Connection m_frameConn;
};
