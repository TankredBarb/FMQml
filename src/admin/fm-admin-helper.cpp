#include "LinuxAdminBroker.h"
#include "LinuxAdminPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QStorageInfo>
#include <QTextStream>

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <dirent.h>
#include <memory>
#include <functional>
#include <limits>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

volatile std::sig_atomic_t g_cancelRequested = 0;
QString g_cancelFilePath;

LinuxAdminBroker::Result helperOkResult()
{
    return {true, {}, {}, {}};
}

LinuxAdminBroker::Result helperFailResult(const QString &code, const QString &message, const QString &path = {})
{
    return {false, code, message, path};
}

void writeFailure(const QString &code, const QString &message, const QString &path = {})
{
    const LinuxAdminBroker::Result result{false, code, message, path};
    QTextStream out(stdout);
    out << QString::fromUtf8(QJsonDocument(LinuxAdminBroker::resultToJson(result)).toJson(QJsonDocument::Compact)) << '\n';
    out.flush();
}

void writeResult(const LinuxAdminBroker::Result &result)
{
    QTextStream out(stdout);
    out << QString::fromUtf8(QJsonDocument(LinuxAdminBroker::resultToJson(result)).toJson(QJsonDocument::Compact)) << '\n';
    out.flush();
}

QString helperParentPathFor(const QString &path)
{
    return QFileInfo(path).absoluteDir().absolutePath();
}

bool pathIsSymlink(const QString &path)
{
    const QByteArray encodedPath = QFile::encodeName(path);
    struct stat info {};
    if (::lstat(encodedPath.constData(), &info) != 0) {
        return false;
    }
    return S_ISLNK(info.st_mode);
}

QString errorMessageForErrno(const QString &prefix)
{
    return QStringLiteral("%1: %2").arg(prefix, QString::fromLocal8Bit(std::strerror(errno)));
}

bool validUnixId(qint64 value, quint64 maximum)
{
    return value == -1 || (value >= 0 && static_cast<quint64>(value) <= maximum);
}

bool closeFd(int fd)
{
    while (::close(fd) != 0) {
        if (errno == EINTR) {
            continue;
        }
        return false;
    }
    return true;
}

using ProgressCallback = std::function<void(qint64 processedBytes, qint64 totalBytes)>;

constexpr qint64 LinuxCrossFilesystemCopyBufferSize = 1 * 1024 * 1024;
constexpr qint64 LinuxCrossFilesystemCopyCacheWindow = 32 * 1024 * 1024;
constexpr int LinuxIoPrioClassShift = 13;
constexpr int LinuxIoPrioClassBE = 2;
constexpr int LinuxIoPrioWhoThread = 1;
constexpr int LinuxIoPrioBELowest = 7;

void handleCancelSignal(int)
{
    g_cancelRequested = 1;
}

void installCancelSignalHandler()
{
    struct sigaction action {};
    action.sa_handler = handleCancelSignal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGTERM, &action, nullptr);
}

bool cancelRequested()
{
    return g_cancelRequested != 0
        || (!g_cancelFilePath.isEmpty() && QFileInfo::exists(g_cancelFilePath));
}

void resetCancelRequested()
{
    g_cancelRequested = 0;
}

int linuxThreadIoPriority()
{
    return static_cast<int>(syscall(SYS_ioprio_get, LinuxIoPrioWhoThread, 0));
}

bool setLinuxThreadIoPriority(int value)
{
    return syscall(SYS_ioprio_set, LinuxIoPrioWhoThread, 0, value) == 0;
}

int linuxIoPrioValue(int ioClass, int priority)
{
    return (ioClass << LinuxIoPrioClassShift) | priority;
}

class LinuxIoPriorityGuard
{
public:
    LinuxIoPriorityGuard()
        : m_previousPriority(linuxThreadIoPriority())
        , m_hasPreviousPriority(m_previousPriority >= 0)
        , m_changed(setLinuxThreadIoPriority(linuxIoPrioValue(LinuxIoPrioClassBE, LinuxIoPrioBELowest)))
    {
    }

    ~LinuxIoPriorityGuard()
    {
        if (m_changed && m_hasPreviousPriority) {
            (void)setLinuxThreadIoPriority(m_previousPriority);
        }
    }

    LinuxIoPriorityGuard(const LinuxIoPriorityGuard &) = delete;
    LinuxIoPriorityGuard &operator=(const LinuxIoPriorityGuard &) = delete;

private:
    const int m_previousPriority = -1;
    const bool m_hasPreviousPriority = false;
    const bool m_changed = false;
};

class LinuxCopyCachePolicy
{
public:
    LinuxCopyCachePolicy(int sourceFd, int destinationFd)
        : m_sourceFd(sourceFd)
        , m_destinationFd(destinationFd)
    {
        if (m_sourceFd >= 0) {
            (void)posix_fadvise(m_sourceFd, 0, 0, POSIX_FADV_SEQUENTIAL);
        }
    }

    void bytesCopied(qint64 bytes)
    {
        if (bytes <= 0) {
            return;
        }

        m_position += bytes;
        const qint64 window = LinuxCrossFilesystemCopyCacheWindow;
        const qint64 readyUntil = (m_position / window) * window;
        while (m_advisedUntil + window <= readyUntil) {
            const qint64 windowStart = m_advisedUntil;
            if (windowStart > 0) {
                const qint64 previousStart = windowStart - window;
                awaitWriteback(previousStart, window);
                dropDestPages(previousStart, window);
            }

            startDestinationWriteback(windowStart, window);
            dropSourcePages(windowStart, window);
            m_advisedUntil += window;
        }
    }

    void finish()
    {
        const qint64 window = LinuxCrossFilesystemCopyCacheWindow;
        if (m_advisedUntil > 0) {
            const qint64 lastStart = m_advisedUntil - window;
            awaitWriteback(lastStart, window);
            dropDestPages(lastStart, window);
        }

        if (m_position <= m_advisedUntil) {
            return;
        }

        const qint64 tailStart = m_advisedUntil;
        const qint64 tailSize = m_position - m_advisedUntil;
        startDestinationWriteback(tailStart, tailSize);
        dropSourcePages(tailStart, tailSize);
        awaitWriteback(tailStart, tailSize);
        dropDestPages(tailStart, tailSize);
        m_advisedUntil = m_position;
    }

private:
    void awaitWriteback(qint64 offset, qint64 length) const
    {
        if (m_destinationFd >= 0) {
            (void)sync_file_range(m_destinationFd,
                                  static_cast<off64_t>(offset),
                                  static_cast<off64_t>(length),
                                  SYNC_FILE_RANGE_WAIT_BEFORE
                                      | SYNC_FILE_RANGE_WRITE
                                      | SYNC_FILE_RANGE_WAIT_AFTER);
        }
    }

    void startDestinationWriteback(qint64 offset, qint64 length) const
    {
        if (m_destinationFd >= 0) {
            (void)sync_file_range(m_destinationFd,
                                  static_cast<off64_t>(offset),
                                  static_cast<off64_t>(length),
                                  SYNC_FILE_RANGE_WRITE);
        }
    }

    void dropSourcePages(qint64 offset, qint64 length) const
    {
        if (m_sourceFd >= 0) {
            (void)posix_fadvise(m_sourceFd,
                                static_cast<off_t>(offset),
                                static_cast<off_t>(length),
                                POSIX_FADV_DONTNEED);
        }
    }

    void dropDestPages(qint64 offset, qint64 length) const
    {
        if (m_destinationFd >= 0) {
            (void)posix_fadvise(m_destinationFd,
                                static_cast<off_t>(offset),
                                static_cast<off_t>(length),
                                POSIX_FADV_DONTNEED);
        }
    }

    const int m_sourceFd = -1;
    const int m_destinationFd = -1;
    qint64 m_position = 0;
    qint64 m_advisedUntil = 0;
};

bool isLinuxCrossFilesystemCopy(const QString &sourcePath, const QString &destinationPath)
{
    const QFileInfo sourceInfo(sourcePath);
    const QFileInfo destinationInfo(destinationPath);
    QStorageInfo sourceStorage(sourceInfo.absoluteFilePath());
    QStorageInfo destinationStorage(destinationInfo.absolutePath());
    sourceStorage.refresh();
    destinationStorage.refresh();

    if (!sourceStorage.isValid() || !destinationStorage.isValid()) {
        return false;
    }

    return sourceStorage.device() != destinationStorage.device()
        || sourceStorage.rootPath() != destinationStorage.rootPath();
}

LinuxAdminBroker::Result copyFileContentsToNewPath(const QString &sourcePath,
                                                   const QString &destinationPath,
                                                   const ProgressCallback &progress = {})
{
    const QByteArray sourceBytes = QFile::encodeName(sourcePath);
    const int sourceFd = ::open(sourceBytes.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (sourceFd < 0) {
        return helperFailResult(QStringLiteral("copy-failed"), errorMessageForErrno(QStringLiteral("Failed to open source file")), sourcePath);
    }

    const QByteArray destinationBytes = QFile::encodeName(destinationPath);
    const int destinationFd = ::open(destinationBytes.constData(),
                                     O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                                     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (destinationFd < 0) {
        const QString message = errorMessageForErrno(QStringLiteral("Failed to create destination file"));
        closeFd(sourceFd);
        return helperFailResult(QStringLiteral("copy-failed"), message, destinationPath);
    }

    const bool conservativeCopy = isLinuxCrossFilesystemCopy(sourcePath, destinationPath);
    std::unique_ptr<LinuxIoPriorityGuard> linuxIoPriorityGuard;
    std::unique_ptr<LinuxCopyCachePolicy> linuxCopyCachePolicy;
    if (conservativeCopy) {
        linuxIoPriorityGuard = std::make_unique<LinuxIoPriorityGuard>();
        linuxCopyCachePolicy = std::make_unique<LinuxCopyCachePolicy>(sourceFd, destinationFd);
    }

    QByteArray buffer(conservativeCopy ? LinuxCrossFilesystemCopyBufferSize : 1024 * 128, Qt::Uninitialized);
    bool ok = true;
    bool canceled = false;
    QString errorMessage;
    qint64 copiedBytes = 0;
    const qint64 totalBytes = std::max<qint64>(1, QFileInfo(sourcePath).size());
    QElapsedTimer progressTimer;
    progressTimer.start();
    auto reportProgress = [&]() {
        if (!progress) {
            return;
        }
        if (copiedBytes < totalBytes && progressTimer.elapsed() < 120) {
            return;
        }
        progressTimer.restart();
        progress(std::clamp<qint64>(copiedBytes, 0, totalBytes), totalBytes);
    };
    while (true) {
        if (cancelRequested()) {
            canceled = true;
            ok = false;
            errorMessage = QStringLiteral("Operation canceled");
            break;
        }
        const ssize_t bytesRead = ::read(sourceFd, buffer.data(), static_cast<size_t>(buffer.size()));
        if (bytesRead == 0) {
            break;
        }
        if (bytesRead < 0) {
            if (errno == EINTR) {
                if (cancelRequested()) {
                    canceled = true;
                    ok = false;
                    errorMessage = QStringLiteral("Operation canceled");
                    break;
                }
                continue;
            }
            ok = false;
            errorMessage = errorMessageForErrno(QStringLiteral("Failed to read source file"));
            break;
        }

        ssize_t bytesWritten = 0;
        while (bytesWritten < bytesRead) {
            if (cancelRequested()) {
                canceled = true;
                ok = false;
                errorMessage = QStringLiteral("Operation canceled");
                break;
            }
            const ssize_t writeResult = ::write(destinationFd,
                                                buffer.constData() + bytesWritten,
                                                static_cast<size_t>(bytesRead - bytesWritten));
            if (writeResult < 0) {
                if (errno == EINTR) {
                    if (cancelRequested()) {
                        canceled = true;
                        ok = false;
                        errorMessage = QStringLiteral("Operation canceled");
                        break;
                    }
                    continue;
                }
                ok = false;
                errorMessage = errorMessageForErrno(QStringLiteral("Failed to write destination file"));
                break;
            }
            bytesWritten += writeResult;
            copiedBytes += writeResult;
            if (linuxCopyCachePolicy) {
                linuxCopyCachePolicy->bytesCopied(writeResult);
            }
            reportProgress();
        }
        if (!ok) {
            break;
        }
    }
    if (ok && linuxCopyCachePolicy) {
        linuxCopyCachePolicy->finish();
    }

    if (ok && ::fsync(destinationFd) != 0) {
        ok = false;
        errorMessage = errorMessageForErrno(QStringLiteral("Failed to flush destination file"));
    }

    const bool destinationClosed = closeFd(destinationFd);
    const bool sourceClosed = closeFd(sourceFd);
    if (ok && (!destinationClosed || !sourceClosed)) {
        ok = false;
        errorMessage = QStringLiteral("Failed to close copied file");
    }

    if (!ok) {
        ::unlink(destinationBytes.constData());
        if (canceled) {
            return helperFailResult(QStringLiteral("operation-canceled"), errorMessage, destinationPath);
        }
        return helperFailResult(QStringLiteral("copy-failed"), errorMessage, destinationPath);
    }
    return helperOkResult();
}

LinuxAdminBroker::Result validateDestinationSymlinkPolicy(const QString &destinationPath, bool rejectExistingDestination)
{
    const QString destination = QDir::cleanPath(destinationPath);
    const QStringList parts = destination.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QString current = destination.startsWith(QLatin1Char('/')) ? QStringLiteral("/") : QString();

    for (const QString &part : parts) {
        current = current == QLatin1String("/") ? current + part : QDir(current).filePath(part);
        const QFileInfo info(current);
        if (!info.exists() && !pathIsSymlink(current)) {
            continue;
        }
        if (pathIsSymlink(current)) {
            if (current == destination && !rejectExistingDestination) {
                return helperFailResult(QStringLiteral("symlink-policy-denied"),
                                        QStringLiteral("Destination symlinks are not supported for administrator operations"),
                                        current);
            }
            return helperFailResult(QStringLiteral("symlink-policy-denied"),
                                    QStringLiteral("Destination path uses a symlinked component"),
                                    current);
        }
    }
    return helperOkResult();
}

LinuxAdminPolicy::Operation helperPolicyOperationFor(LinuxAdminBroker::Operation operation)
{
    switch (operation) {
    case LinuxAdminBroker::Operation::CopyFile:
        return LinuxAdminPolicy::Operation::CopyFile;
    case LinuxAdminBroker::Operation::MakeDirectory:
        return LinuxAdminPolicy::Operation::MakeDirectory;
    case LinuxAdminBroker::Operation::AtomicReplace:
        return LinuxAdminPolicy::Operation::AtomicReplace;
    case LinuxAdminBroker::Operation::CreateFile:
        return LinuxAdminPolicy::Operation::CreateFile;
    case LinuxAdminBroker::Operation::RenamePath:
        return LinuxAdminPolicy::Operation::RenamePath;
    case LinuxAdminBroker::Operation::DeletePath:
        return LinuxAdminPolicy::Operation::DeletePath;
    case LinuxAdminBroker::Operation::ChangeMode:
        return LinuxAdminPolicy::Operation::ChangeMode;
    case LinuxAdminBroker::Operation::ChangeOwnership:
        return LinuxAdminPolicy::Operation::ChangeOwnership;
    case LinuxAdminBroker::Operation::ListDirectory:
        return LinuxAdminPolicy::Operation::ListDirectory;
    case LinuxAdminBroker::Operation::ReadFile:
        return LinuxAdminPolicy::Operation::ReadFile;
    }
    return LinuxAdminPolicy::Operation::CopyFile;
}

LinuxAdminBroker::Result validateRequest(const LinuxAdminBroker::Request &request, const QString &expectedSessionNonce = {})
{
    if (request.operationId.trimmed().isEmpty()) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Operation id is empty"));
    }
    if (!expectedSessionNonce.isEmpty() && request.sessionNonce.trimmed().isEmpty()) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Session nonce is empty"));
    }
    if (!expectedSessionNonce.isEmpty() && request.sessionNonce != expectedSessionNonce) {
        return helperFailResult(QStringLiteral("session-inactive"), QStringLiteral("Administrator session is not active"));
    }
    if (request.operation == LinuxAdminBroker::Operation::ChangeMode && request.mode > 07777) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Mode is outside the supported range"), request.sourcePath);
    }
    if (request.recursive && (request.operation != LinuxAdminBroker::Operation::ChangeMode || request.modeMask > 07777)) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Invalid recursive permission change"), request.sourcePath);
    }
    if (request.operation == LinuxAdminBroker::Operation::ReadFile
            && (request.offset < 0 || request.length <= 0 || request.length > 1024 * 1024)) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Invalid admin file read range"), request.sourcePath);
    }
    if (request.operation == LinuxAdminBroker::Operation::ChangeOwnership
            && request.ownerId < 0 && request.groupId < 0) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Owner or group is required"), request.sourcePath);
    }
    if (request.operation == LinuxAdminBroker::Operation::ChangeOwnership
            && (!validUnixId(request.ownerId, std::numeric_limits<uid_t>::max())
                || !validUnixId(request.groupId, std::numeric_limits<gid_t>::max()))) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Owner or group id is outside the supported range"), request.sourcePath);
    }

    const LinuxAdminPolicy::Decision policy = LinuxAdminPolicy::validate(
        helperPolicyOperationFor(request.operation),
        request.sourcePath,
        request.destinationPath);
    if (!policy.allowed) {
        return helperFailResult(policy.errorCode, policy.errorMessage, policy.failedPath);
    }
    const QString symlinkCheckedPath = (request.operation == LinuxAdminBroker::Operation::ChangeMode
                                        || request.operation == LinuxAdminBroker::Operation::ChangeOwnership
                                        || request.operation == LinuxAdminBroker::Operation::ListDirectory
                                        || request.operation == LinuxAdminBroker::Operation::ReadFile)
        ? request.sourcePath
        : request.destinationPath;
    const LinuxAdminBroker::Result symlinkPolicy = validateDestinationSymlinkPolicy(
        symlinkCheckedPath,
        request.operation != LinuxAdminBroker::Operation::MakeDirectory);
    if (!symlinkPolicy.success) {
        return symlinkPolicy;
    }
    return helperOkResult();
}

LinuxAdminBroker::Result atomicReplaceFile(const QString &sourcePath,
                                           const QString &destinationPath,
                                           bool overwrite,
                                           const ProgressCallback &progress = {});

LinuxAdminBroker::Result createFile(const QString &destinationPath)
{
    const QString destination = QDir::cleanPath(destinationPath);
    const QString parentPath = helperParentPathFor(destination);
    if (!QFileInfo(parentPath).isDir()) {
        return helperFailResult(QStringLiteral("parent-missing"), QStringLiteral("Destination parent directory is missing"), parentPath);
    }

    const QByteArray destinationBytes = QFile::encodeName(destination);
    const int fd = ::open(destinationBytes.constData(),
                          O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
                          S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd < 0) {
        return helperFailResult(QStringLiteral("create-failed"),
                                errorMessageForErrno(QStringLiteral("Failed to create file")),
                                destination);
    }
    if (!closeFd(fd)) {
        ::unlink(destinationBytes.constData());
        return helperFailResult(QStringLiteral("create-failed"), QStringLiteral("Failed to close created file"), destination);
    }
    return helperOkResult();
}

LinuxAdminBroker::Result renamePath(const QString &sourcePath, const QString &destinationPath)
{
    const QString source = QDir::cleanPath(sourcePath);
    const QString destination = QDir::cleanPath(destinationPath);
    if (QFileInfo::exists(destination)) {
        return helperFailResult(QStringLiteral("destination-exists"), QStringLiteral("Destination already exists"), destination);
    }

    const QByteArray sourceBytes = QFile::encodeName(source);
    const QByteArray destinationBytes = QFile::encodeName(destination);
    if (::rename(sourceBytes.constData(), destinationBytes.constData()) != 0) {
        return helperFailResult(QStringLiteral("rename-failed"),
                                errorMessageForErrno(QStringLiteral("Failed to rename item")),
                                source);
    }
    return helperOkResult();
}

LinuxAdminBroker::Result deletePathRecursive(const QString &source,
                                             const ProgressCallback &progress,
                                             qint64 &deletedEntries,
                                             QElapsedTimer &progressTimer)
{
    if (cancelRequested()) {
        return helperFailResult(QStringLiteral("operation-canceled"), QStringLiteral("Operation canceled"), source);
    }

    const QByteArray sourceBytes = QFile::encodeName(source);
    struct stat info {};
    if (::lstat(sourceBytes.constData(), &info) != 0) {
        if (errno == ENOENT) {
            return helperOkResult();
        }
        return helperFailResult(QStringLiteral("delete-failed"),
                                errorMessageForErrno(QStringLiteral("Failed to inspect item")),
                                source);
    }

    auto reportDeleteProgress = [&]() {
        if (!progress) {
            return;
        }
        if (progressTimer.elapsed() < 120) {
            return;
        }
        progressTimer.restart();
        progress(deletedEntries, 0);
    };

    if (S_ISDIR(info.st_mode)) {
        const int directoryFd = ::open(sourceBytes.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        if (directoryFd < 0) {
            return helperFailResult(QStringLiteral("delete-failed"),
                                    errorMessageForErrno(QStringLiteral("Failed to open directory")),
                                    source);
        }
        std::unique_ptr<DIR, int (*)(DIR *)> directory(::fdopendir(directoryFd), ::closedir);
        if (!directory) {
            closeFd(directoryFd);
            return helperFailResult(QStringLiteral("delete-failed"),
                                    errorMessageForErrno(QStringLiteral("Failed to read directory")),
                                    source);
        }

        while (true) {
            if (cancelRequested()) {
                return helperFailResult(QStringLiteral("operation-canceled"), QStringLiteral("Operation canceled"), source);
            }
            errno = 0;
            dirent *entry = ::readdir(directory.get());
            if (!entry) {
                if (errno != 0) {
                    return helperFailResult(QStringLiteral("delete-failed"),
                                            errorMessageForErrno(QStringLiteral("Failed to enumerate directory")),
                                            source);
                }
                break;
            }
            const QByteArray name(entry->d_name);
            if (name == "." || name == "..") {
                continue;
            }
            const QString childPath = QDir(source).filePath(QString::fromLocal8Bit(name));
            const LinuxAdminBroker::Result childResult = deletePathRecursive(childPath, progress, deletedEntries, progressTimer);
            if (!childResult.success) {
                return childResult;
            }
        }

        directory.reset();
        if (cancelRequested()) {
            return helperFailResult(QStringLiteral("operation-canceled"), QStringLiteral("Operation canceled"), source);
        }
        if (::rmdir(sourceBytes.constData()) != 0) {
            return helperFailResult(QStringLiteral("delete-failed"),
                                    errorMessageForErrno(QStringLiteral("Failed to remove directory")),
                                    source);
        }
        ++deletedEntries;
        reportDeleteProgress();
        return helperOkResult();
    }

    if (S_ISLNK(info.st_mode) || S_ISREG(info.st_mode) || S_ISFIFO(info.st_mode) || S_ISSOCK(info.st_mode)) {
        if (::unlink(sourceBytes.constData()) != 0) {
            return helperFailResult(QStringLiteral("delete-failed"),
                                    errorMessageForErrno(QStringLiteral("Failed to remove file")),
                                    source);
        }
        ++deletedEntries;
        reportDeleteProgress();
        return helperOkResult();
    }

    return helperFailResult(QStringLiteral("invalid-path"),
                            QStringLiteral("Source path must be a file or directory"),
                            source);
}

LinuxAdminBroker::Result deletePath(const QString &sourcePath, const ProgressCallback &progress = {})
{
    const QString source = QDir::cleanPath(sourcePath);
    qint64 deletedEntries = 0;
    QElapsedTimer progressTimer;
    progressTimer.start();
    const LinuxAdminBroker::Result result = deletePathRecursive(source, progress, deletedEntries, progressTimer);
    if (result.success && progress) {
        progress(deletedEntries, deletedEntries);
    }
    return result;
}

LinuxAdminBroker::Result copyFile(const QString &sourcePath,
                                  const QString &destinationPath,
                                  bool overwrite,
                                  const ProgressCallback &progress = {})
{
    const QString destination = QDir::cleanPath(destinationPath);
    const QString parentPath = helperParentPathFor(destination);
    if (!QFileInfo(parentPath).isDir()) {
        return helperFailResult(QStringLiteral("parent-missing"), QStringLiteral("Destination parent directory is missing"), parentPath);
    }
    if (QFileInfo::exists(destination)) {
        if (!overwrite) {
            return helperFailResult(QStringLiteral("destination-exists"), QStringLiteral("Destination already exists"), destination);
        }
        return atomicReplaceFile(sourcePath, destination, true, progress);
    }
    return copyFileContentsToNewPath(sourcePath, destination, progress);
}

LinuxAdminBroker::Result atomicReplaceFile(const QString &sourcePath,
                                           const QString &destinationPath,
                                           bool overwrite,
                                           const ProgressCallback &progress)
{
    const QString destination = QDir::cleanPath(destinationPath);
    const QString parentPath = helperParentPathFor(destination);
    if (!QFileInfo(parentPath).isDir()) {
        return helperFailResult(QStringLiteral("parent-missing"), QStringLiteral("Destination parent directory is missing"), parentPath);
    }
    if (QFileInfo::exists(destination) && !overwrite) {
        return helperFailResult(QStringLiteral("destination-exists"), QStringLiteral("Destination already exists"), destination);
    }

    const QString partPath = destination + QStringLiteral(".fm-admin-replace-part");
    QFile::remove(partPath);
    const LinuxAdminBroker::Result copyResult = copyFileContentsToNewPath(sourcePath, partPath, progress);
    if (!copyResult.success) {
        return copyResult;
    }

    const QByteArray partPathBytes = QFile::encodeName(partPath);
    const QByteArray destinationBytes = QFile::encodeName(destination);
    if (::rename(partPathBytes.constData(), destinationBytes.constData()) != 0) {
        const QString error = QString::fromLocal8Bit(std::strerror(errno));
        QFile::remove(partPath);
        return helperFailResult(QStringLiteral("rename-failed"),
                                QStringLiteral("Failed to install replacement file: %1").arg(error),
                                destination);
    }
    return helperOkResult();
}

LinuxAdminBroker::Result executeRequest(const LinuxAdminBroker::Request &request,
                                        const QString &expectedSessionNonce = {},
                                        const ProgressCallback &progress = {})
{
    const LinuxAdminBroker::Result validation = validateRequest(request, expectedSessionNonce);
    if (!validation.success) {
        return validation;
    }

    switch (request.operation) {
    case LinuxAdminBroker::Operation::MakeDirectory: {
        const QString destination = QDir::cleanPath(request.destinationPath);
        if (!QDir().mkpath(destination)) {
            return helperFailResult(QStringLiteral("mkdir-failed"), QStringLiteral("Failed to create destination directory"), destination);
        }
        return helperOkResult();
    }
    case LinuxAdminBroker::Operation::CopyFile:
        return copyFile(request.sourcePath, request.destinationPath, request.overwrite, progress);
    case LinuxAdminBroker::Operation::AtomicReplace:
        return atomicReplaceFile(request.sourcePath, request.destinationPath, request.overwrite, progress);
    case LinuxAdminBroker::Operation::CreateFile:
        return createFile(request.destinationPath);
    case LinuxAdminBroker::Operation::RenamePath:
        return renamePath(request.sourcePath, request.destinationPath);
    case LinuxAdminBroker::Operation::DeletePath:
        return deletePath(request.sourcePath, progress);
    case LinuxAdminBroker::Operation::ChangeMode: {
        const QByteArray target = QFile::encodeName(QDir::cleanPath(request.sourcePath));
        if (request.recursive) {
            if (!QFileInfo(request.sourcePath).isDir()) {
                return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Recursive permission change requires a directory"), request.sourcePath);
            }
            const auto applyMode = [&request](const QString &path) -> LinuxAdminBroker::Result {
                struct stat info {};
                const QByteArray encodedPath = QFile::encodeName(path);
                if (::lstat(encodedPath.constData(), &info) != 0) {
                    return helperFailResult(QStringLiteral("chmod-failed"), errorMessageForErrno(QStringLiteral("Failed to inspect permissions")), path);
                }
                if (S_ISLNK(info.st_mode)) {
                    return helperOkResult();
                }
                const mode_t mode = static_cast<mode_t>((info.st_mode & ~request.modeMask)
                    | (request.mode & request.modeMask));
                if (::chmod(encodedPath.constData(), mode) != 0) {
                    return helperFailResult(QStringLiteral("chmod-failed"), errorMessageForErrno(QStringLiteral("Failed to change permissions")), path);
                }
                return helperOkResult();
            };
            LinuxAdminBroker::Result result = applyMode(request.sourcePath);
            if (!result.success) {
                return result;
            }
            QDirIterator iterator(request.sourcePath, QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
            while (iterator.hasNext()) {
                result = applyMode(iterator.next());
                if (!result.success) {
                    return result;
                }
            }
            return helperOkResult();
        }
        if (::chmod(target.constData(), static_cast<mode_t>(request.mode)) != 0) {
            return helperFailResult(QStringLiteral("chmod-failed"), errorMessageForErrno(QStringLiteral("Failed to change permissions")), request.sourcePath);
        }
        return helperOkResult();
    }
    case LinuxAdminBroker::Operation::ChangeOwnership: {
        const QByteArray target = QFile::encodeName(QDir::cleanPath(request.sourcePath));
        const uid_t owner = request.ownerId < 0 ? static_cast<uid_t>(-1) : static_cast<uid_t>(request.ownerId);
        const gid_t group = request.groupId < 0 ? static_cast<gid_t>(-1) : static_cast<gid_t>(request.groupId);
        if (::chown(target.constData(), owner, group) != 0) {
            return helperFailResult(QStringLiteral("chown-failed"), errorMessageForErrno(QStringLiteral("Failed to change ownership")), request.sourcePath);
        }
        return helperOkResult();
    }
    case LinuxAdminBroker::Operation::ListDirectory: {
        const QDir directory(QDir::cleanPath(request.sourcePath));
        const QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System
            | (request.includeHidden ? QDir::Hidden : QDir::NoFilter);
        const QFileInfoList infos = directory.entryInfoList(filters, QDir::Name | QDir::DirsFirst);
        if (!directory.exists()) {
            return helperFailResult(QStringLiteral("not-found"), QStringLiteral("Directory path is missing"), request.sourcePath);
        }

        LinuxAdminBroker::Result result = helperOkResult();
        for (const QFileInfo &info : infos) {
            QJsonObject entry;
            entry.insert(QStringLiteral("name"), info.fileName());
            entry.insert(QStringLiteral("path"), info.absoluteFilePath());
            entry.insert(QStringLiteral("suffix"), info.suffix());
            entry.insert(QStringLiteral("size"), info.size());
            entry.insert(QStringLiteral("modifiedMs"), info.lastModified().toMSecsSinceEpoch());
            entry.insert(QStringLiteral("createdMs"), info.birthTime().toMSecsSinceEpoch());
            entry.insert(QStringLiteral("isDirectory"), info.isDir());
            entry.insert(QStringLiteral("isHidden"), info.isHidden());
            entry.insert(QStringLiteral("isReadOnly"), !info.isWritable());
            entry.insert(QStringLiteral("isSymLink"), info.isSymLink());
            result.entries.append(entry);
        }
        return result;
    }
    case LinuxAdminBroker::Operation::ReadFile: {
        QFile file(QDir::cleanPath(request.sourcePath));
        if (!file.open(QIODevice::ReadOnly)) {
            return helperFailResult(QStringLiteral("read-failed"), file.errorString(), request.sourcePath);
        }
        if (!file.seek(request.offset)) {
            return helperFailResult(QStringLiteral("read-failed"), QStringLiteral("Failed to seek file"), request.sourcePath);
        }
        LinuxAdminBroker::Result result = helperOkResult();
        result.totalSize = file.size();
        result.data = file.read(request.length);
        if (result.data.size() < request.length && file.error() != QFileDevice::NoError) {
            return helperFailResult(QStringLiteral("read-failed"), file.errorString(), request.sourcePath);
        }
        return result;
    }
    }
    return helperFailResult(QStringLiteral("invalid-operation"), QStringLiteral("Invalid operation"));
}

QJsonObject probeObject()
{
    QJsonObject probe;
    probe.insert(QStringLiteral("protocolVersion"), LinuxAdminBroker::CurrentProtocolVersion);
    probe.insert(QStringLiteral("backend"), QStringLiteral("helper-process"));
    probe.insert(QStringLiteral("privileged"), ::geteuid() == 0);
    probe.insert(QStringLiteral("pid"), static_cast<qint64>(::getpid()));
    return probe;
}

void writeProbe()
{
    QTextStream out(stdout);
    out << QString::fromUtf8(QJsonDocument(probeObject()).toJson(QJsonDocument::Compact)) << '\n';
    out.flush();
}

LinuxAdminBroker::Result handleRequestBytes(const QByteArray &requestBytes,
                                            const QString &expectedSessionNonce = {},
                                            const ProgressCallback &progress = {})
{
    QJsonParseError parseError;
    const QJsonDocument requestDocument = QJsonDocument::fromJson(requestBytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !requestDocument.isObject()) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Admin helper request is not valid JSON"));
    }

    LinuxAdminBroker::Request request;
    const LinuxAdminBroker::Result parseResult = LinuxAdminBroker::requestFromJson(requestDocument.object(), &request);
    if (!parseResult.success) {
        return parseResult;
    }

    resetCancelRequested();
    return executeRequest(request, expectedSessionNonce, progress);
}

int runSession(const QString &expectedSessionNonce = {}, const QString &cancelFilePath = {})
{
    g_cancelFilePath = cancelFilePath;
    writeProbe();

    QFile stdinFile;
    if (!stdinFile.open(stdin, QIODevice::ReadOnly)) {
        return 0;
    }

    while (true) {
        const QByteArray line = stdinFile.readLine();
        if (line.isEmpty()) {
            if (stdinFile.atEnd()) {
                break;
            }
            continue;
        }
        if (line.trimmed().isEmpty()) {
            continue;
        }
        writeResult(handleRequestBytes(line, expectedSessionNonce, [](qint64 processedBytes, qint64 totalBytes) {
            QJsonObject event;
            event.insert(QStringLiteral("type"), QStringLiteral("progress"));
            event.insert(QStringLiteral("processedBytes"), processedBytes);
            event.insert(QStringLiteral("totalBytes"), totalBytes);
            QTextStream out(stdout);
            out << QString::fromUtf8(QJsonDocument(event).toJson(QJsonDocument::Compact)) << '\n';
            out.flush();
        }));
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    installCancelSignalHandler();

    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--session"))) {
        const int sessionArgumentIndex = app.arguments().indexOf(QStringLiteral("--session"));
        const int cancelFileArgumentIndex = app.arguments().indexOf(QStringLiteral("--cancel-file"));
        const QString cancelFilePath = cancelFileArgumentIndex >= 0
            ? app.arguments().value(cancelFileArgumentIndex + 1)
            : QString();
        return runSession(app.arguments().value(sessionArgumentIndex + 1), cancelFilePath);
    }
    if (app.arguments().contains(QStringLiteral("--probe"))) {
        writeProbe();
        return 0;
    }

    QFile stdinFile;
    if (!stdinFile.open(stdin, QIODevice::ReadOnly)) {
        writeFailure(QStringLiteral("invalid-request"), QStringLiteral("Admin helper could not read stdin"));
        return 0;
    }
    const QByteArray requestBytes = stdinFile.readAll();

    writeResult(handleRequestBytes(requestBytes));
    return 0;
}
