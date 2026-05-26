#include "SplashController.h"

#include "../controllers/ThemeController.h"

#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>

SplashController::SplashController(ThemeController *theme, QObject *parent)
    : QObject(parent)
{
    m_engine.rootContext()->setContextProperty(QStringLiteral("themeController"), theme);
    m_engine.loadFromModule(QStringLiteral("FM"), QStringLiteral("Splash"));

    for (QObject *object : m_engine.rootObjects()) {
        m_window = qobject_cast<QWindow *>(object);
        if (m_window) {
            break;
        }
    }
}

void SplashController::show()
{
    if (!m_window) {
        return;
    }

    const QSize splashSize = m_window->size();
    const QRect screenRect = QGuiApplication::primaryScreen()
        ? QGuiApplication::primaryScreen()->availableGeometry()
        : QRect(0, 0, splashSize.width(), splashSize.height());
    const QPoint splashTopLeft = screenRect.center() - QPoint(splashSize.width() / 2, splashSize.height() / 2);
    m_window->setGeometry(QRect(splashTopLeft, splashSize));
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
    qApp->processEvents();
}

void SplashController::closeWhenReady(QQuickWindow *mainWindow)
{
    if (!mainWindow) {
        return;
    }

    connect(mainWindow, &QQuickWindow::frameSwapped, this, [this, mainWindow]() {
        ++m_frameCount;
        if (m_frameCount >= 3 && mainWindow) {
            mainWindow->setOpacity(1.0);
        }
        if (m_frameCount >= 3 && m_window) {
            m_window->close();
        }
    }, Qt::QueuedConnection);
}
