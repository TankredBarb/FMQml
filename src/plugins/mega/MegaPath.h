#pragma once

#include <QLatin1StringView>
#include <QString>

namespace MegaPath {

constexpr QLatin1StringView Root{"mega:///"};
constexpr QLatin1StringView LinkPrefix{"mega://link/"};
constexpr QLatin1StringView CloudDriveName{"Cloud Drive"};
constexpr QLatin1StringView RubbishBinName{"Rubbish Bin"};
constexpr QLatin1StringView InboxName{"Inbox"};

bool isSchemePath(const QString &path);
QString normalizedPath(QString path);
QString parentPath(const QString &path);
QString childPath(const QString &parentPath, const QString &name);
QString fallbackFileNameForPath(const QString &path);

bool isLinkPath(const QString &path);
QString linkIdForPath(const QString &path);
QString relativePathForPath(const QString &path);

// Parses user input URL (e.g. https://mega.nz/folder/abc#key) and returns a normalized mega:// path.
// Extracts linkId, linkKey, and whether it is a folder link.
QString fromUserInput(const QString &input, QString &linkId, QString &linkKey, bool &isFolder);

} // namespace MegaPath
