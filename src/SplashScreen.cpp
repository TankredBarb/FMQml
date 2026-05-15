#include "SplashScreen.h"

#include <QApplication>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QWindow>
#include <QtMath>

namespace {
QPixmap makeSplashPixmap(const QSize &size)
{
    QPixmap pixmap(size);
    pixmap.fill(QColor(QStringLiteral("#0D0D0D")));

    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const QRect bounds(QPoint(0, 0), size);
    const int pad = 56;
    const int centerY = size.height() / 2 - 10;

    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 24));
    p.drawRoundedRect(bounds.adjusted(16, 16, -16, -16), 26, 26);

    QFont titleFont(QStringLiteral("Segoe UI"), 64, QFont::Bold);
    p.setFont(titleFont);
    p.setPen(Qt::white);
    p.drawText(QRect(pad, centerY - 130, size.width() - 2 * pad, 110),
               Qt::AlignHCenter | Qt::AlignVCenter,
               QStringLiteral("FM"));

    p.setPen(QPen(QColor(0, 188, 212), 2));
    p.drawLine(size.width() / 2 - 90, centerY - 10, size.width() / 2 + 90, centerY - 10);

    return pixmap;
}
}

SplashScreen::SplashScreen(QWidget *parent)
    : QSplashScreen(makeSplashPixmap(QSize(800, 480)), Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    Q_UNUSED(parent);
    m_basePixmap = pixmap();
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_AlwaysShowToolTips, false);
    setWindowFlag(Qt::WindowCloseButtonHint, false);
    setWindowFlag(Qt::WindowMaximizeButtonHint, false);
    setWindowFlag(Qt::WindowMinimizeButtonHint, false);
    setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    setWindowFlag(Qt::CustomizeWindowHint, true);
    setFixedSize(pixmap().size());

    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &SplashScreen::renderSplash);
}

void SplashScreen::showSplash()
{
    m_elapsed.start();
    m_frameCount = 0;
    show();
    raise();
    activateWindow();
    renderSplash();
    qApp->processEvents();
    m_timer.start();
}

void SplashScreen::hideSplash()
{
    if (!isVisible()) {
        return;
    }
    m_timer.stop();
    if (m_frameConn) {
        QObject::disconnect(m_frameConn);
        m_frameConn = {};
    }
    close();
}

void SplashScreen::onMainFrameSwapped(QWindow *mainWindow)
{
    ++m_frameCount;
    if (m_frameCount >= 3 && m_elapsed.elapsed() >= 250) {
        if (mainWindow) {
            mainWindow->setOpacity(1.0);
        }
        hideSplash();
    }
}

void SplashScreen::renderSplash()
{
    const qreal pulse = (qSin(m_elapsed.elapsed() * M_PI / 1000.0) + 1.0) * 0.5;
    QPixmap base = m_basePixmap;
    QPainter p(&base);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    const int w = base.width();
    const int h = base.height();
    const int accentY = h / 2 + 10;

    p.setPen(QPen(QColor(0, 188, 212, static_cast<int>(80 + 175 * pulse)), 2));
    p.drawLine(w / 2 - 100, accentY, w / 2 + 100, accentY);

    QFont infoFont(QStringLiteral("Segoe UI"), 12);
    p.setFont(infoFont);
    p.setPen(QColor(110 + static_cast<int>(50 * pulse), 110 + static_cast<int>(50 * pulse), 110 + static_cast<int>(50 * pulse)));
    p.drawText(QRect(0, accentY + 20, w, 30), Qt::AlignHCenter | Qt::AlignTop, QStringLiteral("Loading..."));

    setPixmap(base);
}
