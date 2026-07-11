#include "LinuxAdminPolicy.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

bool writeFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write("x") == 1;
}

bool expectDenied(const LinuxAdminPolicy::Decision &decision, const QString &code, const QString &label)
{
    if (!decision.allowed && decision.errorCode == code) {
        return true;
    }
    QTextStream(stderr) << label << " expected " << code << " got "
                        << (decision.allowed ? QStringLiteral("allowed") : decision.errorCode) << '\n';
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempRoot;
    if (!tempRoot.isValid()) {
        return fail(QStringLiteral("failed to create temp root"));
    }

    const QString sourcePath = QDir(tempRoot.path()).filePath(QStringLiteral("source.txt"));
    if (!writeFile(sourcePath)) {
        return fail(QStringLiteral("failed to create source"));
    }

    const QString destinationPath = QDir(tempRoot.path()).filePath(QStringLiteral("dest.txt"));
    LinuxAdminPolicy::Decision decision = LinuxAdminPolicy::validate(
        LinuxAdminPolicy::Operation::CopyFile,
        sourcePath,
        destinationPath);
    if (!decision.allowed) {
        return fail(QStringLiteral("valid copy was denied: %1").arg(decision.errorCode));
    }

    decision = LinuxAdminPolicy::validate(
        LinuxAdminPolicy::Operation::MakeDirectory,
        {},
        QDir(tempRoot.path()).filePath(QStringLiteral("created/subdir")));
    if (!decision.allowed) {
        return fail(QStringLiteral("valid mkdir was denied: %1").arg(decision.errorCode));
    }

    if (!expectDenied(LinuxAdminPolicy::validate(LinuxAdminPolicy::Operation::CopyFile,
                                                 QStringLiteral("relative.txt"),
                                                 destinationPath),
                      QStringLiteral("invalid-path"),
                      QStringLiteral("relative source"))) {
        return 1;
    }

    if (!expectDenied(LinuxAdminPolicy::validate(LinuxAdminPolicy::Operation::MakeDirectory,
                                                 {},
                                                 QStringLiteral("/proc/fmqml-test")),
                      QStringLiteral("invalid-path"),
                      QStringLiteral("pseudo filesystem"))) {
        return 1;
    }

    if (!expectDenied(LinuxAdminPolicy::validate(LinuxAdminPolicy::Operation::MakeDirectory,
                                                 {},
                                                 tempRoot.path() + QStringLiteral("|/inner")),
                      QStringLiteral("invalid-path"),
                      QStringLiteral("archive path"))) {
        return 1;
    }

    const QString inaccessibleChild = QDir(tempRoot.path()).filePath(
        QStringLiteral("locked/child.txt"));
    const LinuxAdminPolicy::Decision inaccessibleShape =
        LinuxAdminPolicy::validateSourcePathShape(inaccessibleChild);
    if (!inaccessibleShape.allowed) {
        return fail(QStringLiteral("absolute inaccessible source shape was denied: %1")
                        .arg(inaccessibleShape.errorCode));
    }
    if (!expectDenied(LinuxAdminPolicy::validateSourcePathShape(
                          QStringLiteral("relative/child.txt")),
                      QStringLiteral("invalid-path"),
                      QStringLiteral("relative inaccessible source"))) {
        return 1;
    }
    if (!expectDenied(LinuxAdminPolicy::validateSourcePathShape(
                          QStringLiteral("/proc/fmqml-test/child.txt")),
                      QStringLiteral("invalid-path"),
                      QStringLiteral("inaccessible pseudo filesystem source"))) {
        return 1;
    }

    const QString sourceLink = QDir(tempRoot.path()).filePath(QStringLiteral("source-link"));
    if (!QFile::link(sourcePath, sourceLink)) {
        return fail(QStringLiteral("failed to create source symlink"));
    }
    if (!expectDenied(LinuxAdminPolicy::validate(LinuxAdminPolicy::Operation::CopyFile,
                                                 sourceLink,
                                                 destinationPath),
                      QStringLiteral("symlink-policy-denied"),
                      QStringLiteral("source symlink"))) {
        return 1;
    }

    const QString destinationLink = QDir(tempRoot.path()).filePath(QStringLiteral("destination-link"));
    if (!QFile::link(destinationPath, destinationLink)) {
        return fail(QStringLiteral("failed to create destination symlink"));
    }
    if (!expectDenied(LinuxAdminPolicy::validate(LinuxAdminPolicy::Operation::AtomicReplace,
                                                 sourcePath,
                                                 destinationLink),
                      QStringLiteral("symlink-policy-denied"),
                      QStringLiteral("destination symlink"))) {
        return 1;
    }

    return QFileInfo(sourceLink).isSymLink() && QFileInfo(destinationLink).isSymLink()
        ? 0
        : fail(QStringLiteral("test symlinks were not detected"));
}
