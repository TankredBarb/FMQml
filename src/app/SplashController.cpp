#include "SplashController.h"

#include "../controllers/ThemeController.h"

#include <QGuiApplication>
#include <QMetaObject>
#include <QQuickWindow>
#include <QScreen>
#include <QWindow>

namespace {
QRect centeredRect(const QRect &screenRect, const QSize &size)
{
    return QRect(screenRect.center() - QPoint(size.width() / 2, size.height() / 2), size);
}
}

SplashController::SplashController(ThemeController *theme, QObject *parent)
    : QObject(parent)
{
    m_ownedEngine = std::make_unique<QQmlApplicationEngine>();
    m_engine = m_ownedEngine.get();
    createQmlSplash(theme);
}

void SplashController::createQmlSplash(ThemeController *theme)
{
    if (!m_engine) {
        return;
    }

    m_engine->rootContext()->setContextProperty(QStringLiteral("themeController"), theme);
    m_engine->loadFromModule(QStringLiteral("FM"), QStringLiteral("Splash"));

    for (QObject *object : m_engine->rootObjects()) {
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
    m_window->setGeometry(centeredRect(screenRect, splashSize));
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
            if (m_frameCount == 3) {
                QMetaObject::invokeMethod(mainWindow, "startupShellReady", Qt::QueuedConnection);
            }
        }
    }, Qt::QueuedConnection);
}
