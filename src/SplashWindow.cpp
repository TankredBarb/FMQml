#define _USE_MATH_DEFINES
#include <cmath>

#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QExposeEvent>
#include <QEvent>

#include "SplashWindow.h"

SplashWindow::SplashWindow()
    : m_store(this)
{
    setFlags(Qt::SplashScreen | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    resize(1120, 720);
    if (auto *screen = QGuiApplication::primaryScreen()) {
        QRect g = screen->availableGeometry();
        setPosition(g.center().x() - 560, g.center().y() - 360);
    }
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, [this]() {
        requestUpdate();
    });
}

void SplashWindow::showSplash()
{
    m_elapsed.start();
    show();
    render();
    QGuiApplication::processEvents();
    render();
    m_timer.start();
}

void SplashWindow::hideSplash()
{
    if (!isVisible()) return;
    m_timer.stop();
    setVisible(false);
    if (m_frameConn)
        QObject::disconnect(m_frameConn);
}

void SplashWindow::resizeContent(int w, int h, int bx, int by, int cap)
{
    borderX = bx;
    borderY = by;
    caption = cap;
    resize(w, h);
    if (auto *screen = QGuiApplication::primaryScreen()) {
        QRect g = screen->availableGeometry();
        setPosition(g.center().x() - w / 2, g.center().y() - h / 2);
    }
}

void SplashWindow::onFrameSwapped(QWindow *mainWin)
{
    m_frameCount++;
    if (m_frameCount >= 10) {
        if (mainWin) {
            QRect finalFg = mainWin->frameGeometry();
            if (finalFg.isValid() && !finalFg.isEmpty())
                setGeometry(finalFg);
        }
        hideSplash();
    }
}

void SplashWindow::exposeEvent(QExposeEvent *)
{
    render();
}

bool SplashWindow::event(QEvent *e)
{
    if (e->type() == QEvent::UpdateRequest) {
        render();
        return true;
    }
    return QWindow::event(e);
}

void SplashWindow::render()
{
    QRect r(QPoint(0, 0), size());
    m_store.resize(r.size());
    m_store.beginPaint(r);
    if (auto *device = m_store.paintDevice()) {
        QPainter p(device);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);

        int w = width(), h = height();

        p.fillRect(0, 0, w, h, QColor("#0D0D0D"));

        int textW = w - 2 * borderX;
        int textH = h - caption - borderY;
        int textX = borderX;
        int textY = caption;

        qint64 ms = m_elapsed.elapsed();
        double breath = (sin(ms * M_PI / 1000.0) + 1.0) / 2.0;

        QFont titleFont(QStringLiteral("Segoe UI"), 72, QFont::Bold);
        p.setFont(titleFont);
        p.setOpacity(0.90 + 0.10 * breath);
        p.setPen(Qt::white);
        p.drawText(QRect(textX, textY, textW, textH * 2 / 5),
                   Qt::AlignCenter, QStringLiteral("FM"));
        p.setOpacity(1.0);

        int accentY = textY + textH * 2 / 5 + 15;
        QPen accent(QColor(0, 188, 212, static_cast<int>(60 + 195 * breath)), 2);
        p.setPen(accent);
        p.drawLine(textX + textW / 4, accentY, textX + textW * 3 / 4, accentY);

        QFont subFont(QStringLiteral("Segoe UI"), 14);
        p.setFont(subFont);
        int gray = 100 + static_cast<int>(55 * breath);
        p.setPen(QColor(gray, gray, gray));
        int loadY = accentY + 20;
        p.drawText(QRect(textX, loadY, textW, textY + textH - loadY),
                   Qt::AlignHCenter | Qt::AlignTop,
                   QStringLiteral("Loading\u2026"));
    }
    m_store.endPaint();
    m_store.flush(r);
}
