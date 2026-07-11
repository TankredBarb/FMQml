#include "LinuxAdminBroker.h"
#include "LinuxAdminPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
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
#include <signal.h>

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

bool helperProtocolMatches(const QString &helperPath)
{
    QProcess process;
    process.setProgram(helperPath);
    process.setArguments({QStringLiteral("--probe")});
    process.start(QIODevice::ReadOnly);
    if (!process.waitForStarted(3000) || !process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return false;
    }
    return document.object().value(QStringLiteral("protocolVersion")).toInt(-1) == LinuxAdminBroker::CurrentProtocolVersion;
}

bool resetSessionProcess();

QProcess *sessionProcess()
{
    static QProcess *process = nullptr;
    if (!process) {
        process = new QProcess;
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

struct SessionThreadContext {
    SessionThreadContext()
    {
        thread.setObjectName(QStringLiteral("LinuxAdminBrokerSession"));
        worker.moveToThread(&thread);
        thread.start();
        if (QCoreApplication::instance()) {
            QObject::connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
                             &worker, []() { resetSessionProcess(); },
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
    while (process->canReadLine() || process->waitForReadyRead(timeoutMs)) {
        const QByteArray line = process->readLine();
        if (!line.trimmed().isEmpty()) {
            if (ok) {
                *ok = true;
            }
            return line;
        }
    }
    return {};
}

bool resetSessionProcess()
{
    QProcess *process = sessionProcess();
    if (process->state() != QProcess::NotRunning) {
        process->closeWriteChannel();
        if (!process->waitForFinished(1500)) {
            process->terminate();
        }
        if (!process->waitForFinished(1500)) {
            process->kill();
        }
        if (!process->waitForFinished(3000)) {
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
    return true;
}

LinuxAdminBroker::Result ensureSessionProcess(const QString &helperPath)
{
    QProcess *process = sessionProcess();
    if (process->state() == QProcess::Running && sessionHelperPath() == helperPath && !sessionNonce().isEmpty()) {
        return okResult();
    }

    if (!resetSessionProcess()) {
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

    bool readOk = false;
    const QByteArray probeLine = readHelperLine(process, 30000, &readOk);
    if (!readOk) {
        resetSessionProcess();
        return failResult(QStringLiteral("helper-timeout"), QStringLiteral("Linux admin helper authentication timed out"));
    }

    qint64 helperPid = 0;
    const LinuxAdminBroker::Result probe = parseProbeLine(probeLine, true, &helperPid);
    if (!probe.success) {
        resetSessionProcess();
        return probe;
    }

    sessionHelperPath() = helperPath;
    sessionNonce() = nonce;
    {
        QMutexLocker locker(&sessionCancelFileMutex());
        sessionCancelFilePath() = cancelFilePath;
    }
    sessionHelperProcessId().store(helperPid);
    return okResult();
}

LinuxAdminBroker::Result submitSessionRequest(const LinuxAdminBroker::Request &request,
                                              const QString &helperPath,
                                              const LinuxAdminBroker::ProgressCallback &progress)
{
    const LinuxAdminBroker::Result session = ensureSessionProcess(helperPath);
    if (!session.success) {
        return session;
    }

    QProcess *process = sessionProcess();
    {
        QMutexLocker locker(&sessionCancelFileMutex());
        if (!sessionCancelFilePath().isEmpty()) {
            QFile::remove(sessionCancelFilePath());
        }
    }
    const QByteArray requestLine = QJsonDocument(LinuxAdminBroker::requestToJson(request)).toJson(QJsonDocument::Compact) + '\n';
    if (process->write(requestLine) != requestLine.size() || !process->waitForBytesWritten(3000)) {
        resetSessionProcess();
        return failResult(QStringLiteral("helper-failed"), QStringLiteral("Linux admin helper session could not receive the request"));
    }

    while (true) {
        bool readOk = false;
        const QByteArray responseLine = readHelperLine(process, 30000, &readOk);
        if (!readOk) {
            resetSessionProcess();
            return failResult(QStringLiteral("helper-timeout"), QStringLiteral("Linux admin helper session timed out"));
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseLine, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            resetSessionProcess();
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
        return LinuxAdminBroker::resultFromJson(object);
    }
}

} // namespace

LinuxAdminBroker::LinuxAdminBroker()
{
    for (const HelperCandidate &candidate : helperPathCandidates()) {
        const QFileInfo helperInfo(candidate.path);
        if (helperInfo.isFile() && helperInfo.isExecutable() && helperProtocolMatches(helperInfo.absoluteFilePath())) {
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
    }
    if (m_unavailableReason.isEmpty()) {
        m_unavailableReason = QStringLiteral("Linux admin helper is not installed");
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

LinuxAdminBroker::BackendMode LinuxAdminBroker::backendMode() const
{
    return m_backendMode;
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
    runInSessionThread([]() {
        resetSessionProcess();
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
    if (helperPid > 0) {
        (void)::kill(static_cast<pid_t>(helperPid), SIGTERM);
        return;
    }

    const qint64 processPid = sessionProcessId().load();
    if (processPid > 0) {
        (void)::kill(static_cast<pid_t>(processPid), SIGTERM);
    }
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
    return object;
}

LinuxAdminBroker::Result LinuxAdminBroker::resultFromJson(const QJsonObject &object)
{
    Result result;
    result.success = object.value(QStringLiteral("success")).toBool(false);
    result.errorCode = object.value(QStringLiteral("errorCode")).toString();
    result.errorMessage = object.value(QStringLiteral("errorMessage")).toString();
    result.failedPath = object.value(QStringLiteral("failedPath")).toString();
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
    if (request.operation == Operation::ChangeOwnership && request.ownerId < 0 && request.groupId < 0) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Owner or group is required"), request.sourcePath);
    }

    const LinuxAdminPolicy::Decision policy = LinuxAdminPolicy::validate(
        policyOperationFor(request.operation),
        request.sourcePath,
        request.destinationPath);
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
