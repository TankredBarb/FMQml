#include "PortableDevicePath.h"

#include <QUrl>

namespace PortableDevicePath {
namespace {
constexpr QLatin1StringView DevicePrefix{"portable://device/"};
constexpr QLatin1StringView ObjectSegment{"/object/"};

QString encodedSegment(const QString &value)
{
    return QString::fromLatin1(QUrl::toPercentEncoding(value));
}

QString decodedSegment(const QString &value)
{
    return QUrl::fromPercentEncoding(value.toUtf8());
}
} // namespace

QString devicePath(const QString &deviceId)
{
    return QString(DevicePrefix) + encodedSegment(deviceId);
}

QString objectPath(const QString &deviceId, const QString &objectId)
{
    return devicePath(deviceId) + QString(ObjectSegment) + encodedSegment(objectId);
}

ParsedPath parse(QString path)
{
    path = path.trimmed();
    if (path.compare(QStringLiteral("portable:"), Qt::CaseInsensitive) == 0
        || path.compare(QStringLiteral("portable:/"), Qt::CaseInsensitive) == 0
        || path.compare(QString(Root), Qt::CaseInsensitive) == 0) {
        return {true, true, {}, {}};
    }
    if (!path.startsWith(DevicePrefix, Qt::CaseInsensitive)) {
        return {};
    }
    const QString tail = path.mid(QString(DevicePrefix).size());
    if (tail.isEmpty()) {
        return {};
    }
    const int objectSegment = tail.indexOf(QString(ObjectSegment), 0, Qt::CaseInsensitive);
    if (objectSegment < 0) {
        return {true, false, decodedSegment(tail), {}};
    }
    const QString encodedDevice = tail.left(objectSegment);
    const QString encodedObject = tail.mid(objectSegment + QString(ObjectSegment).size());
    if (encodedDevice.isEmpty() || encodedObject.isEmpty()) {
        return {};
    }
    return {true, false, decodedSegment(encodedDevice), decodedSegment(encodedObject)};
}

QString normalized(const QString &path)
{
    const ParsedPath parsed = parse(path);
    if (!parsed.valid) {
        return {};
    }
    if (parsed.root) {
        return QString(Root);
    }
    return parsed.objectId.isEmpty() ? devicePath(parsed.deviceId) : objectPath(parsed.deviceId, parsed.objectId);
}

} // namespace PortableDevicePath
