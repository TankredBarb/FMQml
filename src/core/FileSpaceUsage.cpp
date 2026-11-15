#include "FileSpaceUsage.h"

#include <QDir>
#include <QFile>
#include <QSet>
#include <QHash>
#include <optional>

#ifdef Q_OS_LINUX
#include "BtrfsSpaceProtocol.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDBusMetaType>
#include <QElapsedTimer>
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#include <array>
#include <limits>
#include <cstring>
#endif

namespace FileSpaceUsage {

#ifdef Q_OS_LINUX
namespace {
QString btrfsExplanation()
{
    return QStringLiteral("File data after filesystem compression; shared extents counted once within this selection. Metadata overhead, storage replicas and link targets are excluded. This is not the space freed by deletion.");
}

Result btrfsAllocation(const QJsonObject &object, const struct stat &before,
                       QHash<QString, QSet<quint64>> &seenExtents)
{
    if (object.contains(QStringLiteral("error")))
        return {-1, object.value(QStringLiteral("error")).toString(), 0, {},
            object.value(QStringLiteral("pending")).toBool() ? 1 : 0};
    if (object.value(QStringLiteral("inode")).toString() != QString::number(static_cast<quint64>(before.st_ino))
        || object.value(QStringLiteral("size")).toString() != QString::number(before.st_size)
        || object.value(QStringLiteral("blocks")).toString() != QString::number(before.st_blocks)
        || object.value(QStringLiteral("mtimeSec")).toString() != QString::number(before.st_mtim.tv_sec)
        || object.value(QStringLiteral("mtimeNs")).toString() != QString::number(before.st_mtim.tv_nsec)
        || object.value(QStringLiteral("ctimeSec")).toString() != QString::number(before.st_ctim.tv_sec)
        || object.value(QStringLiteral("ctimeNs")).toString() != QString::number(before.st_ctim.tv_nsec))
        return {-1, QStringLiteral("File data changed during measurement. Refresh to measure again.")};
    const QString filesystem = object.value(QStringLiteral("filesystem")).toString();
    bool valid = false;
    qint64 bytes = object.value(QStringLiteral("inlineBytes")).toString().toLongLong(&valid);
    if (filesystem.size() != 32 || !valid || bytes < 0 || !object.value(QStringLiteral("extents")).isArray())
        return {-1, QStringLiteral("The file-space helper returned incomplete allocation details.")};
    auto &filesystemExtents = seenExtents[filesystem];
    QSet<quint64> newExtents;
    for (const auto &value : object.value(QStringLiteral("extents")).toArray()) {
        const auto extent = value.toObject();
        const quint64 address = extent.value(QStringLiteral("address")).toString().toULongLong(&valid);
        if (!valid || !address)
            return {-1, QStringLiteral("The file-space helper returned incomplete allocation details.")};
        const qint64 length = extent.value(QStringLiteral("bytes")).toString().toLongLong(&valid);
        if (!valid || length <= 0)
            return {-1, QStringLiteral("The file-space helper returned incomplete allocation details.")};
        // Separate numeric sets by filesystem UUID, including across subvolumes.
        if (!filesystemExtents.contains(address) && !newExtents.contains(address)) {
            newExtents.insert(address);
            bytes += length;
        }
    }
    filesystemExtents.unite(newExtents);
    return {bytes, btrfsExplanation()};
}

Result fileAllocation(const QString &path, const struct stat &before,
                      const std::atomic_bool &cancelled)
{
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return {-1, QString::fromLocal8Bit(std::strerror(errno))};
    const auto inspect = [&]() -> Result {
        // Keep the kernel structure naturally aligned; no flexible-array wrapper.
        alignas(struct fiemap) std::array<unsigned char, sizeof(struct fiemap) + 64 * sizeof(struct fiemap_extent)> buffer{};
        auto *map = reinterpret_cast<struct fiemap *>(buffer.data());
        quint64 start = 0;
        while (!cancelled.load()) {
            buffer.fill(0);
            map->fm_start = start;
            map->fm_length = std::numeric_limits<quint64>::max() - start;
            map->fm_extent_count = 64;
            if (::ioctl(fd, FS_IOC_FIEMAP, map) != 0)
                return {-1, QStringLiteral("The filesystem did not provide file allocation details.")};
            if (map->fm_mapped_extents == 0)
                break;
            for (quint32 index = 0; index < map->fm_mapped_extents; ++index) {
                const auto flags = map->fm_extents[index].fe_flags;
                if (flags & FIEMAP_EXTENT_ENCODED)
                    return {-1, QStringLiteral("Compressed or encoded data: its on-disk size is unavailable without a separate detailed analysis.")};
                if (flags & FIEMAP_EXTENT_SHARED)
                    return {-1, QStringLiteral("Shared data: its unique on-disk size is unavailable without a separate detailed analysis.")};
                if (flags & FIEMAP_EXTENT_DELALLOC)
                    return {-1, QStringLiteral("The filesystem is still allocating file data."), 0, {}, 1};
                if (flags & (FIEMAP_EXTENT_UNKNOWN | FIEMAP_EXTENT_DATA_INLINE | FIEMAP_EXTENT_DATA_TAIL))
                    return {-1, QStringLiteral("Some file allocation is unavailable or stored inside filesystem metadata.")};
            }
            const auto &last = map->fm_extents[map->fm_mapped_extents - 1];
            if (last.fe_flags & FIEMAP_EXTENT_LAST)
                break;
            const quint64 next = last.fe_logical + last.fe_length;
            if (next <= start)
                return {-1, QStringLiteral("The filesystem returned incomplete file allocation details.")};
            start = next;
        }
        if (cancelled.load())
            return {-1, QStringLiteral("Measurement stopped.")};
        struct stat after{};
        if (::fstat(fd, &after) != 0 || after.st_dev != before.st_dev || after.st_ino != before.st_ino
            || after.st_size != before.st_size || after.st_blocks != before.st_blocks
            || after.st_mtim.tv_sec != before.st_mtim.tv_sec || after.st_mtim.tv_nsec != before.st_mtim.tv_nsec
            || after.st_ctim.tv_sec != before.st_ctim.tv_sec || after.st_ctim.tv_nsec != before.st_ctim.tv_nsec)
            return {-1, QStringLiteral("File data changed during measurement. Refresh to measure again.")};
        return {static_cast<qint64>(after.st_blocks) * 512,
            QStringLiteral("Filesystem-reported file allocation; directory metadata and link targets are excluded. Other links can retain data after deletion.")};
    };
    const Result result = inspect();
    ::close(fd);
    return result;
}
}
#endif

Result measure(const QStringList &paths, const std::atomic_bool &cancelled, const ProgressCallback &progress)
{
#ifdef Q_OS_LINUX
    if (paths.isEmpty())
        return {-1, QStringLiteral("No local files selected.")};
    struct PendingPath {
        QString path;
        std::optional<quint64> device;
    };
    QList<PendingPath> pending;
    for (const auto &path : paths) pending.append({path, std::nullopt});
    QSet<QPair<quint64, quint64>> seen;
    QHash<QString, QSet<quint64>> seenExtents;
    QHash<quint64, bool> btrfsDevices;
    qint64 bytes = 0;
    QString explanation = QStringLiteral("No regular-file data. Directory metadata and link targets are excluded.");
    int skippedPaths = 0;
    int pendingPaths = 0;
    QString firstSkippedPath;
    QString firstSkippedReason;
    bool measured = false;
    qint64 measuredFiles = 0;
    qint64 helperRequests = 0;
    bool measuredBtrfs = false;
    bool measuredOther = false;
    struct PendingFile {
        QString path;
        struct stat before;
        QDBusUnixFileDescriptor descriptor;
    };
    QList<PendingFile> batch;
    QElapsedTimer progressTimer;
    progressTimer.start();
    const auto report = [&](const QString &path, bool force = false) {
        if (progress && (force || progressTimer.elapsed() >= 200)) {
            progress({bytes, measuredFiles, skippedPaths, path, helperRequests});
            progressTimer.restart();
        }
    };
    const auto skip = [&](const QString &path, const QString &reason) {
        ++skippedPaths;
        if (firstSkippedPath.isEmpty()) {
            firstSkippedPath = path;
            firstSkippedReason = reason;
        }
    };
    Result fatal;
    const auto flush = [&]() -> bool {
        if (batch.isEmpty()) return true;
        if (cancelled.load()) { fatal = {-1, QStringLiteral("Measurement stopped.")}; return false; }
        static const auto descriptorType = qDBusRegisterMetaType<BtrfsSpaceProtocol::Descriptors>();
        Q_UNUSED(descriptorType)
        BtrfsSpaceProtocol::Descriptors descriptors;
        for (const auto &file : batch) descriptors.append(file.descriptor);
        auto request = QDBusMessage::createMethodCall(QString::fromLatin1(BtrfsSpaceProtocol::Service),
            QString::fromLatin1(BtrfsSpaceProtocol::Path), QString::fromLatin1(BtrfsSpaceProtocol::Interface), QStringLiteral("InspectMany"));
        request << QVariant::fromValue(descriptors);
        ++helperRequests;
        const auto reply = QDBusConnection::systemBus().call(request, QDBus::Block, BtrfsSpaceProtocol::TimeoutMs);
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().size() != 1) {
            fatal = {-1, QStringLiteral("The installed file-space helper is unavailable or needs an update.")}; return false;
        }
        const auto object = QJsonDocument::fromJson(reply.arguments().first().toByteArray()).object();
        const auto files = object.value(QStringLiteral("files")).toArray();
        if (object.value(QStringLiteral("version")).toInt() != BtrfsSpaceProtocol::Version || files.size() != batch.size()) {
            fatal = {-1, QStringLiteral("The file-space helper returned an incompatible response.")}; return false;
        }
        for (qsizetype index = 0; index < batch.size(); ++index) {
            const auto allocation = btrfsAllocation(files.at(index).toObject(), batch.at(index).before, seenExtents);
            if (allocation.bytes < 0) {
                pendingPaths += allocation.pendingPaths;
                skip(batch.at(index).path, allocation.explanation);
            } else {
                bytes += allocation.bytes;
                ++measuredFiles;
                measured = true;
                measuredBtrfs = true;
                explanation = allocation.explanation;
            }
        }
        report(batch.last().path);
        batch.clear();
        return true;
    };
    report(paths.first(), true);
    while (!pending.isEmpty()) {
        if (cancelled.load())
            return {-1, QStringLiteral("Measurement stopped.")};
        const auto entryPath = pending.takeLast();
        const QString path = entryPath.path;
        report(path);
        if (path.contains(QStringLiteral("://")))
            return {-1, QStringLiteral("On-disk measurement is available for local files only.")};
        struct stat info{};
        if (::lstat(QFile::encodeName(path).constData(), &info) != 0) {
            skip(path, QString::fromLocal8Bit(std::strerror(errno)));
            continue;
        }
        if (S_ISLNK(info.st_mode)) {
            if (paths.size() == 1 && path == paths.first())
                return {-1, QStringLiteral("The selected item is a link; its target is not included in on-disk measurement.")};
            continue;
        }
        const quint64 device = static_cast<quint64>(info.st_dev);
        if (entryPath.device && *entryPath.device != device) {
            skip(path, QStringLiteral("A filesystem boundary was skipped during this directory traversal."));
            continue;
        }
        const QPair<quint64, quint64> identity{static_cast<quint64>(info.st_dev), static_cast<quint64>(info.st_ino)};
        if (seen.contains(identity))
            continue;
        if (S_ISDIR(info.st_mode)) {
            DIR *directory = ::opendir(QFile::encodeName(path).constData());
            if (!directory) {
                skip(path, QString::fromLocal8Bit(std::strerror(errno)));
                continue;
            }
            seen.insert(identity);
            measured = true;
            int error = 0;
            while (!cancelled.load()) {
                report(path);
                errno = 0;
                const dirent *entry = ::readdir(directory);
                if (!entry) {
                    error = errno;
                    break;
                }
                const QByteArray name(entry->d_name);
                if (name != "." && name != "..")
                    pending.append({QDir(path).filePath(QFile::decodeName(name)), device});
            }
            ::closedir(directory);
            if (error) skip(path, QString::fromLocal8Bit(std::strerror(error)));
        } else if (S_ISREG(info.st_mode)) {
            if (!btrfsDevices.contains(device)) {
                struct statfs filesystem{};
                if (::statfs(QFile::encodeName(path).constData(), &filesystem) != 0) {
                    skip(path, QStringLiteral("The filesystem could not be identified.")); continue;
                }
                btrfsDevices.insert(device, filesystem.f_type == BTRFS_SUPER_MAGIC);
            }
            if (btrfsDevices.value(device)) {
                const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
                if (fd < 0) { skip(path, QString::fromLocal8Bit(std::strerror(errno))); continue; }
                QDBusUnixFileDescriptor descriptor;
                descriptor.giveFileDescriptor(fd);
                seen.insert(identity);
                if (info.st_size == 0 && info.st_blocks == 0) {
                    struct stat opened{};
                    if (::fstat(fd, &opened) != 0 || opened.st_dev != info.st_dev || opened.st_ino != info.st_ino
                        || opened.st_size != 0 || opened.st_blocks != 0
                        || opened.st_mtim.tv_sec != info.st_mtim.tv_sec || opened.st_mtim.tv_nsec != info.st_mtim.tv_nsec
                        || opened.st_ctim.tv_sec != info.st_ctim.tv_sec || opened.st_ctim.tv_nsec != info.st_ctim.tv_nsec) {
                        skip(path, QStringLiteral("File data changed during measurement. Refresh to measure again."));
                        continue;
                    }
                    measured = true; ++measuredFiles; measuredBtrfs = true; explanation = btrfsExplanation();
                } else {
                    batch.append({path, info, descriptor});
                    if (batch.size() == BtrfsSpaceProtocol::BatchSize && !flush()) return fatal;
                }
                continue;
            }
            const Result allocation = fileAllocation(path, info, cancelled);
            if (allocation.bytes < 0) {
                pendingPaths += allocation.pendingPaths;
                skip(path, allocation.explanation);
                continue;
            }
            seen.insert(identity);
            measured = true;
            measuredOther = true;
            ++measuredFiles;
            bytes += allocation.bytes;
            explanation = allocation.explanation;
        }
    }
    if (!flush()) return fatal;
    if (cancelled.load())
        return {-1, QStringLiteral("Measurement stopped.")};
    if (measuredBtrfs && measuredOther)
        explanation = QStringLiteral("Filesystem-reported file allocation; Btrfs data accounts for compression and shared extents counted once within this selection. Directory metadata and link targets are excluded. This is not the space freed by deletion.");
    if (pendingPaths > 0)
        explanation += QStringLiteral(" Pending file allocations: %1. Refresh after allocation completes.").arg(pendingPaths);
    if (skippedPaths > 0) {
        const QString count = skippedPaths == 1 ? QStringLiteral("1 path skipped")
            : QStringLiteral("%1 paths skipped").arg(skippedPaths);
        const QString detail = QStringLiteral("%1. First skipped: %2 (%3).")
            .arg(count, firstSkippedPath, firstSkippedReason);
        explanation = measured
            ? QStringLiteral("Partial: ") + detail + QLatin1Char(' ') + explanation
            : QStringLiteral("No file data could be measured. ") + detail;
    }
    report(paths.first(), true);
    if (cancelled.load()) return {-1, QStringLiteral("Measurement stopped.")};
    return {measured ? bytes : -1, explanation, skippedPaths, firstSkippedPath, pendingPaths};
#else
    Q_UNUSED(paths)
    Q_UNUSED(cancelled)
    Q_UNUSED(progress)
    return {-1, QStringLiteral("On-disk measurement is unavailable on this platform.")};
#endif
}

} // namespace FileSpaceUsage
