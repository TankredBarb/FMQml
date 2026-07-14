#include "PortableDevicePath.h"

#include <QCoreApplication>
#include <QDebug>

namespace {
int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    for (const QString &root : {QStringLiteral("portable:"), QStringLiteral("portable:/"), QStringLiteral("PORTABLE://")}) {
        if (PortableDevicePath::normalized(root) != QStringLiteral("portable://")) {
            return fail(QStringLiteral("portable root normalization changed"));
        }
    }

    const QString deviceId = QStringLiteral("mtp:/Pixel 9/");
    const QString objectId = QStringLiteral("Internal storage/DCIM/Camera");
    const QString path = PortableDevicePath::objectPath(deviceId, objectId);
    const PortableDevicePath::ParsedPath parsed = PortableDevicePath::parse(path);
    if (!parsed.valid || parsed.root || parsed.deviceId != deviceId || parsed.objectId != objectId) {
        return fail(QStringLiteral("portable path percent-encoding round trip changed"));
    }
    if (PortableDevicePath::normalized(path) != path) {
        return fail(QStringLiteral("portable object path is not stable under normalization"));
    }
    if (PortableDevicePath::parse(QStringLiteral("portable://device//object/x")).valid
        || PortableDevicePath::parse(QStringLiteral("file:///tmp/x")).valid) {
        return fail(QStringLiteral("invalid portable paths must remain rejected"));
    }
    return 0;
}
