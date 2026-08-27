#include "FileSearchModel.h"

#include <QCoreApplication>

#include <cstdio>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

FileSearchResult result(const QString &path, int relevance, qint64 size, const QDateTime &modified, int order)
{
    FileSearchResult value;
    value.path = path;
    value.name = path.section(QLatin1Char('/'), -1);
    value.parentPath = path.section(QLatin1Char('/'), 0, -2);
    value.size = size;
    value.modified = modified;
    value.matchKind = QStringLiteral("name");
    value.relevanceScore = relevance;
    value.discoveryOrder = order;
    return value;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QDateTime now = QDateTime::currentDateTime();
    FileSearchModel model;
    model.appendResults({
        result(QStringLiteral("/z/contains.txt"), 200, 30, now.addDays(-2), 0),
        result(QStringLiteral("/a/exact.txt"), 400, 20, now.addDays(-1), 1),
        result(QStringLiteral("/b/prefix.txt"), 300, 10, now, 2),
        result(QStringLiteral("/c/alpha.txt"), 200, 40, now.addDays(-3), 3)
    });

    model.sort(FileSearchModel::RelevanceSort);
    if (!expect(model.pathAt(0) == QStringLiteral("/a/exact.txt")
                    && model.pathAt(1) == QStringLiteral("/b/prefix.txt")
                    && model.pathAt(2) == QStringLiteral("/c/alpha.txt"),
                "Relevance sort must use score precedence and stable name/path ties")) return 1;

    model.sort(FileSearchModel::SizeSort);
    if (!expect(model.pathAt(0) == QStringLiteral("/b/prefix.txt")
                    && model.pathAt(3) == QStringLiteral("/c/alpha.txt"),
                "Size sort must order results by ascending size")) return 1;

    model.sort(FileSearchModel::ModifiedSort);
    if (!expect(model.pathAt(0) == QStringLiteral("/b/prefix.txt")
                    && model.pathAt(3) == QStringLiteral("/c/alpha.txt"),
                "Modified sort must show newest results first")) return 1;

    if (!expect(model.indexOfResult(QStringLiteral("/a/exact.txt"), QStringLiteral("name"), 0) >= 0,
                "Result identity lookup must survive sorting")) return 1;
    return 0;
}
