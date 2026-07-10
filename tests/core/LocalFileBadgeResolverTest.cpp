#include "LocalFileBadgeResolver.h"
#include "LocalMountPointIndex.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include <cstdio>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
    }
    return condition;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir directory;
    if (!expect(directory.isValid(), "Could not create temporary directory")) {
        return 1;
    }

    const QString targetPath = directory.filePath(QStringLiteral("target.txt"));
    QFile target(targetPath);
    if (!expect(target.open(QIODevice::WriteOnly), "Could not create target file")) {
        return 1;
    }
    target.close();

    const QString validLinkPath = directory.filePath(QStringLiteral("valid-link"));
    if (!expect(QFile::link(targetPath, validLinkPath), "Could not create valid symlink")) {
        return 1;
    }
    const LocalFileBadgeState validLink = LocalFileBadgeResolver::resolve(QFileInfo(validLinkPath), true);
    if (!expect(validLink.isSymLink && !validLink.isBrokenSymLink
                && validLink.primaryBadgeKind == QStringLiteral("link"),
                "Valid symlink classification failed")) {
        return 1;
    }

    const QString brokenLinkPath = directory.filePath(QStringLiteral("broken-link"));
    if (!expect(QFile::link(directory.filePath(QStringLiteral("missing.txt")), brokenLinkPath),
                "Could not create broken symlink")) {
        return 1;
    }
    const LocalFileBadgeState brokenLink = LocalFileBadgeResolver::resolve(QFileInfo(brokenLinkPath), true);
    if (!expect(brokenLink.isBrokenSymLink
                && brokenLink.primaryBadgeKind == QStringLiteral("broken-link"),
                "Broken symlink classification failed")) {
        return 1;
    }

    const QString inaccessibleDirectoryPath = directory.filePath(QStringLiteral("inaccessible"));
    if (!expect(QDir().mkpath(inaccessibleDirectoryPath),
                "Could not create inaccessible-target directory")) {
        return 1;
    }
    const QString inaccessibleTargetPath = QDir(inaccessibleDirectoryPath)
        .filePath(QStringLiteral("target.txt"));
    QFile inaccessibleTarget(inaccessibleTargetPath);
    if (!expect(inaccessibleTarget.open(QIODevice::WriteOnly),
                "Could not create inaccessible symlink target")) {
        return 1;
    }
    inaccessibleTarget.close();

    const QString inaccessibleLinkPath = directory.filePath(QStringLiteral("inaccessible-link"));
    if (!expect(QFile::link(inaccessibleTargetPath, inaccessibleLinkPath),
                "Could not create inaccessible-target symlink")) {
        return 1;
    }
    if (!expect(QFile::setPermissions(inaccessibleDirectoryPath, {}),
                "Could not remove target-directory permissions")) {
        return 1;
    }

    const LocalFileBadgeState inaccessibleLink = LocalFileBadgeResolver::resolve(
        QFileInfo(inaccessibleLinkPath), true);
    const bool permissionsRestored = QFile::setPermissions(
        inaccessibleDirectoryPath,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    if (!expect(permissionsRestored, "Could not restore target-directory permissions")) {
        return 1;
    }
    if (!expect(!inaccessibleLink.isBrokenSymLink
                && inaccessibleLink.primaryBadgeKind == QStringLiteral("link"),
                "Inaccessible symlink target was misclassified as broken")) {
        return 1;
    }

    const QString archivePath = directory.filePath(QStringLiteral("sample.zip"));
    QFile archive(archivePath);
    if (!expect(archive.open(QIODevice::WriteOnly), "Could not create archive fixture")) {
        return 1;
    }
    archive.close();
    const LocalFileBadgeState archiveState = LocalFileBadgeResolver::resolve(QFileInfo(archivePath), false);
    if (!expect(archiveState.primaryBadgeKind == QStringLiteral("archive"),
                "Archive classification failed")) {
        return 1;
    }

    LocalMountPointIndex::setMountRoots({directory.path()});
    const LocalFileBadgeState mountState = LocalFileBadgeResolver::resolve(
        QFileInfo(directory.path()), false);
    if (!expect(mountState.isMountPoint
                && mountState.primaryBadgeKind == QStringLiteral("mount-point"),
                "Mount-point index classification failed")) {
        return 1;
    }
    LocalMountPointIndex::setMountRoots({});

    if (!expect(LocalFileBadgeResolver::primaryBadgeKind(false, false, false, true, true)
                    == QStringLiteral("locked"),
                "Locked priority failed")
        || !expect(LocalFileBadgeResolver::primaryBadgeKind(false, true, true, true, true)
                    == QStringLiteral("link"),
                "Link priority failed")
        || !expect(LocalFileBadgeResolver::primaryBadgeKind(false, false, true, true, true)
                    == QStringLiteral("mount-point"),
                "Mount-point priority failed")) {
        return 1;
    }

    return 0;
}
