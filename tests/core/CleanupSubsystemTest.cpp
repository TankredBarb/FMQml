#include "CleanupSubsystem.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <functional>
#include <iostream>

namespace {
bool waitFor(std::function<bool()> predicate, int timeoutMs = 5000)
{
    const qint64 deadline = QDateTime::currentMSecsSinceEpoch() + timeoutMs;
    while (QDateTime::currentMSecsSinceEpoch() < deadline) {
        if (predicate()) {
            return true;
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
        QThread::msleep(25);
    }
    return predicate();
}

bool writeFile(const QString &path, const QByteArray &data = "x")
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    return file.write(data) == data.size();
}

bool fail(const char *message)
{
    std::cerr << message << '\n';
    return false;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("FMQmlCleanupTest"));
    QCoreApplication::setOrganizationName(QStringLiteral("FMQmlTest"));

    bool ok = true;

    QTemporaryDir tempRoot;
    ok &= tempRoot.isValid() || fail("temp root is invalid");

    const QString defaultRoot = StagingLocationPolicy::defaultCleanupRoot();
    ok &= !defaultRoot.isEmpty() || fail("default cleanup root is empty");
    ok &= !defaultRoot.startsWith(QCoreApplication::applicationDirPath()) || fail("default cleanup root uses application dir");

    QString leaseId;
    const QString staging = CleanupSubsystem::instance().allocateStagingDirectory(
        CleanupArtifactKind::ProviderTransfer,
        tempRoot.path(),
        QStringLiteral("test-op"),
        &leaseId);
    ok &= !staging.isEmpty() || fail("allocateStagingDirectory returned empty");
    ok &= QFileInfo::exists(QDir(staging).filePath(QStringLiteral(".fm-cleanup-owner.json"))) || fail("owner marker missing");
    ok &= writeFile(QDir(staging).filePath(QStringLiteral("payload.bin"))) || fail("cannot write payload");
    CleanupSubsystem::instance().scheduleDelete(leaseId);
    ok &= waitFor([&]() { return !QFileInfo::exists(staging); }) || fail("staging dir was not deleted");

    const QString outside = QDir(tempRoot.path()).filePath(QStringLiteral("outside.bin"));
    ok &= writeFile(outside) || fail("cannot write outside file");
    QString rejectedLease;
    const QString rejected = CleanupSubsystem::instance().registerArtifact(
        CleanupArtifactKind::PartFile,
        outside,
        QDir(tempRoot.path()).filePath(QStringLiteral("different-root")),
        false,
        &rejectedLease);
    ok &= rejected.isEmpty() || fail("outside artifact was registered");
    CleanupSubsystem::instance().scheduleDeleteOnFailure(rejectedLease);
    QCoreApplication::processEvents();
    ok &= QFileInfo::exists(outside) || fail("outside file was deleted");

    const QString partFile = QDir(tempRoot.path()).filePath(QStringLiteral("target.bin.part"));
    ok &= writeFile(partFile) || fail("cannot write part file");
    QString partLease;
    ok &= !CleanupSubsystem::instance().registerArtifact(
              CleanupArtifactKind::PartFile,
              partFile,
              tempRoot.path(),
              false,
              &partLease).isEmpty() || fail("part artifact not registered");
    CleanupSubsystem::instance().completeWithoutDelete(partLease);
    QCoreApplication::processEvents();
    ok &= QFileInfo::exists(partFile) || fail("completeWithoutDelete removed part file");
    QFile::remove(partFile);

    const QString recursiveDir = QDir(tempRoot.path()).filePath(QStringLiteral("recursive"));
    ok &= QDir().mkpath(recursiveDir) || fail("cannot create recursive dir");
    ok &= writeFile(QDir(recursiveDir).filePath(QStringLiteral("payload.bin"))) || fail("cannot write recursive payload");
    QString recursiveLease;
    ok &= !CleanupSubsystem::instance().registerArtifact(
              CleanupArtifactKind::ArchiveExtract,
              recursiveDir,
              tempRoot.path(),
              true,
              &recursiveLease).isEmpty() || fail("recursive artifact not registered");
    ok &= QFileInfo::exists(QDir(recursiveDir).filePath(QStringLiteral(".fm-cleanup-owner.json"))) || fail("recursive marker missing");
    CleanupSubsystem::instance().scheduleDeleteOnFailure(recursiveLease);
    ok &= waitFor([&]() { return !QFileInfo::exists(recursiveDir); }) || fail("recursive dir was not deleted");

    return ok ? 0 : 1;
}
