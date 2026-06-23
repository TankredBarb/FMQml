#include "MegaPresentation.h"

#include "FileTypeIconResolver.h"

#include <QMimeDatabase>
#include <QMimeType>
#include <QSet>
#include <QString>

namespace {

QString iconNameFromResolvedPath(const QString &resolved)
{
    const QString prefix = QStringLiteral("qrc:/qt/qml/FM/qml/assets/filetypes-next/");
    if (!resolved.startsWith(prefix) || !resolved.endsWith(QStringLiteral(".svg"))) {
        return {};
    }
    return resolved.mid(prefix.size(), resolved.size() - prefix.size() - 4);
}

const QSet<QString> &imageSuffixes()
{
    static const QSet<QString> suffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"),
        QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("svg"), QStringLiteral("svgz"),
        QStringLiteral("avif"), QStringLiteral("ico")
    };
    return suffixes;
}

const QSet<QString> &previewSuffixes()
{
    static const QSet<QString> suffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"),
        QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("svg"), QStringLiteral("svgz"),
        QStringLiteral("avif"), QStringLiteral("ico"), QStringLiteral("mp4"), QStringLiteral("mov"),
        QStringLiteral("m4v"), QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("avi")
    };
    return suffixes;
}

} // namespace

namespace MegaPresentation {

void enrichEntryPresentation(FileEntry &entry)
{
    if (!entry.isDirectory && entry.suffix.isEmpty()) {
        const int suffixIndex = entry.name.lastIndexOf(QLatin1Char('.'));
        entry.suffix = suffixIndex >= 0 ? entry.name.mid(suffixIndex + 1).toLower() : QString{};
    }

    if (!entry.isDirectory) {
        const QMimeType mime = QMimeDatabase().mimeTypeForFile(entry.name, QMimeDatabase::MatchExtension);
        entry.mimeType = mime.isValid() ? mime.name() : QString{};
        entry.isImage = entry.mimeType.startsWith(QStringLiteral("image/"), Qt::CaseInsensitive)
            || imageSuffixes().contains(entry.suffix);
        entry.hasThumbnail = entry.isImage || previewSuffixes().contains(entry.suffix);
    }

    const bool replaceIcon = entry.iconName.isEmpty()
        || entry.iconName == QLatin1String("folder")
        || entry.iconName == QLatin1String("document");
    if (!replaceIcon) {
        return;
    }

    const QString hint = entry.path.isEmpty() ? entry.name : entry.path;
    const QString resolved = FileTypeIconResolver().iconForPathHint(hint, entry.isDirectory);
    const QString resolvedIconName = iconNameFromResolvedPath(resolved);
    if (!resolvedIconName.isEmpty()) {
        entry.iconName = resolvedIconName;
    }
}

} // namespace MegaPresentation
