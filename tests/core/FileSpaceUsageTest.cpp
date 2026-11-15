#include "FileSpaceUsage.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <algorithm>
#include <cstdio>
#include <cstring>

#ifdef Q_OS_LINUX
#include "BtrfsSpaceProtocol.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusUnixFileDescriptor>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/magic.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <unistd.h>
#endif

namespace {
int fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.flush()) return false;
#ifdef Q_OS_LINUX
    return ::fsync(file.handle()) == 0;
#else
    return true;
#endif
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    std::atomic_bool cancelled{false};
    if (app.arguments().value(1) == "--bench") {
        QList<double> times;
        qint64 bytes = -1;
        for (int count = 0; count < 31; ++count) {
            QElapsedTimer timer;
            timer.start();
            const auto result = FileSpaceUsage::measure(app.arguments().mid(2), cancelled);
            times.append(timer.nsecsElapsed() / 1000000.0);
            bytes = result.bytes;
            if (bytes < 0) return fail(qPrintable(result.explanation));
        }
        const double first = times.takeFirst();
        std::sort(times.begin(), times.end());
        std::puts(QJsonDocument(QJsonObject{{"bytes", bytes}, {"firstMs", first},
            {"medianMs", times.at(times.size() / 2)}, {"p95Ms", times.at(times.size() * 95 / 100)}}).toJson(QJsonDocument::Compact).constData());
        return 0;
    }
#ifdef Q_OS_LINUX
    if (app.arguments().value(1) == "--helper-security") {
        const QString protectedFile = app.arguments().value(2);
        const auto rejected = [](int fd, const QString &method = QStringLiteral("Inspect")) {
            auto message = QDBusMessage::createMethodCall(QString::fromLatin1(BtrfsSpaceProtocol::Service),
                QString::fromLatin1(BtrfsSpaceProtocol::Path), QString::fromLatin1(BtrfsSpaceProtocol::Interface), method);
            message << QVariant::fromValue(QDBusUnixFileDescriptor(fd));
            const auto reply = QDBusConnection::systemBus().call(message, QDBus::Block, 1500);
            if (method != QLatin1String("Inspect"))
                return reply.type() == QDBusMessage::ErrorMessage
                    && reply.errorName() == QLatin1String("org.freedesktop.DBus.Error.UnknownMethod");
            return reply.type() == QDBusMessage::ReplyMessage && reply.arguments().size() == 1
                && QJsonDocument::fromJson(reply.arguments().first().toByteArray()).object().contains("error");
        };
        int fd = ::open(QFile::encodeName(protectedFile).constData(), O_RDONLY | O_CLOEXEC);
        if (fd >= 0) { ::close(fd); return fail("Security fixture is readable by the client"); }
        fd = ::open(QFile::encodeName(protectedFile).constData(), O_PATH | O_CLOEXEC);
        if (fd < 0) return fail("Cannot open O_PATH security fixture");
        const bool pathRejected = rejected(fd);
        ::close(fd);
        QTemporaryDir fixtures(QDir::current().filePath("fm-space-security-XXXXXX"));
        if (!fixtures.isValid()) return fail("Cannot create security fixtures");
        QFile output(fixtures.filePath("write-only"));
        if (!output.open(QIODevice::WriteOnly)) return fail("Cannot open write-only fixture");
        const bool writeRejected = rejected(output.handle());
        fd = ::open(QFile::encodeName(fixtures.path()).constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (fd < 0) return fail("Cannot open directory fixture");
        const bool directoryRejected = rejected(fd);
        const bool operationRejected = rejected(fd, QStringLiteral("Delete"));
        ::close(fd);
        const bool passed = pathRejected && writeRejected && directoryRejected && operationRejected;
        std::puts(QJsonDocument(QJsonObject{{"success", passed}, {"clientUid", static_cast<int>(::getuid())},
            {"opathRejected", pathRejected}, {"writeOnlyRejected", writeRejected},
            {"directoryRejected", directoryRejected}, {"deleteRejected", operationRejected}}).toJson(QJsonDocument::Compact).constData());
        return passed ? 0 : 1;
    }
#endif
    if (app.arguments().value(1) == "--inspect" || app.arguments().value(1) == "--inspect-progress") {
        QElapsedTimer timer;
        timer.start();
        FileSpaceUsage::Progress progress;
        const bool logProgress = app.arguments().value(1) == "--inspect-progress";
        qint64 lastLog = -1000;
        int updates = 0;
        const auto result = FileSpaceUsage::measure(app.arguments().mid(2), cancelled,
            [&](const FileSpaceUsage::Progress &value) {
                progress = value;
                ++updates;
                if (logProgress && timer.elapsed() - lastLog >= 1000) {
                    std::fprintf(stderr, "%lld ms: %lld files, %lld bytes, %lld requests, %d skipped\n",
                        static_cast<long long>(timer.elapsed()), static_cast<long long>(value.files),
                        static_cast<long long>(value.bytes), static_cast<long long>(value.helperRequests), value.skippedPaths);
                    lastLog = timer.elapsed();
                }
            });
        const auto json = QJsonDocument(QJsonObject{{"bytes", result.bytes}, {"partial", result.partial()},
            {"skippedPaths", result.skippedPaths}, {"firstSkippedPath", result.firstSkippedPath},
            {"pendingPaths", result.pendingPaths},
            {"measuredFiles", progress.files}, {"helperRequests", progress.helperRequests},
            {"progressUpdates", updates},
            {"elapsedMs", timer.elapsed()}, {"explanation", result.explanation}}).toJson(QJsonDocument::Compact);
        std::puts(json.constData());
        return 0;
    }
    QTemporaryDir directory(QDir::current().filePath("fm-space-test-XXXXXX"));
    if (!directory.isValid()) return fail("Cannot create fixtures");
    const auto empty = directory.filePath("empty");
    const auto random = directory.filePath("random");
    const auto compressed = directory.filePath("compressed");
    const auto sparse = directory.filePath("sparse");
    QByteArray data(1024 * 1024, Qt::Uninitialized);
    QRandomGenerator generator(12345);
    for (qsizetype index = 0; index < data.size(); index += sizeof(quint32)) {
        const quint32 value = generator.generate();
        std::memcpy(data.data() + index, &value, sizeof(value));
    }
    if (!writeFile(empty, {}) || !writeFile(random, data)
        || !writeFile(compressed, QByteArray(4 * 1024 * 1024, 'x')))
        return fail("Cannot write fixtures");
    QFile sparseFile(sparse);
    if (!sparseFile.open(QIODevice::WriteOnly) || !sparseFile.resize(8 * 1024 * 1024)
        || sparseFile.write("fixture") != 7 || !sparseFile.flush()) return fail("Cannot create sparse fixture");
#ifdef Q_OS_LINUX
    if (::fsync(sparseFile.handle()) != 0) return fail("Cannot flush sparse fixture");
#endif
    sparseFile.close();

    const auto allocation = FileSpaceUsage::measure({random}, cancelled);
    if (app.arguments().contains("--require-helper") && allocation.bytes < 0)
        return fail(qPrintable(allocation.explanation));
    const auto duplicate = FileSpaceUsage::measure({random, random}, cancelled);
    if (allocation.bytes != duplicate.bytes) return fail("Duplicate paths were counted twice");
    if (allocation.bytes < 0 && allocation.explanation.isEmpty()) return fail("Unavailable measurements must explain why");
    if (FileSpaceUsage::measure({directory.filePath("missing")}, cancelled).bytes >= 0)
        return fail("Missing files must not become measured zero");
    if (FileSpaceUsage::measure({"sftp://example/file"}, cancelled).bytes >= 0)
        return fail("Provider paths must not become local allocations");
    cancelled.store(true);
    if (FileSpaceUsage::measure({random}, cancelled).bytes >= 0) return fail("Cancelled measurements must be unavailable");
    cancelled.store(false);
    if (FileSpaceUsage::measure({random}, cancelled, [&](const FileSpaceUsage::Progress &) {
            cancelled.store(true);
        }).bytes >= 0) return fail("Cancellation from progress must stop measurement");
    cancelled.store(false);

#ifdef Q_OS_LINUX
    const auto emptyUsage = FileSpaceUsage::measure({empty}, cancelled);
    if (emptyUsage.bytes != 0) return fail("Measured zero must remain distinct from unavailable");
    struct statfs filesystem{};
    if (::statfs(QFile::encodeName(directory.path()).constData(), &filesystem) != 0) return fail("Cannot identify fixture filesystem");
    const auto compressedUsage = FileSpaceUsage::measure({compressed}, cancelled);
    if (filesystem.f_type == BTRFS_SUPER_MAGIC) {
        const auto sparseUsage = FileSpaceUsage::measure({sparse}, cancelled);
        if (allocation.bytes >= 0) {
            if (allocation.bytes != data.size() || compressedUsage.bytes <= 0
                || compressedUsage.bytes >= 4 * 1024 * 1024 || sparseUsage.bytes != 4096)
                return fail("Btrfs physical sizes must account for compression and sparse data");
            FileSpaceUsage::Progress progress;
            int updates = 0;
            const auto batched = FileSpaceUsage::measure({random, compressed, sparse}, cancelled,
                [&](const FileSpaceUsage::Progress &value) { progress = value; ++updates; });
            if (batched.bytes != allocation.bytes + compressedUsage.bytes + sparseUsage.bytes
                || progress.files != 3 || progress.helperRequests != 1 || updates < 2)
                return fail("Batching must preserve allocation totals and report progress");
            const auto interrupted = FileSpaceUsage::measure({random, compressed, sparse}, cancelled,
                [&](const FileSpaceUsage::Progress &value) { if (value.files > 0) cancelled.store(true); });
            if (interrupted.bytes >= 0) return fail("Cancellation after measured files must not publish a completed total");
            cancelled.store(false);
            const auto reflink = directory.filePath("reflink");
            QFile source(random), destination(reflink);
            if (!source.open(QIODevice::ReadOnly) || !destination.open(QIODevice::WriteOnly)
                || ::ioctl(destination.handle(), FICLONE, source.handle()) != 0
                || ::fsync(destination.handle()) != 0) return fail("Cannot create reflink fixture");
            if (FileSpaceUsage::measure({random, reflink}, cancelled).bytes != allocation.bytes)
                return fail("Shared Btrfs extents were counted twice");
        } else if (compressedUsage.bytes >= 0 || sparseUsage.bytes >= 0) {
            return fail("Without the helper, Btrfs must not substitute stat blocks or logical sizes");
        }
    } else if (allocation.bytes >= 0) {
        struct stat info{};
        if (::stat(QFile::encodeName(random).constData(), &info) != 0
            || allocation.bytes != static_cast<qint64>(info.st_blocks) * 512)
            return fail("Reported allocation differs from file blocks");
        const auto sparseUsage = FileSpaceUsage::measure({sparse}, cancelled);
        if (sparseUsage.bytes >= 8 * 1024 * 1024) return fail("Sparse file was replaced with logical size");
    }
    if (allocation.bytes >= 0) {
        QTemporaryDir otherDirectory;
        const auto other = otherDirectory.filePath("other");
        if (!otherDirectory.isValid() || !writeFile(other, data)) return fail("Cannot create second filesystem fixture");
        struct stat localInfo{}, otherInfo{};
        if (::stat(QFile::encodeName(random).constData(), &localInfo) != 0
            || ::stat(QFile::encodeName(other).constData(), &otherInfo) != 0)
            return fail("Cannot identify fixture devices");
        if (localInfo.st_dev != otherInfo.st_dev) {
            const auto otherUsage = FileSpaceUsage::measure({other}, cancelled);
            const qint64 expected = allocation.bytes + std::max<qint64>(0, otherUsage.bytes);
            const int skipped = otherUsage.bytes < 0 ? 1 : 0;
            for (const auto &selection : {QStringList{random, other}, QStringList{other, random}}) {
                const auto mixed = FileSpaceUsage::measure(selection, cancelled);
                if (mixed.bytes != expected || mixed.skippedPaths != skipped)
                    return fail("Explicit selections across filesystems must not depend on path order");
            }
        }
    }
    const auto hardlink = directory.filePath("hardlink");
    if (::link(QFile::encodeName(random).constData(), QFile::encodeName(hardlink).constData()) != 0)
        return fail("Cannot create hardlink");
    if (FileSpaceUsage::measure({random, hardlink}, cancelled).bytes != allocation.bytes)
        return fail("Hardlinks were counted twice");
    const auto symlink = directory.filePath("symlink");
    if (!QFile::link(random, symlink) || FileSpaceUsage::measure({symlink}, cancelled).bytes >= 0)
        return fail("A symlink must not silently measure its target");
    if (::geteuid() != 0) {
        const auto blocked = directory.filePath("blocked");
        if (!QDir().mkdir(blocked) || ::chmod(QFile::encodeName(blocked).constData(), 0000) != 0)
            return fail("Cannot create inaccessible directory fixture");
        const auto partialZero = FileSpaceUsage::measure({empty, blocked}, cancelled);
        const auto unavailable = FileSpaceUsage::measure({blocked}, cancelled);
        const auto partialData = allocation.bytes >= 0 ? FileSpaceUsage::measure({random, blocked}, cancelled)
                                                      : FileSpaceUsage::Result{};
        if (::chmod(QFile::encodeName(blocked).constData(), 0700) != 0)
            return fail("Cannot restore fixture permissions");
        if (partialZero.bytes != 0 || !partialZero.partial() || partialZero.skippedPaths != 1
            || partialZero.firstSkippedPath != blocked)
            return fail("A readable zero plus a blocked directory must be explicitly partial, not unavailable or complete zero");
        if (unavailable.bytes >= 0 || unavailable.skippedPaths != 1 || unavailable.firstSkippedPath != blocked)
            return fail("A wholly blocked selection must not become measured zero");
        if (allocation.bytes >= 0 && (partialData.bytes != allocation.bytes || !partialData.partial()
            || partialData.skippedPaths != 1 || partialData.firstSkippedPath != blocked))
            return fail("A blocked directory discarded the readable file allocation");

        const auto blockedFile = directory.filePath("blocked-file");
        if (!writeFile(blockedFile, data) || ::chmod(QFile::encodeName(blockedFile).constData(), 0000) != 0)
            return fail("Cannot create inaccessible file fixture");
        const auto partialFile = FileSpaceUsage::measure({empty, blockedFile}, cancelled);
        if (::chmod(QFile::encodeName(blockedFile).constData(), 0600) != 0)
            return fail("Cannot restore file fixture permissions");
        if (partialFile.bytes != 0 || !partialFile.partial() || partialFile.skippedPaths != 1
            || partialFile.firstSkippedPath != blockedFile)
            return fail("An unreadable file discarded other measured data");
    }
#endif
    std::puts("File allocation: no logical fallback, explicit partial totals, cancellation and link handling passed");
    return 0;
}
