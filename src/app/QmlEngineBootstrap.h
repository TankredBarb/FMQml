#pragma once

#include <QQmlApplicationEngine>

class AppServices;
class QQuickWindow;

class QmlEngineBootstrap final {
public:
    explicit QmlEngineBootstrap(AppServices *services);

    QQuickWindow *loadMainWindow();

private:
    QQmlApplicationEngine m_engine;
    AppServices *m_services = nullptr;
};
