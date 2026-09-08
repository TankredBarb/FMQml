#include "ThumbnailDiskCache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QHash>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {
constexpr qint64 kDiskCacheLimitBytes = 256LL * 1024 * 1024;
constexpr qint64 kDiskCacheTargetBytes = 240LL * 1024 * 1024;
constexpr int kCacheVersion = 1;
struct CacheAccounting {
    qint64 bytes = 0;
    QElapsedTimer lastScan;
};
// Local and provider thumbnails share the same on-disk namespace.
QMutex cacheMutex;
QHash<QString, CacheAccounting> cacheAccounting;
}

ThumbnailDiskCache::ThumbnailDiskCache()
{
    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
    }
    if (!cacheRoot.isEmpty()) {
        m_root = QDir(cacheRoot).filePath(QStringLiteral("thumbnails/v%1").arg(kCacheVersion));
        QDir().mkpath(m_root);
    }
}

QString ThumbnailDiskCache::filePathForKey(const QString &key) const
{
    const QByteArray digest = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QDir(m_root).filePath(QString::fromLatin1(digest) + QStringLiteral(".png"));
}

QImage ThumbnailDiskCache::load(const QString &key)
{
    if (m_root.isEmpty() || key.isEmpty()) {
        return {};
    }

    QMutexLocker locker(&cacheMutex);
    const QString path = filePathForKey(key);
    QImage image(path);
    if (image.isNull()) {
        const qint64 bytes = QFileInfo(path).size();
        if (QFile::remove(path)) {
            auto it = cacheAccounting.find(m_root);
            if (it != cacheAccounting.end()) it->bytes = qMax(qint64(0), it->bytes - bytes);
        }
        return {};
    }
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        file.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
    }
    return image;
}

void ThumbnailDiskCache::store(const QString &key, const QImage &image)
{
    if (m_root.isEmpty() || key.isEmpty() || image.isNull()) {
        return;
    }

    QMutexLocker locker(&cacheMutex);
    const QString path = filePathForKey(key);
    const qint64 previousBytes = QFileInfo(path).size();
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) {
        return;
    }
    CacheAccounting &accounting = cacheAccounting[m_root];
    accounting.bytes = qMax(qint64(0), accounting.bytes + QFileInfo(path).size() - previousBytes);
    evictIfNeeded();
}

void ThumbnailDiskCache::evictIfNeeded()
{
    CacheAccounting &accounting = cacheAccounting[m_root];
    // Reconcile external changes on the next store after a minute. Our own
    // writes update the shared total and trigger eviction immediately at budget.
    if (accounting.lastScan.isValid() && accounting.lastScan.elapsed() < 60000
        && accounting.bytes <= kDiskCacheLimitBytes) {
        return;
    }
    QDir directory(m_root);
    QFileInfoList files = directory.entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Unsorted);
    qint64 totalBytes = 0;
    for (const QFileInfo &file : files) {
        totalBytes += file.size();
    }
    if (totalBytes > kDiskCacheLimitBytes) {
        std::sort(files.begin(), files.end(), [](const QFileInfo &a, const QFileInfo &b) {
            return a.lastModified() < b.lastModified();
        });
        for (const QFileInfo &file : files) {
            if (totalBytes <= kDiskCacheTargetBytes) break;
            if (QFile::remove(file.absoluteFilePath())) {
                totalBytes -= file.size();
            }
        }
    }
    accounting.bytes = totalBytes;
    accounting.lastScan.start();
}
