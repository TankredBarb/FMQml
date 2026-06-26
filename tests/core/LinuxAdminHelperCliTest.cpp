#include "LinuxAdminBroker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

LinuxAdminBroker::Result runHelper(const QString &helperPath, const LinuxAdminBroker::Request &request)
{
    QProcess process;
    process.setProgram(helperPath);
    process.start(QIODevice::ReadWrite);
    if (!process.waitForStarted(3000)) {
        return {false, QStringLiteral("start-failed"), QStringLiteral("helper did not start"), {}};
    }

    process.write(QJsonDocument(LinuxAdminBroker::requestToJson(request)).toJson(QJsonDocument::Compact));
    process.closeWriteChannel();
    if (!process.waitForFinished(30000)) {
        process.kill();
        process.waitForFinished(1000);
        return {false, QStringLiteral("timeout"), QStringLiteral("helper timed out"), {}};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readAllStandardOutput(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {false, QStringLiteral("invalid-json"), QStringLiteral("helper returned invalid JSON"), {}};
    }
    return LinuxAdminBroker::resultFromJson(document.object());
}

LinuxAdminBroker::Result runHelperSessionRequest(const QString &helperPath, const LinuxAdminBroker::Request &request)
{
    QProcess process;
    process.setProgram(helperPath);
    process.setArguments({QStringLiteral("--session")});
    process.start(QIODevice::ReadWrite);
    if (!process.waitForStarted(3000)) {
        return {false, QStringLiteral("start-failed"), QStringLiteral("helper session did not start"), {}};
    }
    if (!process.waitForReadyRead(3000) || process.readLine().trimmed().isEmpty()) {
        process.kill();
        process.waitForFinished(1000);
        return {false, QStringLiteral("probe-missing"), QStringLiteral("helper session did not send probe"), {}};
    }

    const QByteArray requestLine = QJsonDocument(LinuxAdminBroker::requestToJson(request)).toJson(QJsonDocument::Compact) + '\n';
    process.write(requestLine);
    process.waitForBytesWritten(3000);
    if (!process.waitForReadyRead(30000)) {
        process.kill();
        process.waitForFinished(1000);
        return {false, QStringLiteral("timeout"), QStringLiteral("helper session timed out"), {}};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(process.readLine(), &parseError);
    process.closeWriteChannel();
    process.waitForFinished(1000);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {false, QStringLiteral("invalid-json"), QStringLiteral("helper session returned invalid JSON"), {}};
    }
    return LinuxAdminBroker::resultFromJson(document.object());
}

LinuxAdminBroker::Request baseRequest(const QString &operationId)
{
    LinuxAdminBroker::Request request;
    request.operationId = operationId;
    request.sessionNonce = QStringLiteral("session-1");
    return request;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString helperPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("fm-admin-helper"));
    if (!QFileInfo(helperPath).isExecutable()) {
        return fail(QStringLiteral("helper is not executable at %1").arg(helperPath));
    }

    QTemporaryDir tempRoot;
    if (!tempRoot.isValid()) {
        return fail(QStringLiteral("failed to create temp root"));
    }

    const QString createdDir = QDir(tempRoot.path()).filePath(QStringLiteral("created/subdir"));
    LinuxAdminBroker::Request mkdirRequest = baseRequest(QStringLiteral("mkdir-1"));
    mkdirRequest.operation = LinuxAdminBroker::Operation::MakeDirectory;
    mkdirRequest.destinationPath = createdDir;
    const LinuxAdminBroker::Result mkdirResult = runHelper(helperPath, mkdirRequest);
    if (!mkdirResult.success || !QFileInfo(createdDir).isDir()) {
        return fail(QStringLiteral("helper mkdir failed: %1").arg(mkdirResult.errorCode));
    }

    const QString sessionDir = QDir(tempRoot.path()).filePath(QStringLiteral("session-created"));
    LinuxAdminBroker::Request sessionMkdirRequest = baseRequest(QStringLiteral("session-mkdir-1"));
    sessionMkdirRequest.operation = LinuxAdminBroker::Operation::MakeDirectory;
    sessionMkdirRequest.destinationPath = sessionDir;
    const LinuxAdminBroker::Result sessionMkdirResult = runHelperSessionRequest(helperPath, sessionMkdirRequest);
    if (!sessionMkdirResult.success || !QFileInfo(sessionDir).isDir()) {
        return fail(QStringLiteral("helper session mkdir failed: %1").arg(sessionMkdirResult.errorCode));
    }

    const QString sourcePath = QDir(tempRoot.path()).filePath(QStringLiteral("source.txt"));
    if (!writeFile(sourcePath, QByteArray("alpha"))) {
        return fail(QStringLiteral("failed to write source"));
    }

    const QString copyPath = QDir(tempRoot.path()).filePath(QStringLiteral("copy.txt"));
    LinuxAdminBroker::Request copyRequest = baseRequest(QStringLiteral("copy-1"));
    copyRequest.operation = LinuxAdminBroker::Operation::CopyFile;
    copyRequest.sourcePath = sourcePath;
    copyRequest.destinationPath = copyPath;
    const LinuxAdminBroker::Result copyResult = runHelper(helperPath, copyRequest);
    if (!copyResult.success || readFile(copyPath) != QByteArray("alpha")) {
        return fail(QStringLiteral("helper copy failed: %1").arg(copyResult.errorCode));
    }

    const QString replacementPath = QDir(tempRoot.path()).filePath(QStringLiteral("replacement.txt"));
    if (!writeFile(replacementPath, QByteArray("beta"))) {
        return fail(QStringLiteral("failed to write replacement"));
    }

    LinuxAdminBroker::Request replaceRequest = baseRequest(QStringLiteral("replace-1"));
    replaceRequest.operation = LinuxAdminBroker::Operation::AtomicReplace;
    replaceRequest.sourcePath = replacementPath;
    replaceRequest.destinationPath = copyPath;
    replaceRequest.overwrite = true;
    const LinuxAdminBroker::Result replaceResult = runHelper(helperPath, replaceRequest);
    if (!replaceResult.success || readFile(copyPath) != QByteArray("beta")) {
        return fail(QStringLiteral("helper atomic replace failed: %1").arg(replaceResult.errorCode));
    }
    if (QFileInfo::exists(copyPath + QStringLiteral(".fm-admin-replace-part"))) {
        return fail(QStringLiteral("helper atomic replace left part file behind"));
    }

    const QString symlinkTargetPath = QDir(tempRoot.path()).filePath(QStringLiteral("symlink-target.txt"));
    if (!writeFile(symlinkTargetPath, QByteArray("target"))) {
        return fail(QStringLiteral("failed to write symlink target"));
    }
    const QString destinationLink = QDir(tempRoot.path()).filePath(QStringLiteral("destination-link"));
    if (!QFile::link(symlinkTargetPath, destinationLink)) {
        return fail(QStringLiteral("failed to create destination symlink"));
    }
    LinuxAdminBroker::Request symlinkCopyRequest = baseRequest(QStringLiteral("copy-symlink-1"));
    symlinkCopyRequest.operation = LinuxAdminBroker::Operation::CopyFile;
    symlinkCopyRequest.sourcePath = sourcePath;
    symlinkCopyRequest.destinationPath = destinationLink;
    symlinkCopyRequest.overwrite = true;
    const LinuxAdminBroker::Result symlinkCopyResult = runHelper(helperPath, symlinkCopyRequest);
    if (symlinkCopyResult.success || symlinkCopyResult.errorCode != QLatin1String("symlink-policy-denied")
            || readFile(symlinkTargetPath) != QByteArray("target")) {
        return fail(QStringLiteral("helper should reject destination symlinks"));
    }

    const QString realParent = QDir(tempRoot.path()).filePath(QStringLiteral("real-parent"));
    if (!QDir().mkpath(realParent)) {
        return fail(QStringLiteral("failed to create real parent"));
    }
    const QString parentLink = QDir(tempRoot.path()).filePath(QStringLiteral("parent-link"));
    if (!QFile::link(realParent, parentLink)) {
        return fail(QStringLiteral("failed to create parent symlink"));
    }
    LinuxAdminBroker::Request symlinkParentRequest = baseRequest(QStringLiteral("copy-symlink-parent-1"));
    symlinkParentRequest.operation = LinuxAdminBroker::Operation::CopyFile;
    symlinkParentRequest.sourcePath = sourcePath;
    symlinkParentRequest.destinationPath = QDir(parentLink).filePath(QStringLiteral("copy.txt"));
    const LinuxAdminBroker::Result symlinkParentResult = runHelper(helperPath, symlinkParentRequest);
    if (symlinkParentResult.success || symlinkParentResult.errorCode != QLatin1String("symlink-policy-denied")) {
        return fail(QStringLiteral("helper should reject symlinked destination parents"));
    }

    LinuxAdminBroker::Request invalidRequest = baseRequest(QStringLiteral("invalid-1"));
    invalidRequest.operation = LinuxAdminBroker::Operation::MakeDirectory;
    invalidRequest.destinationPath = QStringLiteral("relative/path");
    const LinuxAdminBroker::Result invalidResult = runHelper(helperPath, invalidRequest);
    if (invalidResult.success || invalidResult.errorCode != QLatin1String("invalid-path")) {
        return fail(QStringLiteral("helper should reject relative destination"));
    }

    return 0;
}
