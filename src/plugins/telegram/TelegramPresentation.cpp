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
    fileEntry.mimeType = entry.mimeType;
    if (!entry.openUrl.isEmpty()) {
        fileEntry.shortcutOpenPath = entry.openUrl;
        fileEntry.shortcutTargetPath = entry.openUrl;
        fileEntry.shortcutTargetMimeType = QStringLiteral("text/html");
        fileEntry.isShortcut = true;
    }
    fileEntry.isDirectory = entry.directory;
    fileEntry.isReadOnly = true;
    fileEntry.hasThumbnail = entry.hasThumbnail;
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
