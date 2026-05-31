#include "FileAccessResolver.h"
#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QVariantMap>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>
#include <AccCtrl.h>
#endif

namespace {

struct CacheEntry {
    FileCapabilityInfo info;
    qint64 size = -1;
    qint64 lastModifiedMs = -1;
    bool exists = false;
    qint64 cachedAtMs = 0;
};

QMutex &cacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QHash<QString, CacheEntry> &cacheStore()
{
    static QHash<QString, CacheEntry> cache;
    return cache;
}

QString cacheKeyForPath(const QString &path)
{
    QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    key = key.toLower();
#endif
    return key;
}

qint64 lastModifiedStamp(const QFileInfo &info)
{
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : -1;
}

QString formatAccessSummary(const FileCapabilityInfo &info)
{
    QStringList items;
    if (info.isDirectory) {
        if (info.access.canBrowse) {
            items.append(QStringLiteral("Browse"));
        }
        if (info.access.canCreateChildren) {
            items.append(QStringLiteral("Create inside"));
        }
        if (info.access.canDelete) {
            items.append(QStringLiteral("Delete"));
        }
        if (info.access.canTraverse) {
            items.append(QStringLiteral("Traverse"));
        }
    } else {
        if (info.access.canRead) {
            items.append(QStringLiteral("Read"));
        }
        if (info.access.canModify) {
            items.append(QStringLiteral("Modify"));
        }
        if (info.access.canDelete) {
            items.append(QStringLiteral("Delete"));
        }
        if (info.access.canExecute) {
            items.append(QStringLiteral("Execute"));
        }
    }

    if (items.isEmpty()) {
        return QStringLiteral("No access");
    }
    return items.join(QStringLiteral(", "));
}

QString formatAttributesSummary(const FileCapabilityInfo &info)
{
    QStringList items;
    if (info.attributes.hidden) {
        items.append(QStringLiteral("Hidden"));
    }
    if (info.attributes.readOnly) {
        items.append(QStringLiteral("Read-only"));
    }
    if (info.attributes.system) {
        items.append(QStringLiteral("System"));
    }
    return items.join(QStringLiteral(", "));
}

QVariantMap makeProperty(const QString &label, bool allowed)
{
    QVariantMap map;
    map.insert(QStringLiteral("label"), label);
    map.insert(QStringLiteral("value"), allowed ? QStringLiteral("Allowed") : QStringLiteral("Unavailable"));
    map.insert(QStringLiteral("allowed"), allowed);
    return map;
}

QVariantMap makeAttributeProperty(const QString &label, bool enabled)
{
    QVariantMap map;
    map.insert(QStringLiteral("label"), label);
    map.insert(QStringLiteral("value"), enabled ? QStringLiteral("Yes") : QStringLiteral("No"));
    map.insert(QStringLiteral("enabled"), enabled);
    return map;
}

#ifdef Q_OS_WIN
QString lastWindowsErrorString(DWORD errorCode)
{
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags,
                                        nullptr,
                                        errorCode,
                                        0,
                                        reinterpret_cast<LPWSTR>(&buffer),
                                        0,
                                        nullptr);
    QString detail;
    if (length > 0 && buffer) {
        detail = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
        LocalFree(buffer);
    }
    return detail.isEmpty() ? QStringLiteral("Windows error %1").arg(errorCode) : detail;
}

struct ScopedHandle {
    HANDLE handle = nullptr;
    ~ScopedHandle() {
        if (handle && handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
        }
    }
};

FileAttributesInfo readWindowsAttributes(const QString &path, const QFileInfo &info);

bool canOpenWithAccess(const QString &path, DWORD desiredAccess, bool isDirectory)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const DWORD shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    const DWORD flags = isDirectory ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL;
    ScopedHandle handle;
    handle.handle = CreateFileW(nativePath.c_str(),
                                desiredAccess,
                                shareMode,
                                nullptr,
                                OPEN_EXISTING,
                                flags,
                                nullptr);
    return handle.handle != INVALID_HANDLE_VALUE;
}

bool canDeletePathWindows(const QString &path, bool isDirectory)
{
    if (canOpenWithAccess(path, DELETE, isDirectory)) {
        return true;
    }

    const QFileInfo info(path);
    const QString parentPath = info.absolutePath();
    if (parentPath.isEmpty() || parentPath == path) {
        return false;
    }

    return canOpenWithAccess(parentPath, FILE_DELETE_CHILD, true);
}

FileCapabilityInfo resolveLocalWindows(const QString &path, const QFileInfo &info)
{
    FileCapabilityInfo result;
    result.path = path;
    result.exists = info.exists();
    result.isDirectory = info.isDir();
    result.isArchiveLike = false;
    result.attributes = readWindowsAttributes(path, info);

    if (!result.exists) {
        return result;
    }

    result.access.exact = true;
    if (result.isDirectory) {
        result.access.canBrowse = canOpenWithAccess(path, FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES, true);
        result.access.canCreateChildren = canOpenWithAccess(path, FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY | FILE_WRITE_ATTRIBUTES, true);
        result.access.canTraverse = canOpenWithAccess(path, FILE_TRAVERSE, true);
        result.access.canDelete = canDeletePathWindows(path, true);
        result.access.canChangeAttributes = canOpenWithAccess(path, FILE_WRITE_ATTRIBUTES, true);
        result.access.canRead = result.access.canBrowse;
        result.access.canModify = result.access.canCreateChildren;
        result.access.canExecute = result.access.canTraverse;
    } else {
        result.access.canRead = canOpenWithAccess(path, FILE_READ_DATA | FILE_READ_ATTRIBUTES, false);
        result.access.canModify = canOpenWithAccess(path, FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES, false);
        result.access.canExecute = canOpenWithAccess(path, FILE_EXECUTE, false);
        result.access.canDelete = canDeletePathWindows(path, false);
        result.access.canChangeAttributes = canOpenWithAccess(path, FILE_WRITE_ATTRIBUTES, false);
        result.access.canBrowse = false;
        result.access.canCreateChildren = false;
        result.access.canTraverse = false;
    }

    result.accessSummary = formatAccessSummary(result);
    result.attributesSummary = formatAttributesSummary(result);
    return result;
}

bool updateAttributeFlag(const QString &path, DWORD flag, bool enabled, QString *error)
{
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const DWORD current = GetFileAttributesW(nativePath.c_str());
    if (current == INVALID_FILE_ATTRIBUTES) {
        if (error) {
            *error = lastWindowsErrorString(GetLastError());
        }
        return false;
    }

    DWORD updated = current;
    if (enabled) {
        updated |= flag;
    } else {
        updated &= ~flag;
    }

    if (!SetFileAttributesW(nativePath.c_str(), updated)) {
        if (error) {
            *error = lastWindowsErrorString(GetLastError());
        }
        return false;
    }
    return true;
}

FileAttributesInfo readWindowsAttributes(const QString &path, const QFileInfo &info)
{
    FileAttributesInfo attributes;
    const std::wstring nativePath = QDir::toNativeSeparators(path).toStdWString();
    const DWORD value = GetFileAttributesW(nativePath.c_str());
    if (value == INVALID_FILE_ATTRIBUTES) {
        attributes.hidden = info.isHidden();
        attributes.readOnly = false;
        attributes.system = false;
        attributes.archive = false;
        return attributes;
    }

    attributes.hidden = (value & FILE_ATTRIBUTE_HIDDEN) != 0;
    attributes.readOnly = (value & FILE_ATTRIBUTE_READONLY) != 0;
    attributes.system = (value & FILE_ATTRIBUTE_SYSTEM) != 0;
    attributes.archive = (value & FILE_ATTRIBUTE_ARCHIVE) != 0;
    return attributes;
}
#endif

FileCapabilityInfo resolveArchivePath(const QString &path)
{
    FileCapabilityInfo result;
    result.path = path;
    result.exists = true;
    result.isArchiveLike = true;
    result.attributes.archive = true;

    const std::optional<FileEntry> entry = ArchiveFileProvider::cachedEntryInfo(path);
    if (entry) {
        result.isDirectory = entry->isDirectory;
        result.attributes.hidden = entry->isHidden;
        result.attributes.readOnly = entry->isReadOnly;
        result.attributes.system = entry->isSystem;
    } else {
        result.isDirectory = ArchiveSupport::archiveBrowsePath(path) == QLatin1String("/");
        result.attributes.hidden = false;
        result.attributes.readOnly = true;
        result.attributes.system = false;
    }

    result.access.canRead = true;
    result.access.canBrowse = result.isDirectory;
    result.access.canTraverse = result.isDirectory;
    result.access.exact = true;
    result.accessSummary = formatAccessSummary(result);
    result.attributesSummary = formatAttributesSummary(result);
    return result;
}

FileCapabilityInfo resolveFallback(const QString &path, const QFileInfo &info)
{
    FileCapabilityInfo result;
    result.path = path;
    result.exists = info.exists();
    result.isDirectory = info.isDir();
    result.isArchiveLike = false;
    result.attributes = readWindowsAttributes(path, info);
    result.access.exact = false;

    if (result.isDirectory) {
        result.access.canBrowse = info.isReadable();
        result.access.canCreateChildren = info.isWritable();
        result.access.canTraverse = info.isExecutable() || info.isReadable();
        result.access.canDelete = QFileInfo(info.absolutePath()).isWritable();
        result.access.canChangeAttributes = info.isWritable();
        result.access.canRead = result.access.canBrowse;
        result.access.canModify = result.access.canCreateChildren;
        result.access.canExecute = result.access.canTraverse;
    } else {
        result.access.canRead = info.isReadable();
        result.access.canModify = info.isWritable();
        result.access.canDelete = QFileInfo(info.absolutePath()).isWritable();
        result.access.canExecute = info.isExecutable();
        result.access.canChangeAttributes = info.isWritable();
    }

    result.accessSummary = formatAccessSummary(result);
    result.attributesSummary = formatAttributesSummary(result);
    return result;
}

} // namespace

FileCapabilityInfo FileAccessResolver::resolve(const QString &path)
{
    if (path.isEmpty()) {
        return {};
    }

    if (ArchiveSupport::isArchivePath(path)) {
        return resolveArchivePath(path);
    }

    const QFileInfo info(path);
    const QString key = cacheKeyForPath(path);
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const qint64 infoModifiedMs = lastModifiedStamp(info);
    const qint64 infoSize = info.exists() ? info.size() : -1;

    {
        QMutexLocker locker(&cacheMutex());
        const auto it = cacheStore().constFind(key);
        if (it != cacheStore().constEnd()) {
            const CacheEntry &entry = it.value();
            if (entry.exists == info.exists()
                && entry.lastModifiedMs == infoModifiedMs
                && entry.size == infoSize
                && (nowMs - entry.cachedAtMs) < 3000) {
                return entry.info;
            }
        }
    }

    FileCapabilityInfo result;
#ifdef Q_OS_WIN
    result = resolveLocalWindows(path, info);
#else
    result = resolveFallback(path, info);
#endif
    result.accessSummary = formatAccessSummary(result);
    result.attributesSummary = formatAttributesSummary(result);

    {
        QMutexLocker locker(&cacheMutex());
        if (cacheStore().size() > 256) {
            cacheStore().clear();
        }
        cacheStore().insert(key, CacheEntry{result, infoSize, infoModifiedMs, info.exists(), nowMs});
    }

    return result;
}

QVariantList FileAccessResolver::accessProperties(const FileCapabilityInfo &info)
{
    QVariantList rows;
    if (info.isDirectory) {
        rows.append(makeProperty(QStringLiteral("Browse"), info.access.canBrowse));
        rows.append(makeProperty(QStringLiteral("Create inside"), info.access.canCreateChildren));
        rows.append(makeProperty(QStringLiteral("Delete"), info.access.canDelete));
        rows.append(makeProperty(QStringLiteral("Traverse"), info.access.canTraverse));
    } else {
        rows.append(makeProperty(QStringLiteral("Read"), info.access.canRead));
        rows.append(makeProperty(QStringLiteral("Modify"), info.access.canModify));
        rows.append(makeProperty(QStringLiteral("Delete"), info.access.canDelete));
        rows.append(makeProperty(QStringLiteral("Execute"), info.access.canExecute));
    }
    return rows;
}

QVariantList FileAccessResolver::attributeProperties(const FileCapabilityInfo &info)
{
    QVariantList rows;
    rows.append(makeAttributeProperty(QStringLiteral("Hidden"), info.attributes.hidden));
    rows.append(makeAttributeProperty(QStringLiteral("Read-only"), info.attributes.readOnly));
    rows.append(makeAttributeProperty(QStringLiteral("System"), info.attributes.system));
    return rows;
}

bool FileAccessResolver::setHidden(const QString &path, bool enabled, QString *error)
{
#ifdef Q_OS_WIN
    const bool ok = updateAttributeFlag(path, FILE_ATTRIBUTE_HIDDEN, enabled, error);
    if (ok) {
        invalidate(path);
    }
    return ok;
#else
    Q_UNUSED(path)
    Q_UNUSED(enabled)
    if (error) {
        *error = QStringLiteral("Hidden attribute is not supported here");
    }
    return false;
#endif
}

bool FileAccessResolver::setReadOnly(const QString &path, bool enabled, QString *error)
{
#ifdef Q_OS_WIN
    const bool ok = updateAttributeFlag(path, FILE_ATTRIBUTE_READONLY, enabled, error);
    if (ok) {
        invalidate(path);
    }
    return ok;
#else
    Q_UNUSED(path)
    Q_UNUSED(enabled)
    if (error) {
        *error = QStringLiteral("Read-only attribute is not supported here");
    }
    return false;
#endif
}

void FileAccessResolver::invalidate(const QString &path)
{
    QMutexLocker locker(&cacheMutex());
    cacheStore().remove(cacheKeyForPath(path));
}

void FileAccessResolver::invalidateAll()
{
    QMutexLocker locker(&cacheMutex());
    cacheStore().clear();
}
