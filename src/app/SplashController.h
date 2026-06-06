#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlApplicationEngine>
#include <memory>

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
    void createQmlSplash(ThemeController *theme);

    std::unique_ptr<QQmlApplicationEngine> m_ownedEngine;
    QQmlApplicationEngine *m_engine = nullptr;
    QPointer<QWindow> m_window;
    int m_frameCount = 0;
};
