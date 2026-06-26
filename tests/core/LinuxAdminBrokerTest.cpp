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

    QJsonObject wrongVersion = copyJson;
    wrongVersion.insert(QStringLiteral("protocolVersion"), LinuxAdminBroker::CurrentProtocolVersion + 1);
    LinuxAdminBroker::Request ignoredRequest;
    const LinuxAdminBroker::Result wrongVersionResult = LinuxAdminBroker::requestFromJson(wrongVersion, &ignoredRequest);
    if (wrongVersionResult.success || wrongVersionResult.errorCode != QLatin1String("protocol-mismatch")) {
        return fail(QStringLiteral("protocol mismatch should be rejected"));
    }

    return 0;
}
