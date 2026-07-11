#include "LinuxAdminBroker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

#include <limits>
#include <sys/stat.h>
#include <unistd.h>

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

LinuxAdminBroker::Result runHelperSessionRequest(const QString &helperPath,
                                                 const LinuxAdminBroker::Request &request,
                                                 const QString &expectedSessionNonce = {},
                                                 const QString &cancelFilePath = {})
{
    QProcess process;
    process.setProgram(helperPath);
    QStringList arguments{QStringLiteral("--session")};
    if (!expectedSessionNonce.isEmpty()) {
        arguments.append(expectedSessionNonce);
    }
    if (!cancelFilePath.isEmpty()) {
        arguments.append(QStringLiteral("--cancel-file"));
        arguments.append(cancelFilePath);
    }
    process.setArguments(arguments);
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
    QJsonDocument document;
    while (true) {
        if (!process.waitForReadyRead(30000)) {
            process.kill();
            process.waitForFinished(1000);
            return {false, QStringLiteral("timeout"), QStringLiteral("helper session timed out"), {}};
        }

        QJsonParseError parseError;
        document = QJsonDocument::fromJson(process.readLine(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            process.closeWriteChannel();
            process.waitForFinished(1000);
            return {false, QStringLiteral("invalid-json"), QStringLiteral("helper session returned invalid JSON"), {}};
        }
        if (document.object().value(QStringLiteral("type")).toString() == QLatin1String("progress")) {
            continue;
        }
        break;
    }

    process.closeWriteChannel();
    process.waitForFinished(1000);
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

    const QString modeTarget = QDir(tempRoot.path()).filePath(QStringLiteral("mode-target"));
    if (!writeFile(modeTarget, "mode test\n")) {
        return fail(QStringLiteral("failed to create chmod target"));
    }
    LinuxAdminBroker::Request changeModeRequest = baseRequest(QStringLiteral("chmod-1"));
    changeModeRequest.operation = LinuxAdminBroker::Operation::ChangeMode;
    changeModeRequest.sourcePath = modeTarget;
    changeModeRequest.mode = 0640;
    const LinuxAdminBroker::Result changeModeResult = runHelper(helperPath, changeModeRequest);
    struct stat modeTargetStat {};
    if (!changeModeResult.success
            || ::stat(QFile::encodeName(modeTarget).constData(), &modeTargetStat) != 0
            || (modeTargetStat.st_mode & 07777) != 0640) {
        return fail(QStringLiteral("helper chmod failed: %1").arg(changeModeResult.errorCode));
    }

    LinuxAdminBroker::Request invalidModeRequest = changeModeRequest;
    invalidModeRequest.operationId = QStringLiteral("chmod-invalid");
    invalidModeRequest.mode = 010000;
    const LinuxAdminBroker::Result invalidModeResult = runHelper(helperPath, invalidModeRequest);
    if (invalidModeResult.success || invalidModeResult.errorCode != QLatin1String("invalid-request")) {
        return fail(QStringLiteral("helper should reject invalid chmod mode"));
    }

    LinuxAdminBroker::Request changeGroupRequest = baseRequest(QStringLiteral("chown-group-1"));
    changeGroupRequest.operation = LinuxAdminBroker::Operation::ChangeOwnership;
    changeGroupRequest.sourcePath = modeTarget;
    changeGroupRequest.groupId = static_cast<qint64>(::getgid());
    const LinuxAdminBroker::Result changeGroupResult = runHelper(helperPath, changeGroupRequest);
    if (!changeGroupResult.success) {
        return fail(QStringLiteral("helper chgrp failed: %1").arg(changeGroupResult.errorCode));
    }

    LinuxAdminBroker::Request overflowOwnerRequest = changeGroupRequest;
    overflowOwnerRequest.operationId = QStringLiteral("chown-overflow-1");
    overflowOwnerRequest.ownerId = std::numeric_limits<qint64>::max();
    const LinuxAdminBroker::Result overflowOwnerResult = runHelper(helperPath, overflowOwnerRequest);
    if (overflowOwnerResult.success || overflowOwnerResult.errorCode != QLatin1String("invalid-request")) {
        return fail(QStringLiteral("helper should reject an overflowing owner id"));
    }

    const QString specialDirectory = QDir(tempRoot.path()).filePath(QStringLiteral("special-directory"));
    if (!QDir().mkdir(specialDirectory)) {
        return fail(QStringLiteral("failed to create special mode directory"));
    }
    LinuxAdminBroker::Request specialModeRequest = baseRequest(QStringLiteral("chmod-special-1"));
    specialModeRequest.operation = LinuxAdminBroker::Operation::ChangeMode;
    specialModeRequest.sourcePath = specialDirectory;
    specialModeRequest.mode = 01700;
    const LinuxAdminBroker::Result specialModeResult = runHelper(helperPath, specialModeRequest);
    struct stat specialDirectoryStat {};
    if (!specialModeResult.success
            || ::stat(QFile::encodeName(specialDirectory).constData(), &specialDirectoryStat) != 0
            || (specialDirectoryStat.st_mode & 07777) != 01700) {
        return fail(QStringLiteral("helper special chmod failed: %1").arg(specialModeResult.errorCode));
    }

    const QString recursiveDirectory = QDir(tempRoot.path()).filePath(QStringLiteral("recursive-directory"));
    const QString recursiveFile = QDir(recursiveDirectory).filePath(QStringLiteral("child.txt"));
    if (!QDir().mkdir(recursiveDirectory) || !writeFile(recursiveFile, "child\n")
            || ::chmod(QFile::encodeName(recursiveDirectory).constData(), 0755) != 0
            || ::chmod(QFile::encodeName(recursiveFile).constData(), 0644) != 0) {
        return fail(QStringLiteral("failed to prepare recursive chmod test"));
    }
    LinuxAdminBroker::Request recursiveModeRequest = baseRequest(QStringLiteral("chmod-recursive-1"));
    recursiveModeRequest.operation = LinuxAdminBroker::Operation::ChangeMode;
    recursiveModeRequest.sourcePath = recursiveDirectory;
    recursiveModeRequest.mode = 0700;
    recursiveModeRequest.modeMask = 0055;
    recursiveModeRequest.recursive = true;
    const LinuxAdminBroker::Result recursiveModeResult = runHelper(helperPath, recursiveModeRequest);
    struct stat recursiveDirectoryStat {};
    struct stat recursiveFileStat {};
    if (!recursiveModeResult.success
            || ::stat(QFile::encodeName(recursiveDirectory).constData(), &recursiveDirectoryStat) != 0
            || ::stat(QFile::encodeName(recursiveFile).constData(), &recursiveFileStat) != 0
            || (recursiveDirectoryStat.st_mode & 07777) != 0700
            || (recursiveFileStat.st_mode & 07777) != 0600) {
        return fail(QStringLiteral("helper recursive chmod failed: %1").arg(recursiveModeResult.errorCode));
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

    const QString nonceDir = QDir(tempRoot.path()).filePath(QStringLiteral("nonce-created"));
    LinuxAdminBroker::Request nonceMkdirRequest = baseRequest(QStringLiteral("session-nonce-mkdir-1"));
    nonceMkdirRequest.operation = LinuxAdminBroker::Operation::MakeDirectory;
    nonceMkdirRequest.destinationPath = nonceDir;
    const LinuxAdminBroker::Result nonceMkdirResult = runHelperSessionRequest(
        helperPath,
        nonceMkdirRequest,
        nonceMkdirRequest.sessionNonce);
    if (!nonceMkdirResult.success || !QFileInfo(nonceDir).isDir()) {
        return fail(QStringLiteral("helper session nonce mkdir failed: %1").arg(nonceMkdirResult.errorCode));
    }

    LinuxAdminBroker::Request wrongNonceRequest = baseRequest(QStringLiteral("session-wrong-nonce-1"));
    wrongNonceRequest.sessionNonce = QStringLiteral("wrong-session");
    wrongNonceRequest.operation = LinuxAdminBroker::Operation::MakeDirectory;
    wrongNonceRequest.destinationPath = QDir(tempRoot.path()).filePath(QStringLiteral("wrong-nonce-created"));
    const LinuxAdminBroker::Result wrongNonceResult = runHelperSessionRequest(
        helperPath,
        wrongNonceRequest,
        QStringLiteral("session-1"));
    if (wrongNonceResult.success || wrongNonceResult.errorCode != QLatin1String("session-inactive")) {
        return fail(QStringLiteral("helper session should reject wrong nonce"));
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

    const QString cancelMarkerPath = QDir(tempRoot.path()).filePath(QStringLiteral("cancel-marker"));
    if (!writeFile(cancelMarkerPath, QByteArray("cancel\n"))) {
        return fail(QStringLiteral("failed to create cancel marker"));
    }
    const QString canceledCopyPath = QDir(tempRoot.path()).filePath(QStringLiteral("canceled-copy.txt"));
    LinuxAdminBroker::Request canceledCopyRequest = baseRequest(QStringLiteral("session-cancel-copy-1"));
    canceledCopyRequest.operation = LinuxAdminBroker::Operation::CopyFile;
    canceledCopyRequest.sourcePath = sourcePath;
    canceledCopyRequest.destinationPath = canceledCopyPath;
    const LinuxAdminBroker::Result canceledCopyResult = runHelperSessionRequest(
        helperPath,
        canceledCopyRequest,
        canceledCopyRequest.sessionNonce,
        cancelMarkerPath);
    if (canceledCopyResult.success
            || canceledCopyResult.errorCode != QLatin1String("operation-canceled")
            || QFileInfo::exists(canceledCopyPath)) {
        return fail(QStringLiteral("helper session copy should honor cancel marker and remove partial output"));
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

    const QString recursiveRoot = QDir(tempRoot.path()).filePath(QStringLiteral("recursive-delete"));
    if (!QDir().mkpath(QDir(recursiveRoot).filePath(QStringLiteral("nested/deeper")))) {
        return fail(QStringLiteral("failed to create recursive delete tree"));
    }
    if (!writeFile(QDir(recursiveRoot).filePath(QStringLiteral("payload.txt")), QByteArray("payload"))
            || !writeFile(QDir(recursiveRoot).filePath(QStringLiteral("nested/deeper/payload.txt")), QByteArray("nested"))) {
        return fail(QStringLiteral("failed to populate recursive delete tree"));
    }
    LinuxAdminBroker::Request recursiveDeleteRequest = baseRequest(QStringLiteral("delete-recursive-1"));
    recursiveDeleteRequest.operation = LinuxAdminBroker::Operation::DeletePath;
    recursiveDeleteRequest.sourcePath = recursiveRoot;
    const LinuxAdminBroker::Result recursiveDeleteResult = runHelper(helperPath, recursiveDeleteRequest);
    if (!recursiveDeleteResult.success || QFileInfo::exists(recursiveRoot)) {
        return fail(QStringLiteral("helper recursive delete failed: %1").arg(recursiveDeleteResult.errorCode));
    }

    const QString cancelDeleteRoot = QDir(tempRoot.path()).filePath(QStringLiteral("cancel-delete"));
    if (!QDir().mkpath(QDir(cancelDeleteRoot).filePath(QStringLiteral("nested")))) {
        return fail(QStringLiteral("failed to create cancel delete tree"));
    }
    if (!writeFile(QDir(cancelDeleteRoot).filePath(QStringLiteral("payload.txt")), QByteArray("payload"))
            || !writeFile(QDir(cancelDeleteRoot).filePath(QStringLiteral("nested/payload.txt")), QByteArray("nested"))) {
        return fail(QStringLiteral("failed to populate cancel delete tree"));
    }
    const QString cancelDeleteMarkerPath = QDir(tempRoot.path()).filePath(QStringLiteral("cancel-delete-marker"));
    if (!writeFile(cancelDeleteMarkerPath, QByteArray("cancel\n"))) {
        return fail(QStringLiteral("failed to create cancel delete marker"));
    }
    LinuxAdminBroker::Request cancelDeleteRequest = baseRequest(QStringLiteral("session-cancel-delete-1"));
    cancelDeleteRequest.operation = LinuxAdminBroker::Operation::DeletePath;
    cancelDeleteRequest.sourcePath = cancelDeleteRoot;
    const LinuxAdminBroker::Result cancelDeleteResult = runHelperSessionRequest(
        helperPath,
        cancelDeleteRequest,
        cancelDeleteRequest.sessionNonce,
        cancelDeleteMarkerPath);
    if (cancelDeleteResult.success
            || cancelDeleteResult.errorCode != QLatin1String("operation-canceled")
            || !QFileInfo::exists(cancelDeleteRoot)) {
        return fail(QStringLiteral("helper session recursive delete should honor cancel marker"));
    }

    const QString symlinkDeleteRoot = QDir(tempRoot.path()).filePath(QStringLiteral("recursive-delete-symlink"));
    if (!QDir().mkpath(symlinkDeleteRoot)) {
        return fail(QStringLiteral("failed to create symlink delete root"));
    }
    const QString outsideSymlinkTarget = QDir(tempRoot.path()).filePath(QStringLiteral("outside-symlink-target.txt"));
    if (!writeFile(outsideSymlinkTarget, QByteArray("outside"))) {
        return fail(QStringLiteral("failed to create outside symlink target"));
    }
    const QString childSymlink = QDir(symlinkDeleteRoot).filePath(QStringLiteral("child-link"));
    if (!QFile::link(outsideSymlinkTarget, childSymlink)) {
        return fail(QStringLiteral("failed to create child symlink"));
    }
    LinuxAdminBroker::Request symlinkDeleteRequest = baseRequest(QStringLiteral("delete-recursive-symlink-1"));
    symlinkDeleteRequest.operation = LinuxAdminBroker::Operation::DeletePath;
    symlinkDeleteRequest.sourcePath = symlinkDeleteRoot;
    const LinuxAdminBroker::Result symlinkDeleteResult = runHelper(helperPath, symlinkDeleteRequest);
    if (!symlinkDeleteResult.success
            || QFileInfo::exists(symlinkDeleteRoot)
            || readFile(outsideSymlinkTarget) != QByteArray("outside")) {
        return fail(QStringLiteral("helper recursive delete should remove child symlink without touching target: %1")
                        .arg(symlinkDeleteResult.errorCode));
    }

    return 0;
}
