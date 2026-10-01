#include "PanelResizeController.h"

#include <QCoreApplication>
#include <QScopedValueRollback>

PanelResizeController::PanelResizeController(QQuickItem *parent)
    : QQuickItem(parent)
{
    connect(this, &QQuickItem::windowChanged, this, &PanelResizeController::attachWindow);
    connect(this, &QQuickItem::enabledChanged, this, [this] {
        if (!isEnabled()) m_pending.reset();
    });
    attachWindow(window());
}

PanelResizeController::~PanelResizeController()
{
    disconnect(this, nullptr, this, nullptr);
    disconnect(m_frameConnection);
    if (m_window) m_window->removeEventFilter(this);
}

void PanelResizeController::attachWindow(QQuickWindow *window)
{
    if (m_window) m_window->removeEventFilter(this);
    disconnect(m_frameConnection);
    m_pending.reset();
    m_window = window;
    if (!m_window) return;
    m_window->installEventFilter(this);
    // GUI thread, before polish/synchronization: the latest geometry enters this frame.
    m_frameConnection = connect(m_window, &QQuickWindow::afterAnimating,
                                this, &PanelResizeController::flush);
}

void PanelResizeController::flush()
{
    if (!m_window || !isEnabled() || !m_pending) return;
    auto event = std::move(m_pending);
    m_lastDelivered = event->position();
    QScopedValueRollback replaying(m_replaying, true);
    QCoreApplication::sendEvent(m_window, event.get());
}

bool PanelResizeController::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_window || m_replaying) return false;
    if (event->type() == QEvent::MouseButtonPress) {
        m_lastDelivered = static_cast<QMouseEvent *>(event)->position();
    }
    if (event->type() == QEvent::WindowDeactivate || event->type() == QEvent::UngrabMouse
        || event->type() == QEvent::Hide) m_pending.reset();
    if (!isEnabled()) return false;
    if (event->type() == QEvent::MouseMove) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (!(mouse->buttons() & Qt::LeftButton)) return false;
        m_pending.reset(mouse->clone());
        m_window->update();
        return true;
    }
    if (event->type() == QEvent::MouseButtonRelease) {
        auto *mouse = static_cast<QMouseEvent *>(event);
        if (mouse->button() == Qt::LeftButton
            && (m_pending || m_lastDelivered != mouse->position())) {
            // A release may arrive before the requested frame, or at a new position.
            m_pending = std::make_unique<QMouseEvent>(QEvent::MouseMove, mouse->position(),
                mouse->scenePosition(), mouse->globalPosition(), Qt::NoButton,
                mouse->buttons() | Qt::LeftButton, mouse->modifiers(), mouse->pointingDevice());
            m_pending->setTimestamp(mouse->timestamp());
            flush();
        }
    }
    return false;
}
