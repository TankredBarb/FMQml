#pragma once

#include <QObject>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

class PropertiesPreviewController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(QVariantMap request READ request NOTIFY requestChanged)

public:
    using QObject::QObject;
    QString path() const { return m_path; }
    bool active() const { return m_active; }
    bool available() const { return m_available; }
    QVariantMap request() const { return m_request; }
    void setPath(const QString &path);
    void setActive(bool active);
    Q_INVOKABLE void complete(const QString &id, bool success, int width, int height);

signals:
    void pathChanged();
    void activeChanged();
    void availableChanged();
    void requestChanged();

private:
    void refresh();
    QString m_path;
    QVariantMap m_request;
    quint64 m_generation = 0;
    bool m_active = false;
    bool m_available = false;
};
