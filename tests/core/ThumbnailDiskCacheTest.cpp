#include "ThumbnailDiskCache.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

bool expect(bool condition, const QString &message)
{
    if (!condition) {
        QTextStream(stderr) << "FAILED: " << message << '\n';
    }
    return condition;
}

QString cacheRoot()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::CacheLocation))
        .filePath(QStringLiteral("thumbnails/v1"));
}

QString pathForKey(const QString &key)
{
    const QByteArray digest = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QDir(cacheRoot()).filePath(QString::fromLatin1(digest) + QStringLiteral(".png"));
}

} // namespace

int main(int argc, char **argv)
{
    QTemporaryDir cacheHome(QDir::tempPath() + QStringLiteral("/fmqml-thumbnail-cache-test-XXXXXX"));
    if (!cacheHome.isValid()) {
        QTextStream(stderr) << "FAILED: could not create temporary cache directory\n";
        return 1;
    }
    qputenv("XDG_CACHE_HOME", cacheHome.path().toUtf8());
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("FMQmlTest"));
    QCoreApplication::setApplicationName(QStringLiteral("ThumbnailDiskCacheTest"));

    bool ok = true;
    ThumbnailDiskCache cache;

    QImage first(4, 3, QImage::Format_ARGB32);
    first.fill(QColor(20, 40, 60, 255));
    cache.store(QStringLiteral("first-key"), first);
    const QImage loaded = cache.load(QStringLiteral("first-key"));
    ok &= expect(!loaded.isNull() && loaded.size() == first.size()
                     && loaded.pixelColor(1, 1) == first.pixelColor(1, 1),
                 QStringLiteral("stored image did not round-trip"));
    ok &= expect(cache.load(QStringLiteral("different-key")).isNull(),
                 QStringLiteral("different key unexpectedly hit the cache"));

    const qsizetype storedCount = QDir(cacheRoot()).entryList({QStringLiteral("*.png")}, QDir::Files).size();
    cache.store(QString(), first);
    cache.store(QStringLiteral("null-image"), QImage());
    ok &= expect(QDir(cacheRoot()).entryList({QStringLiteral("*.png")}, QDir::Files).size() == storedCount,
                 QStringLiteral("invalid inputs created cache entries"));
    ok &= expect(QDir(cacheRoot()).entryList({QStringLiteral("*.tmp")}, QDir::Files).isEmpty(),
                 QStringLiteral("atomic store left a temporary file"));

    const QString corruptKey = QStringLiteral("corrupt-key");
    QFile corrupt(pathForKey(corruptKey));
    ok &= expect(corrupt.open(QIODevice::WriteOnly), QStringLiteral("could not create corrupt cache fixture"));
    if (corrupt.isOpen()) {
        corrupt.write("not-a-png");
        corrupt.close();
        ok &= expect(cache.load(corruptKey).isNull(), QStringLiteral("corrupt cache entry was decoded"));
        ok &= expect(!QFile::exists(pathForKey(corruptKey)),
                     QStringLiteral("corrupt cache entry was not removed"));
    }

    return ok ? 0 : 1;
}
