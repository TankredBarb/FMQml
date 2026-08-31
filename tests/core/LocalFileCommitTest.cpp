#include "LocalFileCommit.h"

#include <QDebug>
#include <QFile>
#include <QTemporaryDir>

namespace {
int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}
}

int main()
{
    QTemporaryDir directory;
    if (!directory.isValid()) return fail(QStringLiteral("cannot create fixture"));

    const QString target = directory.filePath(QStringLiteral("target.bin"));
    const QString incoming = directory.filePath(QStringLiteral("target.bin.part"));
    if (!writeFile(target, "old") || !writeFile(incoming, "new")) {
        return fail(QStringLiteral("cannot create files"));
    }

    QString error;
    if (LocalFileCommit::replaceAtomically(incoming, target, &error)
        != LocalFileCommit::Result::Committed) {
        return fail(QStringLiteral("atomic replacement failed: %1").arg(error));
    }
    if (readFile(target) != QByteArrayLiteral("new") || QFile::exists(incoming)) {
        return fail(QStringLiteral("successful replacement did not commit the incoming file"));
    }

    if (!writeFile(target, "preserve")) {
        return fail(QStringLiteral("cannot reset target"));
    }
    const QString missing = directory.filePath(QStringLiteral("missing.part"));
    if (LocalFileCommit::replaceAtomically(missing, target, &error)
        != LocalFileCommit::Result::Failed) {
        return fail(QStringLiteral("replacement unexpectedly accepted a missing incoming file"));
    }
    if (readFile(target) != QByteArrayLiteral("preserve")) {
        return fail(QStringLiteral("failed replacement damaged the existing target"));
    }

    return 0;
}
