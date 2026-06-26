#include "LinuxAdminBroker.h"
#include "LinuxAdminPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QMetaObject>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QThread>

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

LinuxAdminBroker::Result validateProbeLine(const QByteArray &line, bool requirePrivileged)
{
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
    return true;
}

LinuxAdminBroker::Result ensureSessionProcess(const QString &helperPath)
{
    QProcess *process = sessionProcess();
    if (process->state() == QProcess::Running && sessionHelperPath() == helperPath) {
        return okResult();
    }

    if (!resetSessionProcess()) {
        return failResult(QStringLiteral("helper-failed"), QStringLiteral("Previous Linux admin helper session could not be stopped"));
    }
    process->setProgram(QStringLiteral("pkexec"));
    process->setArguments({helperPath, QStringLiteral("--session")});
    process->start(QIODevice::ReadWrite);
    if (!process->waitForStarted(3000)) {
        return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin helper could not be started"));
    }

    bool readOk = false;
    const QByteArray probeLine = readHelperLine(process, 30000, &readOk);
    if (!readOk) {
        resetSessionProcess();
        return failResult(QStringLiteral("helper-timeout"), QStringLiteral("Linux admin helper authentication timed out"));
    }

    const LinuxAdminBroker::Result probe = validateProbeLine(probeLine, true);
    if (!probe.success) {
        resetSessionProcess();
        return probe;
    }

    sessionHelperPath() = helperPath;
    return okResult();
}

LinuxAdminBroker::Result submitSessionRequest(const LinuxAdminBroker::Request &request, const QString &helperPath)
{
    const LinuxAdminBroker::Result session = ensureSessionProcess(helperPath);
    if (!session.success) {
        return session;
    }

    QProcess *process = sessionProcess();
    const QByteArray requestLine = QJsonDocument(LinuxAdminBroker::requestToJson(request)).toJson(QJsonDocument::Compact) + '\n';
    if (process->write(requestLine) != requestLine.size() || !process->waitForBytesWritten(3000)) {
        resetSessionProcess();
        return failResult(QStringLiteral("helper-failed"), QStringLiteral("Linux admin helper session could not receive the request"));
    }

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
    return LinuxAdminBroker::resultFromJson(document.object());
}

} // namespace

LinuxAdminBroker::LinuxAdminBroker()
{
    for (const HelperCandidate &candidate : helperPathCandidates()) {
        const QFileInfo helperInfo(candidate.path);
        if (helperInfo.isFile() && helperInfo.isExecutable() && helperProtocolMatches(helperInfo.absoluteFilePath())) {
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

LinuxAdminBroker::Result LinuxAdminBroker::submitBlocking(const Request &request) const
{
    const Result validation = validateRequest(request);
    if (!validation.success) {
        return validation;
    }

    switch (m_backendMode) {
    case BackendMode::Fake:
        return submitFake(request);
    case BackendMode::HelperProcess:
        return submitHelperProcess(request);
    case BackendMode::Unavailable:
        break;
    }
    return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin backend is unavailable"));
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
    }
    return LinuxAdminPolicy::Operation::CopyFile;
}

LinuxAdminBroker::Result LinuxAdminBroker::validateRequest(const Request &request) const
{
    if (request.protocolVersion != CurrentProtocolVersion) {
        return failResult(QStringLiteral("protocol-mismatch"), QStringLiteral("Unsupported admin helper protocol version"));
    }
    if (request.operationId.trimmed().isEmpty()) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Operation id is empty"));
    }
    if (request.sessionNonce.trimmed().isEmpty()) {
        return failResult(QStringLiteral("invalid-request"), QStringLiteral("Session nonce is empty"));
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
    }

    return failResult(QStringLiteral("invalid-operation"), QStringLiteral("Invalid operation"));
}

LinuxAdminBroker::Result LinuxAdminBroker::submitHelperProcess(const Request &request) const
{
    if (m_helperPath.isEmpty()) {
        return failResult(QStringLiteral("backend-unavailable"), QStringLiteral("Linux admin helper path is not configured"));
    }

    QProcess process;
    if (m_helperLaunchMode == HelperLaunchMode::Pkexec) {
        return runInSessionThread([request, helperPath = m_helperPath]() {
            return submitSessionRequest(request, helperPath);
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
