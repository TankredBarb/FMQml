#include "LinuxAdminBroker.h"
#include "LinuxAdminPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {

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

LinuxAdminBroker::Result copyFileContentsToNewPath(const QString &sourcePath, const QString &destinationPath)
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

    QByteArray buffer(1024 * 128, Qt::Uninitialized);
    bool ok = true;
    QString errorMessage;
    while (true) {
        const ssize_t bytesRead = ::read(sourceFd, buffer.data(), static_cast<size_t>(buffer.size()));
        if (bytesRead == 0) {
            break;
        }
        if (bytesRead < 0) {
            if (errno == EINTR) {
                continue;
            }
            ok = false;
            errorMessage = errorMessageForErrno(QStringLiteral("Failed to read source file"));
            break;
        }

        ssize_t bytesWritten = 0;
        while (bytesWritten < bytesRead) {
            const ssize_t writeResult = ::write(destinationFd,
                                                buffer.constData() + bytesWritten,
                                                static_cast<size_t>(bytesRead - bytesWritten));
            if (writeResult < 0) {
                if (errno == EINTR) {
                    continue;
                }
                ok = false;
                errorMessage = errorMessageForErrno(QStringLiteral("Failed to write destination file"));
                break;
            }
            bytesWritten += writeResult;
        }
        if (!ok) {
            break;
        }
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
    }
    return LinuxAdminPolicy::Operation::CopyFile;
}

LinuxAdminBroker::Result validateRequest(const LinuxAdminBroker::Request &request)
{
    if (request.operationId.trimmed().isEmpty()) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Operation id is empty"));
    }
    if (request.sessionNonce.trimmed().isEmpty()) {
        return helperFailResult(QStringLiteral("invalid-request"), QStringLiteral("Session nonce is empty"));
    }

    const LinuxAdminPolicy::Decision policy = LinuxAdminPolicy::validate(
        helperPolicyOperationFor(request.operation),
        request.sourcePath,
        request.destinationPath);
    if (!policy.allowed) {
        return helperFailResult(policy.errorCode, policy.errorMessage, policy.failedPath);
    }
    const LinuxAdminBroker::Result symlinkPolicy = validateDestinationSymlinkPolicy(
        request.destinationPath,
        request.operation != LinuxAdminBroker::Operation::MakeDirectory);
    if (!symlinkPolicy.success) {
        return symlinkPolicy;
    }
    return helperOkResult();
}

LinuxAdminBroker::Result atomicReplaceFile(const QString &sourcePath, const QString &destinationPath, bool overwrite);

LinuxAdminBroker::Result copyFile(const QString &sourcePath, const QString &destinationPath, bool overwrite)
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
        return atomicReplaceFile(sourcePath, destination, true);
    }
    return copyFileContentsToNewPath(sourcePath, destination);
}

LinuxAdminBroker::Result atomicReplaceFile(const QString &sourcePath, const QString &destinationPath, bool overwrite)
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
    const LinuxAdminBroker::Result copyResult = copyFileContentsToNewPath(sourcePath, partPath);
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

LinuxAdminBroker::Result executeRequest(const LinuxAdminBroker::Request &request)
{
    const LinuxAdminBroker::Result validation = validateRequest(request);
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
        return copyFile(request.sourcePath, request.destinationPath, request.overwrite);
    case LinuxAdminBroker::Operation::AtomicReplace:
        return atomicReplaceFile(request.sourcePath, request.destinationPath, request.overwrite);
    }
    return helperFailResult(QStringLiteral("invalid-operation"), QStringLiteral("Invalid operation"));
}

QJsonObject probeObject()
{
    QJsonObject probe;
    probe.insert(QStringLiteral("protocolVersion"), LinuxAdminBroker::CurrentProtocolVersion);
    probe.insert(QStringLiteral("backend"), QStringLiteral("helper-process"));
    probe.insert(QStringLiteral("privileged"), ::geteuid() == 0);
    return probe;
}

void writeProbe()
{
    QTextStream out(stdout);
    out << QString::fromUtf8(QJsonDocument(probeObject()).toJson(QJsonDocument::Compact)) << '\n';
    out.flush();
}

LinuxAdminBroker::Result handleRequestBytes(const QByteArray &requestBytes)
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

    return executeRequest(request);
}

int runSession()
{
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
        writeResult(handleRequestBytes(line));
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    if (app.arguments().contains(QStringLiteral("--session"))) {
        return runSession();
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
