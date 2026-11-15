#include "ui/PropertiesLayout.h"
#include <QCoreApplication>
#include <QVariantMap>
#include <cstdio>

namespace {
QVariantMap row(const char *key, const char *value = "value")
{
    return {{"key", key}, {"label", "Label"}, {"value", value}, {"busy", true}, {"copyable", true}};
}
bool preservesRows(const QVariantList &input, const QVariantList &sections)
{
    QVariantList remaining = input;
    for (const auto &section : sections) {
        const auto rows = section.toMap().value("rows").toList();
        if (rows.isEmpty()) return false;
        for (const auto &value : rows) {
            const int index = remaining.indexOf(value);
            if (index < 0) return false;
            remaining.removeAt(index);
        }
    }
    return remaining.isEmpty();
}
}
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    PropertiesLayout layout;
    const QVariantList general = {row("general.name"), row("general.location"), row("general.fullPath"),
        row("general.type"), row("general.size"), row("general.onDisk", "Unavailable"),
        row("general.fileSystem", "btrfs"), row("general.spaceNote"), row("general.contents"), row("general.created"),
        row("general.modified"), row("general.accessed"), row("general.unknown", "")};
    const QVariantList drive = {row("general.name"), row("general.root"), row("general.type"),
        row("general.fileSystem"), row("general.usedSpace"), row("general.freeSpace"), row("general.totalSpace"),
        row("general.ready"), row("general.spaceStatus")};
    const QVariantList multiple = {row("selection.count"), row("selection.location"), row("selection.contains"),
        row("selection.contents"), row("selection.totalSize"), row("selection.onDisk", "Unavailable"),
        row("selection.fileSystem", "btrfs"), row("selection.spaceNote"), row("selection.createdRange"),
        row("selection.modifiedRange"), row("selection.accessedRange")};
    for (const auto &input : {general, drive, multiple}) {
        if (!preservesRows(input, layout.generalSections(input))) return 1;
    }
    for (const auto &input : {general, multiple}) {
        bool found = false;
        for (const auto &section : layout.generalSections(input)) {
            const auto map = section.toMap();
            if (map.value("layout") != "statistics") continue;
            QStringList keys;
            for (const auto &value : map.value("rows").toList())
                keys.append(value.toMap().value("key").toString());
            const QString prefix = input == general ? "general." : "selection.";
            if (!keys.contains(prefix + "fileSystem") || !keys.contains(prefix + "onDisk")
                || !keys.contains(prefix + "spaceNote")) return 5;
            found = true;
        }
        if (!found) return 6;
    }
    const QVariantList details = {row("details.title"), row("details.artist"), row("details.album"),
        row("details.comment"), row("details.duration"), row("details.bitrate"), row("details.sample_rate"),
        row("details.channels"), row("details.unknown"), row("details.unknown")};
    const auto sections = layout.detailsSections(details);
    if (!preservesRows(details, sections) || sections.size() != 3
        || sections.at(1).toMap().value("title") != "Audio"
        || sections.at(1).toMap().value("rows").toList().size() != 4) return 2;
    for (const auto &keys : {QVariantList{row("details.dimensions"), row("details.color_space"), row("details.custom")},
                             QVariantList{row("details.pages"), row("details.title")},
                             QVariantList{row("details.packed"), row("details.encrypted")}}) {
        if (!preservesRows(keys, layout.detailsSections(keys))) return 3;
    }
    if (!layout.generalSections({}).isEmpty() || !layout.detailsSections({}).isEmpty()) return 4;
    std::puts("Properties presentation preserves all rows and metadata");
    return 0;
}
