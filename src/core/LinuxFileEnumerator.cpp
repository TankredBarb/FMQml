#include "LinuxFileEnumerator.h"

#ifdef Q_OS_LINUX

#include "DriveUtils.h"
#include "ArchiveSupport.h"
#include "LocalFileBadgeResolver.h"
#include "LocalMountPointIndex.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QTextStream>

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace LinuxFileEnumerator {
namespace {

struct MountInfo {
    QString target;
    QString source;
    QString fileSystem;
};

void traceEnum(const QString &message)
{
    if (!qEnvironmentVariableIsSet("FM_LINUX_ENUM_TRACE")) {
        return;
    }
    QTextStream(stderr) << "[LinuxEnum] " << message << '\n';
}

bool isImageSuffix(const QString &suffix)
{
    static const QStringList imageSuffixes = {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("png"),
        QStringLiteral("gif"),
        QStringLiteral("bmp"),
        QStringLiteral("webp"),
        QStringLiteral("ico")
    };
    return imageSuffixes.contains(suffix.toLower());
}

bool hasThumbnailSuffix(const QString &suffix)
{
    static const QStringList thumbnailSuffixes = {
        QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"),  QStringLiteral("bmp"),  QStringLiteral("webp"),
        QStringLiteral("ico"),
        QStringLiteral("svg"),  QStringLiteral("svgz"),
        QStringLiteral("mp3"),  QStringLiteral("flac"), QStringLiteral("ogg"),
        QStringLiteral("m4a"),  QStringLiteral("m4b"),  QStringLiteral("wav"),
        QStringLiteral("wma"),
        QStringLiteral("mp4"),  QStringLiteral("avi"),  QStringLiteral("mkv"),
        QStringLiteral("mov"),  QStringLiteral("wmv"),
        QStringLiteral("pdf"),
        QStringLiteral("ttf"),  QStringLiteral("otf"),  QStringLiteral("woff"),
        QStringLiteral("woff2")
    };
    return thumbnailSuffixes.contains(suffix.toLower());
}

QString suffixFromName(const QString &name)
{
    const qsizetype dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || dot == name.size() - 1) {
        return {};
    }
    return name.mid(dot + 1);
}

QDateTime dateTimeFromTimespec(const timespec &time)
{
    if (time.tv_sec <= 0) {
        return {};
    }
    return QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(time.tv_sec) * 1000
                                          + static_cast<qint64>(time.tv_nsec / 1000000));
}

QString parentPrefixForPath(const QString &path)
{
    QString parentPrefix = QDir::fromNativeSeparators(path);
    if (!parentPrefix.endsWith(QLatin1Char('/'))) {
        parentPrefix += QLatin1Char('/');
    }
    return parentPrefix;
}

Entry entryFromStat(const QString &name,
                    const QString &parentPath,
                    const struct stat &statBuffer,
                    const struct stat &linkStatBuffer,
                    bool isSymlink,
                    bool isBrokenSymlink,
                    bool isReadOnly,
                    bool isLocked,
                    bool isMountBoundary)
{
    const bool isDirectory = S_ISDIR(statBuffer.st_mode);
    const QString parentPathClean = QDir::fromNativeSeparators(parentPath);

    Entry entry;
    entry.name = name;
    entry.parentPath = parentPathClean;
    entry.path = parentPrefixForPath(parentPathClean) + name;
    entry.size = isDirectory ? 0 : static_cast<qint64>(statBuffer.st_size);
    entry.apparentSize = static_cast<qint64>(isSymlink ? linkStatBuffer.st_size : statBuffer.st_size);
    entry.device = static_cast<quint64>(isSymlink ? linkStatBuffer.st_dev : statBuffer.st_dev);
    entry.inode = static_cast<quint64>(isSymlink ? linkStatBuffer.st_ino : statBuffer.st_ino);
    entry.hardLinkCount = static_cast<quint64>(isSymlink ? linkStatBuffer.st_nlink : statBuffer.st_nlink);
    entry.modified = dateTimeFromTimespec(statBuffer.st_mtim);
    entry.created = entry.modified;
    entry.isDirectory = isDirectory;
    entry.isHidden = name.startsWith(QLatin1Char('.'));
    entry.isReadOnly = isReadOnly;
    entry.isLocked = isLocked;
    entry.isSymlink = isSymlink;
    entry.isBrokenSymlink = isBrokenSymlink;
    entry.isMountBoundary = isMountBoundary;
    return entry;
}

QString posixErrorMessage(const QString &path, int errorCode)
{
    return QStringLiteral("Cannot read %1: %2")
        .arg(QDir::toNativeSeparators(path), QString::fromLocal8Bit(std::strerror(errorCode)));
}

QString unescapeMountInfoField(QString value)
{
    value.replace(QStringLiteral("\\040"), QStringLiteral(" "));
    value.replace(QStringLiteral("\\011"), QStringLiteral("\t"));
    value.replace(QStringLiteral("\\012"), QStringLiteral("\n"));
    value.replace(QStringLiteral("\\134"), QStringLiteral("\\"));
    return value;
}

QList<MountInfo> readMountInfo()
{
    QList<MountInfo> result;
    QFile file(QStringLiteral("/proc/self/mountinfo"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        traceEnum(QStringLiteral("mountinfo open failed: %1").arg(file.errorString()));
        return result;
    }

    while (true) {
        const QByteArray rawLine = file.readLine();
        if (rawLine.isEmpty()) {
            break;
        }
        const QString line = QString::fromUtf8(rawLine).trimmed();
        const QStringList sections = line.split(QStringLiteral(" - "));
        if (sections.size() != 2) {
            continue;
        }

        const QStringList left = sections.at(0).split(QLatin1Char(' '));
        const QStringList right = sections.at(1).split(QLatin1Char(' '));
        if (left.size() < 5 || right.size() < 2) {
            continue;
        }

        MountInfo info;
        info.target = QDir::cleanPath(unescapeMountInfoField(left.at(4)));
        info.fileSystem = right.at(0).toLower();
        info.source = unescapeMountInfoField(right.at(1));
        result.append(info);
    }
    return result;
}

const QList<MountInfo> &mountInfo()
{
    static const QList<MountInfo> mounts = readMountInfo();
    if (qEnvironmentVariableIsSet("FM_LINUX_ENUM_TRACE")) {
        static bool logged = false;
        if (!logged) {
            logged = true;
            traceEnum(QStringLiteral("mountinfo entries=%1").arg(mounts.size()));
            for (int i = 0; i < mounts.size() && i < 12; ++i) {
                const MountInfo &mount = mounts.at(i);
                traceEnum(QStringLiteral("mount target=%1 fs=%2 source=%3")
                              .arg(mount.target, mount.fileSystem, mount.source));
            }
        }
    }
    return mounts;
}

std::optional<MountInfo> exactMountForPath(const QString &path)
{
    const QString cleanPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    for (const MountInfo &mount : mountInfo()) {
        if (mount.target == cleanPath) {
            return mount;
        }
    }
    return std::nullopt;
}

QString rootMountSource()
{
    static const QString source = [] {
        const std::optional<MountInfo> rootMount = exactMountForPath(QStringLiteral("/"));
        return rootMount ? rootMount->source : QString();
    }();
    return source;
}

QString mountSourceDevice(QString source)
{
    const qsizetype subvolumeIndex = source.indexOf(QLatin1Char('['));
    if (subvolumeIndex > 0) {
        source = source.left(subvolumeIndex);
    }
    return source.trimmed();
}

bool isPseudoFileSystem(const QString &fileSystem)
{
    static const QSet<QString> pseudoFileSystems = {
        QStringLiteral("autofs"),
        QStringLiteral("binfmt_misc"),
        QStringLiteral("bpf"),
        QStringLiteral("cgroup"),
        QStringLiteral("cgroup2"),
        QStringLiteral("configfs"),
        QStringLiteral("debugfs"),
        QStringLiteral("devpts"),
        QStringLiteral("devtmpfs"),
        QStringLiteral("efivarfs"),
        QStringLiteral("fusectl"),
        QStringLiteral("fuse.portal"),
        QStringLiteral("hugetlbfs"),
        QStringLiteral("mqueue"),
        QStringLiteral("proc"),
        QStringLiteral("pstore"),
        QStringLiteral("securityfs"),
        QStringLiteral("sysfs"),
        QStringLiteral("tmpfs"),
        QStringLiteral("tracefs"),
    };
    return pseudoFileSystems.contains(fileSystem.toLower());
}

bool isNetworkFileSystem(const QString &fileSystem)
{
    static const QSet<QString> networkFileSystems = {
        QStringLiteral("nfs"),
        QStringLiteral("nfs4"),
        QStringLiteral("cifs"),
        QStringLiteral("smb3"),
        QStringLiteral("sshfs"),
        QStringLiteral("fuse.sshfs"),
        QStringLiteral("davfs"),
        QStringLiteral("fuse.davfs"),
    };
    return networkFileSystems.contains(fileSystem.toLower());
}

bool isUserFacingExternalMountPath(const QString &path)
{
    const QString cleanPath = QDir::cleanPath(QDir::fromNativeSeparators(path));
    const QString userName = QString::fromLocal8Bit(qgetenv("USER")).trimmed();
    return cleanPath == QLatin1String("/mnt")
        || cleanPath.startsWith(QStringLiteral("/mnt/"))
        || cleanPath == QLatin1String("/media")
        || cleanPath.startsWith(QStringLiteral("/media/"))
        || (!userName.isEmpty()
            && (cleanPath == QStringLiteral("/run/media/%1").arg(userName)
                || cleanPath.startsWith(QStringLiteral("/run/media/%1/").arg(userName))))
        || cleanPath == QLatin1String("/run/user")
        || cleanPath.startsWith(QStringLiteral("/run/user/"));
}

bool shouldSkipMountBoundary(const QString &path, dev_t device, dev_t rootDevice)
{
    if (device == rootDevice) {
        return false;
    }

    const std::optional<MountInfo> mount = exactMountForPath(path);
    if (!mount) {
        traceEnum(QStringLiteral("keep boundary no-mount path=%1 device=%2 rootDevice=%3")
                      .arg(path)
                      .arg(static_cast<qulonglong>(device))
                      .arg(static_cast<qulonglong>(rootDevice)));
        return false;
    }
    if (isPseudoFileSystem(mount->fileSystem) || isNetworkFileSystem(mount->fileSystem)) {
        traceEnum(QStringLiteral("skip boundary fs path=%1 fs=%2 source=%3")
                      .arg(path, mount->fileSystem, mount->source));
        return true;
    }
    if (isUserFacingExternalMountPath(path)) {
        traceEnum(QStringLiteral("skip boundary user-facing path=%1 fs=%2 source=%3")
                      .arg(path, mount->fileSystem, mount->source));
        return true;
    }

    const QString rootSource = mountSourceDevice(rootMountSource());
    const QString boundarySource = mountSourceDevice(mount->source);
    const bool skip = rootSource.isEmpty() || boundarySource.isEmpty() || boundarySource != rootSource;
    traceEnum(QStringLiteral("boundary source path=%1 fs=%2 source=%3 rootSource=%4 boundaryDevice=%5 rootDeviceSource=%6 skip=%7")
                  .arg(path,
                       mount->fileSystem,
                       mount->source,
                       rootMountSource(),
                       boundarySource,
                       rootSource,
                       skip ? QStringLiteral("true") : QStringLiteral("false")));
    return skip;
}

} // namespace

std::optional<dev_t> deviceForPath(const QString &path)
{
    const QByteArray nativePath = QFile::encodeName(QDir(path).absolutePath());
    struct stat statBuffer {};
    if (stat(nativePath.constData(), &statBuffer) != 0) {
        return std::nullopt;
    }
    return statBuffer.st_dev;
}

qint64 apparentSizeForPath(const QString &path)
{
    const QByteArray nativePath = QFile::encodeName(QDir(path).absolutePath());
    struct stat statBuffer {};
    if (lstat(nativePath.constData(), &statBuffer) != 0) {
        return 0;
    }
    return static_cast<qint64>(statBuffer.st_size);
}

bool enumerateChildren(const QString &path, const Options &options, QList<Entry> *entries, QString *error)
{
    if (!entries) {
        return false;
    }

    const QString absolutePath = QDir(path).absolutePath();
    const QByteArray nativePath = QFile::encodeName(absolutePath);
    DIR *directory = opendir(nativePath.constData());
    if (!directory) {
        if (error) {
            *error = posixErrorMessage(absolutePath, errno);
        }
        return false;
    }

    errno = 0;
    while (dirent *dirEntry = readdir(directory)) {
        const QByteArray nameBytes(dirEntry->d_name);
        if (nameBytes == "." || nameBytes == "..") {
            errno = 0;
            continue;
        }

        const QString name = QString::fromLocal8Bit(nameBytes);
        if (!options.includeHidden && name.startsWith(QLatin1Char('.'))) {
            errno = 0;
            continue;
        }

        struct stat linkStatBuffer {};
        if (fstatat(dirfd(directory), nameBytes.constData(), &linkStatBuffer, AT_SYMLINK_NOFOLLOW) != 0) {
            errno = 0;
            continue;
        }

        struct stat statBuffer = linkStatBuffer;
        const bool isSymlink = S_ISLNK(linkStatBuffer.st_mode);
        bool isBrokenSymlink = false;
        if (isSymlink) {
            struct stat targetStat {};
            if (fstatat(dirfd(directory), nameBytes.constData(), &targetStat, 0) == 0) {
                statBuffer = targetStat;
            } else {
                const int targetError = errno;
                // A lookup can also fail because the target is temporarily
                // inaccessible. Only errors that prove the target path cannot
                // resolve should turn a link into a broken-link badge.
                isBrokenSymlink = targetError == ENOENT
                    || targetError == ENOTDIR
                    || targetError == ELOOP;
            }
        }

        const bool isMountBoundary = options.stayOnRootDevice
            && S_ISDIR(statBuffer.st_mode)
            && shouldSkipMountBoundary(parentPrefixForPath(absolutePath) + name,
                                       statBuffer.st_dev,
                                       options.rootDevice);
        entries->append(entryFromStat(name, absolutePath, statBuffer, linkStatBuffer,
                                      isSymlink, isBrokenSymlink,
                                      faccessat(dirfd(directory), nameBytes.constData(), W_OK, AT_EACCESS) != 0,
                                      faccessat(dirfd(directory), nameBytes.constData(),
                                                S_ISDIR(statBuffer.st_mode) ? X_OK : R_OK,
                                                AT_EACCESS) != 0,
                                      isMountBoundary));
        errno = 0;
    }

    const int readError = errno;
    closedir(directory);

    if (readError != 0) {
        if (error) {
            *error = posixErrorMessage(absolutePath, readError);
        }
        return false;
    }
    return true;
}

FileEntry toFileEntry(const Entry &source, const QLocale &locale)
{
    FileEntry entry;
    entry.name = source.name;
    entry.path = source.path;
    entry.suffix = suffixFromName(source.name);
    entry.size = source.size;
    entry.modified = source.modified;
    entry.created = source.created.isValid() ? source.created : source.modified;
    entry.isDirectory = source.isDirectory;
    entry.isHidden = source.isHidden;
    entry.isReadOnly = source.isReadOnly;
    entry.isLocked = source.isLocked;
    entry.isSystem = false;
    entry.isSymLink = source.isSymlink;
    entry.isBrokenSymLink = source.isBrokenSymlink;
    entry.isMountPoint = entry.isDirectory && LocalMountPointIndex::isMountPoint(entry.path);
    const bool isArchive = !entry.isDirectory
        && ArchiveSupport::isArchiveExtension(entry.suffix);
    entry.primaryBadgeKind = LocalFileBadgeResolver::primaryBadgeKind(
        entry.isBrokenSymLink, entry.isSymLink, entry.isMountPoint, entry.isLocked, isArchive);

    entry.sizeText = entry.isDirectory
        ? QString()
        : DriveUtils::formatSize(entry.size);
    entry.modifiedText = locale.toString(entry.modified, QLocale::ShortFormat);
    entry.createdText = locale.toString(entry.created, QLocale::ShortFormat);

    QString attrs;
    if (entry.isDirectory) attrs += QLatin1Char('D');
    if (entry.isHidden)    attrs += QLatin1Char('H');
    if (entry.isReadOnly)  attrs += QLatin1Char('R');
    if (source.isSymlink)  attrs += QLatin1Char('L');
    entry.attributesText = attrs;

    entry.isImage = !entry.isDirectory && isImageSuffix(entry.suffix);
    entry.hasThumbnail = !entry.isDirectory && hasThumbnailSuffix(entry.suffix);
    return entry;
}

} // namespace LinuxFileEnumerator

#endif
