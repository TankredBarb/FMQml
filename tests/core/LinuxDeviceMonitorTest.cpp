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

    QList<LinuxDeviceInfo> partitions;
    for (int i = 2; i <= 4; ++i) {
        LinuxDeviceInfo partition;
        partition.driveObjectPath = drivePath;
        partition.stableId = QStringLiteral("partition%1").arg(i);
        partition.blockDevice = QStringLiteral("/dev/sdc%1").arg(i);
        partition.model = QStringLiteral("TOSHIBA HDWD110");
        partition.label = QStringLiteral("Volume %1").arg(i);
        partition.fileSystem = QStringLiteral("ntfs");
        partitions.append(partition);
    }
    const auto sleeping = LinuxDeviceMonitor::groupStandbyDevices(partitions, {drivePath});
    if (sleeping.size() != 1 || !sleeping.first().inStandby
        || sleeping.first().stableId != drivePath || !sleeping.first().blockDevice.isEmpty()
        || !sleeping.first().label.isEmpty() || !sleeping.first().fileSystem.isEmpty()
        || sleeping.first().model != QLatin1String("TOSHIBA HDWD110")) {
        return fail(QStringLiteral("standby partitions must collapse into one physical drive"));
    }
    if (LinuxDeviceMonitor::groupStandbyDevices(partitions, {}).size() != 3) {
        return fail(QStringLiteral("unknown or awake drives must retain all partitions"));
    }
    LinuxDeviceInfo otherDrive = partitions.first();
    otherDrive.driveObjectPath += QStringLiteral("_other");
    partitions.append(otherDrive);
    if (LinuxDeviceMonitor::groupStandbyDevices(partitions, {drivePath, otherDrive.driveObjectPath}).size() != 2) {
        return fail(QStringLiteral("different physical drives must remain separate"));
    }
    partitions[0].rootPath = QStringLiteral("/mnt/test");
    const auto mounted = LinuxDeviceMonitor::groupStandbyDevices(partitions, {drivePath});
    if (mounted.size() != 4 || mounted.first().inStandby) {
        return fail(QStringLiteral("a drive with a mounted partition must not collapse"));
    }

    return 0;
}
