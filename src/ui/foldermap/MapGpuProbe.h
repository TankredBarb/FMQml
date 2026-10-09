#pragma once

#include <QQuickItem>
#include <QtQml/qqmlregistration.h>

class MapGpuProbe : public QQuickItem
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(bool available READ available NOTIFY capabilityChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY capabilityChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY capabilityChanged)
public:
    explicit MapGpuProbe(QQuickItem *parent = nullptr);
    bool available() const { return m_available; }
    bool ready() const { return m_ready; }
    QString deviceName() const { return m_deviceName; }
    Q_INVOKABLE static bool supportsPath(const QString &path);
signals:
    void capabilityChanged();
private:
    void inspect(QQuickWindow *window);
    void publish(bool available, bool ready, const QString &name);
    QMetaObject::Connection m_initialized;
    QMetaObject::Connection m_invalidated;
    QMetaObject::Connection m_synchronizing;
    bool m_available = false;
    bool m_ready = false;
    QString m_deviceName;
};
