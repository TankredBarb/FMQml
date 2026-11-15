#include "BtrfsSpaceProtocol.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusUnixFileDescriptor>
#include <QDBusMetaType>
#include <QDBusConnectionInterface>
#include <QDBusReply>
#include <QFile>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>
#include <QtEndian>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <linux/btrfs.h>
#include <linux/btrfs_tree.h>
#include <linux/fiemap.h>
#include <linux/fs.h>
#include <limits>
#include <array>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <signal.h>
#include <poll.h>
#include <unistd.h>

namespace {
int stopRunningHelper()
{
    if (::geteuid() != 0) return 1;
    const auto bus = QDBusConnection::systemBus();
    if (!bus.isConnected() || !bus.interface()) return 1;
    const QDBusReply<uint> reply = bus.interface()->servicePid(QString::fromLatin1(BtrfsSpaceProtocol::Service));
    if (!reply.isValid()) return reply.error().name() == QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner") ? 0 : 1;
    const uint pid = reply.value();
    if (pid <= 1) return 1;
    const int process = static_cast<int>(::syscall(SYS_pidfd_open, pid, 0));
    if (process < 0) return errno == ESRCH ? 0 : 1;
    const QByteArray proc = QByteArray("/proc/") + QByteArray::number(pid);
    struct stat owner{};
    std::array<char, 4096> target{};
    const auto length = ::readlink((proc + "/exe").constData(), target.data(), target.size());
    QByteArray executable = length > 0 ? QByteArray(target.data(), length) : QByteArray();
    if (executable.endsWith(" (deleted)")) executable.chop(10);
    int result = 1;
    if (::stat(proc.constData(), &owner) == 0 && owner.st_uid == 0
        && executable == QFile::encodeName(QCoreApplication::applicationFilePath())) {
        if (::syscall(SYS_pidfd_send_signal, process, SIGTERM, static_cast<siginfo_t *>(nullptr), 0) == 0) {
            pollfd wait{process, POLLIN, 0};
            result = ::poll(&wait, 1, 2000) > 0 ? 0 : 1;
        } else if (errno == ESRCH) result = 0;
    }
    ::close(process);
    return result;
}

QJsonObject failure(const QString &message)
{
    return {{"version", BtrfsSpaceProtocol::Version}, {"error", message}};
}

bool sameFileState(const struct stat &a, const struct stat &b)
{
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size
        && a.st_blocks == b.st_blocks
        && a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec
        && a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}

bool allocationReady(int fd, const QElapsedTimer &timer, bool &pending)
{
    alignas(struct fiemap) std::array<unsigned char, sizeof(struct fiemap) + 64 * sizeof(struct fiemap_extent)> buffer{};
    auto *map = reinterpret_cast<struct fiemap *>(buffer.data());
    quint64 start = 0;
    while (true) {
        if (timer.elapsed() > 1000) return false;
        buffer.fill(0);
        map->fm_start = start;
        map->fm_length = std::numeric_limits<quint64>::max() - start;
        map->fm_extent_count = 64;
        if (::ioctl(fd, FS_IOC_FIEMAP, map) != 0) return false;
        for (quint32 index = 0; index < map->fm_mapped_extents; ++index) {
            if (map->fm_extents[index].fe_flags & FIEMAP_EXTENT_DELALLOC) { pending = true; return false; }
            if (map->fm_extents[index].fe_flags & FIEMAP_EXTENT_UNKNOWN) return false;
        }
        if (!map->fm_mapped_extents) return true;
        const auto &last = map->fm_extents[map->fm_mapped_extents - 1];
        if (last.fe_flags & FIEMAP_EXTENT_LAST) return true;
        const quint64 next = last.fe_logical + last.fe_length;
        if (next <= start) return false;
        start = next;
    }
}
}

class SpaceHelper final : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "io.fmqml.fm.Space2")
public:
    QTimer idle;
    SpaceHelper()
    {
        idle.setSingleShot(true);
        connect(&idle, &QTimer::timeout, qApp, &QCoreApplication::quit);
        idle.start(90000);
    }

public slots:
    QByteArray Inspect(const QDBusUnixFileDescriptor &descriptor)
    {
        idle.start(90000);
        QElapsedTimer timer;
        timer.start();
        return QJsonDocument(inspectFile(descriptor, timer)).toJson(QJsonDocument::Compact);
    }

    QByteArray InspectMany(const BtrfsSpaceProtocol::Descriptors &descriptors)
    {
        idle.start(90000);
        if (descriptors.isEmpty() || descriptors.size() > BtrfsSpaceProtocol::BatchSize)
            return QJsonDocument(failure(QStringLiteral("Invalid descriptor batch size."))).toJson(QJsonDocument::Compact);
        QElapsedTimer timer;
        timer.start();
        QJsonArray files;
        for (const auto &descriptor : descriptors) files.append(inspectFile(descriptor, timer));
        return QJsonDocument(QJsonObject{{"version", BtrfsSpaceProtocol::Version}, {"files", files}}).toJson(QJsonDocument::Compact);
    }

private:
    QJsonObject inspectFile(const QDBusUnixFileDescriptor &descriptor, const QElapsedTimer &timer)
    {
        if (timer.elapsed() > 1000)
            return failure(QStringLiteral("This file needs a longer detailed analysis."));
        // No pathname is accepted or opened here. The kernel granted the caller
        // a readable descriptor before it could send this request.
        const int fd = descriptor.fileDescriptor();
        const int flags = ::fcntl(fd, F_GETFL);
        struct stat before{};
        if (!descriptor.isValid() || flags < 0 || (flags & O_PATH)
            || (flags & O_ACCMODE) == O_WRONLY || ::fstat(fd, &before) != 0
            || !S_ISREG(before.st_mode))
            return failure(QStringLiteral("A readable regular-file descriptor is required."));

        btrfs_ioctl_fs_info_args filesystem{};
        if (::ioctl(fd, BTRFS_IOC_FS_INFO, &filesystem) != 0)
            return failure(QStringLiteral("Btrfs filesystem information is unavailable."));
        bool pending = false;
        if (!allocationReady(fd, timer, pending)) {
            auto result = failure(pending ? QStringLiteral("The filesystem is still allocating file data.")
                                          : QStringLiteral("File allocation details are unavailable. Refresh to measure again."));
            result.insert(QStringLiteral("pending"), pending);
            return result;
        }

        QJsonArray extents;
        quint64 inlineBytes = 0;
        quint64 start = 0;
        while (true) {
            if (timer.elapsed() > 1000 || extents.size() >= 65536)
                return failure(QStringLiteral("This file needs a longer detailed analysis."));
            btrfs_ioctl_search_args search{};
            auto &key = search.key;
            key.min_objectid = key.max_objectid = before.st_ino;
            key.min_offset = start;
            key.max_offset = key.max_transid = std::numeric_limits<quint64>::max();
            key.min_type = key.max_type = BTRFS_EXTENT_DATA_KEY;
            key.nr_items = 128;
            // tree_id stays zero: the descriptor's own subvolume only.
            if (::ioctl(fd, BTRFS_IOC_TREE_SEARCH, &search) != 0)
                return failure(QStringLiteral("Btrfs extent information is unavailable."));
            if (key.nr_items == 0) break;
            size_t position = 0;
            quint64 lastOffset = start;
            for (quint32 index = 0; index < key.nr_items; ++index) {
                btrfs_ioctl_search_header header{};
                if (position + sizeof(header) > sizeof(search.buf))
                    return failure(QStringLiteral("Incomplete Btrfs extent information."));
                std::memcpy(&header, search.buf + position, sizeof(header));
                position += sizeof(header);
                if (header.objectid != static_cast<quint64>(before.st_ino)
                    || header.type != BTRFS_EXTENT_DATA_KEY || header.len < 21
                    || position + header.len > sizeof(search.buf))
                    return failure(QStringLiteral("Unexpected Btrfs extent information."));
                const auto *data = reinterpret_cast<const uchar *>(search.buf + position);
                const auto kind = data[20];
                if (kind == BTRFS_FILE_EXTENT_INLINE) {
                    inlineBytes += header.len - 21;
                } else if ((kind == BTRFS_FILE_EXTENT_REG || kind == BTRFS_FILE_EXTENT_PREALLOC) && header.len >= 53) {
                    const quint64 address = qFromLittleEndian<quint64>(data + 21);
                    const quint64 length = qFromLittleEndian<quint64>(data + 29);
                    if (address && length)
                        extents.append(QJsonObject{{"address", QString::number(address)}, {"bytes", QString::number(length)}});
                } else {
                    return failure(QStringLiteral("Unsupported Btrfs extent information."));
                }
                lastOffset = header.offset;
                position += header.len;
            }
            if (lastOffset == std::numeric_limits<quint64>::max()) break;
            if (lastOffset < start)
                return failure(QStringLiteral("Btrfs extent search did not advance."));
            start = lastOffset + 1;
        }
        struct stat after{};
        if (::fstat(fd, &after) != 0 || !sameFileState(before, after))
            return failure(QStringLiteral("File data changed during measurement. Refresh to measure again."));
        const auto uuid = QByteArray(reinterpret_cast<const char *>(filesystem.fsid), BTRFS_FSID_SIZE).toHex();
        return QJsonObject{
            {"version", BtrfsSpaceProtocol::Version}, {"filesystem", QString::fromLatin1(uuid)},
            {"inode", QString::number(static_cast<quint64>(before.st_ino))},
            {"size", QString::number(before.st_size)}, {"blocks", QString::number(before.st_blocks)},
            {"mtimeSec", QString::number(before.st_mtim.tv_sec)}, {"mtimeNs", QString::number(before.st_mtim.tv_nsec)},
            {"ctimeSec", QString::number(before.st_ctim.tv_sec)}, {"ctimeNs", QString::number(before.st_ctim.tv_nsec)},
            {"inlineBytes", QString::number(inlineBytes)}, {"extents", extents}
        };
    }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--stop-running"))) return stopRunningHelper();
    qDBusRegisterMetaType<BtrfsSpaceProtocol::Descriptors>();
    // A private bus can exercise the identical descriptor protocol in tests.
    auto bus = QDBusConnection::systemBus();
    SpaceHelper helper;
    if (!bus.isConnected() || !bus.registerObject(QString::fromLatin1(BtrfsSpaceProtocol::Path), &helper, QDBusConnection::ExportAllSlots)
        || !bus.registerService(QString::fromLatin1(BtrfsSpaceProtocol::Service))) return 1;
    return app.exec();
}

#include "fm-space-helper.moc"
