#include "ArchiveFileProvider.h"

#include "ArchiveOperationCallbacks.h"
#include "ArchiveSupport.h"
#include "DriveUtils.h"
#include "CleanupSubsystem.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QLocale>
#include <QMimeDatabase>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtConcurrent>
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <vector>

#ifdef Q_OS_LINUX
#include <sched.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <sys/vfs.h>
#include <unistd.h>
#endif

#ifdef HAS_UNOFFICIAL_BIT7Z
#include <bit7z/bit7z.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitformat.hpp>
#endif

#include "ArchiveFileProviderInternal.h"

using namespace ArchiveFileProviderInternal;

QString ArchiveFileProvider::archivePasswordCacheKey(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }

    if (ArchiveSupport::isArchivePath(path)) {
        return archiveContainerPart(path);
    }

    if (ArchiveSupport::isArchiveFilePath(path)) {
        return archiveContainerPart(ArchiveSupport::archiveRootPath(path));
    }

    return {};
}

QString ArchiveFileProvider::archivePasswordForPath(const QString &path)
{
    const QString key = archivePasswordCacheKey(path);
    if (key.isEmpty()) {
        return {};
    }

    QMutexLocker locker(&archivePasswordMutex());
    return archivePasswords().value(key);
}

bool ArchiveFileProvider::errorNeedsPassword(const QString &error)
{
    const QString lower = error.toLower();
    return lower.contains(QStringLiteral("password"))
        || lower.contains(QStringLiteral("encrypted"))
        || lower.contains(QStringLiteral("wrong password"))
        || lower.contains(QStringLiteral("can not open encrypted archive"))
        || lower.contains(QStringLiteral("cannot open encrypted archive"));
}

bool ArchiveFileProvider::needsPasswordForPath(const QString &path)
{
    if (path.isEmpty() || !archivePasswordForPath(path).isEmpty()) {
        return false;
    }

    const QString archivePath = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::physicalArchivePath(path)
        : path;
    if (!ArchiveSupport::isArchiveFilePath(archivePath) || !QFileInfo::exists(archivePath)) {
        return false;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    const auto library = getGlobalLibrary();
    if (!library) {
        return false;
    }

    const QString normalizedArchivePath = QDir::fromNativeSeparators(QFileInfo(archivePath).absoluteFilePath());
    const QString suffix = QFileInfo(normalizedArchivePath).suffix().toLower();
    const QStringList candidates = suffix.compare(QStringLiteral("rar"), Qt::CaseInsensitive) == 0
        ? QStringList{rarFormatCandidateForFile(normalizedArchivePath)}
        : archiveFormatCandidatesForSuffix(suffix);

    QString lastError;
    for (const QString &candidate : candidates) {
        try {
            const auto &format = archiveFormatForSuffix(candidate);
            bit7z::BitArchiveReader reader(
                *library,
                toBit7zString(QDir::toNativeSeparators(normalizedArchivePath)),
                bit7z::ArchiveStartOffset::FileStart,
                format);
            return reader.hasEncryptedItems();
        } catch (const std::exception &exception) {
            lastError = QString::fromUtf8(exception.what());
        }
    }
    return errorNeedsPassword(lastError);
#else
    return false;
#endif
}

void ArchiveFileProvider::setPasswordForPath(const QString &path, const QString &password)
{
    const QString key = archivePasswordCacheKey(path);
    if (key.isEmpty()) {
        return;
    }

    QMutexLocker locker(&archivePasswordMutex());
    if (password.isEmpty()) {
        archivePasswords().remove(key);
    } else {
        archivePasswords().insert(key, password);
    }
}

void ArchiveFileProvider::clearPasswordForPath(const QString &path)
{
    setPasswordForPath(path, {});
}

std::shared_ptr<ArchiveFileProvider::ArchiveState> ArchiveFileProvider::cachedStateForKey(const QString &key)
{
    QMutexLocker locker(&archiveCacheMutex());
    auto &cache = archiveCache();
    auto it = cache.find(key);
    if (it == cache.end()) {
        return {};
    }
    return it.value();
}

void ArchiveFileProvider::invalidateCacheForPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    const QString physicalPath = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::physicalArchivePath(path)
        : path;
    const QString normalizedPhysicalPath = QDir::fromNativeSeparators(QFileInfo(physicalPath).absoluteFilePath());
    if (normalizedPhysicalPath.isEmpty()) {
        return;
    }

    QMutexLocker locker(&archiveCacheMutex());
    auto &cache = archiveCache();
    auto &order = archiveCacheOrder();

    QStringList keysToRemove;
    for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
        const std::shared_ptr<ArchiveState> &state = it.value();
        if (!state) {
            keysToRemove.append(it.key());
            continue;
        }
        const QString stateSourcePath = QDir::fromNativeSeparators(QFileInfo(state->sourcePath).absoluteFilePath());
        if (stateSourcePath.compare(normalizedPhysicalPath, Qt::CaseInsensitive) == 0) {
            keysToRemove.append(it.key());
        }
    }

    for (const QString &key : std::as_const(keysToRemove)) {
        cache.remove(key);
        order.removeAll(key);
    }
}

bool ArchiveFileProvider::hasCachedContainerForPath(const QString &path)
{
    if (path.isEmpty() || !ArchiveSupport::isArchivePath(path) || !archiveNestedDepthAllowed(path)) {
        return false;
    }

    const auto cached = cachedStateForKey(archiveCacheKey(path));
    return cached && cached->valid;
}

void ArchiveFileProvider::storeStateInCache(const QString &key, const std::shared_ptr<ArchiveState> &state)
{
    if (key.isEmpty() || !state || !state->valid) {
        return;
    }

    QMutexLocker locker(&archiveCacheMutex());
    auto &cache = archiveCache();
    auto &order = archiveCacheOrder();

    if (state->items.size() > kMaxCachedArchiveItems) {
        cache.remove(key);
        order.removeAll(key);
        return;
    }

    order.removeAll(key);
    cache.insert(key, state);
    order.append(key);

    qsizetype cachedItems = 0;
    for (const QString &cacheKey : std::as_const(order)) {
        if (const auto cached = cache.value(cacheKey)) {
            cachedItems += cached->items.size();
        }
    }

    while ((!order.isEmpty() && order.size() > kMaxCachedArchiveStates)
           || (!order.isEmpty() && cachedItems > kMaxCachedArchiveItems)) {
        const QString evictedKey = order.takeFirst();
        if (const auto evicted = cache.take(evictedKey)) {
            cachedItems -= evicted->items.size();
        }
    }
}

QHash<QString, std::shared_ptr<ArchiveFileProvider::ArchiveState>> &ArchiveFileProvider::archiveCache()
{
    static QHash<QString, std::shared_ptr<ArchiveState>> cache;
    return cache;
}

QStringList &ArchiveFileProvider::archiveCacheOrder()
{
    static QStringList order;
    return order;
}

QMutex &ArchiveFileProvider::archiveCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, QString> &ArchiveFileProvider::archivePasswords()
{
    static QHash<QString, QString> passwords;
    return passwords;
}

QMutex &ArchiveFileProvider::archivePasswordMutex()
{
    static QMutex mutex;
    return mutex;
}

