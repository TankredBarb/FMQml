#include "FileAccessResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <sys/stat.h>
#include <unistd.h>
#include <tuple>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

QVariantMap rowByLabel(const QVariantList &rows, const QString &label)
{
    for (const QVariant &rowValue : rows) {
        const QVariantMap row = rowValue.toMap();
        if (row.value(QStringLiteral("label")).toString() == label) {
            return row;
        }
    }
    return {};
}

auto accessFields(const FileAccessInfo &access)
{
    return std::make_tuple(access.canRead, access.canModify, access.canDelete,
        access.canExecute, access.canBrowse, access.canCreateChildren, access.canTraverse,
        access.canChangeAttributes, access.exact, access.readState, access.modifyState,
        access.deleteState, access.executeState, access.browseState, access.createChildrenState,
        access.traverseState, access.changeAttributesState);
}

bool accessMatchesFull(const QString &path)
{
    // Read the fast path before invalidation: it must observe permission changes immediately.
    const FileCapabilityInfo access = FileAccessResolver::resolveAccess(path);
    FileAccessResolver::invalidate(path);
    const FileCapabilityInfo full = FileAccessResolver::resolve(path);
    const FileCapabilityInfo cachedFull = FileAccessResolver::resolve(path);
    return access.path == full.path && access.exists == full.exists
        && access.isDirectory == full.isDirectory
        && accessFields(access.access) == accessFields(full.access)
        && FileAccessResolver::accessProperties(access) == FileAccessResolver::accessProperties(full)
        && FileAccessResolver::unixProperties(full) == FileAccessResolver::unixProperties(cachedFull)
        && FileAccessResolver::attributeProperties(full) == FileAccessResolver::attributeProperties(cachedFull)
        && full.accessSummary == cachedFull.accessSummary
        && full.attributesSummary == cachedFull.attributesSummary;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail(QStringLiteral("failed to create temp dir"));
    }

    const QString filePath = tempDir.filePath(QStringLiteral("sample.sh"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return fail(QStringLiteral("failed to create test file"));
    }
    file.write("#!/bin/sh\nexit 0\n");
    file.close();

    if (::chmod(QFile::encodeName(filePath).constData(), 0750) != 0) {
        return fail(QStringLiteral("failed to chmod test file"));
    }

    const FileCapabilityInfo fileInfo = FileAccessResolver::resolve(filePath);
    if (!fileInfo.unixInfo.available) {
        return fail(QStringLiteral("unix info was not populated"));
    }
    if (fileInfo.unixInfo.modeOctal != QLatin1String("750")) {
        return fail(QStringLiteral("unexpected file octal mode: %1").arg(fileInfo.unixInfo.modeOctal));
    }
    if (fileInfo.unixInfo.modeString.size() != 10 || !fileInfo.unixInfo.modeString.endsWith(QLatin1String("r-x---"))) {
        return fail(QStringLiteral("unexpected file mode string: %1").arg(fileInfo.unixInfo.modeString));
    }
    if (!fileInfo.access.canRead || !fileInfo.access.canExecute || !fileInfo.access.canDelete) {
        return fail(QStringLiteral("expected test file to be readable, executable, and deletable"));
    }

    const QVariantList unixRows = FileAccessResolver::unixProperties(fileInfo);
    if (rowByLabel(unixRows, QStringLiteral("Owner")).isEmpty()
            || rowByLabel(unixRows, QStringLiteral("Group")).isEmpty()
            || rowByLabel(unixRows, QStringLiteral("Mode")).isEmpty()
            || rowByLabel(unixRows, QStringLiteral("Octal")).value(QStringLiteral("value")).toString() != QLatin1String("750")) {
        return fail(QStringLiteral("unix property rows are incomplete"));
    }

    const QString dirPath = tempDir.filePath(QStringLiteral("folder"));
    if (!QDir().mkdir(dirPath)) {
        return fail(QStringLiteral("failed to create test directory"));
    }
    if (::chmod(QFile::encodeName(dirPath).constData(), 0700) != 0) {
        return fail(QStringLiteral("failed to chmod test directory"));
    }

    const FileCapabilityInfo dirInfo = FileAccessResolver::resolve(dirPath);
    if (!dirInfo.isDirectory || dirInfo.unixInfo.modeOctal != QLatin1String("700")) {
        return fail(QStringLiteral("unexpected directory unix info"));
    }
    if (!dirInfo.access.canBrowse || !dirInfo.access.canTraverse || !dirInfo.access.canCreateChildren) {
        return fail(QStringLiteral("expected directory browse/traverse/create access"));
    }

    const QString fileLink = tempDir.filePath(QStringLiteral("file-link"));
    const QString dirLink = tempDir.filePath(QStringLiteral("dir-link"));
    const QString missing = tempDir.filePath(QStringLiteral("missing"));
    const QString broken = tempDir.filePath(QStringLiteral("broken-link"));
    if (!QFile::link(filePath, fileLink) || !QFile::link(dirPath, dirLink)
        || !QFile::link(missing, broken)) return fail(QStringLiteral("could not create access fixtures"));
    for (const mode_t mode : {0000, 0400, 0600, 0700, 0755, 01777, 02750, 04700}) {
        if (::chmod(QFile::encodeName(filePath).constData(), mode) != 0
            || ::chmod(QFile::encodeName(dirPath).constData(), mode) != 0) {
            return fail(QStringLiteral("could not change access fixture permissions"));
        }
        for (const QString &path : {filePath, dirPath, fileLink, dirLink, missing, broken, QString{}}) {
            if (!accessMatchesFull(path)) {
                ::chmod(QFile::encodeName(dirPath).constData(), 0700);
                return fail(QStringLiteral("fast/full access differs for %1 at mode %2")
                                .arg(path).arg(mode, 0, 8));
            }
        }
    }
    ::chmod(QFile::encodeName(filePath).constData(), 0600);
    ::chmod(QFile::encodeName(dirPath).constData(), 0700);
    const QString childPath = QDir(dirPath).filePath(QStringLiteral("child"));
    QFile child(childPath);
    if (!child.open(QIODevice::WriteOnly)) return fail(QStringLiteral("could not create child"));
    child.close();
    FileAccessResolver::resolve(childPath); // Populate the full metadata cache before parent changes.
    if (::chmod(QFile::encodeName(dirPath).constData(), 0500) != 0) return fail(QStringLiteral("chmod failed"));
    const bool denied = ::geteuid() == 0 || !FileAccessResolver::resolveAccess(childPath).access.canDelete;
    const bool parentMatches = accessMatchesFull(childPath);
    const bool restored = ::chmod(QFile::encodeName(dirPath).constData(), 0700) == 0;
    if (!denied || !parentMatches || !restored
        || !FileAccessResolver::resolveAccess(childPath).access.canDelete) {
        return fail(QStringLiteral("parent permission changes were not observed correctly"));
    }

    return 0;
}
