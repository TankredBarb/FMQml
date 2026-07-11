#include "LinuxAdminBroker.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
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

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempRoot;
    if (!tempRoot.isValid()) {
        return fail(QStringLiteral("failed to create temp root"));
    }

    LinuxAdminBroker broker;
    if (broker.available() && broker.backendName() != QLatin1String("helper-process")) {
        return fail(QStringLiteral("unexpected default backend: %1").arg(broker.backendName()));
    }

    broker.setBackendModeForTesting(LinuxAdminBroker::BackendMode::Fake);
    if (!broker.available() || broker.backendName() != QLatin1String("fake")) {
        return fail(QStringLiteral("fake backend was not enabled"));
    }

    const QString sourcePath = QDir(tempRoot.path()).filePath(QStringLiteral("source.txt"));
    if (!writeFile(sourcePath, QByteArray("alpha"))) {
        return fail(QStringLiteral("failed to write source"));
    }

    const QString copyPath = QDir(tempRoot.path()).filePath(QStringLiteral("copy.txt"));
    LinuxAdminBroker::Request copyRequest;
    copyRequest.operationId = QStringLiteral("copy-1");
    copyRequest.sessionNonce = QStringLiteral("session-1");
    copyRequest.operation = LinuxAdminBroker::Operation::CopyFile;
    copyRequest.sourcePath = sourcePath;
    copyRequest.destinationPath = copyPath;
    const QJsonObject copyJson = LinuxAdminBroker::requestToJson(copyRequest);
    LinuxAdminBroker::Request parsedCopyRequest;
    const LinuxAdminBroker::Result parseResult = LinuxAdminBroker::requestFromJson(copyJson, &parsedCopyRequest);
    if (!parseResult.success || parsedCopyRequest.operationId != copyRequest.operationId
            || parsedCopyRequest.sessionNonce != copyRequest.sessionNonce
            || parsedCopyRequest.operation != LinuxAdminBroker::Operation::CopyFile) {
        return fail(QStringLiteral("request serialization round-trip failed: %1").arg(parseResult.errorCode));
    }
    const LinuxAdminBroker::Result resultRoundTrip = LinuxAdminBroker::resultFromJson(
        LinuxAdminBroker::resultToJson({false, QStringLiteral("sample-error"), QStringLiteral("sample message"), copyPath}));
    if (resultRoundTrip.success || resultRoundTrip.errorCode != QLatin1String("sample-error")
            || resultRoundTrip.failedPath != copyPath) {
        return fail(QStringLiteral("result serialization round-trip failed"));
    }

    const LinuxAdminBroker::Result copyResult = broker.submitBlocking(copyRequest);
    if (!copyResult.success || readFile(copyPath) != QByteArray("alpha")) {
        return fail(QStringLiteral("copy request failed: %1").arg(copyResult.errorCode));
    }

    const LinuxAdminBroker::Result duplicateResult = broker.submitBlocking(copyRequest);
    if (duplicateResult.success || duplicateResult.errorCode != QLatin1String("destination-exists")) {
        return fail(QStringLiteral("copy request should reject existing destination"));
    }

    const QString createdDir = QDir(tempRoot.path()).filePath(QStringLiteral("created/subdir"));
    LinuxAdminBroker::Request mkdirRequest;
    mkdirRequest.operationId = QStringLiteral("mkdir-1");
    mkdirRequest.sessionNonce = QStringLiteral("session-1");
    mkdirRequest.operation = LinuxAdminBroker::Operation::MakeDirectory;
    mkdirRequest.destinationPath = createdDir;
    const LinuxAdminBroker::Result mkdirResult = broker.submitBlocking(mkdirRequest);
    if (!mkdirResult.success || !QFileInfo(createdDir).isDir()) {
        return fail(QStringLiteral("mkdir request failed: %1").arg(mkdirResult.errorCode));
    }

    const QString replacementPath = QDir(tempRoot.path()).filePath(QStringLiteral("replacement.txt"));
    if (!writeFile(replacementPath, QByteArray("beta"))) {
        return fail(QStringLiteral("failed to write replacement"));
    }

    LinuxAdminBroker::Request replaceRequest;
    replaceRequest.operationId = QStringLiteral("replace-1");
    replaceRequest.sessionNonce = QStringLiteral("session-1");
    replaceRequest.operation = LinuxAdminBroker::Operation::AtomicReplace;
    replaceRequest.sourcePath = replacementPath;
    replaceRequest.destinationPath = copyPath;
    replaceRequest.overwrite = true;
    const LinuxAdminBroker::Result replaceResult = broker.submitBlocking(replaceRequest);
    if (!replaceResult.success || readFile(copyPath) != QByteArray("beta")) {
        return fail(QStringLiteral("atomic replace request failed: %1").arg(replaceResult.errorCode));
    }
    if (QFileInfo::exists(copyPath + QStringLiteral(".fm-admin-replace-part"))) {
        return fail(QStringLiteral("atomic replace left part file behind"));
    }

    LinuxAdminBroker::Request invalidRequest;
    invalidRequest.operationId = QStringLiteral("invalid-1");
    invalidRequest.sessionNonce = QStringLiteral("session-1");
    invalidRequest.operation = LinuxAdminBroker::Operation::MakeDirectory;
    invalidRequest.destinationPath = QStringLiteral("relative/path");
    const LinuxAdminBroker::Result invalidResult = broker.submitBlocking(invalidRequest);
    if (invalidResult.success || invalidResult.errorCode != QLatin1String("invalid-path")) {
        return fail(QStringLiteral("relative destination should be rejected"));
    }

    // The desktop-side broker cannot stat paths hidden behind an inaccessible
    // parent.  Structurally valid list/read requests must reach the backend;
    // the privileged helper performs the authoritative filesystem checks.
    LinuxAdminBroker::Request inaccessibleListRequest;
    inaccessibleListRequest.operationId = QStringLiteral("list-inaccessible-1");
    inaccessibleListRequest.sessionNonce = QStringLiteral("session-1");
    inaccessibleListRequest.operation = LinuxAdminBroker::Operation::ListDirectory;
    inaccessibleListRequest.sourcePath = QDir(tempRoot.path()).filePath(
        QStringLiteral("locked/missing-directory"));
    const LinuxAdminBroker::Result inaccessibleListResult =
        broker.submitBlocking(inaccessibleListRequest);
    if (inaccessibleListResult.success
            || inaccessibleListResult.errorCode != QLatin1String("unsupported")) {
        return fail(QStringLiteral("valid inaccessible list request did not reach backend: %1")
                        .arg(inaccessibleListResult.errorCode));
    }

    LinuxAdminBroker::Request inaccessibleReadRequest = inaccessibleListRequest;
    inaccessibleReadRequest.operationId = QStringLiteral("read-inaccessible-1");
    inaccessibleReadRequest.operation = LinuxAdminBroker::Operation::ReadFile;
    inaccessibleReadRequest.sourcePath = QDir(tempRoot.path()).filePath(
        QStringLiteral("locked/missing-file.txt"));
    inaccessibleReadRequest.length = 16;
    const LinuxAdminBroker::Result inaccessibleReadResult =
        broker.submitBlocking(inaccessibleReadRequest);
    if (inaccessibleReadResult.success
            || inaccessibleReadResult.errorCode != QLatin1String("unsupported")) {
        return fail(QStringLiteral("valid inaccessible read request did not reach backend: %1")
                        .arg(inaccessibleReadResult.errorCode));
    }

    LinuxAdminBroker::Request inaccessibleCopyRequest = inaccessibleReadRequest;
    inaccessibleCopyRequest.operationId = QStringLiteral("copy-inaccessible-1");
    inaccessibleCopyRequest.operation = LinuxAdminBroker::Operation::CopyFile;
    inaccessibleCopyRequest.destinationPath = QDir(tempRoot.path()).filePath(QStringLiteral("copy-target.txt"));
    const LinuxAdminBroker::Result inaccessibleCopyResult =
        broker.submitBlocking(inaccessibleCopyRequest);
    if (inaccessibleCopyResult.success
            || inaccessibleCopyResult.errorCode != QLatin1String("copy-failed")) {
        return fail(QStringLiteral("valid inaccessible copy request did not reach backend: %1")
                        .arg(inaccessibleCopyResult.errorCode));
    }

    LinuxAdminBroker::Request inaccessibleRenameRequest = inaccessibleCopyRequest;
    inaccessibleRenameRequest.operationId = QStringLiteral("rename-inaccessible-1");
    inaccessibleRenameRequest.operation = LinuxAdminBroker::Operation::RenamePath;
    inaccessibleRenameRequest.destinationPath = QDir(tempRoot.path()).filePath(QStringLiteral("renamed-target.txt"));
    const LinuxAdminBroker::Result inaccessibleRenameResult =
        broker.submitBlocking(inaccessibleRenameRequest);
    if (inaccessibleRenameResult.success
            || inaccessibleRenameResult.errorCode != QLatin1String("rename-failed")) {
        return fail(QStringLiteral("valid inaccessible rename request did not reach backend: %1")
                        .arg(inaccessibleRenameResult.errorCode));
    }

    QJsonObject wrongVersion = copyJson;
    wrongVersion.insert(QStringLiteral("protocolVersion"), LinuxAdminBroker::CurrentProtocolVersion + 1);
    LinuxAdminBroker::Request ignoredRequest;
    const LinuxAdminBroker::Result wrongVersionResult = LinuxAdminBroker::requestFromJson(wrongVersion, &ignoredRequest);
    if (wrongVersionResult.success || wrongVersionResult.errorCode != QLatin1String("protocol-mismatch")) {
        return fail(QStringLiteral("protocol mismatch should be rejected"));
    }

    return 0;
}
