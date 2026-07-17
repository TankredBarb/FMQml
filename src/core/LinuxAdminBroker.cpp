#include "LinuxAdminBroker.h"
#include "LinuxAdminPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QThread>
#include <QUuid>

#include <atomic>
#include <cerrno>
#ifdef Q_OS_UNIX
#include <signal.h>
#endif

namespace {

LinuxAdminBroker::Result okResult()
{
    return {true, {}, {}, {}};
}

LinuxAdminBroker::Result failResult(const QString &code, const QString &message, const QString &path = {})
{
    return {false, code, message, path};
}

QString parentPathFor(const QString &path)
{
    return QFileInfo(path).absoluteDir().absolutePath();
}

QString helperExecutableName()
{
#ifdef Q_OS_WIN
    return QStringLiteral("fm-admin-helper.exe");
#else
    return QStringLiteral("fm-admin-helper");
#endif
}

struct HelperCandidate {
    QString path;
    bool launchWithPkexec = false;
};

QList<HelperCandidate> helperPathCandidates()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    const QString helperName = helperExecutableName();
    return {
        {appDir.filePath(helperName), false},
        {appDir.absoluteFilePath(QStringLiteral("../libexec/fm/%1").arg(helperName)), true}
    };
}

bool adminTraceEnabled()
{
    return qEnvironmentVariableIntValue("FM_ADMIN_TRACE") != 0;
}

bool helperProtocolMatches(const QString &helperPath, QString *failureReason = nullptr)
{
    QProcess process;
    process.setProgram(helperPath);
    process.setArguments({QStringLiteral("--probe")});
    process.start(QIODevice::ReadOnly);
    if (!process.waitForStarted(3000) || !process.waitForFinished(3000)) {
        if (failureReason) {
            *failureReason = process.errorString();
        }
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (failureReason) {
            *failureReason = QStringLiteral("probe exited with code %1: %2")
                .arg(process.exitCode())
                .arg(QString::fromUtf8(process.readAllStandardError()).trimmed());
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (failureReason) {
            *failureReason = QStringLiteral("invalid probe response: %1").arg(parseError.errorString());
        }
        return false;
    }
    const int version = document.object().value(QStringLiteral("protocolVersion")).toInt(-1);
    if (version != LinuxAdminBroker::CurrentProtocolVersion && failureReason) {
        *failureReason = QStringLiteral("protocol %1, expected %2")
            .arg(version)
            .arg(LinuxAdminBroker::CurrentProtocolVersion);
    }
    return version == LinuxAdminBroker::CurrentProtocolVersion;
}

bool resetSessionProcess(const char *reason);

QProcess *sessionProcess()
{
    static QProcess *process = nullptr;
    if (!process) {
        process = new QProcess;
        QProcess *const createdProcess = process;
        QObject::connect(process, &QProcess::errorOccurred, process,
                         [createdProcess](QProcess::ProcessError error) {
            if (!adminTraceEnabled()) return;
            qInfo().noquote() << "[AdminTrace] process-error"
                              << "pid=" << createdProcess->processId()
                              << "error=" << static_cast<int>(error)
                              << "state=" << static_cast<int>(createdProcess->state())
                              << "message=" << createdProcess->errorString();
        });
        QObject::connect(process,
                         qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
                         process,
                         [createdProcess](int exitCode, QProcess::ExitStatus exitStatus) {
            if (!adminTraceEnabled()) return;
            qInfo().noquote() << "[AdminTrace] process-finished"
                              << "pid=" << createdProcess->processId()
                              << "exitCode=" << exitCode
                              << "exitStatus=" << static_cast<int>(exitStatus)
                              << "stderr=" << QString::fromUtf8(createdProcess->readAllStandardError()).trimmed();
        });
    }
    return process;
}

QString &sessionHelperPath()
{
    static QString path;
    return path;
}

QString &sessionNonce()
{
    static QString nonce;
    return nonce;
}

QMutex &sessionCancelFileMutex()
{
    static QMutex mutex;
    return mutex;
}

QString &sessionCancelFilePath()
{
    static QString path;
    return path;
}

std::atomic<qint64> &sessionProcessId()
{
    static std::atomic<qint64> pid{0};
    return pid;
}

std::atomic<qint64> &sessionHelperProcessId()
{
    static std::atomic<qint64> pid{0};
    return pid;
}

std::atomic<qint64> &sessionLastActivityMs()
{
    static std::atomic<qint64> timestamp{0};
    return timestamp;
}

QByteArray &sessionReadBuffer()
{
    static QByteArray buffer;
    return buffer;
}

struct SessionThreadContext {
    SessionThreadContext()
    {
        thread.setObjectName(QStringLiteral("LinuxAdminBrokerSession"));
        worker.moveToThread(&thread);
        thread.start();
        if (QCoreApplication::instance()) {
            QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                             &worker, []() { resetSessionProcess("application-quit"); },
                             Qt::BlockingQueuedConnection);
        }
    }

    QThread thread;
    QObject worker;
};

SessionThreadContext *sessionThreadContext()
{
    static auto *context = new SessionThreadContext;
    return context;
}

template<typename Callback>
LinuxAdminBroker::Result runInSessionThread(Callback callback)
{
    SessionThreadContext *context = sessionThreadContext();
    if (QThread::currentThread() == &context->thread) {
        return callback();
    }

    LinuxAdminBroker::Result result;
    QMetaObject::invokeMethod(&context->worker, [&result, callback]() {
        result = callback();
    }, Qt::BlockingQueuedConnection);
    return result;
}

LinuxAdminBroker::Result parseProbeLine(const QByteArray &line,
                                        bool requirePrivileged,
                                        qint64 *helperPid = nullptr)
{
    if (helperPid) {
        *helperPid = 0;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failResult(QStringLiteral("invalid-response"), QStringLiteral("Linux admin helper returned invalid probe JSON"));
    }
    const QJsonObject object = document.object();
    if (object.value(QStringLiteral("protocolVersion")).toInt(-1) != LinuxAdminBroker::CurrentProtocolVersion) {
        return failResult(QStringLiteral("protocol-mismatch"), QStringLiteral("Unsupported admin helper protocol version"));
    }
    if (requirePrivileged && !object.value(QStringLiteral("privileged")).toBool(false)) {
        return failResult(QStringLiteral("authentication-failed"), QStringLiteral("Linux admin helper did not start with administrator privileges"));
    }
    if (helperPid) {
        *helperPid = object.value(QStringLiteral("pid")).toVariant().toLongLong();
    }
    return okResult();
}

QByteArray readHelperLine(QProcess *process, int timeoutMs, bool *ok)
{
    if (ok) {
        *ok = false;
    }
    QByteArray &buffer = sessionReadBuffer();
    QElapsedTimer timer;
    timer.start();
    while (true) {
        const qsizetype newline = buffer.indexOf('\n');
        if (newline >= 0) {
            const QByteArray line = buffer.left(newline + 1);
            buffer.remove(0, newline + 1);
            if (!line.trimmed().isEmpty()) {
                if (ok) *ok = true;
                return line;
            }
            continue;
        }

        const QByteArray available = process->readAllStandardOutput();
        if (!available.isEmpty()) {
            buffer.append(available);
            continue;
        }

        const int remaining = qMax(0, timeoutMs - static_cast<int>(timer.elapsed()));
        if (remaining == 0 || !process->waitForReadyRead(remaining)) {
            return {};
        }
    }
}

bool resetSessionProcess(const char *reason)
{
    QProcess *process = sessionProcess();
    if (adminTraceEnabled()) {
        qInfo().noquote() << "[AdminTrace] session-reset-begin"
                          << "reason=" << reason
                          << "state=" << static_cast<int>(process->state())
                          << "processPid=" << sessionProcessId().load()
                          << "helperPid=" << sessionHelperProcessId().load()
                          << "nonce=" << (sessionNonce().isEmpty() ? "missing" : "present");
    }
    if (process->state() != QProcess::NotRunning) {
        process->closeWriteChannel();
        if (process->state() != QProcess::NotRunning && !process->waitForFinished(1500)) {
            process->terminate();
        }
        if (process->state() != QProcess::NotRunning && !process->waitForFinished(1500)) {
            process->kill();
        }
        if (process->state() != QProcess::NotRunning && !process->waitForFinished(3000)) {
            if (adminTraceEnabled()) {
                qInfo().noquote() << "[AdminTrace] session-reset-failed"
                                  << "reason=" << reason
                                  << "state=" << static_cast<int>(process->state());
            }
            sessionHelperPath().clear();
            return false;
        }
    }
    sessionHelperPath().clear();
    sessionNonce().clear();
    {
        QMutexLocker locker(&sessionCancelFileMutex());
        if (!sessionCancelFilePath().isEmpty()) {
            QFile::remove(sessionCancelFilePath());
            sessionCancelFilePath().clear();
        }
    }
    sessionProcessId().store(0);
    sessionHelperProcessId().store(0);
    sessionLastActivityMs().store(0);
    sessionReadBuffer().clear();
    if (adminTraceEnabled()) {
        qInfo().noquote() << "[AdminTrace] session-reset-end" << "reason=" << reason;
    }
    return true;
}

LinuxAdminBroker::Result ensureSessionProcess(const QString &helperPath)
{
    QProcess *process = sessionProcess();
    if (process->state() == QProcess::Running && sessionHelperPath() == helperPath && !sessionNonce().isEmpty()) {
        return okResult();
    }

    if (!resetSessionProcess("authenticate-replace-existing")) {
        return failResult(QStringLiteral("helper-failed"), QStringLiteral("Previous Linux admin helper session could not be stopped"));
    }
    const QString nonce = QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString cancelFilePath = QDir(QDir::tempPath()).filePath(QStringLiteral("fmqml-admin-cancel-%1").arg(nonce));
    QFile::remove(cancelFilePath);
    process->setProgram(QStringLiteral("pkexec"));
    process->setArguments({helperPath, QStringLiteral("--session"), nonce, QStringLiteral("--cancel-file"), cancelFilePath});
    process->start(QIODevice::ReadWrite);
    if (!process->waitForStarted(3000)) {
        return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin helper could not be started"));
    }
    sessionProcessId().store(process->processId());
    if (adminTraceEnabled()) {
        qInfo().noquote() << "[AdminTrace] process-started"
                          << "processPid=" << sessionProcessId().load()
                          << "helper=" << helperPath;
    }

    bool readOk = false;
    const QByteArray probeLine = readHelperLine(process, 30000, &readOk);
    if (!readOk) {
        resetSessionProcess("authentication-probe-timeout");
        return failResult(QStringLiteral("helper-timeout"), QStringLiteral("Linux admin helper authentication timed out"));
    }

    qint64 helperPid = 0;
    const LinuxAdminBroker::Result probe = parseProbeLine(probeLine, true, &helperPid);
    if (!probe.success) {
        resetSessionProcess("authentication-probe-invalid");
        return probe;
    }

    sessionHelperPath() = helperPath;
    sessionNonce() = nonce;
    {
        QMutexLocker locker(&sessionCancelFileMutex());
        sessionCancelFilePath() = cancelFilePath;
    }
    sessionHelperProcessId().store(helperPid);
    sessionLastActivityMs().store(QDateTime::currentMSecsSinceEpoch());
    return okResult();
}

LinuxAdminBroker::Result submitSessionRequest(const LinuxAdminBroker::Request &request,
                                              const QString &helperPath,
                                              const LinuxAdminBroker::ProgressCallback &progress)
{
    if (adminTraceEnabled()) {
        qInfo().noquote() << "[AdminTrace] session-request"
                          << "operation=" << LinuxAdminBroker::requestToJson(request)
                                                     .value(QStringLiteral("operation")).toString()
                          << "operationId=" << request.operationId
                          << "source=" << QDir::toNativeSeparators(request.sourcePath)
                          << "destination=" << QDir::toNativeSeparators(request.destinationPath)
                          << "includeHidden=" << request.includeHidden
                          << "offset=" << request.offset
                          << "length=" << request.length;
    }
    QProcess *process = sessionProcess();
    if (process->state() != QProcess::Running
            || sessionHelperPath() != helperPath
            || sessionNonce().isEmpty()) {
        const LinuxAdminBroker::Result session = failResult(
            QStringLiteral("session-inactive"),
            QStringLiteral("Linux administrator session is not active; unlock administrator mode again"));
        if (adminTraceEnabled()) {
            qInfo().noquote() << "[AdminTrace] session-unavailable"
                              << "operationId=" << request.operationId
                              << "code=" << session.errorCode
                              << "message=" << session.errorMessage;
        }
        return session;
    }
    // A request accepted by the already-authenticated session counts as user
    // activity immediately.  This also prevents the UI idle timer from
    // revoking the helper while a longer read/materialization is in flight.
    sessionLastActivityMs().store(QDateTime::currentMSecsSinceEpoch());
    {
        QMutexLocker locker(&sessionCancelFileMutex());
        if (!sessionCancelFilePath().isEmpty()) {
            QFile::remove(sessionCancelFilePath());
        }
    }
    const QByteArray requestLine = QJsonDocument(LinuxAdminBroker::requestToJson(request)).toJson(QJsonDocument::Compact) + '\n';
    if (process->write(requestLine) != requestLine.size() || !process->waitForBytesWritten(3000)) {
        resetSessionProcess("request-write-failed");
        return failResult(QStringLiteral("helper-failed"), QStringLiteral("Linux admin helper session could not receive the request"));
    }

    while (true) {
        bool readOk = false;
        const QByteArray responseLine = readHelperLine(process, 30000, &readOk);
        if (!readOk) {
            resetSessionProcess("request-response-timeout");
            return failResult(QStringLiteral("helper-timeout"), QStringLiteral("Linux admin helper session timed out"));
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseLine, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            resetSessionProcess("request-invalid-json");
            return failResult(QStringLiteral("invalid-response"), QStringLiteral("Linux admin helper returned invalid JSON"));
        }

        const QJsonObject object = document.object();
        if (object.value(QStringLiteral("type")).toString() == QLatin1String("progress")) {
            if (progress) {
                progress(object.value(QStringLiteral("processedBytes")).toVariant().toLongLong(),
                         object.value(QStringLiteral("totalBytes")).toVariant().toLongLong());
            }
            continue;
        }
        const LinuxAdminBroker::Result result = LinuxAdminBroker::resultFromJson(object);
        if (result.success) {
            sessionLastActivityMs().store(QDateTime::currentMSecsSinceEpoch());
        }
        if (adminTraceEnabled()) {
            qInfo().noquote() << "[AdminTrace] session-result"
                              << "operationId=" << request.operationId
                              << "success=" << result.success
                              << "code=" << result.errorCode
                              << "message=" << result.errorMessage
                              << "failedPath=" << QDir::toNativeSeparators(result.failedPath)
                              << "entries=" << result.entries.size()
                              << "dataBytes=" << result.data.size()
                              << "totalSize=" << result.totalSize;
        }
        return result;
    }
}

} // namespace

LinuxAdminBroker::LinuxAdminBroker()
{
    bool installedHelperFound = false;
    for (const HelperCandidate &candidate : helperPathCandidates()) {
        const QFileInfo helperInfo(candidate.path);
        QString probeFailure;
        const bool validFile = helperInfo.isFile() && helperInfo.isExecutable();
        const bool protocolMatches = validFile && helperProtocolMatches(helperInfo.absoluteFilePath(), &probeFailure);
        if (adminTraceEnabled()) {
            qInfo().noquote() << "[AdminTrace] helper-candidate"
                              << "path=" << helperInfo.absoluteFilePath()
                              << "file=" << helperInfo.isFile()
                              << "executable=" << helperInfo.isExecutable()
                              << "installed=" << candidate.launchWithPkexec
                              << "protocolMatches=" << protocolMatches
                              << "probeFailure=" << probeFailure;
        }
        if (candidate.launchWithPkexec && validFile) {
            installedHelperFound = true;
        }
        if (protocolMatches) {
            if (m_userHelperPath.isEmpty()) {
                m_userHelperPath = helperInfo.absoluteFilePath();
            }
            if (!candidate.launchWithPkexec) {
                m_unavailableReason = QStringLiteral("Linux admin helper is a build helper and is not installed for administrator authentication");
                continue;
            }
            if (QStandardPaths::findExecutable(QStringLiteral("pkexec")).isEmpty()) {
                m_unavailableReason = QStringLiteral("pkexec is not installed");
                continue;
            }
            m_backendMode = BackendMode::HelperProcess;
            m_helperPath = helperInfo.absoluteFilePath();
            m_helperLaunchMode = candidate.launchWithPkexec ? HelperLaunchMode::Pkexec : HelperLaunchMode::Direct;
            return;
        }
        if (candidate.launchWithPkexec && validFile && !probeFailure.isEmpty()) {
            m_unavailableReason = QStringLiteral("Linux admin helper is incompatible: %1").arg(probeFailure);
        }
    }
    if (m_unavailableReason.isEmpty()) {
        m_unavailableReason = installedHelperFound
            ? QStringLiteral("Linux admin helper could not be validated")
            : QStringLiteral("Linux admin helper is not installed");
    }
}

bool LinuxAdminBroker::available() const
{
    return m_backendMode != BackendMode::Unavailable;
}

QString LinuxAdminBroker::backendName() const
{
    switch (m_backendMode) {
    case BackendMode::Fake:
        return QStringLiteral("fake");
    case BackendMode::HelperProcess:
        return QStringLiteral("helper-process");
    case BackendMode::Unavailable:
        break;
    }
    return QStringLiteral("unavailable");
}

QString LinuxAdminBroker::unavailableReason() const
{
    return m_unavailableReason;
}

void LinuxAdminBroker::setBackendModeForTesting(BackendMode mode)
{
    m_backendMode = mode;
    m_unavailableReason.clear();
    if (mode != BackendMode::HelperProcess) {
        m_helperPath.clear();
        m_helperLaunchMode = HelperLaunchMode::Direct;
    }
}

LinuxAdminBroker::Result LinuxAdminBroker::authenticateBlocking() const
{
    if (m_backendMode != BackendMode::HelperProcess || m_helperPath.isEmpty()) {
        return failResult(QStringLiteral("backend-unavailable"),
                          m_unavailableReason.isEmpty() ? QStringLiteral("Linux admin backend is unavailable") : m_unavailableReason);
    }
    if (m_helperLaunchMode != HelperLaunchMode::Pkexec) {
        return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin helper is not configured for administrator authentication"));
    }
    return runInSessionThread([helperPath = m_helperPath]() {
        return ensureSessionProcess(helperPath);
    });
}

void LinuxAdminBroker::revokeSession()
{
    if (adminTraceEnabled()) {
        qInfo().noquote() << "[AdminTrace] revoke-session-called";
    }
    runInSessionThread([]() {
        resetSessionProcess("explicit-revoke");
        return okResult();
    });
}

QString LinuxAdminBroker::activeSessionNonce()
{
    QString nonce;
    runInSessionThread([&nonce]() {
        nonce = sessionNonce();
        return okResult();
    });
    return nonce;
}

qint64 LinuxAdminBroker::lastSuccessfulSessionActivityMs()
{
    return sessionLastActivityMs().load();
}

void LinuxAdminBroker::cancelActiveSessionOperation()
{
    QString cancelFilePath;
    {
        QMutexLocker locker(&sessionCancelFileMutex());
        cancelFilePath = sessionCancelFilePath();
    }
    if (!cancelFilePath.isEmpty()) {
        QFile cancelFile(cancelFilePath);
        if (cancelFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            cancelFile.write("cancel\n");
            cancelFile.close();
        }
    }

    const qint64 helperPid = sessionHelperProcessId().load();
    if (adminTraceEnabled()) {
        qInfo().noquote() << "[AdminTrace] cancel-session-operation"
                          << "processPid=" << sessionProcessId().load()
                          << "helperPid=" << helperPid
                          << "cancelFile=" << cancelFilePath;
    }
#ifdef Q_OS_UNIX
    if (helperPid > 0) {
        const int rc = ::kill(static_cast<pid_t>(helperPid), SIGTERM);
        if (adminTraceEnabled()) {
            qInfo().noquote() << "[AdminTrace] cancel-sigterm"
                              << "target=helper" << "pid=" << helperPid
                              << "result=" << rc << "errno=" << errno;
        }
        return;
    }

    const qint64 processPid = sessionProcessId().load();
    if (processPid > 0) {
        const int rc = ::kill(static_cast<pid_t>(processPid), SIGTERM);
        if (adminTraceEnabled()) {
            qInfo().noquote() << "[AdminTrace] cancel-sigterm"
                              << "target=process" << "pid=" << processPid
                              << "result=" << rc << "errno=" << errno;
        }
    }
#endif
}

LinuxAdminBroker::Result LinuxAdminBroker::submitBlocking(const Request &request, const ProgressCallback &progress) const
{
    const Result validation = validateRequest(request, true);
    if (!validation.success) {
        return validation;
    }

    switch (m_backendMode) {
    case BackendMode::Fake:
        return submitFake(request);
    case BackendMode::HelperProcess:
        return submitHelperProcess(request, progress);
    case BackendMode::Unavailable:
        break;
    }
    return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin backend is unavailable"));
}

LinuxAdminBroker::Result LinuxAdminBroker::submitUnprivilegedBlocking(const Request &request) const
{
    const Result validation = validateRequest(request, false);
    if (!validation.success) {
        return validation;
    }
    if (request.operation != Operation::ChangeMode && request.operation != Operation::ChangeOwnership) {
        return failResult(QStringLiteral("invalid-operation"), QStringLiteral("Operation requires administrator mode"));
    }
    return submitDirectHelperProcess(request);
}

QJsonObject LinuxAdminBroker::requestToJson(const Request &request)
{
    QJsonObject object;
    object.insert(QStringLiteral("protocolVersion"), request.protocolVersion);
    object.insert(QStringLiteral("operationId"), request.operationId);
    object.insert(QStringLiteral("sessionNonce"), request.sessionNonce);
    object.insert(QStringLiteral("operation"), operationToString(request.operation));
    object.insert(QStringLiteral("sourcePath"), request.sourcePath);
    object.insert(QStringLiteral("destinationPath"), request.destinationPath);
    object.insert(QStringLiteral("overwrite"), request.overwrite);
    object.insert(QStringLiteral("preserveMetadata"), request.preserveMetadata);
    object.insert(QStringLiteral("mode"), static_cast<int>(request.mode));
    object.insert(QStringLiteral("modeMask"), static_cast<int>(request.modeMask));
    object.insert(QStringLiteral("recursive"), request.recursive);
    object.insert(QStringLiteral("ownerId"), request.ownerId);
    object.insert(QStringLiteral("groupId"), request.groupId);
    object.insert(QStringLiteral("includeHidden"), request.includeHidden);
    object.insert(QStringLiteral("offset"), request.offset);
    object.insert(QStringLiteral("length"), request.length);
    return object;
}

LinuxAdminBroker::Result LinuxAdminBroker::requestFromJson(const QJsonObject &object, Request *request)
{
    if (!request) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Request output is null"));
    }

    const QJsonValue versionValue = object.value(QStringLiteral("protocolVersion"));
    if (!versionValue.isDouble()) {
        return failResult(QStringLiteral("protocol-mismatch"), QStringLiteral("Protocol version is missing"));
    }
    const int protocolVersion = versionValue.toInt();
    if (protocolVersion != CurrentProtocolVersion) {
        return failResult(QStringLiteral("protocol-mismatch"), QStringLiteral("Unsupported admin helper protocol version"));
    }

    Operation operation = Operation::CopyFile;
    if (!operationFromString(object.value(QStringLiteral("operation")).toString(), &operation)) {
        return failResult(QStringLiteral("invalid-operation"), QStringLiteral("Invalid operation"));
    }

    Request parsed;
    parsed.protocolVersion = protocolVersion;
    parsed.operationId = object.value(QStringLiteral("operationId")).toString();
    parsed.sessionNonce = object.value(QStringLiteral("sessionNonce")).toString();
    parsed.operation = operation;
    parsed.sourcePath = object.value(QStringLiteral("sourcePath")).toString();
    parsed.destinationPath = object.value(QStringLiteral("destinationPath")).toString();
    parsed.overwrite = object.value(QStringLiteral("overwrite")).toBool(false);
    parsed.preserveMetadata = object.value(QStringLiteral("preserveMetadata")).toBool(false);
    const QJsonValue modeValue = object.value(QStringLiteral("mode"));
    if (operation == Operation::ChangeMode && (!modeValue.isDouble() || modeValue.toInt(-1) < 0)) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Mode is missing"));
    }
    parsed.mode = static_cast<quint32>(modeValue.toInt(0));
    parsed.modeMask = static_cast<quint32>(object.value(QStringLiteral("modeMask")).toInt(0));
    parsed.recursive = object.value(QStringLiteral("recursive")).toBool(false);
    parsed.ownerId = object.value(QStringLiteral("ownerId")).toVariant().toLongLong();
    parsed.groupId = object.value(QStringLiteral("groupId")).toVariant().toLongLong();
    parsed.includeHidden = object.value(QStringLiteral("includeHidden")).toBool(false);
    parsed.offset = object.value(QStringLiteral("offset")).toVariant().toLongLong();
    parsed.length = object.value(QStringLiteral("length")).toVariant().toLongLong();
    if (operation == Operation::ChangeOwnership && parsed.ownerId < 0 && parsed.groupId < 0) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Owner or group is required"));
    }
    *request = parsed;
    return okResult();
}

QJsonObject LinuxAdminBroker::resultToJson(const Result &result)
{
    QJsonObject object;
    object.insert(QStringLiteral("success"), result.success);
    object.insert(QStringLiteral("errorCode"), result.errorCode);
    object.insert(QStringLiteral("errorMessage"), result.errorMessage);
    object.insert(QStringLiteral("failedPath"), result.failedPath);
    object.insert(QStringLiteral("entries"), result.entries);
    object.insert(QStringLiteral("data"), QString::fromLatin1(result.data.toBase64()));
    object.insert(QStringLiteral("totalSize"), result.totalSize);
    return object;
}

LinuxAdminBroker::Result LinuxAdminBroker::resultFromJson(const QJsonObject &object)
{
    Result result;
    result.success = object.value(QStringLiteral("success")).toBool(false);
    result.errorCode = object.value(QStringLiteral("errorCode")).toString();
    result.errorMessage = object.value(QStringLiteral("errorMessage")).toString();
    result.failedPath = object.value(QStringLiteral("failedPath")).toString();
    result.entries = object.value(QStringLiteral("entries")).toArray();
    result.data = QByteArray::fromBase64(object.value(QStringLiteral("data")).toString().toLatin1());
    result.totalSize = object.value(QStringLiteral("totalSize")).toVariant().toLongLong();
    if (!result.success && result.errorCode.trimmed().isEmpty()) {
        return failResult(QStringLiteral("invalid-response"), QStringLiteral("Admin helper response is missing an error code"));
    }
    return result;
}

QString LinuxAdminBroker::operationToString(Operation operation)
{
    switch (operation) {
    case Operation::CopyFile:
        return QStringLiteral("copyFile");
    case Operation::MakeDirectory:
        return QStringLiteral("makeDirectory");
    case Operation::AtomicReplace:
        return QStringLiteral("atomicReplace");
    case Operation::CreateFile:
        return QStringLiteral("createFile");
    case Operation::RenamePath:
        return QStringLiteral("renamePath");
    case Operation::DeletePath:
        return QStringLiteral("deletePath");
    case Operation::ChangeMode:
        return QStringLiteral("changeMode");
    case Operation::ChangeOwnership:
        return QStringLiteral("changeOwnership");
    case Operation::ListDirectory:
        return QStringLiteral("listDirectory");
    case Operation::ReadFile:
        return QStringLiteral("readFile");
    }
    return {};
}

bool LinuxAdminBroker::operationFromString(const QString &value, Operation *operation)
{
    if (!operation) {
        return false;
    }
    if (value == QLatin1String("copyFile")) {
        *operation = Operation::CopyFile;
        return true;
    }
    if (value == QLatin1String("makeDirectory")) {
        *operation = Operation::MakeDirectory;
        return true;
    }
    if (value == QLatin1String("atomicReplace")) {
        *operation = Operation::AtomicReplace;
        return true;
    }
    if (value == QLatin1String("createFile")) {
        *operation = Operation::CreateFile;
        return true;
    }
    if (value == QLatin1String("renamePath")) {
        *operation = Operation::RenamePath;
        return true;
    }
    if (value == QLatin1String("deletePath")) {
        *operation = Operation::DeletePath;
        return true;
    }
    if (value == QLatin1String("changeMode")) {
        *operation = Operation::ChangeMode;
        return true;
    }
    if (value == QLatin1String("changeOwnership")) {
        *operation = Operation::ChangeOwnership;
        return true;
    }
    if (value == QLatin1String("listDirectory")) {
        *operation = Operation::ListDirectory;
        return true;
    }
    if (value == QLatin1String("readFile")) {
        *operation = Operation::ReadFile;
        return true;
    }
    return false;
}

LinuxAdminPolicy::Operation policyOperationFor(LinuxAdminBroker::Operation operation)
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

LinuxAdminBroker::Result LinuxAdminBroker::validateRequest(const Request &request, bool requireSession) const
{
    if (request.protocolVersion != CurrentProtocolVersion) {
        return failResult(QStringLiteral("protocol-mismatch"), QStringLiteral("Unsupported admin helper protocol version"));
    }
    if (request.operationId.trimmed().isEmpty()) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Operation id is empty"));
    }
    if (requireSession && request.sessionNonce.trimmed().isEmpty()) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Session nonce is empty"));
    }
    if (!requireSession && !request.sessionNonce.isEmpty()) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Unprivileged request must not include a session nonce"));
    }
    if (request.operation == Operation::ChangeMode && request.mode > 07777) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Mode is outside the supported range"), request.sourcePath);
    }
    if (request.recursive && (request.operation != Operation::ChangeMode || request.modeMask > 07777)) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Invalid recursive permission change"), request.sourcePath);
    }
    if (request.operation == Operation::ReadFile
            && (request.offset < 0 || request.length <= 0 || request.length > 1024 * 1024)) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Invalid admin file read range"), request.sourcePath);
    }
    if (request.operation == Operation::ChangeOwnership && request.ownerId < 0 && request.groupId < 0) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Owner or group is required"), request.sourcePath);
    }

    // A session request can address a path that the desktop process cannot
    // inspect at all. Validate only path shape here; the privileged helper is
    // responsible for existence, type and symlink validation.
    LinuxAdminPolicy::Decision policy;
    if (requireSession) {
        const bool hasSource = request.operation != Operation::MakeDirectory
            && request.operation != Operation::CreateFile;
        if (hasSource) {
            policy = LinuxAdminPolicy::validateSourcePathShape(request.sourcePath);
        }
        if (policy.allowed || !hasSource) {
            const bool hasDestination = request.operation != Operation::DeletePath
                && request.operation != Operation::ChangeMode
                && request.operation != Operation::ChangeOwnership
                && request.operation != Operation::ListDirectory
                && request.operation != Operation::ReadFile;
            if (hasDestination) {
                policy = LinuxAdminPolicy::validateDestinationPathShape(request.destinationPath);
            }
        }
    } else {
        policy = LinuxAdminPolicy::validate(policyOperationFor(request.operation),
                                            request.sourcePath,
                                            request.destinationPath);
    }
    if (!policy.allowed) {
        return failResult(policy.errorCode, policy.errorMessage, policy.failedPath);
    }

    return okResult();
}

LinuxAdminBroker::Result LinuxAdminBroker::submitFake(const Request &request) const
{
    const QString destination = QDir::cleanPath(request.destinationPath);

    switch (request.operation) {
    case Operation::MakeDirectory:
        if (!QDir().mkpath(destination)) {
            return failResult(QStringLiteral("mkdir-failed"), QStringLiteral("Failed to create destination directory"), destination);
        }
        return okResult();

    case Operation::CopyFile: {
        const QString parentPath = parentPathFor(destination);
        if (!QFileInfo(parentPath).isDir()) {
            return failResult(QStringLiteral("parent-missing"), QStringLiteral("Destination parent directory is missing"), parentPath);
        }
        if (QFileInfo::exists(destination)) {
            if (!request.overwrite) {
                return failResult(QStringLiteral("destination-exists"), QStringLiteral("Destination already exists"), destination);
            }
            if (!QFile::remove(destination)) {
                return failResult(QStringLiteral("remove-failed"), QStringLiteral("Failed to remove existing destination"), destination);
            }
        }
        if (!QFile::copy(request.sourcePath, destination)) {
            return failResult(QStringLiteral("copy-failed"), QStringLiteral("Failed to copy file"), destination);
        }
        return okResult();
    }

    case Operation::AtomicReplace: {
        const QString parentPath = parentPathFor(destination);
        if (!QFileInfo(parentPath).isDir()) {
            return failResult(QStringLiteral("parent-missing"), QStringLiteral("Destination parent directory is missing"), parentPath);
        }
        if (QFileInfo::exists(destination) && !request.overwrite) {
            return failResult(QStringLiteral("destination-exists"), QStringLiteral("Destination already exists"), destination);
        }

        const QString partPath = destination + QStringLiteral(".fm-admin-replace-part");
        QFile::remove(partPath);
        if (!QFile::copy(request.sourcePath, partPath)) {
            return failResult(QStringLiteral("copy-failed"), QStringLiteral("Failed to copy replacement file"), partPath);
        }
        if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
            QFile::remove(partPath);
            return failResult(QStringLiteral("remove-failed"), QStringLiteral("Failed to remove existing destination"), destination);
        }
        if (!QFile::rename(partPath, destination)) {
            QFile::remove(partPath);
            return failResult(QStringLiteral("rename-failed"), QStringLiteral("Failed to install replacement file"), destination);
        }
        return okResult();
    }

    case Operation::CreateFile: {
        const QString destination = QDir::cleanPath(request.destinationPath);
        const QString parentPath = parentPathFor(destination);
        if (!QFileInfo(parentPath).isDir()) {
            return failResult(QStringLiteral("parent-missing"), QStringLiteral("Destination parent directory is missing"), parentPath);
        }
        if (QFileInfo::exists(destination)) {
            return failResult(QStringLiteral("destination-exists"), QStringLiteral("Destination already exists"), destination);
        }
        QFile file(destination);
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
            return failResult(QStringLiteral("create-failed"), QStringLiteral("Failed to create file"), destination);
        }
        return okResult();
    }

    case Operation::RenamePath: {
        const QString source = QDir::cleanPath(request.sourcePath);
        const QString destination = QDir::cleanPath(request.destinationPath);
        if (QFileInfo::exists(destination)) {
            return failResult(QStringLiteral("destination-exists"), QStringLiteral("Destination already exists"), destination);
        }
        if (!QFile::rename(source, destination)) {
            return failResult(QStringLiteral("rename-failed"), QStringLiteral("Failed to rename item"), source);
        }
        return okResult();
    }

    case Operation::DeletePath: {
        const QString source = QDir::cleanPath(request.sourcePath);
        const QFileInfo info(source);
        if (!info.exists()) {
            return okResult();
        }
        if (info.isDir() && !info.isSymLink()) {
            if (!QDir().rmdir(source)) {
                return failResult(QStringLiteral("delete-failed"), QStringLiteral("Failed to remove empty directory"), source);
            }
            return okResult();
        }
        if (!QFile::remove(source)) {
            return failResult(QStringLiteral("delete-failed"), QStringLiteral("Failed to remove file"), source);
        }
        return okResult();
    }
    case Operation::ChangeMode:
        return failResult(QStringLiteral("unsupported"), QStringLiteral("Fake backend does not support changing mode"));
    case Operation::ChangeOwnership:
        return failResult(QStringLiteral("unsupported"), QStringLiteral("Fake backend does not support changing ownership"));
    case Operation::ListDirectory:
        return failResult(QStringLiteral("unsupported"), QStringLiteral("Fake backend does not support listing directories"));
    case Operation::ReadFile:
        return failResult(QStringLiteral("unsupported"), QStringLiteral("Fake backend does not support reading files"));
    }

    return failResult(QStringLiteral("invalid-operation"), QStringLiteral("Invalid operation"));
}

LinuxAdminBroker::Result LinuxAdminBroker::submitDirectHelperProcess(const Request &request) const
{
    if (m_userHelperPath.isEmpty()) {
        return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin helper is not installed"));
    }

    QProcess process;
    process.setProgram(m_userHelperPath);
    process.start(QIODevice::ReadWrite);
    if (!process.waitForStarted(3000)) {
        return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin helper could not be started"));
    }
    process.write(QJsonDocument(requestToJson(request)).toJson(QJsonDocument::Compact));
    process.closeWriteChannel();
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(1000);
        return failResult(QStringLiteral("helper-timeout"), QStringLiteral("Linux admin helper timed out"));
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failResult(QStringLiteral("invalid-response"), QStringLiteral("Linux admin helper returned invalid JSON"));
    }
    return resultFromJson(document.object());
}

LinuxAdminBroker::Result LinuxAdminBroker::submitHelperProcess(const Request &request, const ProgressCallback &progress) const
{
    if (m_helperPath.isEmpty()) {
        return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin helper path is not configured"));
    }

    QProcess process;
    if (m_helperLaunchMode == HelperLaunchMode::Pkexec) {
        const QString nonce = activeSessionNonce();
        if (nonce.isEmpty() || request.sessionNonce != nonce) {
            return failResult(QStringLiteral("session-inactive"), QStringLiteral("Linux administrator mode is not active"));
        }
        return runInSessionThread([request, helperPath = m_helperPath, progress]() {
            return submitSessionRequest(request, helperPath, progress);
        });
    } else {
        process.setProgram(m_helperPath);
    }
    process.start(QIODevice::ReadWrite);
    if (!process.waitForStarted(3000)) {
        return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin helper could not be started"));
    }

    process.write(QJsonDocument(requestToJson(request)).toJson(QJsonDocument::Compact));
    process.closeWriteChannel();

    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(1000);
        return failResult(QStringLiteral("helper-timeout"), QStringLiteral("Linux admin helper timed out"));
    }

    const QByteArray output = process.readAllStandardOutput();
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString errorText = QString::fromUtf8(process.readAllStandardError()).trimmed();
        return failResult(QStringLiteral("helper-failed"),
                          errorText.isEmpty() ? QStringLiteral("Linux admin helper failed") : errorText);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(output, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failResult(QStringLiteral("invalid-response"), QStringLiteral("Linux admin helper returned invalid JSON"));
    }
    return resultFromJson(document.object());
}
