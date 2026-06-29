#include "BatchRenameEngine.h"

#include <QCoreApplication>
#include <QVariantList>
#include <QVariantMap>

#include <cstdio>

namespace {
bool expectName(const QList<BatchRenameEngine::RenamePreview> &preview, int index, const QString &expected)
{
    if (index < 0 || index >= preview.size()) {
        std::fprintf(stderr, "Missing preview row %d\n", index);
        return false;
    }
    if (preview.at(index).newName != expected) {
        std::fprintf(stderr,
                     "Unexpected name at %d expected %s got %s\n",
                     index,
                     qPrintable(expected),
                     qPrintable(preview.at(index).newName));
        return false;
    }
    if (preview.at(index).hasConflict) {
        std::fprintf(stderr, "Unexpected conflict at %d: %s\n", index, qPrintable(preview.at(index).error));
        return false;
    }
    return true;
}

QVariantMap rule(QVariantMap values)
{
    return values;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    BatchRenameEngine engine;

    QVariantList stackedRules;
    stackedRules << rule({
        {QStringLiteral("type"), QStringLiteral("replace")},
        {QStringLiteral("search"), QStringLiteral("^IMG_(\\d+)$")},
        {QStringLiteral("replace"), QStringLiteral("photo_$1")},
        {QStringLiteral("caseSensitive"), false},
        {QStringLiteral("regex"), true},
    });
    stackedRules << rule({
        {QStringLiteral("type"), QStringLiteral("transform")},
        {QStringLiteral("mode"), QStringLiteral("uppercase")},
    });
    stackedRules << rule({
        {QStringLiteral("type"), QStringLiteral("numbering")},
        {QStringLiteral("start"), 1},
        {QStringLiteral("padding"), 2},
        {QStringLiteral("position"), QStringLiteral("suffix")},
    });

    const auto stackedPreview = engine.generatePreview({
        QStringLiteral("/tmp/IMG_1001.jpg"),
        QStringLiteral("/tmp/IMG_1002.jpg"),
    }, stackedRules);
    if (!expectName(stackedPreview, 0, QStringLiteral("PHOTO_100101.jpg"))
        || !expectName(stackedPreview, 1, QStringLiteral("PHOTO_100202.jpg"))) {
        return 1;
    }

    QVariantList cleanupRules;
    cleanupRules << rule({
        {QStringLiteral("type"), QStringLiteral("transform")},
        {QStringLiteral("mode"), QStringLiteral("spaces-dash")},
    });
    cleanupRules << rule({
        {QStringLiteral("type"), QStringLiteral("transform")},
        {QStringLiteral("mode"), QStringLiteral("lowercase")},
    });
    const auto cleanupPreview = engine.generatePreview({QStringLiteral("/tmp/My  File Name.txt")}, cleanupRules);
    if (!expectName(cleanupPreview, 0, QStringLiteral("my-file-name.txt"))) {
        return 1;
    }

    QVariantList badRegexRules;
    badRegexRules << rule({
        {QStringLiteral("type"), QStringLiteral("replace")},
        {QStringLiteral("search"), QStringLiteral("(")},
        {QStringLiteral("replace"), QString()},
        {QStringLiteral("regex"), true},
    });
    const auto badRegexPreview = engine.generatePreview({QStringLiteral("/tmp/file.txt")}, badRegexRules);
    if (badRegexPreview.isEmpty() || !badRegexPreview.at(0).hasConflict
        || !badRegexPreview.at(0).error.startsWith(QStringLiteral("Invalid regular expression:"))) {
        std::fprintf(stderr, "Invalid regex was not reported\n");
        return 1;
    }

    return 0;
}
