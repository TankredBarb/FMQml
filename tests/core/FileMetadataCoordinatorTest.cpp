#include "FileMetadataCoordinator.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QThread>

#include <atomic>
#include <cstdio>
#include <functional>
#include <memory>
#include <thread>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        return false;
    }
    return true;
}

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(1);
    }
    QCoreApplication::processEvents();
    return condition();
}

bool writeFile(const QString &path, const QByteArray &contents, bool append = false)
{
    QFile file(path);
    const QIODevice::OpenMode mode = QIODevice::WriteOnly | (append ? QIODevice::Append : QIODevice::Truncate);
    return file.open(mode) && file.write(contents) == contents.size();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!expect(directory.isValid(), "Could not create metadata coordinator fixture directory")) {
        return 1;
    }

    const QString path = directory.filePath(QStringLiteral("track.bin"));
    if (!expect(writeFile(path, QByteArrayLiteral("first")), "Could not create metadata coordinator fixture")) {
        return 1;
    }

    std::atomic<int> extractionCount{0};
    QSemaphore firstStarted;
    QSemaphore releaseFirst;
    FileMetadataCoordinator coordinator([&](const QString &) {
        const int count = ++extractionCount;
        if (count == 1) {
            firstStarted.release();
            releaseFirst.acquire();
        }
        return QVariantMap{{QStringLiteral("artist"), QStringLiteral("Test Artist")}};
    });

    int resultCount = 0;
    QStringList resultPaths;
    QObject::connect(&coordinator, &FileMetadataCoordinator::metadataReady,
                     [&](const QString &resultPath, const QVariantMap &metadata) {
        ++resultCount;
        resultPaths.append(resultPath);
        if (metadata.value(QStringLiteral("artist")) != QStringLiteral("Test Artist")) {
            resultCount = -100;
        }
    });

    if (!expect(QDir().mkpath(directory.filePath(QStringLiteral("sub"))),
                "Could not create alternate metadata path fixture")) {
        return 1;
    }
    const QString alternatePath = QDir(directory.path()).filePath(QStringLiteral("sub/../track.bin"));
    coordinator.request(path);
    if (!expect(firstStarted.tryAcquire(1, 1000), "First metadata extraction did not start")) {
        return 1;
    }
    coordinator.request(path);
    coordinator.request(alternatePath);
    releaseFirst.release();

    bool ok = expect(waitUntil([&]() { return resultCount == 2; }),
                     "Coalesced metadata consumers did not all receive a result")
        && expect(extractionCount.load() == 1, "Coalesced requests ran the extractor more than once")
        && expect(resultPaths.contains(path) && resultPaths.contains(alternatePath),
                  "Coalesced path variants were not preserved");

    coordinator.request(path);
    ok = expect(waitUntil([&]() { return resultCount == 3; }), "Cached metadata result was not delivered")
        && expect(extractionCount.load() == 1, "Cache hit unexpectedly reran the extractor")
        && ok;

    ok = expect(writeFile(path, QByteArrayLiteral("-changed"), true), "Could not mutate metadata fixture") && ok;
    coordinator.request(path);
    ok = expect(waitUntil([&]() { return resultCount == 4; }), "Changed file metadata was not delivered")
        && expect(extractionCount.load() == 2, "Changed size did not invalidate cached metadata")
        && ok;

    std::atomic<int> activeWorkers{0};
    std::atomic<int> maxActiveWorkers{0};
    FileMetadataCoordinator boundedCoordinator([&](const QString &) {
        const int active = ++activeWorkers;
        int observed = maxActiveWorkers.load();
        while (active > observed && !maxActiveWorkers.compare_exchange_weak(observed, active)) {
        }
        QThread::msleep(30);
        --activeWorkers;
        return QVariantMap{};
    });

    int boundedResults = 0;
    QObject::connect(&boundedCoordinator, &FileMetadataCoordinator::metadataReady,
                     [&](const QString &, const QVariantMap &) { ++boundedResults; });
    for (int i = 0; i < 6; ++i) {
        const QString workerPath = directory.filePath(QStringLiteral("worker-%1.bin").arg(i));
        ok = expect(writeFile(workerPath, QByteArray::number(i)), "Could not create bounded worker fixture") && ok;
        boundedCoordinator.request(workerPath);
    }
    ok = expect(waitUntil([&]() { return boundedResults == 6; }), "Bounded metadata requests did not finish")
        && expect(maxActiveWorkers.load() <= 2, "Metadata coordinator exceeded its worker limit")
        && expect(maxActiveWorkers.load() == 2, "Metadata coordinator did not use both workers")
        && ok;

    QSemaphore lifetimeStarted;
    QSemaphore releaseLifetime;
    auto lifetimeCoordinator = std::make_unique<FileMetadataCoordinator>([&](const QString &) {
        lifetimeStarted.release();
        releaseLifetime.acquire();
        return QVariantMap{};
    });
    lifetimeCoordinator->request(path);
    ok = expect(lifetimeStarted.tryAcquire(1, 1000), "Lifetime metadata extraction did not start") && ok;
    std::thread releaseThread([&]() {
        QThread::msleep(20);
        releaseLifetime.release();
    });
    lifetimeCoordinator.reset();
    releaseThread.join();
    QCoreApplication::processEvents();

    return ok ? 0 : 1;
}
