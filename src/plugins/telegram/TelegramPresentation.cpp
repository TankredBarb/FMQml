#include "TelegramPresentation.h"

#include <QLocale>

namespace TelegramProviderInternal {

namespace {

QString byteSizeText(qint64 size)
{
    return QLocale().formattedDataSize(size);
}

QString suffixForName(const QString &name)
{
    const int dotIndex = name.lastIndexOf(QLatin1Char('.'));
    if (dotIndex <= 0 || dotIndex == name.size() - 1) {
        return {};
    }
    return name.mid(dotIndex + 1).toLower();
}

} // namespace

FileEntry fileEntryFromTelegramEntry(const TelegramEntry &entry)
{
    FileEntry fileEntry;
    fileEntry.name = entry.name;
    fileEntry.path = entry.path;
    fileEntry.suffix = entry.directory ? QString{} : suffixForName(entry.name);
    fileEntry.size = entry.directory ? 0 : entry.size;
    fileEntry.sizeText = entry.directory ? QString{} : byteSizeText(entry.size);
    fileEntry.modified = entry.date;
    fileEntry.created = entry.date;
    fileEntry.modifiedText = entry.date.isValid() ? entry.date.toString(Qt::ISODate) : QString{};
    fileEntry.createdText = fileEntry.modifiedText;
    fileEntry.attributesText = entry.directory ? QStringLiteral("Virtual folder") : QStringLiteral("Read-only");
    fileEntry.providerCapabilitiesText = entry.providerLabel;
    fileEntry.iconName = entry.iconName;
    if (entry.iconName == QLatin1String("telegram-saved")) {
        fileEntry.overlayIconName = QStringLiteral("telegram");
    } else if (entry.iconName == QLatin1String("telegram-chats")) {
        fileEntry.overlayIconName = QStringLiteral("telegram-badge-chat");
    } else if (entry.iconName == QLatin1String("telegram-downloads")) {
        fileEntry.overlayIconName = QStringLiteral("telegram-badge-downloads");
    } else if (entry.iconName == QLatin1String("telegram-badge-chat")
               || entry.iconName == QLatin1String("telegram-badge-channel")
               || entry.iconName == QLatin1String("telegram-badge-load-more")) {
        fileEntry.overlayIconName = entry.iconName;
    }
    if (!fileEntry.overlayIconName.isEmpty()) {
        fileEntry.iconRecolorAllowed = false;
    }
    fileEntry.mimeType = entry.mimeType;
    if (!entry.openUrl.isEmpty()) {
        fileEntry.shortcutOpenPath = entry.openUrl;
        fileEntry.shortcutTargetPath = entry.openUrl;
        fileEntry.shortcutTargetMimeType = QStringLiteral("text/html");
        fileEntry.isShortcut = true;
    }
    fileEntry.isDirectory = entry.directory;
    fileEntry.isReadOnly = true;
    const bool svgImage = fileEntry.suffix == QLatin1String("svg")
        || fileEntry.suffix == QLatin1String("svgz");
    fileEntry.isImage = !entry.directory
        && (entry.mimeType.startsWith(QStringLiteral("image/"), Qt::CaseInsensitive)
            || svgImage);
    fileEntry.hasThumbnail = entry.hasThumbnail
        || svgImage
        || (fileEntry.isImage && entry.downloaded && !entry.localPath.isEmpty());
    fileEntry.specialAction = entry.loadMore ? FileEntrySpecialAction::LoadMore : FileEntrySpecialAction::None;
    return fileEntry;
}

TelegramEntry rootEntry(const QString &name, const QString &path, const QString &label)
{
    TelegramEntry entry;
    entry.name = name;
    entry.path = path;
    entry.providerLabel = label;
    entry.directory = true;
    entry.iconName = QStringLiteral("folder");
    return entry;
}

} // namespace TelegramProviderInternal
