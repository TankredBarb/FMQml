#include "ArchiveSupport.h"

namespace ArchiveSupport {

bool isArchiveExtension(const QString &) { return false; }
bool isArchiveFilePath(const QString &) { return false; }
bool isArchivePath(const QString &path) { return path.startsWith(QLatin1String("archive://")); }
bool archiveBackendAvailable() { return false; }
QString archiveLibraryPath() { return QString(); }
QString sevenZipExecutablePath() { return QString(); }

QString physicalArchivePath(const QString &path) {
    if (path.startsWith(QLatin1String("archive://"))) {
        return path.mid(10);
    }
    return path;
}
QStringList archiveSegments(const QString &) { return QStringList(); }
QString archiveBrowsePath(const QString &) { return QString(); }
QString archiveRootPath(const QString &) { return QString(); }
QString archiveRootPathForPath(const QString &) { return QString(); }
QString archiveChildPath(const QString &, const QString &) { return QString(); }
QString archiveParentPath(const QString &) { return QString(); }
QString archiveFileName(const QString &) { return QString(); }
QString normalizeArchivePath(const QString &) { return QString(); }
QString stripArchiveScheme(const QString &) { return QString(); }
QStringList splitArchiveTokens(const QString &) { return QStringList(); }

} // namespace ArchiveSupport
