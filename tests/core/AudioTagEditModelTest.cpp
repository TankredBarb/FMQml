#include "AudioTagEditModel.h"

#include <QCoreApplication>
#include <cstdio>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}
QVariantMap item(const QString &path, const QString &title, bool coverSupported = true)
{
    return {{QStringLiteral("path"), path}, {QStringLiteral("name"), path.section('/', -1)},
            {QStringLiteral("ok"), true}, {QStringLiteral("title"), title},
            {QStringLiteral("artist"), QStringLiteral("Artist")}, {QStringLiteral("album"), QStringLiteral("Album")},
            {QStringLiteral("year"), QStringLiteral("2024")}, {QStringLiteral("track"), QStringLiteral("1")},
            {QStringLiteral("genre"), QStringLiteral("Rock")}, {QStringLiteral("comment"), QString()},
            {QStringLiteral("lyrics"), QString()}, {QStringLiteral("coverWriteSupported"), coverSupported}};
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    AudioTagEditModel model;
    model.setRecords({item(QStringLiteral("/tmp/a.mp3"), QStringLiteral("A")),
                      item(QStringLiteral("/tmp/b.ogg"), QStringLiteral("B"), false)});
    if (!expect(model.rowCount() == 2 && model.dirtyCount() == 0, "Load must create clean rows")) return 1;
    model.updateField(0, QStringLiteral("title"), QStringLiteral("Changed"));
    if (!expect(model.dirtyCount() == 1 && model.record(0).value(QStringLiteral("dirty")).toBool(),
                "Field edit must dirty one row")) return 1;
    model.updateField(0, QStringLiteral("title"), QStringLiteral("A"));
    if (!expect(model.dirtyCount() == 0, "Restoring original field must clear dirty state")) return 1;
    if (!expect(model.setCover(0, QStringLiteral("/tmp/cover.jpg"), QStringLiteral("file:///tmp/cover.jpg"), false),
                "Supported row must accept cover")) return 1;
    if (!expect(model.applyCoverToAll(0) == 1 && model.dirtyCount() == 1,
                "Bulk cover must skip unsupported rows")) return 1;
    model.applyLookupFields(1, {{QStringLiteral("title"), QStringLiteral("Lookup")},
                                {QStringLiteral("artist"), QString()}});
    if (!expect(model.record(1).value(QStringLiteral("title")).toString() == QStringLiteral("Lookup")
                    && model.record(1).value(QStringLiteral("artist")).toString() == QStringLiteral("Artist"),
                "Lookup must apply non-empty fields only")) return 1;
    model.reconcileApplyResults({QVariantMap{{QStringLiteral("path"), QStringLiteral("/tmp/b.ogg")},
                                             {QStringLiteral("ok"), false},
                                             {QStringLiteral("message"), QStringLiteral("failed")}},
                                 QVariantMap{{QStringLiteral("path"), QStringLiteral("/tmp/a.mp3")},
                                             {QStringLiteral("ok"), true}}});
    if (!expect(!model.record(0).value(QStringLiteral("dirty")).toBool()
                    && model.record(1).value(QStringLiteral("dirty")).toBool()
                    && model.record(1).value(QStringLiteral("error")).toString() == QStringLiteral("failed"),
                "Apply results must reconcile by path and preserve failed dirty rows")) return 1;
    model.clearTags(1);
    if (!expect(model.record(1).value(QStringLiteral("title")).toString().isEmpty(),
                "Clear tags must clear editable fields")) return 1;
    return 0;
}
