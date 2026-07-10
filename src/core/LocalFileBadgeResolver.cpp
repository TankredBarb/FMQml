#include "LocalFileBadgeResolver.h"

#include "ArchiveSupport.h"
#include "LocalMountPointIndex.h"

#include <QFileInfo>

LocalFileBadgeState LocalFileBadgeResolver::resolve(const QFileInfo &fileInfo, bool isSymLink,
                                                     bool isMountPoint)
{
    LocalFileBadgeState state;
    state.isSymLink = isSymLink;
    state.isBrokenSymLink = isSymLink && !fileInfo.exists();
    state.isMountPoint = isMountPoint
        || (fileInfo.isDir() && LocalMountPointIndex::isMountPoint(fileInfo.absoluteFilePath()));
    state.isLocked = fileInfo.isDir() ? !fileInfo.isExecutable() : !fileInfo.isReadable();
    const bool isArchive = !fileInfo.isDir()
        && ArchiveSupport::isArchiveExtension(fileInfo.suffix());
    state.primaryBadgeKind = primaryBadgeKind(state.isBrokenSymLink, state.isSymLink,
                                              state.isMountPoint, state.isLocked, isArchive);
    return state;
}

QString LocalFileBadgeResolver::primaryBadgeKind(bool isBrokenSymLink, bool isSymLink,
                                                 bool isMountPoint, bool isLocked, bool isArchive)
{
    if (isBrokenSymLink) {
        return QStringLiteral("broken-link");
    }
    if (isSymLink) {
        return QStringLiteral("link");
    }
    if (isMountPoint) {
        return QStringLiteral("mount-point");
    }
    if (isLocked) {
        return QStringLiteral("locked");
    }
    if (isArchive) {
        return QStringLiteral("archive");
    }
    return {};
}
