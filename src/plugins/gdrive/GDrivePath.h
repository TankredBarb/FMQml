#pragma once

#include <QLatin1StringView>
#include <QString>

namespace GDrivePath {

constexpr QLatin1StringView Root{"gdrive://"};
constexpr QLatin1StringView MyDrive{"gdrive://my-drive"};
constexpr QLatin1StringView SharedWithMe{"gdrive://shared-with-me"};
constexpr QLatin1StringView ShortcutsRoot{"gdrive://shortcuts"};
constexpr QLatin1StringView Trash{"gdrive://trash"};
constexpr QLatin1StringView ItemPrefix{"gdrive://item/"};
constexpr QLatin1StringView ShortcutPrefix{"gdrive://shortcuts/"};
constexpr QLatin1StringView NewPrefix{"gdrive://new/"};

struct PendingPath {
    QString parentId;
    QString name;

    bool valid() const
    {
        return !parentId.trimmed().isEmpty() && !name.trimmed().isEmpty();
    }
};

bool isSchemePath(const QString &path);
QString itemPathForId(const QString &id);
QString idForItemPath(const QString &path);
QString shortcutPathForId(const QString &id);
QString idForShortcutPath(const QString &path);
bool isShortcutsViewPath(const QString &path);
QString pendingPathForParentIdAndName(const QString &parentId, const QString &name);
PendingPath pendingPathInfo(const QString &path);
QString parentPathForDriveParentId(const QString &parentId);
QString driveParentIdForPath(const QString &path);
QString normalizedPath(QString path);
QString parentPath(const QString &path);
QString fallbackFileNameForPath(const QString &path);
QString virtualIconNameForPath(const QString &path);
QString childPath(const QString &parentPath, const QString &name);

} // namespace GDrivePath
