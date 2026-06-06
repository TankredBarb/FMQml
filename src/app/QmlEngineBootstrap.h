#pragma once

#include <QQmlApplicationEngine>

#include <memory>

class AppServices;
class QQuickWindow;

class QmlEngineBootstrap final {
public:
    explicit QmlEngineBootstrap(AppServices *services);

    QQmlApplicationEngine *engine();
    QQuickWindow *loadMainWindow();

private:
    std::unique_ptr<QQmlApplicationEngine> m_ownedEngine;
    QQmlApplicationEngine *m_engine = nullptr;
    AppServices *m_services = nullptr;
};
