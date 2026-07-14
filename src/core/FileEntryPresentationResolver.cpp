#include "FileEntryPresentationResolver.h"

#include "FileTypeIconResolver.h"

#include <QSet>
#include <QUrl>

QString FileEntryPresentationResolver::breadcrumbIconNameForPath(const QString &path)
{
    const QString lower = path.toLower();
    if (lower.startsWith(QStringLiteral("mega://"))) return QStringLiteral("mega");
    if (lower == QLatin1String("gdrive://shared-with-me")) return QStringLiteral("gdrive-badge-shared");
    if (lower == QLatin1String("gdrive://shortcuts")) return QStringLiteral("gdrive-badge-shortcut");
    if (lower == QLatin1String("gdrive://trash")) return QStringLiteral("gdrive-badge-trash");
    if (lower.startsWith(QStringLiteral("gdrive://"))) return QStringLiteral("gdrive");
    if (lower.startsWith(QStringLiteral("instagram://"))) return QStringLiteral("instagram");
    if (lower == QLatin1String("telegram://saved")) return QStringLiteral("telegram-saved");
    if (lower == QLatin1String("telegram://chats")) return QStringLiteral("telegram-chats");
    if (lower == QLatin1String("telegram://downloads")) return QStringLiteral("telegram-downloads");
    if (lower.startsWith(QStringLiteral("telegram://channel/"))) return QStringLiteral("telegram-badge-channel");
    if (lower.startsWith(QStringLiteral("telegram://chat/"))) return QStringLiteral("telegram-badge-chat");
    if (lower.startsWith(QStringLiteral("telegram://"))) return QStringLiteral("telegram");
    return {};
}

QString FileEntryPresentationResolver::previewIconNameForPath(const QString &path)
{
    return path.compare(QStringLiteral("gdrive://"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("gdrive") : QString{};
}

QString FileEntryPresentationResolver::previewIconSource(const QString &path,
                                                         bool directory,
                                                         const QString &suffix,
                                                         const QString &mimeName,
                                                         bool useNativeIcons,
                                                         bool useHighQualitySystemIcons)
{
    if (path.isEmpty()) return {};
    if (path == QLatin1String("devices://")) return QStringLiteral("qrc:/qt/qml/FM/qml/assets/icons/computer.svg");
    if (path == QLatin1String("favorites://")) return QStringLiteral("qrc:/qt/qml/FM/qml/assets/icons/star.svg");
    if (path == QLatin1String("selection://")) return QStringLiteral("qrc:/qt/qml/FM/qml/assets/icons/grid.svg");

    const QString presentationIconName = previewIconNameForPath(path);
    if (!presentationIconName.isEmpty()) {
        return QStringLiteral("qrc:/qt/qml/FM/qml/assets/filetypes-next/%1.svg").arg(presentationIconName);
    }

    const QString lower = path.toLower();
    const bool providerPath = path.indexOf(QStringLiteral("://")) > 0
        && !lower.startsWith(QStringLiteral("archive://"))
        && !lower.startsWith(QStringLiteral("file://"));
    static const FileTypeIconResolver iconResolver;

    QString nativeOverride = iconResolver.nativeIconOverrideForPathHint(path, directory);
    if (nativeOverride.isEmpty() && providerPath && !suffix.isEmpty()) {
        nativeOverride = iconResolver.nativeIconOverrideForPathHint(QStringLiteral("file.%1").arg(suffix), directory);
    }
    if (!nativeOverride.isEmpty()) return nativeOverride;

    if (!useNativeIcons) {
        return providerPath && !suffix.isEmpty()
            ? iconResolver.iconForSuffix(suffix, directory)
            : iconResolver.iconForPathHint(path, directory);
    }

    QString query = directory
        ? QStringLiteral("?directory=true&hq=%1").arg(useHighQualitySystemIcons ? 1 : 0)
        : QStringLiteral("?hq=%1").arg(useHighQualitySystemIcons ? 1 : 0);
    if (providerPath) query += QStringLiteral("&provider=true");
    if (!suffix.isEmpty()) query += QStringLiteral("&suffix=") + QString::fromLatin1(QUrl::toPercentEncoding(suffix));
    if (!mimeName.isEmpty()) query += QStringLiteral("&mime=") + QString::fromLatin1(QUrl::toPercentEncoding(mimeName));
    return QStringLiteral("image://icon/")
        + QString::fromLatin1(QUrl::toPercentEncoding(path + query));
}

QString FileEntryPresentationResolver::menuIconName(const FileEntry &entry)
{
    const QString path = entry.path.toLower();
    if (path == QLatin1String("gdrive://my-drive")) return QStringLiteral("gdrive");
    if (path == QLatin1String("gdrive://shared-with-me")) return QStringLiteral("gdrive-badge-shared");
    if (path == QLatin1String("gdrive://shortcuts")) return QStringLiteral("gdrive-badge-shortcut");
    if (path == QLatin1String("gdrive://trash")) return QStringLiteral("gdrive-badge-trash");
    if (path == QLatin1String("mega:///cloud drive")) return QStringLiteral("mega");

    static const QSet<QString> semanticIcons{
        QStringLiteral("gdrive"), QStringLiteral("gdrive-badge-shared"),
        QStringLiteral("gdrive-badge-shortcut"), QStringLiteral("gdrive-badge-trash"),
        QStringLiteral("gdrive-shortcut"), QStringLiteral("mega"),
        QStringLiteral("instagram-stories"), QStringLiteral("instagram-badge-stories"),
        QStringLiteral("instagram-load-more"), QStringLiteral("instagram-badge-load-more"),
        QStringLiteral("telegram-saved"), QStringLiteral("telegram-chats"),
        QStringLiteral("telegram-downloads"), QStringLiteral("telegram-badge-downloads"),
        QStringLiteral("telegram-badge-load-more"),
    };
    return semanticIcons.contains(entry.iconName) ? entry.iconName : QString{};
}

bool FileEntryPresentationResolver::menuUsesAvatar(const FileEntry &entry)
{
    return entry.hasThumbnail
        && (entry.iconName == QLatin1String("telegram-badge-chat")
            || entry.iconName == QLatin1String("telegram-badge-channel"));
}

bool FileEntryPresentationResolver::isRemotePreviewContentPath(const QString &path)
{
    const QString scheme = path.left(path.indexOf(QStringLiteral("://"))).toLower();
    return scheme == QLatin1String("portable") || scheme == QLatin1String("gdrive")
        || scheme == QLatin1String("mega") || scheme == QLatin1String("ftp")
        || scheme == QLatin1String("instagram");
}

bool FileEntryPresentationResolver::canRequestThumbnail(const QString &path)
{
    if (path.isEmpty() || isRemotePreviewContentPath(path) || path == QLatin1String("devices://")
        || path.endsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('\\'))) return false;
    const qsizetype slash = qMax(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
    const QString name = slash >= 0 ? path.mid(slash + 1) : path;
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    return dot > 0 && dot < name.size() - 1;
}
