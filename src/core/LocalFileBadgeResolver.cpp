#include "LocalFileBadgeResolver.h"

#include "ArchiveSupport.h"
#include "LocalMountPointIndex.h"

#include <QFile>
#include <QFileInfo>

#ifdef Q_OS_UNIX
#include <cerrno>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
bool symLinkTargetIsDefinitelyBroken(const QFileInfo &fileInfo)
{
#ifdef Q_OS_UNIX
    const QByteArray nativePath = QFile::encodeName(fileInfo.absoluteFilePath());
    struct stat targetStat {};
    if (::stat(nativePath.constData(), &targetStat) == 0) {
        return false;
    }

    // EACCES, I/O errors and other transient lookup failures do not prove that
    // the target is absent. Keep those entries as ordinary symbolic links.
    return errno == ENOENT || errno == ENOTDIR || errno == ELOOP;
#else
    // QFileInfo is the portable fallback for Windows reparse points. Native
    // ACL-aware classification can replace this without changing the badge API.
    return !fileInfo.exists();
#endif
}
}

LocalFileBadgeState LocalFileBadgeResolver::resolve(const QFileInfo &fileInfo, bool isSymLink,
                                                     bool isMountPoint)
{
    LocalFileBadgeState state;
    state.isSymLink = isSymLink;
    state.isBrokenSymLink = isSymLink && symLinkTargetIsDefinitelyBroken(fileInfo);
    state.isMountPoint = isMountPoint
        || (fileInfo.isDir() && LocalMountPointIndex::isMountPoint(fileInfo.absoluteFilePath()));
#ifdef Q_OS_UNIX
    const QByteArray nativePath = QFile::encodeName(fileInfo.absoluteFilePath());
    const int requiredAccess = fileInfo.isDir() ? (R_OK | X_OK) : R_OK;
    state.isLocked = ::access(nativePath.constData(), requiredAccess) != 0;
#else
    state.isLocked = fileInfo.isDir() ? !fileInfo.isExecutable() : !fileInfo.isReadable();
#endif
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
