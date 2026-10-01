#pragma once

#include <QMouseEvent>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQml>
#include <memory>

// Enabled only while the workspace's native SplitView handle is pressed.
class PanelResizeController : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT

public:
    explicit PanelResizeController(QQuickItem *parent = nullptr);
    ~PanelResizeController() override;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void attachWindow(QQuickWindow *window);
    void flush();

    QPointer<QQuickWindow> m_window;
    QMetaObject::Connection m_frameConnection;
    std::unique_ptr<QMouseEvent> m_pending;
    QPointF m_lastDelivered;
    bool m_replaying = false;
};
