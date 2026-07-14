#include "ArchiveFileProvider.h"

#include "ArchiveOperationCallbacks.h"
#include "ArchiveSupport.h"
#include "DriveUtils.h"
#include "CleanupSubsystem.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMimeDatabase>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtConcurrent>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

#ifdef Q_OS_LINUX
#include <sched.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>
#endif

#ifdef HAS_UNOFFICIAL_BIT7Z
#include <bit7z/bit7z.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitformat.hpp>
#endif

#include "ArchiveFileProviderInternal.h"

namespace ArchiveFileProviderInternal {
constexpr int kMaxNestedArchiveDepth = 8;

thread_local QString g_currentThreadTemporaryParentPath;

bool archiveNestedTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_ARCHIVE_NESTED_TRACE");
    return enabled;
}

bool archiveExtractTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_ARCHIVE_EXTRACT_TRACE")
        || archiveNestedTraceEnabled();
    return enabled;
}

#ifdef Q_OS_LINUX
QString filesystemTypeForPath(const QString &path)
{
    struct statfs fs {};
    const QByteArray nativePath = QFile::encodeName(QFileInfo(path).absoluteFilePath());
    if (statfs(nativePath.constData(), &fs) != 0) {
        return QStringLiteral("unknown:%1").arg(QString::fromLocal8Bit(std::strerror(errno)));
    }
    return QStringLiteral("0x%1").arg(QString::number(static_cast<qulonglong>(fs.f_type), 16));
}

bool shouldThrottleArchiveExtract(const QString &archiveFilesystem, const QString &destinationFilesystem)
{
    return archiveFilesystem.startsWith(QStringLiteral("0x"))
        && destinationFilesystem.startsWith(QStringLiteral("0x"))
        && archiveFilesystem != destinationFilesystem;
}

QString applyBackgroundPriorityToProcess(qint64 processId)
{
    if (processId <= 0) {
        return QStringLiteral("invalid pid");
    }

    QStringList errors;
    const auto pid = static_cast<pid_t>(processId);
    if (setpriority(PRIO_PROCESS, static_cast<id_t>(pid), 19) != 0) {
        errors.append(QStringLiteral("nice:%1").arg(QString::fromLocal8Bit(std::strerror(errno))));
    }

    sched_param idleParam {};
    if (sched_setscheduler(pid, SCHED_IDLE, &idleParam) != 0) {
        errors.append(QStringLiteral("sched_idle:%1").arg(QString::fromLocal8Bit(std::strerror(errno))));
    }

    constexpr int ioprioWhoProcess = 1;
    constexpr int ioprioClassIdle = 3;
    constexpr int ioprioValue = ioprioClassIdle << 13;
    if (syscall(SYS_ioprio_set, ioprioWhoProcess, pid, ioprioValue) != 0) {
        errors.append(QStringLiteral("ioprio:%1").arg(QString::fromLocal8Bit(std::strerror(errno))));
    }

    return errors.join(QStringLiteral("; "));
}

class LinuxProcessDutyCycleThrottle {
public:
    LinuxProcessDutyCycleThrottle(qint64 processId, bool enabled)
        : m_processId(static_cast<pid_t>(processId))
        , m_enabled(enabled && processId > 0)
    {
        m_timer.start();
    }

    ~LinuxProcessDutyCycleThrottle()
    {
        resume();
    }

    void tick()
    {
        if (!m_enabled) {
            return;
        }

        constexpr qint64 activeMs = 60;
        constexpr qint64 pausedMs = 40;
        constexpr qint64 cycleMs = activeMs + pausedMs;
        setPaused((m_timer.elapsed() % cycleMs) >= activeMs);
    }

    void resume()
    {
        setPaused(false);
    }

private:
    void setPaused(bool paused)
    {
        if (!m_enabled || m_paused == paused) {
            return;
        }

        const int signal = paused ? SIGSTOP : SIGCONT;
        if (kill(m_processId, signal) != 0) {
            if (errno != ESRCH && archiveExtractTraceEnabled()) {
                qInfo().noquote() << "[ArchiveExtract] 7z throttle failed"
                                  << "pid=" << m_processId
                                  << "signal=" << signal
                                  << "error=" << QString::fromLocal8Bit(std::strerror(errno));
            }
            m_enabled = false;
            m_paused = false;
            return;
        }
        m_paused = paused;
    }

    pid_t m_processId = -1;
    bool m_enabled = false;
    bool m_paused = false;
    QElapsedTimer m_timer;
};
#endif

void scheduleRecursiveRemove(QString path);

QMutex &archiveTemporaryLeaseMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, QString> &archiveTemporaryLeaseByPath()
{
    static QHash<QString, QString> leases;
    return leases;
}

QString normalizedArchiveTempPath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(QDir::fromNativeSeparators(path)).absoluteFilePath());
}

QString registerArchiveTemporaryDirectory(const QString &path,
                                          const QString &safetyRoot,
                                          CleanupArtifactKind kind)
{
    const QString normalizedPath = normalizedArchiveTempPath(path);
    const QString normalizedRoot = normalizedArchiveTempPath(safetyRoot);
    if (normalizedPath.isEmpty() || normalizedRoot.isEmpty()) {
        return {};
    }

    QString leaseId;
    CleanupSubsystem::instance().registerArtifact(
        kind,
        normalizedPath,
        normalizedRoot,
        true,
        &leaseId);
    if (!leaseId.isEmpty()) {
        QMutexLocker locker(&archiveTemporaryLeaseMutex());
        archiveTemporaryLeaseByPath().insert(normalizedPath, leaseId);
    }
    return leaseId;
}

QString takeArchiveTemporaryDirectoryLease(const QString &path)
{
    const QString normalizedPath = normalizedArchiveTempPath(path);
    if (normalizedPath.isEmpty()) {
        return {};
    }
    QMutexLocker locker(&archiveTemporaryLeaseMutex());
    auto it = archiveTemporaryLeaseByPath().find(normalizedPath);
    if (it == archiveTemporaryLeaseByPath().end()) {
        return {};
    }
    const QString leaseId = it.value();
    archiveTemporaryLeaseByPath().erase(it);
    return leaseId;
}

void scheduleArchiveTemporaryDirectoryCleanup(const QString &path)
{
    const QString leaseId = takeArchiveTemporaryDirectoryLease(path);
    if (!leaseId.isEmpty()) {
        CleanupSubsystem::instance().scheduleDelete(leaseId);
    } else {
        scheduleRecursiveRemove(path);
    }
}

QStringList redactedSevenZipArguments(QStringList arguments)
{
    for (QString &argument : arguments) {
        if (argument.startsWith(QStringLiteral("-p")) && argument.size() > 2) {
            argument = QStringLiteral("-p<redacted>");
        }
    }
    return arguments;
}

void scheduleRecursiveRemove(QString path)
{
    path = QDir::fromNativeSeparators(path.trimmed());
    if (path.isEmpty()) {
        return;
    }

    (void)QtConcurrent::run([path]() {
        QDir(path).removeRecursively();
    });
}

void releaseTemporaryDirAsync(std::unique_ptr<QTemporaryDir> tempDir)
{
    if (!tempDir) {
        return;
    }

    const QString path = QDir::fromNativeSeparators(tempDir->path());
    tempDir->setAutoRemove(false);
    tempDir.reset();
    scheduleArchiveTemporaryDirectoryCleanup(path);
}

class TemporaryFileDevice : public QFile {
public:
    explicit TemporaryFileDevice(const QString &fileName, QString cleanupRoot = {}, QObject *parent = nullptr)
        : QFile(fileName, parent)
        , m_cleanupRoot(std::move(cleanupRoot))
    {
    }

    ~TemporaryFileDevice() override
    {
        close();
        if (!m_cleanupRoot.isEmpty()) {
            scheduleArchiveTemporaryDirectoryCleanup(m_cleanupRoot);
        } else if (!fileName().isEmpty()) {
            QFile::remove(fileName());
        }
    }

private:
    QString m_cleanupRoot;
};

#ifdef HAS_UNOFFICIAL_BIT7Z
std::shared_ptr<bit7z::Bit7zLibrary> getGlobalLibrary()
{
    static std::mutex s_mutex;
    static std::shared_ptr<bit7z::Bit7zLibrary> s_library;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_library) {
        try {
            s_library = std::make_shared<bit7z::Bit7zLibrary>();
        } catch (const std::exception &) {
            try {
                const QString libraryPath = ArchiveSupport::archiveLibraryPath();
                if (!libraryPath.isEmpty()) {
                    s_library = std::make_shared<bit7z::Bit7zLibrary>(libraryPath.toStdString());
                }
            } catch (const std::exception &) {
                // Ignore failure
            }
        }
    }
    return s_library;
}
#endif

bool isHiddenName(const QString &name)
{
    return name.startsWith(QLatin1Char('.'));
}

QMutex &archiveReaderMutex()
{
    static QMutex mutex;
    return mutex;
}

QString extractedArchiveItemPath(const QString &rootPath, const QString &relativePath, const QString &itemName)
{
    const QString directPath = QDir(rootPath).filePath(QDir::fromNativeSeparators(relativePath));
    if (QFileInfo::exists(directPath) && QFileInfo(directPath).isFile()) {
        return directPath;
    }

    const QString fileName = QFileInfo(itemName).fileName();
    if (fileName.isEmpty()) {
        return {};
    }

    QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString candidate = it.next();
        if (QFileInfo(candidate).fileName() == fileName) {
            return candidate;
        }
    }
    return {};
}

QStringList sampledExtractedFiles(const QString &rootPath, int limit)
{
    QStringList files;
    QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext() && files.size() < limit) {
        files.append(QDir(rootPath).relativeFilePath(it.next()));
    }
    return files;
}

QString sevenZipExecutablePath()
{
    return ArchiveSupport::sevenZipExecutablePath();
}

bool isCompressedTarArchivePath(const QString &archivePath)
{
    const QString lower = archivePath.toLower();
    return lower.endsWith(QStringLiteral(".tar.gz"))
        || lower.endsWith(QStringLiteral(".tgz"))
        || lower.endsWith(QStringLiteral(".tar.xz"))
        || lower.endsWith(QStringLiteral(".txz"))
        || lower.endsWith(QStringLiteral(".tar.bz2"))
        || lower.endsWith(QStringLiteral(".tbz"))
        || lower.endsWith(QStringLiteral(".tbz2"))
        || lower.endsWith(QStringLiteral(".tar.zst"))
        || lower.endsWith(QStringLiteral(".tzst"));
}

bool extractCompressedTarWithSevenZipPipe(const QString &archivePath,
                                          const QString &destinationPath,
                                          const std::function<bool(uint64_t)> &progressCallback,
                                          QString *error,
                                          const std::function<void(uint64_t, uint64_t)> &progressReporter,
                                          const QString &passwordOverride)
{
    const QString executable = sevenZipExecutablePath();
    if (executable.isEmpty()) {
        return false;
    }

    QElapsedTimer extractTimer;
    extractTimer.start();
    QProcess unpackProcess;
    QProcess tarProcess;
    unpackProcess.setProgram(executable);
    tarProcess.setProgram(executable);

    QStringList unpackArguments = {
        QStringLiteral("x"),
        QStringLiteral("-y"),
        QStringLiteral("-bso0"),
        QStringLiteral("-bsp0"),
        QStringLiteral("-bse1"),
        QStringLiteral("-so"),
    };
    const QString password = passwordOverride.isNull()
        ? ArchiveFileProvider::archivePasswordForPath(archivePath)
        : passwordOverride;
    if (!password.isEmpty()) {
        unpackArguments.append(QStringLiteral("-p%1").arg(password));
    }
    unpackArguments.append(QDir::toNativeSeparators(archivePath));

    QStringList tarArguments = {
        QStringLiteral("x"),
        QStringLiteral("-ttar"),
        QStringLiteral("-si"),
        QStringLiteral("-y"),
        QStringLiteral("-aos"),
        QStringLiteral("-bso0"),
        QStringLiteral("-bsp1"),
        QStringLiteral("-bse1"),
#ifdef Q_OS_LINUX
        QStringLiteral("-mmt=1"),
#endif
        QStringLiteral("-o%1").arg(QDir::toNativeSeparators(destinationPath)),
    };

    unpackProcess.setArguments(unpackArguments);
    tarProcess.setArguments(tarArguments);
    unpackProcess.setProcessChannelMode(QProcess::SeparateChannels);
    tarProcess.setProcessChannelMode(QProcess::MergedChannels);
    unpackProcess.setStandardOutputProcess(&tarProcess);

#ifdef Q_OS_LINUX
    const QString archiveFilesystem = filesystemTypeForPath(archivePath);
    const QString destinationFilesystem = filesystemTypeForPath(destinationPath);
    const bool throttleExtractProcess = shouldThrottleArchiveExtract(archiveFilesystem, destinationFilesystem);
#endif
    if (archiveExtractTraceEnabled()) {
        qInfo().noquote() << "[ArchiveExtract] 7z tar-pipe start"
                          << "exe=" << executable
                          << "archive=" << QDir::toNativeSeparators(archivePath)
                          << "archiveSize=" << QFileInfo(archivePath).size()
                          << "destination=" << QDir::toNativeSeparators(destinationPath)
#ifdef Q_OS_LINUX
                          << "archiveFs=" << archiveFilesystem
                          << "destinationFs=" << destinationFilesystem
                          << "throttle=" << throttleExtractProcess
#endif
                          << "unpackArgs=" << redactedSevenZipArguments(unpackArguments).join(QLatin1Char(' '))
                          << "tarArgs=" << redactedSevenZipArguments(tarArguments).join(QLatin1Char(' '));
    }

    tarProcess.start();
    if (!tarProcess.waitForStarted(5000)) {
        if (error) {
            *error = QStringLiteral("Could not start 7-Zip tar extractor: %1").arg(tarProcess.errorString());
        }
        return false;
    }
    unpackProcess.start();
    if (!unpackProcess.waitForStarted(5000)) {
        tarProcess.kill();
        tarProcess.waitForFinished(3000);
        if (error) {
            *error = QStringLiteral("Could not start 7-Zip decompressor: %1").arg(unpackProcess.errorString());
        }
        return false;
    }

#ifdef Q_OS_LINUX
    const QString unpackPriorityError = applyBackgroundPriorityToProcess(unpackProcess.processId());
    const QString tarPriorityError = applyBackgroundPriorityToProcess(tarProcess.processId());
    LinuxProcessDutyCycleThrottle unpackThrottle(unpackProcess.processId(), throttleExtractProcess);
    LinuxProcessDutyCycleThrottle tarThrottle(tarProcess.processId(), throttleExtractProcess);
    if (archiveExtractTraceEnabled()) {
        qInfo().noquote() << "[ArchiveExtract] 7z tar-pipe priority"
                          << "unpackPid=" << unpackProcess.processId()
                          << "tarPid=" << tarProcess.processId()
                          << "nice=19"
                          << "scheduler=idle"
                          << "ioprio=idle"
                          << "throttle=60/40ms"
                          << "unpackError=" << unpackPriorityError
                          << "tarError=" << tarPriorityError;
    }
#endif

    QByteArray tarOutputBuffer;
    QByteArray unpackErrorBuffer;
    bool tarHadDangerousLinkWarning = false;
    bool tarHadOtherError = false;
    int lastPercent = -1;
    QElapsedTimer progressTimer;
    progressTimer.start();
    const uint64_t archiveSize = static_cast<uint64_t>((std::max<qint64>)(1, QFileInfo(archivePath).size()));
    const QRegularExpression percentPattern(QStringLiteral("(\\d{1,3})%"));
    if (progressReporter) {
        progressReporter(0, archiveSize);
    }

    auto killProcesses = [&]() {
#ifdef Q_OS_LINUX
        unpackThrottle.resume();
        tarThrottle.resume();
#endif
        unpackProcess.kill();
        tarProcess.kill();
        unpackProcess.waitForFinished(3000);
        tarProcess.waitForFinished(3000);
    };

    auto consumeOutput = [&]() -> bool {
        tarOutputBuffer.append(tarProcess.readAll());
        unpackErrorBuffer.append(unpackProcess.readAllStandardError());
        if (tarOutputBuffer.size() > 4096) {
            tarOutputBuffer = tarOutputBuffer.right(4096);
        }
        if (unpackErrorBuffer.size() > 4096) {
            unpackErrorBuffer = unpackErrorBuffer.right(4096);
        }

        const QString text = QString::fromLocal8Bit(tarOutputBuffer);
        const QStringList lines = text.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (!line.contains(QStringLiteral("ERROR:"))) {
                continue;
            }
            if (line.contains(QStringLiteral("Dangerous link"))) {
                tarHadDangerousLinkWarning = true;
            } else {
                tarHadOtherError = true;
            }
        }
        QRegularExpressionMatchIterator matches = percentPattern.globalMatch(text);
        int percent = -1;
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            if (ok) {
                percent = std::clamp(value, 0, 100);
            }
        }

        if (percent >= 0 && percent != lastPercent && progressTimer.elapsed() >= 120) {
            lastPercent = percent;
            progressTimer.restart();
            const uint64_t processed = (archiveSize * static_cast<uint64_t>(percent)) / 100U;
            if (progressReporter) {
                progressReporter(processed, archiveSize);
            }
            if (progressCallback && !progressCallback(processed)) {
                killProcesses();
                if (error) {
                    *error = QStringLiteral("Archive extraction was cancelled");
                }
                return false;
            }
        }
        return true;
    };

    while (unpackProcess.state() != QProcess::NotRunning || tarProcess.state() != QProcess::NotRunning) {
#ifdef Q_OS_LINUX
        unpackThrottle.tick();
        tarThrottle.tick();
#endif
        unpackProcess.waitForFinished(40);
        tarProcess.waitForFinished(1);
        if (!consumeOutput()) {
            return false;
        }
        if (ArchiveOperationCallbacks::current().isAbortRequested()) {
            killProcesses();
            if (error) {
                *error = QStringLiteral("Archive extraction was cancelled");
            }
            return false;
        }
    }
#ifdef Q_OS_LINUX
    unpackThrottle.resume();
    tarThrottle.resume();
#endif
    if (!consumeOutput()) {
        return false;
    }
    if (progressCallback) {
        progressCallback(archiveSize);
    }
    if (progressReporter) {
        progressReporter(archiveSize, archiveSize);
    }

    const int unpackExitCode = unpackProcess.exitCode();
    const int tarExitCode = tarProcess.exitCode();
    const bool unpackOk = unpackProcess.exitStatus() == QProcess::NormalExit
        && (unpackExitCode == 0 || unpackExitCode == 1);
    const bool tarOk = tarProcess.exitStatus() == QProcess::NormalExit
        && (tarExitCode == 0
            || tarExitCode == 1
            || (tarExitCode == 2 && tarHadDangerousLinkWarning && !tarHadOtherError));
    if (archiveExtractTraceEnabled()) {
        qInfo().noquote() << "[ArchiveExtract] 7z tar-pipe finished"
                          << "archive=" << QDir::toNativeSeparators(archivePath)
                          << "unpackExitStatus=" << static_cast<int>(unpackProcess.exitStatus())
                          << "unpackExitCode=" << unpackExitCode
                          << "tarExitStatus=" << static_cast<int>(tarProcess.exitStatus())
                          << "tarExitCode=" << tarExitCode
                          << "dangerousLinkWarnings=" << tarHadDangerousLinkWarning
                          << "otherErrors=" << tarHadOtherError
                          << "elapsedMs=" << extractTimer.elapsed()
                          << "unpackTail=" << QString::fromLocal8Bit(unpackErrorBuffer).trimmed().left(1000)
                          << "tarTail=" << QString::fromLocal8Bit(tarOutputBuffer).trimmed().left(1000);
    }
    if (unpackOk && tarOk) {
        return true;
    }

    if (error) {
        const QString unpackOutput = QString::fromLocal8Bit(unpackErrorBuffer).trimmed();
        const QString tarOutput = QString::fromLocal8Bit(tarOutputBuffer).trimmed();
        *error = !tarOutput.isEmpty()
            ? tarOutput.left(1000)
            : (!unpackOutput.isEmpty()
                ? unpackOutput.left(1000)
                : QStringLiteral("7-Zip tar pipe failed with exit codes %1/%2").arg(unpackExitCode).arg(tarExitCode));
    }
    return false;
}

bool extractArchiveWithSevenZip(const QString &archivePath,
                                const QString &destinationPath,
                                const std::function<bool(uint64_t)> &progressCallback,
                                QString *error,
                                const QStringList &itemPaths,
                                const std::function<void(uint64_t, uint64_t)> &progressReporter,
                                const QString &passwordOverride)
{
    const QString executable = sevenZipExecutablePath();
    if (executable.isEmpty()) {
        return false;
    }
    if (itemPaths.isEmpty() && isCompressedTarArchivePath(archivePath)) {
        return extractCompressedTarWithSevenZipPipe(archivePath,
                                                    destinationPath,
                                                    progressCallback,
                                                    error,
                                                    progressReporter,
                                                    passwordOverride);
    }

    QElapsedTimer extractTimer;
    extractTimer.start();
    QProcess process;
    process.setProgram(executable);
    QStringList arguments = {
        QStringLiteral("x"),
        QStringLiteral("-y"),
        QStringLiteral("-aos"),
        QStringLiteral("-bso0"),
        QStringLiteral("-bsp1"),
        QStringLiteral("-bse1"),
#ifdef Q_OS_LINUX
        QStringLiteral("-mmt=1"),
#endif
        QStringLiteral("-o%1").arg(QDir::toNativeSeparators(destinationPath)),
    };
    const QString password = passwordOverride.isNull()
        ? ArchiveFileProvider::archivePasswordForPath(archivePath)
        : passwordOverride;
    if (!password.isEmpty()) {
        arguments.append(QStringLiteral("-p%1").arg(password));
    }
    arguments.append(QDir::toNativeSeparators(archivePath));
    for (const QString &itemPath : itemPaths) {
        arguments.append(QDir::toNativeSeparators(itemPath));
    }
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_LINUX
    const QString archiveFilesystem = filesystemTypeForPath(archivePath);
    const QString destinationFilesystem = filesystemTypeForPath(destinationPath);
    const bool throttleExtractProcess = shouldThrottleArchiveExtract(archiveFilesystem, destinationFilesystem);
#endif
    if (archiveExtractTraceEnabled()) {
        qInfo().noquote() << "[ArchiveExtract] 7z start"
                          << "exe=" << executable
                          << "archive=" << QDir::toNativeSeparators(archivePath)
                          << "archiveSize=" << QFileInfo(archivePath).size()
                          << "destination=" << QDir::toNativeSeparators(destinationPath)
#ifdef Q_OS_LINUX
                          << "archiveFs=" << archiveFilesystem
                          << "destinationFs=" << destinationFilesystem
                          << "throttle=" << throttleExtractProcess
#endif
                          << "items=" << itemPaths.join(QLatin1Char('|'))
                          << "args=" << redactedSevenZipArguments(arguments).join(QLatin1Char(' '));
    }
    process.start();
    if (!process.waitForStarted(5000)) {
        if (error) {
            *error = QStringLiteral("Could not start 7-Zip: %1").arg(process.errorString());
        }
        if (archiveExtractTraceEnabled()) {
            qInfo().noquote() << "[ArchiveExtract] 7z start failed"
                              << "archive=" << QDir::toNativeSeparators(archivePath)
                              << "error=" << process.errorString();
        }
        return false;
    }

#ifdef Q_OS_LINUX
    const QString priorityError = applyBackgroundPriorityToProcess(process.processId());
    LinuxProcessDutyCycleThrottle processThrottle(process.processId(), throttleExtractProcess);
    if (archiveExtractTraceEnabled()) {
        qInfo().noquote() << "[ArchiveExtract] 7z priority"
                          << "pid=" << process.processId()
                          << "nice=19"
                          << "scheduler=idle"
                          << "ioprio=idle"
                          << "throttle=60/40ms"
                          << "error=" << priorityError;
    }
#endif

    QByteArray outputBuffer;
    int lastPercent = -1;
    int progressBasePercent = -1;
    QElapsedTimer progressTimer;
    progressTimer.start();
    const uint64_t archiveSize = static_cast<uint64_t>((std::max<qint64>)(1, QFileInfo(archivePath).size()));
    const QRegularExpression percentPattern(QStringLiteral("(\\d{1,3})%"));
    if (progressReporter) {
        progressReporter(0, archiveSize);
    }

    auto killProcess = [&]() {
#ifdef Q_OS_LINUX
        processThrottle.resume();
#endif
        process.kill();
        process.waitForFinished(3000);
    };

    auto consumeProcessOutput = [&]() -> bool {
        outputBuffer.append(process.readAll());
        if (outputBuffer.size() > 4096) {
            outputBuffer = outputBuffer.right(4096);
        }

        const QString text = QString::fromLocal8Bit(outputBuffer);
        QRegularExpressionMatchIterator matches = percentPattern.globalMatch(text);
        int percent = -1;
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            if (ok) {
                percent = std::clamp(value, 0, 100);
            }
        }

        if (percent >= 0 && percent != lastPercent && progressTimer.elapsed() >= 120) {
            lastPercent = percent;
            progressTimer.restart();
            const uint64_t processed = (archiveSize * static_cast<uint64_t>(percent)) / 100U;
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] 7z progress"
                                  << "archive=" << QDir::toNativeSeparators(archivePath)
                                  << "percent=" << percent
                                  << "processed=" << processed
                                  << "total=" << archiveSize;
            }
            if (progressReporter) {
                if (progressBasePercent < 0) {
                    progressBasePercent = percent;
                }
                const int range = qMax(1, 100 - progressBasePercent);
                const int normalizedPercent = std::clamp(((percent - progressBasePercent) * 100) / range, 0, 100);
                const uint64_t visibleProcessed = (archiveSize * static_cast<uint64_t>(normalizedPercent)) / 100U;
                progressReporter(visibleProcessed, archiveSize);
            }
            if (progressCallback) {
                if (!progressCallback(processed)) {
                    if (archiveNestedTraceEnabled()) {
                        qInfo().noquote() << "[ArchiveNested] 7z cancelled by callback"
                                          << "archive=" << QDir::toNativeSeparators(archivePath)
                                          << "processed=" << processed
                                          << "total=" << archiveSize;
                    }
                    killProcess();
                    if (error) {
                        *error = QStringLiteral("Archive extraction was cancelled");
                    }
                    return false;
                }
            }
        }
        return true;
    };

    while (!process.waitForFinished(40)) {
#ifdef Q_OS_LINUX
        processThrottle.tick();
#endif
        if (!consumeProcessOutput()) {
            return false;
        }
        if (ArchiveOperationCallbacks::current().isAbortRequested()) {
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] 7z aborted by operation queue"
                                  << "archive=" << QDir::toNativeSeparators(archivePath);
            }
            killProcess();
            if (error) {
                *error = QStringLiteral("Archive extraction was cancelled");
            }
            return false;
        }
    }
#ifdef Q_OS_LINUX
    processThrottle.resume();
#endif
    if (!consumeProcessOutput()) {
        return false;
    }
    if (progressCallback) {
        progressCallback(archiveSize);
    }
    if (progressReporter) {
        progressReporter(archiveSize, archiveSize);
    }

    const int exitCode = process.exitCode();
    if (archiveExtractTraceEnabled()) {
        qInfo().noquote() << "[ArchiveExtract] 7z finished"
                          << "archive=" << QDir::toNativeSeparators(archivePath)
                          << "exitStatus=" << static_cast<int>(process.exitStatus())
                          << "exitCode=" << exitCode
                          << "elapsedMs=" << extractTimer.elapsed()
                          << "tail=" << QString::fromLocal8Bit(outputBuffer).trimmed().left(1000);
    }
    if (process.exitStatus() == QProcess::NormalExit && (exitCode == 0 || exitCode == 1)) {
        return true;
    }

    if (error) {
        const QString output = QString::fromLocal8Bit(outputBuffer).trimmed();
        *error = output.isEmpty()
            ? QStringLiteral("7-Zip failed with exit code %1").arg(exitCode)
            : output.left(1000);
    }
    return false;
}

bool moveExtractedPath(const QString &sourcePath, const QString &destinationPath)
{
    if (QFileInfo(sourcePath).isDir()) {
        return QDir().rename(sourcePath, destinationPath);
    }
    return QFile::rename(sourcePath, destinationPath);
}

QString archiveRelativeToken(const QString &token);


QString archiveTemporaryParentPath(const QString &temporaryParentPath)
{
    if (!temporaryParentPath.isEmpty()) {
        return QDir::fromNativeSeparators(temporaryParentPath);
    }
    if (!g_currentThreadTemporaryParentPath.isEmpty()) {
        return QDir::fromNativeSeparators(g_currentThreadTemporaryParentPath);
    }

    const QString appData = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    if (appData.isEmpty()) {
        return {};
    }
    return QDir(appData).filePath(QStringLiteral("temporary-archives"));
}

QString archiveSourceTemporaryParentPath(const QString &archivePath)
{
    const QString physicalPath = ArchiveSupport::isArchivePath(archivePath)
        ? ArchiveSupport::physicalArchivePath(archivePath)
        : archivePath;
    if (physicalPath.isEmpty()) {
        return {};
    }

    const QFileInfo info(QDir::fromNativeSeparators(physicalPath));
    return QDir::fromNativeSeparators(info.absolutePath());
}

int archiveNestedDepthForPath(const QString &path)
{
    if (path.isEmpty()) {
        return 0;
    }

    const QString normalized = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::normalizeArchivePath(path)
        : ArchiveSupport::archiveRootPath(path);
    const QStringList tokens = ArchiveSupport::splitArchiveTokens(normalized);
    if (tokens.isEmpty()) {
        return 0;
    }

    const int containerTokenCount = qMax(1, static_cast<int>(tokens.size()) - 1);
    return qMax(0, containerTokenCount - 1);
}

QString archiveNestedDepthLimitError(int depth)
{
    return QStringLiteral("Nested archive depth limit exceeded: %1 levels (limit is %2)")
        .arg(depth)
        .arg(kMaxNestedArchiveDepth);
}

bool archiveNestedDepthAllowed(const QString &path, QString *error)
{
    const int depth = archiveNestedDepthForPath(path);
    if (depth <= kMaxNestedArchiveDepth) {
        return true;
    }
    if (error) {
        *error = archiveNestedDepthLimitError(depth);
    }
    return false;
}

void cleanupStaleArchiveTemporaryDirs(const QString &parentPath)
{
    if (parentPath.isEmpty()) {
        return;
    }

    static QMutex cleanupMutex;
    static QSet<QString> cleanedParents;

    const QString normalizedParent = QDir::fromNativeSeparators(QFileInfo(parentPath).absoluteFilePath());
    {
        QMutexLocker locker(&cleanupMutex);
        if (cleanedParents.contains(normalizedParent)) {
            return;
        }
        cleanedParents.insert(normalizedParent);
    }

    QDir parentDir(normalizedParent);
    const QDateTime cutoff = QDateTime::currentDateTimeUtc().addDays(-1);
    const QFileInfoList entries = parentDir.entryInfoList(
        {
            QStringLiteral(".fm-nested-*"),
            QStringLiteral(".fm-read-*"),
            QStringLiteral(".fm-full-extract-*"),
            QStringLiteral(".fm-extract-*"),
            QStringLiteral(".fm-7z-extract-*"),
        },
        QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (entry.lastModified().toUTC() < cutoff) {
            scheduleRecursiveRemove(entry.absoluteFilePath());
        }
    }
}

std::unique_ptr<QTemporaryDir> createArchiveTemporaryDir(const QString &temporaryParentPath)
{
    QString parentPath = archiveTemporaryParentPath(temporaryParentPath);
    if (parentPath.isEmpty()) {
        parentPath = StagingLocationPolicy::defaultCleanupRoot();
    }
    if (parentPath.isEmpty()) {
        return {};
    }
    QDir().mkpath(parentPath);
    cleanupStaleArchiveTemporaryDirs(parentPath);
    auto tempDir = std::make_unique<QTemporaryDir>(
        QDir(parentPath).filePath(QStringLiteral(".fm-nested-XXXXXX")));
    if (tempDir->isValid()) {
        tempDir->setAutoRemove(false);
        registerArchiveTemporaryDirectory(tempDir->path(), parentPath, CleanupArtifactKind::ArchiveBrowse);
    }
    return tempDir;
}

bool resolveArchiveContainerWithSevenZip(const QString &archivePath,
                                         const QString &temporaryParentPath,
                                         ResolvedArchiveContainer *resolved,
                                         QString *error,
                                         const std::function<bool(uint64_t)> &progressCallback,
                                         const std::function<void(uint64_t, uint64_t)> &progressReporter)
{
    if (error) {
        error->clear();
    }
    if (!resolved) {
        if (error) {
            *error = QStringLiteral("Archive container resolution target is missing");
        }
        return false;
    }

    const QString normalized = ArchiveSupport::isArchivePath(archivePath)
        ? ArchiveSupport::normalizeArchivePath(archivePath)
        : ArchiveSupport::archiveRootPath(archivePath);
    const QStringList tokens = ArchiveSupport::splitArchiveTokens(normalized);
    if (archiveNestedTraceEnabled()) {
        qInfo().noquote() << "[ArchiveNested] resolve start"
                          << "path=" << archivePath
                          << "normalized=" << normalized
                          << "temporaryParent=" << QDir::toNativeSeparators(temporaryParentPath)
                          << "tokens=" << tokens.join(QLatin1Char('|'));
    }
    if (tokens.isEmpty() || tokens.first().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Archive path is empty");
        }
        return false;
    }

    QString currentArchivePath = QDir::fromNativeSeparators(QFileInfo(tokens.first()).absoluteFilePath());
    if (!QFileInfo::exists(currentArchivePath)) {
        if (error) {
            *error = QStringLiteral("Archive file was not found");
        }
        return false;
    }
    const QString materializationParent = !archiveSourceTemporaryParentPath(currentArchivePath).isEmpty()
        ? archiveSourceTemporaryParentPath(currentArchivePath)
        : archiveTemporaryParentPath(temporaryParentPath);
    if (archiveNestedTraceEnabled()) {
        qInfo().noquote() << "[ArchiveNested] resolve source"
                          << "currentArchive=" << QDir::toNativeSeparators(currentArchivePath)
                          << "sourceSize=" << QFileInfo(currentArchivePath).size()
                          << "materializationParent=" << QDir::toNativeSeparators(materializationParent)
                          << "containerTokenCount=" << qMax(1, static_cast<int>(tokens.size()) - 1);
    }

    std::unique_ptr<QTemporaryDir> currentTempDir;
    const auto cleanupCurrentTempDir = qScopeGuard([&currentTempDir]() {
        releaseTemporaryDirAsync(std::move(currentTempDir));
    });
    const int containerTokenCount = qMax(1, static_cast<int>(tokens.size()) - 1);
    if (!archiveNestedDepthAllowed(normalized, error)) {
        return false;
    }

    for (int i = 1; i < containerTokenCount; ++i) {
        if (ArchiveOperationCallbacks::current().isAbortRequested()) {
            if (error) {
                *error = QStringLiteral("Archive extraction was cancelled");
            }
            return false;
        }

        const QString rel = archiveRelativeToken(tokens.at(i));
        if (archiveNestedTraceEnabled()) {
            qInfo().noquote() << "[ArchiveNested] materialize begin"
                              << "level=" << i
                              << "currentArchive=" << QDir::toNativeSeparators(currentArchivePath)
                              << "rel=" << rel;
        }
        if (rel.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Nested archive entry path is empty");
            }
            return false;
        }
        if (!ArchiveSupport::isArchiveExtension(QFileInfo(rel).suffix().toLower())) {
            if (error) {
                *error = QStringLiteral("Nested archive item is not an archive");
            }
            return false;
        }
        const QString logicalContainerPath = QStringLiteral("archive://")
            + tokens.mid(0, i).join(QLatin1Char('|'))
            + QStringLiteral("|/");

        std::unique_ptr<QTemporaryDir> nextTempDir = createArchiveTemporaryDir(materializationParent);
        if (!nextTempDir || !nextTempDir->isValid()) {
            if (error) {
                *error = QStringLiteral("Could not create temporary folder for nested archive");
            }
            return false;
        }

        const QString tempRoot = QDir::fromNativeSeparators(nextTempDir->path());
        nextTempDir->setAutoRemove(false);
        if (archiveNestedTraceEnabled()) {
            qInfo().noquote() << "[ArchiveNested] materialize temp"
                              << "level=" << i
                              << "tempRoot=" << QDir::toNativeSeparators(tempRoot)
                              << "valid=" << nextTempDir->isValid();
        }
        const auto cleanupNextTempDir = qScopeGuard([&nextTempDir, tempRoot]() {
            if (nextTempDir) {
                scheduleArchiveTemporaryDirectoryCleanup(tempRoot);
            }
        });
        QString extractError;
        if (!extractArchiveWithSevenZip(currentArchivePath,
                                        tempRoot,
                                        progressCallback,
                                        &extractError,
                                        {rel},
                                        progressReporter,
                                        ArchiveFileProvider::archivePasswordForPath(logicalContainerPath))) {
            if (error) {
                *error = extractError.isEmpty()
                    ? QStringLiteral("7-Zip could not prepare nested archive")
                    : extractError;
            }
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] materialize failed"
                                  << "level=" << i
                                  << "currentArchive=" << QDir::toNativeSeparators(currentArchivePath)
                                  << "rel=" << rel
                                  << "error=" << (extractError.isEmpty()
                                                      ? QStringLiteral("7-Zip could not prepare nested archive")
                                                      : extractError);
            }
            return false;
        }

        const QString extractedPath = extractedArchiveItemPath(tempRoot, rel, QFileInfo(rel).fileName());
        if (archiveNestedTraceEnabled()) {
            const QStringList sample = sampledExtractedFiles(tempRoot);
            qInfo().noquote() << "[ArchiveNested] materialize lookup"
                              << "level=" << i
                              << "tempRoot=" << QDir::toNativeSeparators(tempRoot)
                              << "rel=" << rel
                              << "itemName=" << QFileInfo(rel).fileName()
                              << "found=" << QDir::toNativeSeparators(extractedPath)
                              << "sample=" << sample.join(QLatin1Char('|'));
        }
        if (extractedPath.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Prepared nested archive file was not found");
            }
            return false;
        }

        currentArchivePath = QDir::fromNativeSeparators(QFileInfo(extractedPath).absoluteFilePath());
        if (archiveNestedTraceEnabled()) {
            qInfo().noquote() << "[ArchiveNested] materialize done"
                              << "level=" << i
                              << "physical=" << QDir::toNativeSeparators(currentArchivePath)
                              << "size=" << QFileInfo(currentArchivePath).size();
        }
        releaseTemporaryDirAsync(std::move(currentTempDir));
        currentTempDir = std::move(nextTempDir);
    }

    resolved->physicalPath = currentArchivePath;
    resolved->tempDir = std::move(currentTempDir);
    if (archiveNestedTraceEnabled()) {
        qInfo().noquote() << "[ArchiveNested] resolve done"
                          << "physical=" << QDir::toNativeSeparators(resolved->physicalPath)
                          << "size=" << QFileInfo(resolved->physicalPath).size()
                          << "hasTempDir=" << static_cast<bool>(resolved->tempDir);
    }
    return true;
}

QString archiveTokenPath(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    const QString stripped = ArchiveSupport::stripArchiveScheme(path);
    return stripped;
}

QString archiveRelativeToken(const QString &token)
{
    QString out = QDir::fromNativeSeparators(token.trimmed());
    if (out == QLatin1String("/")) {
        return {};
    }
    if (out.startsWith(QLatin1Char('/'))) {
        out.remove(0, 1);
    }
    while (out.endsWith(QLatin1Char('/'))) {
        out.chop(1);
    }
    return out;
}

QString archiveSuffixFromName(const QString &name)
{
    const QString lower = name.toLower();
    if (lower.endsWith(QStringLiteral(".tar.gz"))) {
        return QStringLiteral("tar.gz");
    }
    if (lower.endsWith(QStringLiteral(".tar.bz2"))) {
        return QStringLiteral("tar.bz2");
    }
    if (lower.endsWith(QStringLiteral(".tar.xz"))) {
        return QStringLiteral("tar.xz");
    }
    if (lower.endsWith(QStringLiteral(".tar.zst"))) {
        return QStringLiteral("tar.zst");
    }
    return QFileInfo(name).suffix().toLower();
}

#ifdef HAS_UNOFFICIAL_BIT7Z
QString toQString(const bit7z::tstring &value)
{
    return QString::fromUtf8(value.c_str());
}

const bit7z::BitInFormat &archiveFormatForSuffix(const QString &suffix)
{
    const QString lower = suffix.toLower();
    if (lower == QLatin1String("7z")) {
        return bit7z::BitFormat::SevenZip;
    }
    if (lower == QLatin1String("rar") || lower == QLatin1String("rev")) {
        return bit7z::BitFormat::Rar;
    }
    if (lower == QLatin1String("rar5")) {
        return bit7z::BitFormat::Rar5;
    }
    if (lower == QLatin1String("cab")) {
        return bit7z::BitFormat::Cab;
    }
    if (lower == QLatin1String("udf")) {
        return bit7z::BitFormat::Udf;
    }
    if (lower == QLatin1String("iso")) {
        return bit7z::BitFormat::Iso;
    }
    if (lower == QLatin1String("tar")) {
        return bit7z::BitFormat::Tar;
    }
    if (lower == QLatin1String("gz") || lower == QLatin1String("tgz")) {
        return bit7z::BitFormat::GZip;
    }
    if (lower == QLatin1String("bz2") || lower == QLatin1String("tbz2")) {
        return bit7z::BitFormat::BZip2;
    }
    if (lower == QLatin1String("xz") || lower == QLatin1String("txz")) {
        return bit7z::BitFormat::Xz;
    }
    return bit7z::BitFormat::Zip;
}

QStringList archiveFormatCandidatesForSuffix(const QString &suffix)
{
    const QString lower = suffix.toLower();
    if (lower == QLatin1String("iso")) {
        return {QStringLiteral("udf"), QStringLiteral("iso")};
    }
    if (lower == QLatin1String("udf")) {
        return {QStringLiteral("udf")};
    }
    if (lower == QLatin1String("rar")) {
        return {QStringLiteral("rar"), QStringLiteral("rar5")};
    }
    if (lower == QLatin1String("rar5")) {
        return {QStringLiteral("rar5")};
    }
    return {lower};
}

QString rarFormatCandidateForFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("rar5");
    }

    const QByteArray signature = file.read(8);
    static const QByteArray rar4Signature = QByteArray::fromHex("526172211A0700");
    static const QByteArray rar5Signature = QByteArray::fromHex("526172211A070100");
    if (signature.startsWith(rar5Signature)) {
        return QStringLiteral("rar5");
    }
    if (signature.startsWith(rar4Signature)) {
        return QStringLiteral("rar");
    }
    return QStringLiteral("rar5");
}
#endif
} // namespace ArchiveFileProviderInternal

using namespace ArchiveFileProviderInternal;

ArchiveFileProvider::ArchiveState::~ArchiveState()
{
    reader.reset();
    if (!tempLeaseId.isEmpty()) {
        if (tempDir) {
            tempDir->setAutoRemove(false);
            tempDir.reset();
        }
        CleanupSubsystem::instance().scheduleDelete(tempLeaseId);
    } else {
        releaseTemporaryDirAsync(std::move(tempDir));
    }
    tempFile.reset();
}

ArchiveFileProvider::ArchiveFileProvider(QObject *parent)
    : FileProvider(parent)
{
}

ArchiveFileProvider::~ArchiveFileProvider()
{
    cancel();
}

QString ArchiveFileProvider::scheme() const
{
    return QStringLiteral("archive");
}

bool ArchiveFileProvider::canHandle(const QString &path) const
{
    return ArchiveSupport::isArchivePath(path);
}

FileProvider::Capabilities ArchiveFileProvider::capabilities() const
{
    return Browse | ReadMetadata | Transfer;
}

void ArchiveFileProvider::scan(const QString &path)
{
    cancelCurrentScan(false);

    m_currentPath = normalizedPath(path);
    m_running.store(true);
    const int myGeneration = m_generation.fetch_add(1) + 1;
    const bool showHidden = m_showHidden;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = m_cancelled;
    if (archiveNestedTraceEnabled()) {
        qInfo().noquote() << "[ArchiveNested] scan start"
                          << "path=" << path
                          << "current=" << m_currentPath
                          << "generation=" << myGeneration
                          << "showHidden=" << showHidden;
    }
    emit started();

    const QString scanPath = m_currentPath;
    QPointer<ArchiveFileProvider> self(this);
    m_scanFuture = QtConcurrent::run([self, scanPath, myGeneration, showHidden, cancelled]() mutable {
        if (!self) {
            return;
        }

        QString depthError;
        if (!archiveNestedDepthAllowed(scanPath, &depthError)) {
            QMetaObject::invokeMethod(self.data(), [self, scanPath, myGeneration, depthError]() {
                if (!self || myGeneration != self->m_generation.load()) {
                    return;
                }
                self->m_running.store(false);
                emit self->finished(scanPath, false, myGeneration, depthError);
            }, Qt::QueuedConnection);
            return;
        }

        std::shared_ptr<bit7z::Bit7zLibrary> library;
#ifdef HAS_UNOFFICIAL_BIT7Z
        library = getGlobalLibrary();
#endif
        if (!library) {
            QMetaObject::invokeMethod(self.data(), [self, scanPath, myGeneration]() {
                if (!self || myGeneration != self->m_generation.load()) {
                    return;
                }
                self->m_running.store(false);
                emit self->finished(scanPath,
                                    false,
                                    myGeneration,
                                    QStringLiteral("bit7z backend was not found or could not be loaded"));
            }, Qt::QueuedConnection);
            return;
        }

        auto emitBatch = [self, myGeneration](const QList<FileEntry> &batch) {
            if (!self || batch.isEmpty() || myGeneration != self->m_generation.load()) {
                return;
            }
            emit self->batchReady(batch, myGeneration);
        };

        const QString cacheKey = archiveCacheKey(scanPath);
        if (auto cachedState = cachedStateForKey(cacheKey);
            cachedState
            && cachedState->valid
            && archiveContainerPart(cachedState->currentPath) == archiveContainerPart(scanPath)) {
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] scan cache hit"
                                  << "path=" << scanPath
                                  << "generation=" << myGeneration
                                  << "cacheKey=" << cacheKey
                                  << "physical=" << QDir::toNativeSeparators(cachedState->physicalContainerPath);
            }
            emitBatch(visibleEntriesForBrowse(*cachedState, archiveBrowsePathForPath(scanPath), showHidden));
            QMetaObject::invokeMethod(self.data(), [self, scanPath, myGeneration, library, cachedState]() mutable {
                if (!self || myGeneration != self->m_generation.load()) {
                    return;
                }
                self->m_library = library;
                self->m_state = cachedState;
                self->m_running.store(false);
                emit self->finished(scanPath, true, myGeneration, {});
            }, Qt::QueuedConnection);
            return;
        }

        if (archiveNestedTraceEnabled()) {
            qInfo().noquote() << "[ArchiveNested] scan cache miss"
                              << "path=" << scanPath
                              << "generation=" << myGeneration
                              << "cacheKey=" << cacheKey;
        }
        ArchiveFileProvider::ArchiveState state = ArchiveFileProvider::buildStateFromScratch(
            scanPath,
            library,
            emitBatch,
            showHidden,
            cancelled,
            {},
            [self, myGeneration](uint64_t processed, uint64_t total) {
                if (!self || myGeneration != self->m_generation.load()) {
                    return;
                }
                const uint64_t maxBytes = static_cast<uint64_t>((std::numeric_limits<qint64>::max)());
                emit self->progress(
                    static_cast<qint64>((std::min)(processed, maxBytes)),
                    static_cast<qint64>((std::min)(total, maxBytes)),
                    QStringLiteral("Preparing nested archive..."),
                    myGeneration);
            });

        if (!self) {
            return;
        }
        auto statePtr = std::make_shared<ArchiveFileProvider::ArchiveState>(std::move(state));
        QMetaObject::invokeMethod(self.data(), [self, scanPath, myGeneration, library, statePtr]() mutable {
            if (!self || myGeneration != self->m_generation.load()) {
                if (archiveNestedTraceEnabled()) {
                    qInfo().noquote() << "[ArchiveNested] scan apply drop"
                                      << "path=" << scanPath
                                      << "generation=" << myGeneration
                                      << "currentGeneration=" << (self ? self->m_generation.load() : -1);
                }
                return;
            }

            if (!statePtr->valid) {
                self->m_running.store(false);
                if (archiveNestedTraceEnabled()) {
                    qInfo().noquote() << "[ArchiveNested] scan finished"
                                      << "path=" << scanPath
                                      << "generation=" << myGeneration
                                      << "success=false"
                                      << "error=" << statePtr->error;
                }
                emit self->finished(scanPath, false, myGeneration, statePtr->error);
                return;
            }

            self->m_library = library;
            self->m_state = statePtr;
            storeStateInCache(archiveCacheKey(scanPath), statePtr);
            self->m_running.store(false);
            if (archiveNestedTraceEnabled()) {
                qInfo().noquote() << "[ArchiveNested] scan finished"
                                  << "path=" << scanPath
                                  << "generation=" << myGeneration
                                  << "success=true"
                                  << "physical=" << QDir::toNativeSeparators(statePtr->physicalContainerPath)
                                  << "items=" << statePtr->items.size();
            }
            emit self->finished(scanPath, true, myGeneration, {});
        }, Qt::QueuedConnection);
    });
}


void ArchiveFileProvider::cancel()
{
    cancelCurrentScan(true);
}

void ArchiveFileProvider::cancelCurrentScan(bool invalidateCache)
{
    m_generation.fetch_add(1);
    m_running.store(false);
    if (m_cancelled) {
        m_cancelled->store(true);
    }
    if (invalidateCache && !m_currentPath.isEmpty()) {
        invalidateCacheForPath(m_currentPath);
    }
    m_state.reset();
}

void ArchiveFileProvider::setShowHidden(bool show)
{
    m_showHidden = show;
}

bool ArchiveFileProvider::isRunning() const
{
    return m_running.load();
}

QString ArchiveFileProvider::currentPath() const
{
    return m_currentPath;
}

int ArchiveFileProvider::currentGeneration() const
{
    return m_generation.load();
}

bool ArchiveFileProvider::pathExists(const QString &path) const
{
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        const QString rel = archiveRelativeToken(browsePath);
        if (rel.isEmpty()) {
            return true;
        }
        return state->pathIndex.contains(rel) || state->directories.contains(rel);
    }

    ArchiveState state = stateForPath(path);
    if (!state.valid) {
        return false;
    }
    const QString rel = archiveRelativeToken(state.browsePath);
    if (rel.isEmpty()) {
        return true;
    }
    return state.pathIndex.contains(rel) || state.directories.contains(rel);
}

bool ArchiveFileProvider::isDirectory(const QString &path) const
{
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        const QString rel = archiveRelativeToken(browsePath);
        if (rel.isEmpty()) {
            return true;
        }
        const int idx = state->pathIndex.value(rel, -1);
        if (idx >= 0 && idx < state->items.size()) {
            return state->items.at(idx).isDirectory || state->directories.contains(rel);
        }
        return state->directories.contains(rel);
    }

    ArchiveState state = stateForPath(path);
    if (!state.valid) {
        return false;
    }
    const QString rel = archiveRelativeToken(state.browsePath);
    if (rel.isEmpty()) {
        return true;
    }
    const int idx = state.pathIndex.value(rel, -1);
    if (idx >= 0 && idx < state.items.size()) {
        return state.items.at(idx).isDirectory || state.directories.contains(rel);
    }
    return state.directories.contains(rel);
}

bool ArchiveFileProvider::isSymLink(const QString &path) const
{
    const auto info = entryInfo(path);
    return info ? info->isSystem : false;
}

QString ArchiveFileProvider::normalizedPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::normalizeArchivePath(path);
    }
    if (ArchiveSupport::isArchiveFilePath(path)) {
        return ArchiveSupport::archiveRootPath(path);
    }
    return ArchiveSupport::normalizeArchivePath(path);
}

QString ArchiveFileProvider::fileName(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveFileName(path);
    }
    return QFileInfo(path).fileName();
}

QString ArchiveFileProvider::absolutePath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::normalizeArchivePath(path);
    }
    return QFileInfo(path).absoluteFilePath();
}

QString ArchiveFileProvider::parentPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveParentPath(path);
    }
    return QFileInfo(path).absoluteDir().absolutePath();
}

QString ArchiveFileProvider::childPath(const QString &parentPath, const QString &name) const
{
    if (ArchiveSupport::isArchivePath(parentPath)) {
        return ArchiveSupport::archiveChildPath(parentPath, name);
    }
    return QDir(parentPath).filePath(name);
}

std::optional<FileEntry> ArchiveFileProvider::entryInfo(const QString &path) const
{
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        const QString rel = archiveRelativeToken(browsePath);
        if (rel.isEmpty()) {
            FileEntry entry;
            entry.name = ArchiveSupport::archiveFileName(path);
            entry.path = ArchiveSupport::normalizeArchivePath(path);
            entry.suffix = QFileInfo(state->sourcePath).suffix().toLower();
            entry.isDirectory = true;
            entry.sizeText = QStringLiteral("Folder");
            entry.attributesText = QStringLiteral("D");
            return entry;
        }

        const int absoluteIdx = state->pathIndex.value(rel, -1);
        if (absoluteIdx < 0 || absoluteIdx >= state->items.size()) {
            return std::nullopt;
        }
        return fileEntryFromRecord(*state, state->items.at(absoluteIdx));
    }

    ArchiveState state = stateForPath(path);
    if (!state.valid) {
        return std::nullopt;
    }

    const QString rel = archiveRelativeToken(state.browsePath);
    if (rel.isEmpty()) {
        FileEntry entry;
        entry.name = ArchiveSupport::archiveFileName(path);
        entry.path = state.currentPath;
        entry.suffix = QFileInfo(state.sourcePath).suffix().toLower();
        entry.isDirectory = true;
        entry.sizeText = QStringLiteral("Folder");
        entry.modifiedText = {};
        entry.createdText = {};
        entry.attributesText = QStringLiteral("D");
        return entry;
    }

    const int absoluteIdx = state.pathIndex.value(rel, -1);
    if (absoluteIdx < 0 || absoluteIdx >= state.items.size()) {
        return std::nullopt;
    }
    return fileEntryFromRecord(state, state.items.at(absoluteIdx));
}

std::optional<FileEntry> ArchiveFileProvider::cachedEntryInfo(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return std::nullopt;
    }

    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    auto state = cachedStateForKey(archiveCacheKey(normalized));
    if (!state || !state->valid) {
        return std::nullopt;
    }

    const QString browsePath = archiveBrowsePathForPath(normalized);
    const QString rel = archiveRelativeToken(browsePath);
    if (rel.isEmpty()) {
        FileEntry entry;
        entry.name = ArchiveSupport::archiveFileName(normalized);
        entry.path = normalized;
        entry.suffix = QFileInfo(state->sourcePath).suffix().toLower();
        entry.isDirectory = true;
        entry.sizeText = QStringLiteral("Folder");
        entry.attributesText = QStringLiteral("D");
        return entry;
    }

    const int absoluteIdx = state->pathIndex.value(rel, -1);
    if (absoluteIdx < 0 || absoluteIdx >= state->items.size()) {
        return std::nullopt;
    }
    return fileEntryFromRecord(*state, state->items.at(absoluteIdx));
}

std::optional<FileEntry> ArchiveFileProvider::entryInfoForPath(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return std::nullopt;
    }

    if (const auto cached = cachedEntryInfo(path)) {
        return cached;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    const auto library = getGlobalLibrary();
    if (!library) {
        return std::nullopt;
    }

    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    ArchiveState state = buildStateFromScratch(normalized, library, {}, true);
    if (!state.valid) {
        return std::nullopt;
    }

    auto sharedState = std::make_shared<ArchiveState>(std::move(state));
    storeStateInCache(archiveCacheKey(normalized), sharedState);

    const QString browsePath = archiveBrowsePathForPath(normalized);
    const QString rel = archiveRelativeToken(browsePath);
    if (rel.isEmpty()) {
        FileEntry entry;
        entry.name = ArchiveSupport::archiveFileName(normalized);
        entry.path = normalized;
        entry.suffix = QFileInfo(sharedState->sourcePath).suffix().toLower();
        entry.isDirectory = true;
        entry.sizeText = QStringLiteral("Folder");
        entry.attributesText = QStringLiteral("D");
        return entry;
    }

    const int absoluteIdx = sharedState->pathIndex.value(rel, -1);
    if (absoluteIdx < 0 || absoluteIdx >= sharedState->items.size()) {
        return std::nullopt;
    }
    return fileEntryFromRecord(*sharedState, sharedState->items.at(absoluteIdx));
#else
    return std::nullopt;
#endif
}

QByteArray ArchiveFileProvider::readCachedFilePrefix(const QString &path, qint64 maxEntrySize, qint64 maxBytes, bool *tooLarge)
{
    if (tooLarge) {
        *tooLarge = false;
    }
    if (!ArchiveSupport::isArchivePath(path) || maxEntrySize < 0 || maxBytes <= 0) {
        return {};
    }

    const auto entry = cachedEntryInfo(path);
    if (!entry || entry->isDirectory) {
        return {};
    }
    if (entry->size > maxEntrySize) {
        if (tooLarge) {
            *tooLarge = true;
        }
        return {};
    }

    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    auto state = cachedStateForKey(archiveCacheKey(normalized));
    if (!state || !state->valid || !state->reader) {
        return {};
    }

    auto device = openReadFromState(*state, archiveBrowsePathForPath(normalized));
    if (!device) {
        return {};
    }
    return device->read(maxBytes);
}

void ArchiveFileProvider::setCurrentThreadTemporaryParent(const QString &path)
{
    g_currentThreadTemporaryParentPath = QDir::fromNativeSeparators(path);
}
