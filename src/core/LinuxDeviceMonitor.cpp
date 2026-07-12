#include "LinuxDeviceMonitor.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QDir>
#include <QStorageInfo>
#include <QSet>
#include <QVariantMap>

#include <algorithm>

namespace {
constexpr auto kService = "org.freedesktop.UDisks2";
constexpr auto kRootPath = "/org/freedesktop/UDisks2";
constexpr auto kObjectManager = "org.freedesktop.DBus.ObjectManager";
constexpr auto kDrive = "org.freedesktop.UDisks2.Drive";
constexpr auto kBlock = "org.freedesktop.UDisks2.Block";
constexpr auto kFilesystem = "org.freedesktop.UDisks2.Filesystem";
constexpr auto kLoop = "org.freedesktop.UDisks2.Loop";
constexpr auto kProperties = "org.freedesktop.DBus.Properties";
using MountPointList = QList<QByteArray>;

bool deviceTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIntValue("FM_DEVICE_TRACE") > 0;
    return enabled;
}

QString stringProperty(const QVariantMap &properties, const QString &name)
{
    const QVariant value = properties.value(name);
    if (value.metaType().id() == QMetaType::QByteArray) {
        return QString::fromUtf8(value.toByteArray());
    }
    return value.toString();
}

QString objectPathProperty(const QVariantMap &properties, const QString &name)
{
    const QVariant value = properties.value(name);
    if (value.canConvert<QDBusObjectPath>()) {
        return value.value<QDBusObjectPath>().path();
    }
    return value.toString();
}

QStringList mountPoints(const QVariantMap &properties)
{
    QStringList result;
    const QVariant mountPointsValue = properties.value(QStringLiteral("MountPoints"));
    const MountPointList rawMountPoints = mountPointsValue.value<MountPointList>();
    for (const QByteArray &raw : rawMountPoints) {
        const QString root = QDir::cleanPath(QString::fromUtf8(raw));
        if (!root.isEmpty()) {
            result.append(root);
        }
    }
    return result;
}

QVariantMap interfaceProperties(const QVariantMap &interfaces, const QString &interfaceName)
{
    const QVariant value = interfaces.value(interfaceName);
    QVariantMap properties = value.toMap();
    if (!properties.isEmpty() || !value.canConvert<QDBusArgument>()) {
        return properties;
    }

    const QDBusArgument argument = value.value<QDBusArgument>();
    argument.beginMap();
    while (!argument.atEnd()) {
        argument.beginMapEntry();
        QString name;
        QVariant property;
        argument >> name >> property;
        argument.endMapEntry();
        properties.insert(name, property);
    }
    argument.endMap();
    return properties;
}

QVariantMap managedObjects(const QVariant &value)
{
    QVariantMap objects = value.toMap();
    if (!objects.isEmpty() || !value.canConvert<QDBusArgument>()) {
        return objects;
    }

    const QDBusArgument argument = value.value<QDBusArgument>();
    argument.beginMap();
    while (!argument.atEnd()) {
        argument.beginMapEntry();
        QDBusObjectPath objectPath;
        QVariantMap interfaces;
        argument >> objectPath >> interfaces;
        argument.endMapEntry();
        if (!objectPath.path().isEmpty() && !interfaces.isEmpty()) {
            objects.insert(objectPath.path(), interfaces);
        }
    }
    argument.endMap();
    return objects;
}

QVariant objectProperty(const QString &objectPath, const QString &interfaceName, const QString &propertyName)
{
    QDBusInterface properties(QString::fromLatin1(kService), objectPath,
                              QString::fromLatin1(kProperties), QDBusConnection::systemBus());
    const QDBusMessage reply = properties.call(QStringLiteral("Get"), interfaceName, propertyName);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return {};
    }
    return reply.arguments().constFirst().value<QDBusVariant>().variant();
}
}

LinuxDeviceMonitor::LinuxDeviceMonitor(QObject *parent)
    : QObject(parent)
{
    qDBusRegisterMetaType<MountPointList>();
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kRootPath),
                QString::fromLatin1(kObjectManager), QStringLiteral("InterfacesAdded"), this,
                SLOT(onInterfacesAdded(QDBusObjectPath,QVariantMap)));
    bus.connect(QString::fromLatin1(kService), QString::fromLatin1(kRootPath),
                QString::fromLatin1(kObjectManager), QStringLiteral("InterfacesRemoved"), this,
                SLOT(onInterfacesRemoved(QDBusObjectPath,QStringList)));
    bus.connect(QString::fromLatin1(kService), QString(),
                QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"), this,
                SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
}

bool LinuxDeviceMonitor::available() const
{
    return QDBusConnection::systemBus().interface()
        && QDBusConnection::systemBus().interface()->isServiceRegistered(QString::fromLatin1(kService));
}

QList<LinuxDeviceInfo> LinuxDeviceMonitor::mountedDevices() const
{
    const QList<LinuxDeviceInfo> allDevices = unmountedDevices();
    QList<LinuxDeviceInfo> result;
    for (const LinuxDeviceInfo &device : allDevices) {
        if (!device.rootPath.isEmpty()) result.append(device);
    }
    return result;
}

QList<LinuxDeviceInfo> LinuxDeviceMonitor::unmountedDevices() const
{
    if (m_snapshotValid) {
        return m_cachedDevices;
    }
    if (!available()) {
        if (deviceTraceEnabled()) qInfo().noquote() << "[DeviceTrace] udisks-unavailable";
        return {};
    }

    QDBusInterface manager(QString::fromLatin1(kService), QString::fromLatin1(kRootPath),
                            QString::fromLatin1(kObjectManager), QDBusConnection::systemBus());
    const QDBusMessage reply = manager.call(QStringLiteral("GetManagedObjects"));
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        if (deviceTraceEnabled()) {
            qInfo().noquote() << "[DeviceTrace] managed-objects-failed"
                              << reply.errorName() << reply.errorMessage();
        }
        return {};
    }

    const QVariantMap objects = managedObjects(reply.arguments().constFirst());
    QHash<QString, MountPointList> mountPointsByBlockDevice;
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || storage.rootPath().isEmpty() || storage.device().isEmpty()) {
            continue;
        }
        mountPointsByBlockDevice[QString::fromLocal8Bit(storage.device())]
            .append(QDir::cleanPath(storage.rootPath()).toUtf8());
    }
    QList<LinuxDeviceInfo> devices;
    QSet<QString> seenRoots;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const QVariantMap interfaces = it.value().toMap();
        if (!interfaces.contains(QString::fromLatin1(kFilesystem))
            || !interfaces.contains(QString::fromLatin1(kBlock))
            || interfaces.contains(QString::fromLatin1(kLoop))) {
            continue;
        }
        const QString blockDevice = QStringLiteral("/dev/%1").arg(it.key().section(QLatin1Char('/'), -1));
        const auto roots = mountPointsByBlockDevice.constFind(blockDevice);
        const QString drivePath = objectPathProperty(
            QVariantMap{{QStringLiteral("Drive"), objectProperty(it.key(), QString::fromLatin1(kBlock), QStringLiteral("Drive"))}},
            QStringLiteral("Drive"));
        const QVariantMap driveProperties{
            {QStringLiteral("Vendor"), objectProperty(drivePath, QString::fromLatin1(kDrive), QStringLiteral("Vendor"))},
            {QStringLiteral("Model"), objectProperty(drivePath, QString::fromLatin1(kDrive), QStringLiteral("Model"))},
            {QStringLiteral("Removable"), objectProperty(drivePath, QString::fromLatin1(kDrive), QStringLiteral("Removable"))},
            {QStringLiteral("Optical"), objectProperty(drivePath, QString::fromLatin1(kDrive), QStringLiteral("Optical"))},
            {QStringLiteral("Ejectable"), objectProperty(drivePath, QString::fromLatin1(kDrive), QStringLiteral("Ejectable"))},
            {QStringLiteral("CanPowerOff"), objectProperty(drivePath, QString::fromLatin1(kDrive), QStringLiteral("CanPowerOff"))},
        };
        const QVariantMap blockProperties{
            {QStringLiteral("IdLabel"), objectProperty(it.key(), QString::fromLatin1(kBlock), QStringLiteral("IdLabel"))},
            {QStringLiteral("IdType"), objectProperty(it.key(), QString::fromLatin1(kBlock), QStringLiteral("IdType"))},
        };
        const MountPointList deviceRoots = roots == mountPointsByBlockDevice.cend()
            ? MountPointList{QByteArray{}}
            : roots.value();
        for (const QByteArray &rawRoot : deviceRoots) {
            const QString root = QDir::cleanPath(QString::fromUtf8(rawRoot));
            if (!root.isEmpty() && seenRoots.contains(root)) continue;
            if (!root.isEmpty()) seenRoots.insert(root);
            LinuxDeviceInfo info;
            info.rootPath = root;
            info.stableId = it.key();
            info.blockDevice = blockDevice;
            info.driveObjectPath = drivePath;
            info.label = stringProperty(blockProperties, QStringLiteral("IdLabel"));
            info.fileSystem = stringProperty(blockProperties, QStringLiteral("IdType"));
            info.model = QStringList{
                stringProperty(driveProperties, QStringLiteral("Vendor")).trimmed(),
                stringProperty(driveProperties, QStringLiteral("Model")).trimmed()}.join(QLatin1Char(' ')).trimmed();
            info.removable = driveProperties.value(QStringLiteral("Removable")).toBool();
            info.optical = driveProperties.value(QStringLiteral("Optical")).toBool();
            info.ejectable = driveProperties.value(QStringLiteral("Ejectable")).toBool();
            info.canPowerOff = driveProperties.value(QStringLiteral("CanPowerOff")).toBool();
            devices.append(info);
        }
    }
    if (deviceTraceEnabled()) {
        qInfo().noquote() << "[DeviceTrace] managed-objects" << "count=" << objects.size()
                          << "mounted-filesystems=" << devices.size();
        for (const LinuxDeviceInfo &device : devices) {
            qInfo().noquote() << "[DeviceTrace] udisks-device"
                              << "root=" << device.rootPath
                              << "block=" << device.blockDevice
                              << "drive=" << device.driveObjectPath
                              << "ejectable=" << device.ejectable
                              << "canPowerOff=" << device.canPowerOff;
        }
    }
    m_cachedDevices = devices;
    m_snapshotValid = true;
    return m_cachedDevices;
}

QList<LinuxDeviceInfo> LinuxDeviceMonitor::devicesFromManagedObjects(const QVariantMap &objects)
{
    QList<LinuxDeviceInfo> result;
    QHash<QString, QVariantMap> drives;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const QVariantMap interfaces = it.value().toMap();
        const QVariantMap drive = interfaceProperties(interfaces, QString::fromLatin1(kDrive));
        if (!drive.isEmpty()) {
            drives.insert(it.key(), drive);
        }
    }

    QSet<QString> seenRoots;
    for (auto it = objects.cbegin(); it != objects.cend(); ++it) {
        const QVariantMap interfaces = it.value().toMap();
        if (deviceTraceEnabled()) {
            QStringList interfaceTypes;
            for (auto interfaceIt = interfaces.cbegin(); interfaceIt != interfaces.cend(); ++interfaceIt) {
                interfaceTypes.append(QStringLiteral("%1:%2")
                    .arg(interfaceIt.key(), QString::fromLatin1(interfaceIt.value().typeName())));
            }
            qInfo().noquote() << "[DeviceTrace] object"
                              << it.key() << interfaceTypes.join(QLatin1Char(','));
        }
        if (interfaces.contains(QString::fromLatin1(kLoop))) {
            continue;
        }
        const QVariantMap filesystem = interfaceProperties(interfaces, QString::fromLatin1(kFilesystem));
        const QVariantMap block = interfaceProperties(interfaces, QString::fromLatin1(kBlock));
        if (filesystem.isEmpty() || block.isEmpty()) {
            continue;
        }

        const QString drivePath = objectPathProperty(block, QStringLiteral("Drive"));
        const QVariantMap drive = drives.value(drivePath);
        for (const QString &root : mountPoints(filesystem)) {
            if (seenRoots.contains(root)) {
                continue;
            }
            seenRoots.insert(root);
            LinuxDeviceInfo info;
            info.rootPath = root;
            info.stableId = it.key();
            info.blockDevice = stringProperty(block, QStringLiteral("PreferredDevice"));
            info.driveObjectPath = drivePath;
            info.label = stringProperty(block, QStringLiteral("IdLabel"));
            info.fileSystem = stringProperty(block, QStringLiteral("IdType"));
            const QString vendor = stringProperty(drive, QStringLiteral("Vendor")).trimmed();
            const QString model = stringProperty(drive, QStringLiteral("Model")).trimmed();
            info.model = QStringList{vendor, model}.join(QLatin1Char(' ')).trimmed();
            info.removable = drive.value(QStringLiteral("Removable")).toBool();
            info.optical = drive.value(QStringLiteral("Optical")).toBool();
            info.ejectable = drive.value(QStringLiteral("Ejectable")).toBool();
            info.canPowerOff = drive.value(QStringLiteral("CanPowerOff")).toBool();
            result.append(info);
        }
    }
    return result;
}

bool LinuxDeviceMonitor::unmountOrEject(const QString &rootPath, QString *error) const
{
    const QString normalizedRoot = QDir::cleanPath(QDir::fromNativeSeparators(rootPath.trimmed()));
    const auto devices = mountedDevices();
    const auto it = std::find_if(devices.cbegin(), devices.cend(), [&normalizedRoot](const LinuxDeviceInfo &device) {
        return device.rootPath == normalizedRoot;
    });
    if (it == devices.cend()) {
        if (error) *error = QStringLiteral("The mounted device is no longer available.");
        return false;
    }

    const QVariantMap options;
    const bool shouldPowerOff = it->canPowerOff && !it->driveObjectPath.isEmpty();
    QList<LinuxDeviceInfo> unmountTargets;
    if (shouldPowerOff) {
        for (const LinuxDeviceInfo &device : devices) {
            if (device.driveObjectPath == it->driveObjectPath) {
                unmountTargets.append(device);
            }
        }
    } else {
        unmountTargets.append(*it);
    }

    for (const LinuxDeviceInfo &device : unmountTargets) {
        QDBusInterface filesystem(QString::fromLatin1(kService), device.stableId,
                                  QString::fromLatin1(kFilesystem), QDBusConnection::systemBus());
        const QDBusMessage unmountReply = filesystem.call(QStringLiteral("Unmount"), options);
        if (unmountReply.type() == QDBusMessage::ErrorMessage) {
            if (error) {
                *error = QStringLiteral("Cannot unmount %1: %2")
                    .arg(device.rootPath, unmountReply.errorMessage());
            }
            return false;
        }
    }

    const QString action = it->ejectable ? QStringLiteral("Eject")
        : (shouldPowerOff ? QStringLiteral("PowerOff") : QString());
    if (action.isEmpty() || it->driveObjectPath.isEmpty()) {
        return true;
    }

    QDBusInterface drive(QString::fromLatin1(kService), it->driveObjectPath,
                         QString::fromLatin1(kDrive), QDBusConnection::systemBus());
    const QDBusMessage actionReply = drive.call(action, options);
    if (actionReply.type() == QDBusMessage::ErrorMessage) {
        if (error) *error = actionReply.errorMessage();
        return false;
    }
    return true;
}

bool LinuxDeviceMonitor::mount(const QString &stableId, QString *rootPath, QString *error) const
{
    if (stableId.isEmpty()) {
        if (error) *error = QStringLiteral("Missing UDisks filesystem identifier.");
        return false;
    }
    QDBusInterface filesystem(QString::fromLatin1(kService), stableId,
                              QString::fromLatin1(kFilesystem), QDBusConnection::systemBus());
    const QDBusMessage reply = filesystem.call(QStringLiteral("Mount"), QVariantMap{});
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (error) *error = reply.errorMessage();
        return false;
    }
    if (rootPath && !reply.arguments().isEmpty()) {
        *rootPath = reply.arguments().constFirst().toString();
    }
    return true;
}

void LinuxDeviceMonitor::onInterfacesAdded(const QDBusObjectPath &, const QVariantMap &)
{
    m_snapshotValid = false;
    emit topologyChanged();
}

void LinuxDeviceMonitor::onInterfacesRemoved(const QDBusObjectPath &, const QStringList &)
{
    m_snapshotValid = false;
    emit topologyChanged();
}

void LinuxDeviceMonitor::onPropertiesChanged(const QString &, const QVariantMap &, const QStringList &)
{
    m_snapshotValid = false;
    emit topologyChanged();
}
