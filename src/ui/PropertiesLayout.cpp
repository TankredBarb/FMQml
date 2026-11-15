#include "PropertiesLayout.h"

#include <QSet>

namespace {
void appendSection(QVariantList &sections, const QString &title, const QString &layout,
                   const QVariantList &rows, const QString &icon = {})
{
    if (!rows.isEmpty())
        sections.append(QVariantMap{{"title", title}, {"layout", layout}, {"rows", rows}, {"icon", icon}});
}
}

QVariantList PropertiesLayout::generalSections(const QVariantList &rows) const
{
    QVariantList identity, paths, statistics, dates, other;
    bool drive = false;
    for (const auto &value : rows) {
        const auto key = value.toMap().value("key").toString();
        if (key == QLatin1String("general.name") || key == QLatin1String("general.type")
            || key == QLatin1String("selection.count") || key == QLatin1String("selection.contains")) {
            identity.append(value);
        } else if (key == QLatin1String("general.location") || key == QLatin1String("general.fullPath")
                   || key == QLatin1String("general.root") || key == QLatin1String("selection.location")) {
            paths.append(value);
            drive |= key == QLatin1String("general.root");
        } else if (key == QLatin1String("general.size") || key == QLatin1String("general.contents")
                   || key == QLatin1String("selection.totalSize") || key == QLatin1String("selection.contents")
                   || key == QLatin1String("general.usedSpace") || key == QLatin1String("general.freeSpace")
                   || key == QLatin1String("general.totalSpace")
                   || key == QLatin1String("general.fileSystem") || key == QLatin1String("selection.fileSystem")
                   || key == QLatin1String("general.onDisk") || key == QLatin1String("selection.onDisk")
                   || key == QLatin1String("general.spaceNote") || key == QLatin1String("selection.spaceNote")) {
            statistics.append(value);
        } else if (key.startsWith(QLatin1String("general.created")) || key.startsWith(QLatin1String("general.modified"))
                   || key.startsWith(QLatin1String("general.accessed")) || key.startsWith(QLatin1String("selection.created"))
                   || key.startsWith(QLatin1String("selection.modified")) || key.startsWith(QLatin1String("selection.accessed"))) {
            dates.append(value);
        } else {
            other.append(value);
        }
    }
    identity.append(paths);
    QVariantList sections;
    appendSection(sections, {}, "identity", identity);
    appendSection(sections, {}, "statistics", statistics);
    appendSection(sections, "Dates", "rows", dates);
    appendSection(sections, drive ? "Storage" : "General", "rows", other);
    return sections;
}

QVariantList PropertiesLayout::detailsSections(const QVariantList &rows) const
{
    static const QSet<QString> audioKeys = {"details.duration", "details.bitrate", "details.sample_rate", "details.channels"};
    static const QSet<QString> imageKeys = {"details.dimensions", "details.megapixels", "details.pixel_format", "details.color_depth",
                                           "details.alpha_channel", "details.dpi", "details.color_space", "details.frames", "details.loop_count", "details.frame_delay", "details.animated"};
    static const QSet<QString> documentKeys = {"details.pages", "details.pdf_version", "details.lines", "details.encoding"};
    static const QSet<QString> archiveKeys = {"details.entries", "details.files", "details.folders", "details.uncompressed", "details.packed",
                                             "details.compressed", "details.archive_ratio", "details.encrypted", "details.contains"};
    QVariantList information, audio, image, document, archive, notes;
    bool track = false;
    for (const auto &value : rows) {
        const auto key = value.toMap().value("key").toString();
        track |= key == QLatin1String("details.artist") || key == QLatin1String("details.album");
        if (audioKeys.contains(key)) audio.append(value);
        else if (imageKeys.contains(key)) image.append(value);
        else if (documentKeys.contains(key)) document.append(value);
        else if (archiveKeys.contains(key)) archive.append(value);
        else if (key == QLatin1String("details.comment")) notes.append(value);
        else information.append(value);
    }
    QVariantList sections;
    appendSection(sections, track ? "Track Information" : "Information", "rows", information, track ? "music" : "info");
    appendSection(sections, "Audio", "statistics", audio);
    appendSection(sections, "Image", "statistics", image);
    appendSection(sections, "Document", "statistics", document);
    appendSection(sections, "Archive", "statistics", archive);
    appendSection(sections, {}, "rows", notes);
    return sections;
}
