#include "GDrivePath.h"
#include "GDriveExportPolicy.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QString sharedLoadMore = GDrivePath::loadMorePath(QStringLiteral("gdrive://shared-with-me"));
    if (sharedLoadMore != QStringLiteral("gdrive://shared-with-me/__load_more__")) {
        return fail(QStringLiteral("Shared with me load-more path changed"));
    }
    if (GDrivePath::normalizedPath(QStringLiteral("GDRIVE://SHARED-WITH-ME/__LOAD_MORE__"))
        != sharedLoadMore) {
        return fail(QStringLiteral("Load-more normalization should be case-insensitive"));
    }
    if (GDrivePath::loadMoreParentPath(sharedLoadMore) != QStringLiteral("gdrive://shared-with-me")) {
        return fail(QStringLiteral("Shared with me load-more parent changed"));
    }

    const QString itemPath = GDrivePath::itemPathForId(QStringLiteral("folder/id"));
    const QString itemLoadMore = GDrivePath::loadMorePath(itemPath);
    if (GDrivePath::loadMoreParentPath(itemLoadMore) != itemPath) {
        return fail(QStringLiteral("Item load-more path should preserve the encoded folder id"));
    }
    if (GDrivePath::parentPath(itemLoadMore) != itemPath) {
        return fail(QStringLiteral("Load-more parentPath should return its container"));
    }
    if (!GDrivePath::loadMorePath(QStringLiteral("gdrive://")).isEmpty()) {
        return fail(QStringLiteral("Google Drive virtual root should not support load more"));
    }
    if (!GDrivePath::loadMorePath(itemLoadMore).isEmpty()) {
        return fail(QStringLiteral("Nested load-more paths should be rejected"));
    }

    if (GDriveExportPolicy::safeLocalExportFileName(QStringLiteral("album/photo.jpg"))
        != QStringLiteral("album_photo.jpg")) {
        return fail(QStringLiteral("Drive slash should be sanitized for a local filename"));
    }
    if (GDriveExportPolicy::safeLocalExportFileName(QStringLiteral("album\\photo.jpg"))
        != QStringLiteral("album_photo.jpg")) {
        return fail(QStringLiteral("Drive backslash should be sanitized for a local filename"));
    }

    return 0;
}
