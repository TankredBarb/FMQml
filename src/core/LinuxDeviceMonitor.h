#pragma once

#include <QObject>
#include <QDBusObjectPath>
#include <QList>
#include <QString>
#include <QVariantMap>

struct LinuxDeviceInfo {
    QString rootPath;
    QString stableId;
    QString blockDevice;
    QString driveObjectPath;
    QString label;
    QString fileSystem;
    QString model;
    bool removable = false;
    bool optical = false;
    bool ejectable = false;
    bool canPowerOff = false;
};

class LinuxDeviceMonitor final : public QObject {
    Q_OBJECT

public:
    explicit LinuxDeviceMonitor(QObject *parent = nullptr);

    bool available() const;
    QList<LinuxDeviceInfo> mountedDevices() const;
    QList<LinuxDeviceInfo> unmountedDevices() const;
    static QList<LinuxDeviceInfo> devicesFromManagedObjects(const QVariantMap &objects);
    bool unmountOrEject(const QString &rootPath, QString *error) const;
    bool mount(const QString &stableId, QString *rootPath, QString *error) const;

signals:
    void topologyChanged();

private slots:
    void onInterfacesAdded(const QDBusObjectPath &objectPath, const QVariantMap &interfaces);
    void onInterfacesRemoved(const QDBusObjectPath &objectPath, const QStringList &interfaces);
    void onPropertiesChanged(const QString &interfaceName, const QVariantMap &changedProperties,
                             const QStringList &invalidatedProperties);

private:
    mutable bool m_snapshotValid = false;
    mutable QList<LinuxDeviceInfo> m_cachedDevices;
};
