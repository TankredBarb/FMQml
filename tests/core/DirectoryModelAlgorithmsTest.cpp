#include "DirectoryModelAlgorithms.h"
#include "DirectoryWatchPolicy.h"

#include <QCoreApplication>

#include <iostream>

namespace {
bool fail(const char *message)
{
    std::cerr << message << '\n';
    return false;
}

FileEntry entry(QString name, QString suffix, bool directory = false)
{
    FileEntry value;
    value.name = std::move(name);
    value.path = QStringLiteral("/root/") + value.name;
    value.suffix = std::move(suffix);
    value.isDirectory = directory;
    return value;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    ok &= DirectoryModelAlgorithms::selectionWithoutAncestors({"/w", "/w/a.png", "/w/b.png", "/w-other"})
        == QStringList({"/w/a.png", "/w/b.png", "/w-other"})
        || fail("Map selection retained an ancestor or removed an unrelated sibling");
    ok &= DirectoryModelAlgorithms::selectionWithoutAncestors({"/w", "/w-early", "/w/sub", "/w/sub/image.png"})
        == QStringList({"/w-early", "/w/sub/image.png"})
        || fail("Nested ancestors or prefix collisions bypassed selection safety");
    ok &= DirectoryModelAlgorithms::selectionWithoutAncestors({"sftp://user@host/a", "sftp://user@host/a/image.png"})
        == QStringList({"sftp://user@host/a/image.png"})
        || fail("Remote hierarchy bypassed selection safety");
    const QStringList remoteCaseSensitivePaths{
        "sftp://user@host/Photo.jpg", "sftp://user@host/photo.jpg",
        "sftp://user@host/Dir", "sftp://user@host/dir/file"};
    ok &= DirectoryModelAlgorithms::selectionWithoutAncestors(remoteCaseSensitivePaths)
        == remoteCaseSensitivePaths
        || fail("Remote path case was lost during selection normalization");

    QBitArray selection(12);
    selection.setBit(2);
    selection.setBit(10);
    DirectoryModelAlgorithms::extendOrTrimSelection(selection, 2, 6);
    for (int i = 0; i < selection.size(); ++i)
        ok &= selection.testBit(i) == ((i >= 2 && i <= 6) || i == 10)
            || fail("Shift expansion did not preserve a disjoint selection");
    DirectoryModelAlgorithms::extendOrTrimSelection(selection, 2, 4);
    for (int i = 0; i < selection.size(); ++i)
        ok &= selection.testBit(i) == ((i >= 2 && i <= 4) || i == 10)
            || fail("Shift trimming modified a disjoint selected block");
    selection.fill(false);
    selection.setBit(8);
    DirectoryModelAlgorithms::extendOrTrimSelection(selection, 8, 3);
    DirectoryModelAlgorithms::extendOrTrimSelection(selection, 3, 5);
    for (int i = 0; i < selection.size(); ++i)
        ok &= selection.testBit(i) == (i >= 3 && i <= 5)
            || fail("Shift reverse range did not trim from the first selected item");


    FileEntry folder = entry(QStringLiteral("z-folder"), {}, true);
    FileEntry image = entry(QStringLiteral("b-image.PNG"), QStringLiteral("PNG"));
    FileEntry document = entry(QStringLiteral("a-book.fb2.zip"), QStringLiteral("zip"));
    FileEntry hidden = entry(QStringLiteral(".hidden.jpg"), QStringLiteral("jpg"));
    hidden.isHidden = true;
    FileEntry loadMore = entry(QStringLiteral("Load more"), {});
    loadMore.specialAction = FileEntrySpecialAction::LoadMore;

    const QList<FileEntry> entries{image, loadMore, folder, hidden, document};
    QList<int> indices = DirectoryModelAlgorithms::filteredAndSortedIndices(
        entries, false, {}, DirectoryModel::FilterAll, false,
        DirectoryModel::SortByName, Qt::AscendingOrder);
    ok &= indices == QList<int>({2, 4, 0, 1})
        || fail("folder grouping, hidden filtering, or load-more ordering changed");

    indices = DirectoryModelAlgorithms::filteredAndSortedIndices(
        entries, true, {}, DirectoryModel::FilterImages, true,
        DirectoryModel::SortByName, Qt::AscendingOrder);
    ok &= indices == QList<int>({3, 0}) || fail("image category filter changed");

    indices = DirectoryModelAlgorithms::filteredAndSortedIndices(
        entries, true, QStringLiteral("BOOK"), DirectoryModel::FilterDocuments, true,
        DirectoryModel::SortByName, Qt::AscendingOrder);
    ok &= indices == QList<int>({4}) || fail("search or fb2.zip document matching changed");

    image.size = 10;
    document.size = 20;
    ok &= DirectoryModelAlgorithms::lessThan(
              document, image, true, DirectoryModel::SortBySize, Qt::DescendingOrder)
        || fail("descending size ordering changed");
    ok &= DirectoryModelAlgorithms::lessThan(
              image, loadMore, true, DirectoryModel::SortByName, Qt::DescendingOrder)
        || fail("load-more item no longer stays last in descending order");

    const QString normalized = DirectoryModelAlgorithms::pathKey(
        QStringLiteral("/root/child/../file"));
    ok &= normalized.endsWith(QStringLiteral("/root/file"))
        || fail("path key normalization changed");

    DirectoryChangeEvent added;
    added.type = DirectoryChangeEvent::Type::Added;
    added.path = QStringLiteral("/root/item");
    added.sourcePath = QStringLiteral("/root");
    DirectoryChangeEvent modified = added;
    modified.type = DirectoryChangeEvent::Type::Modified;
    QList<DirectoryChangeEvent> pending;
    DirectoryWatchPolicy::appendCoalesced(pending, added);
    DirectoryWatchPolicy::appendCoalesced(pending, modified);
    ok &= (pending.size() == 1 && pending.constFirst().type == DirectoryChangeEvent::Type::Modified)
        || fail("same-path watch events were not coalesced");

    DirectoryChangeEvent renamed;
    renamed.type = DirectoryChangeEvent::Type::Renamed;
    renamed.oldPath = QStringLiteral("/root/old");
    renamed.newPath = QStringLiteral("/root/new");
    DirectoryWatchPolicy::appendCoalesced(pending, renamed);
    ok &= pending.size() == 2 || fail("rename event was coalesced away");

    DirectoryChangeEvent overflow;
    overflow.type = DirectoryChangeEvent::Type::Overflow;
    DirectoryWatchPolicy::appendCoalesced(pending, overflow);
    ok &= (pending.size() == 1 && pending.constFirst().type == DirectoryChangeEvent::Type::Overflow)
        || fail("overflow did not replace pending watch events");

    modified.path = QStringLiteral("/root/upload.part");
    ok &= DirectoryWatchPolicy::isTransientPartWrite(modified)
        || fail("transient .part write was not recognized");
    ok &= DirectoryWatchPolicy::sourceMatches(added, QStringLiteral("/root"))
        || fail("matching watch source was rejected");
    ok &= !DirectoryWatchPolicy::sourceMatches(added, QStringLiteral("/other"))
        || fail("foreign watch source was accepted");

    return ok ? 0 : 1;
}
