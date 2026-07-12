#include "LinuxDeviceMonitor.h"

#include <QCoreApplication>
#include <QDBusObjectPath>
#include <QTextStream>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

QVariantMap interfaces(const QVariantMap &drive, const QVariantMap &block, const QVariantMap &filesystem)
{
    return {
        {QStringLiteral("org.freedesktop.UDisks2.Drive"), drive},
        {QStringLiteral("org.freedesktop.UDisks2.Block"), block},
        {QStringLiteral("org.freedesktop.UDisks2.Filesystem"), filesystem},
    };
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString drivePath = QStringLiteral("/org/freedesktop/UDisks2/drives/USB_Test");
    const QVariantMap drive{
        {QStringLiteral("Vendor"), QStringLiteral("TestCo")},
        {QStringLiteral("Model"), QStringLiteral("USB")},
        {QStringLiteral("Removable"), true},
        {QStringLiteral("Ejectable"), false},
        {QStringLiteral("CanPowerOff"), true},
    };
    const QVariantMap block{
        {QStringLiteral("Drive"), QVariant::fromValue(QDBusObjectPath(drivePath))},
        {QStringLiteral("PreferredDevice"), QByteArray("/dev/sdb1")},
        {QStringLiteral("IdLabel"), QStringLiteral("BACKUP")},
        {QStringLiteral("IdType"), QStringLiteral("vfat")},
    };
    const QVariantMap filesystem{
        {QStringLiteral("MountPoints"), QVariantList{QByteArray("/run/media/test/BACKUP\0")}},
    };
    const QVariantMap duplicateFilesystem{
        {QStringLiteral("MountPoints"), QVariantList{QByteArray("/run/media/test/BACKUP\0")}},
    };
    const QVariantMap loopInterfaces{
        {QStringLiteral("org.freedesktop.UDisks2.Loop"), QVariantMap{}},
        {QStringLiteral("org.freedesktop.UDisks2.Block"), block},
        {QStringLiteral("org.freedesktop.UDisks2.Filesystem"), filesystem},
    };
    const QVariantMap objects{
        {drivePath, QVariant::fromValue(QVariantMap{{QStringLiteral("org.freedesktop.UDisks2.Drive"), drive}})},
        {QStringLiteral("/org/freedesktop/UDisks2/block_devices/sdb1"), QVariant::fromValue(interfaces({}, block, filesystem))},
        {QStringLiteral("/org/freedesktop/UDisks2/block_devices/sdb2"), QVariant::fromValue(interfaces({}, block, duplicateFilesystem))},
        {QStringLiteral("/org/freedesktop/UDisks2/block_devices/loop0"), QVariant::fromValue(loopInterfaces)},
    };

    const QList<LinuxDeviceInfo> devices = LinuxDeviceMonitor::devicesFromManagedObjects(objects);
    if (devices.size() != 1) {
        return fail(QStringLiteral("expected one physical mounted device, got %1").arg(devices.size()));
    }
    const LinuxDeviceInfo &device = devices.constFirst();
    if (device.rootPath != QLatin1String("/run/media/test/BACKUP")
        || device.stableId != QLatin1String("/org/freedesktop/UDisks2/block_devices/sdb1")
        || device.blockDevice != QLatin1String("/dev/sdb1")
        || device.label != QLatin1String("BACKUP")
        || device.fileSystem != QLatin1String("vfat")
        || device.model != QLatin1String("TestCo USB")
        || !device.removable
        || device.ejectable
        || !device.canPowerOff) {
        return fail(QStringLiteral("mapped UDisks device metadata is incorrect"));
    }

    return 0;
}
