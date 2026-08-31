#include "FileEntrySortPolicy.h"

#include <QCoreApplication>
#include <QTextStream>

#include <algorithm>

namespace {

bool expect(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAILED: " << message << '\n';
    }
    return condition;
}

FileEntry entry(const QString &name, const QString &path, bool directory = false)
{
    FileEntry value;
    value.name = name;
    value.path = path;
    value.isDirectory = directory;
    return value;
}

QStringList sortedNames(QList<FileEntry> entries, bool mixFilesAndFolders,
                        int sortRole, Qt::SortOrder sortOrder)
{
    std::sort(entries.begin(), entries.end(), [=](const FileEntry &left, const FileEntry &right) {
        return FileEntrySortPolicy::lessThan(left, right, mixFilesAndFolders, sortRole, sortOrder);
    });

    QStringList names;
    for (const FileEntry &value : entries) {
        names.append(value.name);
    }
    return names;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    const FileEntry folder = entry(QStringLiteral("Zoo"), QStringLiteral("/zoo"), true);
    const FileEntry file = entry(QStringLiteral("Alpha"), QStringLiteral("/alpha"));
    ok &= expect(sortedNames({file, folder}, false, 0, Qt::AscendingOrder)
                     == QStringList({QStringLiteral("Zoo"), QStringLiteral("Alpha")}),
                 QStringLiteral("folders-first ordering changed"));
    ok &= expect(sortedNames({file, folder}, true, 0, Qt::AscendingOrder)
                     == QStringList({QStringLiteral("Alpha"), QStringLiteral("Zoo")}),
                 QStringLiteral("mixed file and folder ordering changed"));

    QList<FileEntry> values{
        entry(QStringLiteral("charlie"), QStringLiteral("/3")),
        entry(QStringLiteral("Alpha"), QStringLiteral("/1")),
        entry(QStringLiteral("bravo"), QStringLiteral("/2")),
    };
    ok &= expect(sortedNames(values, true, 0, Qt::AscendingOrder)
                     == QStringList({QStringLiteral("Alpha"), QStringLiteral("bravo"), QStringLiteral("charlie")}),
                 QStringLiteral("ascending name ordering changed"));
    ok &= expect(sortedNames(values, true, 0, Qt::DescendingOrder)
                     == QStringList({QStringLiteral("charlie"), QStringLiteral("bravo"), QStringLiteral("Alpha")}),
                 QStringLiteral("descending name ordering changed"));

    values[0].size = 20;
    values[1].size = 30;
    values[2].size = 10;
    ok &= expect(sortedNames(values, true, 1, Qt::AscendingOrder)
                     == QStringList({QStringLiteral("bravo"), QStringLiteral("charlie"), QStringLiteral("Alpha")}),
                 QStringLiteral("size ordering changed"));

    values[0].suffix = QStringLiteral("txt");
    values[1].suffix = QStringLiteral("JPG");
    values[2].suffix = QStringLiteral("pdf");
    ok &= expect(sortedNames(values, true, 2, Qt::AscendingOrder)
                     == QStringList({QStringLiteral("Alpha"), QStringLiteral("bravo"), QStringLiteral("charlie")}),
                 QStringLiteral("suffix ordering changed"));

    values[0].modified = QDateTime::fromSecsSinceEpoch(30);
    values[1].modified = QDateTime::fromSecsSinceEpoch(10);
    values[2].modified = QDateTime::fromSecsSinceEpoch(20);
    ok &= expect(sortedNames(values, true, 3, Qt::AscendingOrder)
                     == QStringList({QStringLiteral("Alpha"), QStringLiteral("bravo"), QStringLiteral("charlie")}),
                 QStringLiteral("modified-time ordering changed"));

    FileEntry sameNameA = entry(QStringLiteral("same"), QStringLiteral("/a"));
    FileEntry sameNameB = entry(QStringLiteral("SAME"), QStringLiteral("/b"));
    ok &= expect(sortedNames({sameNameB, sameNameA}, true, 0, Qt::AscendingOrder)
                     == QStringList({QStringLiteral("same"), QStringLiteral("SAME")}),
                 QStringLiteral("path tie-breaker changed"));

    FileEntry loadMore = entry(QStringLiteral("Load more"), QStringLiteral("action://load-more"));
    loadMore.specialAction = FileEntrySpecialAction::LoadMore;
    ok &= expect(sortedNames({loadMore, file, folder}, true, 0, Qt::DescendingOrder).last()
                     == QStringLiteral("Load more"),
                 QStringLiteral("LoadMore must remain last in descending order"));

    return ok ? 0 : 1;
}
