#include "DirectoryChangeWatcher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTimer>

#include <memory>

namespace {
int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

bool waitUntil(int timeoutMs, const std::function<bool()> &condition)
{
    QEventLoop loop;
    QTimer timeout;
    QTimer poll;

    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (condition()) {
            loop.quit();
        }
    });

    timeout.start(timeoutMs);
    poll.start(20);
    if (!condition()) {
        loop.exec();
    }
    return condition();
}

bool writeFile(const QString &path, const QByteArray &data, QIODevice::OpenMode mode = QIODevice::WriteOnly)
{
    QFile file(path);
    if (!file.open(mode)) {
        return false;
    }
    return file.write(data) == data.size();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    auto watcher = createDirectoryChangeWatcher(&app);
    bool missingPathFailed = false;
    QObject::connect(watcher.get(), &DirectoryChangeWatcher::watchFailed,
                     &app, [&](const QString &, const QString &) {
        missingPathFailed = true;
    });

    if (watcher->watch(QStringLiteral("/tmp/fm-missing-watch-path-for-test"))) {
        return fail(QStringLiteral("watch() unexpectedly accepted a missing path"));
    }
    if (!missingPathFailed) {
        return fail(QStringLiteral("watchFailed was not emitted for a missing path"));
    }

    QTemporaryDir dir;
    if (!dir.isValid()) {
        return fail(QStringLiteral("could not create temporary directory"));
    }

    QSet<DirectoryChangeEvent::Type> seenTypes;
    QString renamedOldPath;
    QString renamedNewPath;
    QObject::connect(watcher.get(), &DirectoryChangeWatcher::eventsReady,
                     &app, [&](const QList<DirectoryChangeEvent> &events) {
        for (const DirectoryChangeEvent &event : events) {
            seenTypes.insert(event.type);
            if (event.type == DirectoryChangeEvent::Type::Renamed) {
                renamedOldPath = event.oldPath;
                renamedNewPath = event.newPath;
            }
        }
    });

    if (!watcher->watch(dir.path())) {
        return fail(QStringLiteral("watch() failed for a valid temporary directory"));
    }

    const QString createdPath = dir.filePath(QStringLiteral("created.txt"));
    const QString renamedPath = dir.filePath(QStringLiteral("renamed.txt"));
    if (!writeFile(createdPath, QByteArrayLiteral("one"))) {
        return fail(QStringLiteral("could not create watched file"));
    }
    if (!writeFile(createdPath, QByteArrayLiteral("two"), QIODevice::WriteOnly | QIODevice::Append)) {
        return fail(QStringLiteral("could not modify watched file"));
    }
    if (!QFile::rename(createdPath, renamedPath)) {
        return fail(QStringLiteral("could not rename watched file"));
    }
    if (!QFile::remove(renamedPath)) {
        return fail(QStringLiteral("could not remove watched file"));
    }

    const bool observed = waitUntil(1500, [&]() {
        return seenTypes.contains(DirectoryChangeEvent::Type::Added)
            && seenTypes.contains(DirectoryChangeEvent::Type::Modified)
            && seenTypes.contains(DirectoryChangeEvent::Type::Renamed)
            && seenTypes.contains(DirectoryChangeEvent::Type::Removed);
    });

    if (!observed) {
        return fail(QStringLiteral("did not observe add/modify/rename/remove watcher events"));
    }
    if (renamedOldPath != createdPath || renamedNewPath != renamedPath) {
        return fail(QStringLiteral("rename event did not preserve old/new paths"));
    }

    auto removedDirectoryWatcher = createDirectoryChangeWatcher(&app);
    bool removedDirectoryFailed = false;
    QString removedDirectoryFailedPath;
    QObject::connect(removedDirectoryWatcher.get(), &DirectoryChangeWatcher::watchFailed,
                     &app, [&](const QString &path, const QString &) {
        removedDirectoryFailed = true;
        removedDirectoryFailedPath = path;
    });

    QTemporaryDir parentDir;
    if (!parentDir.isValid()) {
        return fail(QStringLiteral("could not create parent temporary directory"));
    }
    const QString removedDirectoryPath = parentDir.filePath(QStringLiteral("removed"));
    if (!QDir(parentDir.path()).mkpath(QStringLiteral("removed"))) {
        return fail(QStringLiteral("could not create directory for self-delete test"));
    }
    if (!removedDirectoryWatcher->watch(removedDirectoryPath)) {
        return fail(QStringLiteral("watch() failed for directory self-delete test"));
    }
    if (!QDir(removedDirectoryPath).removeRecursively()) {
        return fail(QStringLiteral("could not remove watched directory"));
    }

    if (!waitUntil(1500, [&]() { return removedDirectoryFailed; })) {
        return fail(QStringLiteral("watchFailed was not emitted when watched directory was removed"));
    }
    if (removedDirectoryFailedPath != removedDirectoryPath) {
        return fail(QStringLiteral("watchFailed path did not match removed watched directory"));
    }

    auto parentDirectoryWatcher = createDirectoryChangeWatcher(&app);
    bool childDirectoryRemoved = false;
    QObject::connect(parentDirectoryWatcher.get(), &DirectoryChangeWatcher::eventsReady,
                     &app, [&](const QList<DirectoryChangeEvent> &events) {
        for (const DirectoryChangeEvent &event : events) {
            if (event.type == DirectoryChangeEvent::Type::Removed
                && event.path == removedDirectoryPath) {
                childDirectoryRemoved = true;
            }
        }
    });

    if (!QDir(parentDir.path()).mkpath(QStringLiteral("removed"))) {
        return fail(QStringLiteral("could not recreate directory for parent removal test"));
    }
    if (!parentDirectoryWatcher->watch(parentDir.path())) {
        return fail(QStringLiteral("watch() failed for parent removal test"));
    }
    if (!QDir(removedDirectoryPath).removeRecursively()) {
        return fail(QStringLiteral("could not remove child directory"));
    }

    if (!waitUntil(1500, [&]() { return childDirectoryRemoved; })) {
        return fail(QStringLiteral("parent watcher did not report removed child directory"));
    }

    return 0;
}
