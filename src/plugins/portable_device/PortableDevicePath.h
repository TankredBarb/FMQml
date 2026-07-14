#pragma once

#include <QString>

namespace PortableDevicePath {

inline constexpr QLatin1StringView Root{"portable://"};

struct ParsedPath {
    bool valid = false;
    bool root = false;
    QString deviceId;
    QString objectId;
};

QString devicePath(const QString &deviceId);
QString objectPath(const QString &deviceId, const QString &objectId);
ParsedPath parse(QString path);
QString normalized(const QString &path);

} // namespace PortableDevicePath
