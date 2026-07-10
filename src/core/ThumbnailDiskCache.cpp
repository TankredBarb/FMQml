#include "ThumbnailDiskCache.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace {
constexpr qint64 kDiskCacheLimitBytes = 256LL * 1024 * 1024;
constexpr int kCacheVersion = 1;
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

    QMutexLocker locker(&m_mutex);
    const QString path = filePathForKey(key);
    QImage image(path);
    if (image.isNull()) {
        QFile::remove(path);
        return {};
    }
    QFile file(path);
    file.setFileTime(QDateTime::currentDateTimeUtc(), QFileDevice::FileModificationTime);
    return image;
}

void ThumbnailDiskCache::store(const QString &key, const QImage &image)
{
    if (m_root.isEmpty() || key.isEmpty() || image.isNull()) {
        return;
    }

    QMutexLocker locker(&m_mutex);
    QSaveFile file(filePathForKey(key));
    if (!file.open(QIODevice::WriteOnly) || !image.save(&file, "PNG") || !file.commit()) {
        return;
    }
    evictIfNeeded();
}

void ThumbnailDiskCache::evictIfNeeded()
{
    QDir directory(m_root);
    const QFileInfoList files = directory.entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Time);
    qint64 totalBytes = 0;
    for (const QFileInfo &file : files) {
        totalBytes += file.size();
    }
    for (auto it = files.crbegin(); totalBytes > kDiskCacheLimitBytes && it != files.crend(); ++it) {
        if (QFile::remove(it->absoluteFilePath())) {
            totalBytes -= it->size();
        }
    }
}
