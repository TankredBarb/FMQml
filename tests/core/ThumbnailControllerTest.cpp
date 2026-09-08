#include "ThumbnailController.h"
#include "FileProviderPluginRegistry.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <atomic>

namespace {
std::atomic<int> providerCalls{0};
std::atomic<bool> providerOnUiThread{false};

bool expect(bool condition, const char *message)
{
    if (!condition) QTextStream(stderr) << "FAILED: " << message << '\n';
    return condition;
}

bool waitFor(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    return condition();
}

QString diskKey(const QString &path, int size)
{
    // Preserve the existing persisted key format, including its trailing token.
    return QStringLiteral("%1::%2::%3x%4").arg(path).arg(size).arg(size);
}
}

// Link-time provider double: exercise the production controller and disk cache
// without requiring an authenticated cloud account or a plugin installation.
FileProviderPluginRegistry &FileProviderPluginRegistry::instance()
{
    static FileProviderPluginRegistry registry;
    return registry;
}

bool FileProviderPluginRegistry::hasProviderForPath(const QString &path) const
{
    return path.startsWith(QStringLiteral("test://"));
}

QString FileProviderPluginRegistry::thumbnailCacheIdentity(const QString &path) const
{
    return path;
}

ProviderThumbnailResult FileProviderPluginRegistry::thumbnailForPath(const QString &, const QSize &, QString *) const
{
    ++providerCalls;
    providerOnUiThread = QThread::currentThread() == QCoreApplication::instance()->thread();
    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::blue);
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    ProviderThumbnailResult result;
    result.kind = ProviderThumbnailResult::Kind::EncodedBytes;
    result.encodedBytes = bytes;
    return result;
}

int main(int argc, char **argv)
{
    QTemporaryDir cacheHome;
    if (!cacheHome.isValid()) return 1;
    qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());
    qunsetenv("FM_THUMBNAIL_DISK_CACHE_REMOTE");
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("ThumbnailControllerTest"));
    bool ok = true;
    const QString path = QStringLiteral("test://cached");
    ThumbnailDiskCache disk;
    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::red);
    disk.store(diskKey(path, 64), image);

    ThumbnailController controller;
    int ready = 0;
    QObject::connect(&controller, &ThumbnailController::thumbnailReady, &app, [&]() { ++ready; });
    ok &= expect(controller.providerThumbnail(path, {64, 64}).state == ThumbnailController::State::Pending,
                 "disk hit completed synchronously");
    ok &= expect(controller.providerThumbnail(path, {64, 64}).state == ThumbnailController::State::Pending,
                 "duplicate request was not pending");
    ok &= expect(waitFor([&]() { return ready == 1; }), "disk hit did not publish readiness");
    const auto cached = controller.providerThumbnail(path, {64, 64});
    ok &= expect(cached.state == ThumbnailController::State::Ready && cached.image.pixelColor(0, 0) == Qt::red,
                 "memory lookup did not return the disk image");
    ok &= expect(providerCalls == 0, "disk hit fetched provider thumbnail");

    controller.requestThumbnail(QStringLiteral("test://miss"), 64, 64);
    controller.requestThumbnail(QStringLiteral("test://miss"), 64, 64);
    ok &= expect(waitFor([&]() { return ready == 2; }), "disk miss did not complete");
    ok &= expect(providerCalls == 1 && !providerOnUiThread, "provider fetch was duplicated or ran on UI thread");
    ok &= expect(!disk.load(diskKey(QStringLiteral("test://miss"), 64)).isNull(), "provider result was not persisted");

    // Fill the 64 MiB memory cache through public requests, then revisit an
    // evicted Ready entry. Its state must not prevent a fresh disk lookup.
    QImage large(2048, 2048, QImage::Format_ARGB32);
    large.fill(Qt::green);
    for (int i = 0; i < 5; ++i) {
        const QString largePath = QStringLiteral("test://large-%1").arg(i);
        disk.store(diskKey(largePath, 2048), large);
        controller.requestThumbnail(largePath, 2048, 2048);
        ok &= expect(waitFor([&]() { return ready == 3 + i; }), "large disk image did not complete");
    }
    ok &= expect(controller.providerThumbnail(path, {64, 64}).state == ThumbnailController::State::Pending,
                 "evicted Ready entry did not restart its lookup");
    ok &= expect(waitFor([&]() { return ready == 8; }), "evicted image was not restored from disk");
    ok &= expect(providerCalls == 1, "memory eviction caused a provider fetch despite a disk hit");

    qputenv("FM_THUMBNAIL_DISK_CACHE_REMOTE", "0");
    ThumbnailController privateController;
    int privateReady = 0;
    QObject::connect(&privateController, &ThumbnailController::thumbnailReady, &app, [&]() { ++privateReady; });
    privateController.requestThumbnail(path, 64, 64);
    ok &= expect(waitFor([&]() { return privateReady == 1; }), "cache-disabled request did not complete");
    ok &= expect(providerCalls == 2 && privateController.providerThumbnail(path, {64, 64}).image.pixelColor(0, 0) == Qt::blue,
                 "disabled disk cache was still read");
    ok &= expect(disk.load(diskKey(path, 64)).pixelColor(0, 0) == Qt::red,
                 "disabled disk cache was overwritten");
    return ok ? 0 : 1;
}
