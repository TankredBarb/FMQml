#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>

class QQuickWindow;
class QWindow;
class ThemeController;

class SplashController final : public QObject {
    Q_OBJECT

public:
    explicit SplashController(ThemeController *theme, QObject *parent = nullptr);

    void show();
    void closeWhenReady(QQuickWindow *mainWindow);

private:
    QQmlApplicationEngine m_engine;
    QPointer<QWindow> m_window;
    int m_frameCount = 0;
};
