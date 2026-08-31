#include "FavoritesStore.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

bool expect(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAILED: " << message << '\n';
    }
    return condition;
}

QString storagePath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("favorites.json"));
}

} // namespace

int main(int argc, char **argv)
{
    QTemporaryDir dataHome(QDir::tempPath() + QStringLiteral("/fmqml-favorites-store-test-XXXXXX"));
    if (!dataHome.isValid()) {
        QTextStream(stderr) << "FAILED: could not create temporary data directory\n";
        return 1;
    }
    qputenv("XDG_DATA_HOME", dataHome.path().toUtf8());
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("FMQmlTest"));
    QCoreApplication::setApplicationName(QStringLiteral("FavoritesStoreTest"));

    bool ok = true;

    const QString first = QStringLiteral("/tmp/fmqml-favorite-one");
    const QString equivalentFirst = QStringLiteral("/tmp/unused/../fmqml-favorite-one/");
    const QString second = QStringLiteral("/tmp/fmqml-favorite-two");

    FavoritesStore store;
    ok &= expect(store.pinnedEntries().isEmpty() && store.usageEntries().isEmpty(),
                 QStringLiteral("a new store should be empty"));
    ok &= expect(!store.pinPath(QStringLiteral("   ")), QStringLiteral("an empty path was pinned"));
    ok &= expect(store.pinPath(first) && store.pinPath(second), QStringLiteral("paths were not pinned"));
    ok &= expect(!store.pinPath(equivalentFirst), QStringLiteral("an equivalent path was pinned twice"));
    ok &= expect(store.movePinnedPath(second, -1), QStringLiteral("pinned path was not moved"));
    ok &= expect(store.pinnedEntries().at(0).targetPath == second
                     && store.pinnedEntries().at(0).order == 0
                     && store.pinnedEntries().at(1).order == 1,
                 QStringLiteral("pinned ordering was not normalized"));

    ok &= expect(store.setPinnedLabel(first, QStringLiteral("  Custom label  ")),
                 QStringLiteral("custom label was not set"));
    const QStringList rawTags{
        QStringLiteral(" #Work "), QStringLiteral("work"), QStringLiteral("two, words"),
        QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d"),
        QStringLiteral("e"), QStringLiteral("f"), QStringLiteral("ignored")};
    ok &= expect(store.setPinnedTags(first, rawTags), QStringLiteral("tags were not set"));
    const QStringList tags = store.tagsForPath(equivalentFirst);
    ok &= expect(tags.size() == 8 && tags.contains(QStringLiteral("Work"))
                     && tags.contains(QStringLiteral("two words")),
                 QStringLiteral("tags were not normalized, deduplicated, or limited"));

    ok &= expect(store.recordVisit(first) && store.recordVisit(first) && store.recordVisit(second),
                 QStringLiteral("visits were not recorded"));
    ok &= expect(store.usageEntries().size() == 2
                     && store.usageEntries().first().targetPath == first
                     && store.usageEntries().first().visitCount == 2,
                 QStringLiteral("usage entries were not counted or ranked"));

    FavoritesStore reloaded;
    ok &= expect(reloaded.pinnedEntries().size() == 2
                     && reloaded.pinnedEntries().at(0).targetPath == second
                     && reloaded.pinnedEntries().at(1).label == QStringLiteral("Custom label")
                     && reloaded.tagsForPath(first) == tags,
                 QStringLiteral("pinned metadata did not survive reload"));
    ok &= expect(reloaded.usageEntries().size() == 2
                     && reloaded.usageEntries().first().visitCount == 2,
                 QStringLiteral("usage did not survive reload"));
    ok &= expect(reloaded.forgetUsagePath(second) && reloaded.clearUsage()
                     && reloaded.usageEntries().isEmpty(),
                 QStringLiteral("usage cleanup failed"));
    ok &= expect(reloaded.unpinPath(first) && !reloaded.isPinned(first),
                 QStringLiteral("unpin failed"));

    QFile corrupt(storagePath());
    ok &= expect(corrupt.open(QIODevice::WriteOnly | QIODevice::Truncate),
                 QStringLiteral("could not create corrupt fixture"));
    if (corrupt.isOpen()) {
        corrupt.write("not-json");
        corrupt.close();
        FavoritesStore corruptStore;
        ok &= expect(corruptStore.pinnedEntries().isEmpty() && !corruptStore.load(),
                     QStringLiteral("corrupt storage was accepted"));
    }

    return ok ? 0 : 1;
}
