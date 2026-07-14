#include "GDriveFileProviderInternal.h"

#include "GDriveApiClient.h"
#include "GDriveRequestPolicy.h"
#include "GDriveTransferClient.h"

#include "FileProvider.h"

#include "GDriveAuth.h"
#include "GDriveCache.h"
#include "GDriveExportPolicy.h"
#include "GDriveEntryMapper.h"
#include "GDrivePath.h"
#include "GDriveThumbnailLoader.h"
#include "GDriveTypes.h"

#include <algorithm>
#include <atomic>
#include <optional>

#include <QDateTime>
#include <QtConcurrent>
#include <QThread>
#include <QRandomGenerator>
#include <QMutex>
#include <QFuture>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMimeDatabase>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QRegularExpression>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>
#include <QVector>
#include <QUuid>
#include <QThreadPool>

namespace {

constexpr QLatin1StringView GoogleDriveFolderMime{"application/vnd.google-apps.folder"};
constexpr QLatin1StringView GoogleDriveShortcutMime{"application/vnd.google-apps.shortcut"};
constexpr QLatin1StringView GoogleDriveAppsMimePrefix{"application/vnd.google-apps."};
constexpr QLatin1StringView DriveListFields{
    "nextPageToken,files(id,name,mimeType,size,modifiedTime,createdTime,parents,webViewLink,ownedByMe,shared,"
    "thumbnailLink,"
    "shortcutDetails(targetId,targetMimeType,targetResourceKey),"
    "capabilities(canDownload,canEdit,canAddChildren,canListChildren,canRename,canTrash,canDelete,canCopy))"};
constexpr QLatin1StringView DriveFileFields{
    "id,name,mimeType,size,modifiedTime,createdTime,parents,webViewLink,ownedByMe,shared,"
    "thumbnailLink,"
    "shortcutDetails(targetId,targetMimeType,targetResourceKey),"
    "capabilities(canDownload,canEdit,canAddChildren,canListChildren,canRename,canTrash,canDelete,canCopy)"};
constexpr QLatin1StringView DriveAboutFields{"storageQuota(limit,usage),user(displayName,emailAddress)"};
constexpr qint64 SmallMultipartUploadThresholdBytes = 1 * 1024 * 1024;
constexpr qint64 ResumableUploadChunkBytes = 8 * 1024 * 1024;
constexpr int TransferIdleTimeoutMs = 120000;
constexpr int DefaultUploadStallLogMs = 15000;
constexpr int DefaultSmallUploadConcurrency = 6;
constexpr int MaxSmallUploadConcurrency = 12;
constexpr int DefaultDownloadConcurrency = 4;
constexpr int MaxDownloadConcurrency = 8;
constexpr int MaxResumableChunkAttempts = 5;
constexpr int DriveApiMaxAttempts = 3;
constexpr int DriveApiCooldownMaxMs = 180000;
using GDriveAuth::AccountInfo;
using GDriveAuth::OAuthClientConfig;
using GDriveAuth::accessTokenForBlockingRequest;
using GDriveAuth::clearSavedAuthorization;
using GDriveAuth::hasSavedAuthorization;
using GDriveAuth::loadOAuthClientConfig;
using GDriveAuth::rememberAccountInfo;
using GDriveAuth::rememberAccessToken;
using GDriveAuth::rememberRefreshToken;
using GDriveAuth::savedAccountInfo;
using GDriveAuth::sessionRefreshToken;
using GDriveAuth::validSessionAccessToken;
using GDriveCache::cacheSharedChildren;
using GDriveCache::cacheSharedEntry;
using GDriveCache::cacheSharedQuota;
using GDriveCache::cacheSharedThumbnailLink;
using GDriveCache::clearSharedMetadata;
using GDriveCache::removeSharedPath;
using GDriveCache::sharedCapabilities;
using GDriveCache::sharedChildren;
using GDriveCache::sharedChildrenIfCached;
using GDriveCache::sharedEntry;
using GDriveCache::sharedMimeType;
using GDriveCache::sharedParent;
using GDriveCache::sharedQuota;
using GDriveCache::sharedThumbnailLink;

using GDriveApiClient::createMetadataBlocking;
using GDriveApiClient::errorMessage;
using GDriveApiClient::fetchFileMetadataBlocking;
using GDriveApiClient::quotaFromAboutObject;
using GDriveApiClient::refreshStorageQuotaBlocking;
using GDriveApiClient::rememberAccountInfoFromAboutObject;
using GDriveApiClient::safeReadAll;
using GDriveApiClient::storageInfoForQuota;
using GDriveApiClient::trashFileBlocking;
using GDriveApiClient::waitForReply;
using GDriveApiClient::listChildrenBlocking;
using GDriveEntryMapper::cacheSharedShortcutAlias;
using GDriveEntryMapper::cacheSharedShortcutInRoot;
using GDriveEntryMapper::driveCapabilitiesProperties;
using GDriveEntryMapper::driveCapabilitiesText;
using GDriveEntryMapper::driveCapabilitiesFromDriveFileObject;
using GDriveEntryMapper::driveContentIdForPath;
using GDriveEntryMapper::driveQueryForPath;
using GDriveEntryMapper::entryFromDriveFileObject;
using GDriveEntryMapper::googleAppsExportTargetForPath;
using GDriveEntryMapper::isSharedTrashViewPath;
using GDriveEntryMapper::shortcutAliasCapabilities;
using GDriveEntryMapper::shortcutAliasEntryFor;
using GDriveEntryMapper::shortcutEntryWithTargetMetadata;
using GDriveEntryMapper::shortcutsRootCapabilities;
using GDriveEntryMapper::shortcutViewCapabilities;
using GDriveEntryMapper::shortcutViewEntry;
using GDriveEntryMapper::trashRootCapabilities;
using GDriveEntryMapper::trashViewCapabilities;
using GDriveEntryMapper::trashViewEntry;
using GDriveEntryMapper::virtualDirectoryEntry;
using GDriveExportPolicy::defaultExportFormatForGoogleAppsMimeType;
using GDriveExportPolicy::isGoogleAppsMimeType;
using GDriveExportPolicy::withExportSuffix;
using GDriveTransferClient::downloadConcurrency;
using GDriveTransferClient::downloadFileToLocalFile;
using GDriveTransferClient::downloadLoggingEnabled;
using GDriveTransferClient::downloadRangeLoggingEnabled;
using GDriveTransferClient::smallUploadConcurrency;
using GDriveTransferClient::uploadFileBlockingWithRetry;
using GDriveTransferClient::uploadLoggingEnabled;
using GDriveUploadLogContext = GDriveTransferClient::UploadLogContext;
using GDrivePreparedDownloadItem = GDriveTransferClient::BatchDownloadItem;

struct GDriveCreateTarget {
    QString parentPath;
    QString parentId;
    QString name;

    bool valid() const
    {
        return !parentPath.isEmpty() && !parentId.isEmpty() && !name.trimmed().isEmpty();
    }
};
class GDriveFileProvider final : public FileProvider
{
public:
    explicit GDriveFileProvider(QObject *parent = nullptr)
        : FileProvider(parent)
    {
        cacheRootEntries();
    }

    QString scheme() const override { return QStringLiteral("gdrive"); }
    bool canHandle(const QString &path) const override { return !GDrivePath::normalizedPath(path).isEmpty(); }
    Capabilities capabilities() const override { return Browse | ReadMetadata | Create | Remove | Transfer; }
    bool isReadOnlyContainer(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        return GDrivePath::isShortcutsViewPath(normalized) || isTrashReadOnlyPath(normalized);
    }
    bool canCreateChildren(const QString &path) const override { return canCreateInFolder(path); }
    bool canCopyPath(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        return !normalized.isEmpty() && !isTrashReadOnlyPath(normalized);
    }
    bool canRemovePath(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        if (normalized.isEmpty() || isServiceReadOnlyPath(normalized)) {
            return false;
        }

        const auto localCapabilities = m_itemCapabilities.constFind(normalized);
        if (localCapabilities != m_itemCapabilities.constEnd()) {
            return localCapabilities->canTrash;
        }
        const std::optional<GDriveItemCapabilities> capabilities = sharedCapabilities(normalized);
        return capabilities && capabilities->canTrash;
    }

    void scan(const QString &path) override
    {
        clearLastError();
        const QString normalized = normalizedPath(path);
        const int generation = m_generation.fetch_add(1) + 1;
        m_currentPath = normalized;
        m_running.store(true);
        emit started();

        if (normalized.isEmpty()) {
            finish(generation, false, QStringLiteral("Google Drive path is invalid"));
            return;
        }

        if (normalized == GDrivePath::Root) {
            emitRootEntries(generation);
            finish(generation, true, {});
            return;
        }

        if (!driveQueryForPath(normalized).isEmpty()) {
            ensureAuthorizedAndList(generation, normalized, {});
            return;
        }

        finish(generation, false, QStringLiteral("Google Drive path is not available yet"));
    }

    void cancel() override
    {
        m_generation.fetch_add(1);
        m_running.store(false);
        if (m_activeReply) {
            m_activeReply->abort();
        }
    }

    void setShowHidden(bool show) override { m_showHidden = show; }
    bool isRunning() const override { return m_running.load(); }
    QString currentPath() const override { return m_currentPath; }
    int currentGeneration() const override { return m_generation.load(); }

    bool pathExists(const QString &path) const override
    {
        const QString normalized = normalizedPath(path);
        if (m_createdPaths.contains(normalized)) {
            return pathExists(m_createdPaths.value(normalized));
        }
        if (GDrivePath::pendingPathInfo(normalized).valid()) {
            return false;
        }
        return normalized == GDrivePath::Root
            || normalized == GDrivePath::MyDrive
            || normalized == GDrivePath::SharedWithMe
            || normalized == GDrivePath::ShortcutsRoot
            || normalized == GDrivePath::Trash
            || m_entries.contains(normalized)
            || sharedEntry(normalized).has_value()
            || normalized.startsWith(GDrivePath::ItemPrefix);
    }

    bool isDirectory(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        if (normalized == GDrivePath::Root || normalized == GDrivePath::MyDrive || normalized == GDrivePath::SharedWithMe
            || normalized == GDrivePath::ShortcutsRoot || normalized == GDrivePath::Trash) {
            return true;
        }
        const auto it = m_entries.constFind(normalized);
        if (it != m_entries.constEnd()) {
            return it->isDirectory;
        }
        const auto entry = sharedEntry(normalized);
        return entry && entry->isDirectory;
    }

    bool isSymLink(const QString &) const override { return false; }
    QString normalizedPath(const QString &path) const override { return GDrivePath::normalizedPath(path); }

    QString fileName(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        const auto it = m_entries.constFind(normalized);
        if (it != m_entries.constEnd()) {
            return it->name;
        }
        const auto entry = sharedEntry(normalized);
        return entry ? entry->name : GDrivePath::fallbackFileNameForPath(normalized);
    }

    QString localCopyFileName(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        std::optional<FileEntry> entry = entryInfo(normalized);
        if (!entry) {
            return fileName(normalized);
        }
        if (entry->isShortcut && !entry->shortcutTargetPath.isEmpty()) {
            if (const std::optional<FileEntry> targetEntry = sharedEntry(entry->shortcutTargetPath)) {
                entry = shortcutEntryWithTargetMetadata(*entry, *targetEntry);
            }
        }
        if (entry->isDirectory) {
            return entry->name;
        }

        QString mimeType = entry->isShortcut && !entry->shortcutTargetMimeType.isEmpty()
            ? entry->shortcutTargetMimeType
            : entry->mimeType;
        if (mimeType.isEmpty()) {
            mimeType = m_mimeTypes.value(normalized);
        }
        if (!isGoogleAppsMimeType(mimeType)) {
            return entry->name;
        }

        return withExportSuffix(entry->name, defaultExportFormatForGoogleAppsMimeType(mimeType).suffix);
    }

    QString absolutePath(const QString &path) const override { return normalizedPath(path); }

    QString parentPath(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        const auto it = m_parents.constFind(normalized);
        if (it != m_parents.constEnd()) {
            return it.value();
        }
        const QString cachedParent = sharedParent(normalized);
        if (!cachedParent.isEmpty()) {
            return cachedParent;
        }
        return GDrivePath::parentPath(normalized);
    }

    QString childPath(const QString &parentPath, const QString &name) const override
    {
        const QString rootChild = GDrivePath::childPath(parentPath, name);
        if (!rootChild.isEmpty()) {
            return rootChild;
        }

        const QString normalizedParent = resolveCreatedPath(normalizedPath(parentPath));
        const QString cleanName = name.trimmed();
        if (normalizedParent.isEmpty() || cleanName.isEmpty()
            || cleanName.contains(QLatin1Char('/')) || cleanName.contains(QLatin1Char('\\'))) {
            return {};
        }

        const bool shortcutsRootContext = normalizedParent == GDrivePath::ShortcutsRoot;
        const bool shortcutContext = !GDrivePath::idForShortcutPath(normalizedParent).isEmpty();
        const bool trashContext = isTrashReadOnlyPath(normalizedParent);
        QString parentId;
        if (!shortcutsRootContext && !trashContext) {
            parentId = shortcutContext
                ? driveContentIdForPath(normalizedParent)
                : GDrivePath::driveParentIdForPath(normalizedParent);
            if (parentId.isEmpty()) {
                return {};
            }
        }

        QStringList children;
        const auto localChildren = m_children.constFind(normalizedParent);
        if (localChildren != m_children.constEnd()) {
            children = localChildren.value();
        } else if (const auto cachedChildren = sharedChildrenIfCached(normalizedParent)) {
            children = *cachedChildren;
        } else {
            QString error;
            children = listChildrenBlocking(m_network, normalizedParent, &error);
            if (!error.isEmpty()) {
                setLastError(error);
            }
        }
        for (const QString &child : children) {
            const auto localEntry = m_entries.constFind(child);
            QString childName;
            if (localEntry != m_entries.constEnd()) {
                childName = localEntry->name;
            } else if (const auto cachedEntry = sharedEntry(child)) {
                childName = cachedEntry->name;
            }
            if (childName.compare(cleanName, Qt::CaseInsensitive) == 0) {
                return child;
            }
        }
        if (shortcutContext || shortcutsRootContext || trashContext) {
            return {};
        }
        return GDrivePath::pendingPathForParentIdAndName(parentId, cleanName);
    }

    std::optional<FileEntry> entryInfo(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        const auto it = m_entries.constFind(normalized);
        if (it != m_entries.constEnd()) {
            FileEntry entry = it.value();
            if (entry.isShortcut && !entry.shortcutTargetPath.isEmpty()) {
                if (const std::optional<FileEntry> targetEntry = sharedEntry(entry.shortcutTargetPath)) {
                    entry = shortcutEntryWithTargetMetadata(entry, *targetEntry);
                }
            }
            return entry;
        }
        const auto cached = sharedEntry(normalized);
        if (cached) {
            FileEntry entry = *cached;
            if (entry.isShortcut && !entry.shortcutTargetPath.isEmpty()) {
                if (const std::optional<FileEntry> targetEntry = sharedEntry(entry.shortcutTargetPath)) {
                    entry = shortcutEntryWithTargetMetadata(entry, *targetEntry);
                }
            }
            return entry;
        }
        if (normalized == GDrivePath::Root) {
            GDriveItemCapabilities rootCapabilities;
            rootCapabilities.canListChildren = true;
            return virtualDirectoryEntry(QStringLiteral("Google Drive"), QString(GDrivePath::Root), rootCapabilities);
        }
        if (normalized == GDrivePath::ShortcutsRoot) {
            return virtualDirectoryEntry(QStringLiteral("Shortcuts"), QString(GDrivePath::ShortcutsRoot), shortcutsRootCapabilities());
        }
        if (normalized == GDrivePath::Trash) {
            return virtualDirectoryEntry(QStringLiteral("Trash"), QString(GDrivePath::Trash), trashRootCapabilities());
        }
        return std::nullopt;
    }

    bool ensureParentDirectory(const QString &path) const override
    {
        clearLastError();
        const QString parent = parentPath(path);
        if (parent.isEmpty() || parent == GDrivePath::Root || parent == GDrivePath::SharedWithMe) {
            setLastError(QStringLiteral("Google Drive cannot create items in this location"));
            return false;
        }
        if (!canCreateInFolder(parent)) {
            setLastError(QStringLiteral("Google Drive does not allow creating items in this folder"));
            return false;
        }
        return true;
    }

    QString gdriveThumbnailCacheIdentity(const QString &normalized) const
    {
        const std::optional<FileEntry> entry = entryInfo(normalized);
        if (!entry || entry->isDirectory) {
            return {};
        }
        const QString fileId = GDrivePath::idForItemPath(normalized);
        const QString modifiedIso = entry->modified.isValid()
            ? entry->modified.toUTC().toString(Qt::ISODateWithMs)
            : QString{};
        const QString thumbnailLink = sharedThumbnailLink(normalized);
        if (thumbnailLink.isEmpty()) {
            return {};
        }
        const QByteArray linkHash = QCryptographicHash::hash(thumbnailLink.toUtf8(), QCryptographicHash::Sha1)
                                        .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        return QStringLiteral("gdrive:%1:%2:%3:%4")
            .arg(fileId, modifiedIso, QString::number(entry->size), QString::fromLatin1(linkHash));
    }

    QString thumbnailCacheIdentity(const QString &path) const override
    {
        const QString normalized = normalizedPath(path);
        if (normalized.isEmpty()) {
            return {};
        }
        return gdriveThumbnailCacheIdentity(normalized);
    }

    ProviderThumbnailResult thumbnailForPath(const QString &path,
                                             const QSize &requestedSize,
                                             QString *error) const override
    {
        Q_UNUSED(requestedSize)

        const QString normalized = normalizedPath(path);
        if (normalized.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Google Drive path is invalid");
            }
            return {};
        }

        const std::optional<FileEntry> entry = entryInfo(normalized);
        if (!entry || entry->isDirectory) {
            if (error) {
                *error = QStringLiteral("Google Drive entry is not a file");
            }
            return {};
        }

        const QString thumbnailLink = sharedThumbnailLink(normalized);
        const QString cacheIdentity = gdriveThumbnailCacheIdentity(normalized);
        if (thumbnailLink.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Google Drive entry has no thumbnail metadata");
            }
            ProviderThumbnailResult none;
            return none;
        }

        const QByteArray cachedBytes = GDriveThumbnailLoader::cachedBytes(cacheIdentity);
        if (!cachedBytes.isEmpty()) {
            ProviderThumbnailResult result;
            result.kind = ProviderThumbnailResult::Kind::EncodedBytes;
            result.encodedBytes = cachedBytes;
            result.mimeType = QStringLiteral("image/*");
            result.cacheIdentity = cacheIdentity;
            return result;
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            if (error) {
                *error = authError;
            }
            ProviderThumbnailResult unavailable;
            unavailable.kind = ProviderThumbnailResult::Kind::TemporaryUnavailable;
            unavailable.cacheIdentity = cacheIdentity;
            return unavailable;
        }

        const GDriveThumbnailLoader::DownloadResult download =
            GDriveThumbnailLoader::downloadBytes(QUrl(thumbnailLink), accessToken);

        if (download.timedOut) {
            if (error) {
                *error = QStringLiteral("Google Drive thumbnail request timed out");
            }
            ProviderThumbnailResult unavailable;
            unavailable.kind = ProviderThumbnailResult::Kind::TemporaryUnavailable;
            unavailable.cacheIdentity = cacheIdentity;
            return unavailable;
        }
        if (download.oversize) {
            if (error) {
                *error = QStringLiteral("Google Drive thumbnail exceeds size limit");
            }
            ProviderThumbnailResult unavailable;
            unavailable.kind = ProviderThumbnailResult::Kind::TemporaryUnavailable;
            unavailable.cacheIdentity = cacheIdentity;
            return unavailable;
        }
        if (download.httpStatus == 401) {
            if (error) {
                *error = QStringLiteral("Google Drive thumbnail request was unauthorized");
            }
            ProviderThumbnailResult unavailable;
            unavailable.kind = ProviderThumbnailResult::Kind::TemporaryUnavailable;
            unavailable.cacheIdentity = cacheIdentity;
            return unavailable;
        }
        if (download.httpStatus == 404 || download.httpStatus == 403) {
            if (error) {
                *error = QStringLiteral("Google Drive thumbnail is unavailable");
            }
            ProviderThumbnailResult none;
            none.cacheIdentity = cacheIdentity;
            return none;
        }
        if (download.httpStatus == 408 || download.httpStatus == 429 || download.httpStatus >= 500) {
            if (error) {
                *error = QStringLiteral("Google Drive thumbnail request is temporarily unavailable (HTTP %1)").arg(download.httpStatus);
            }
            ProviderThumbnailResult unavailable;
            unavailable.kind = ProviderThumbnailResult::Kind::TemporaryUnavailable;
            unavailable.cacheIdentity = cacheIdentity;
            return unavailable;
        }
        if (download.httpStatus >= 400 || download.body.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Google Drive thumbnail request failed (HTTP %1)").arg(download.httpStatus);
            }
            ProviderThumbnailResult none;
            none.cacheIdentity = cacheIdentity;
            return none;
        }

        GDriveThumbnailLoader::cacheBytes(cacheIdentity, download.body);
        ProviderThumbnailResult result;
        result.kind = ProviderThumbnailResult::Kind::EncodedBytes;
        result.encodedBytes = download.body;
        result.mimeType = QStringLiteral("image/*");
        result.cacheIdentity = cacheIdentity;
        return result;
    }

    bool makePath(const QString &path) const override
    {
        clearLastError();
        const QString normalized = normalizedPath(path);
        const QString resolved = resolveCreatedPath(normalized);
        if (resolved != normalized && isDirectory(resolved)) {
            return true;
        }
        if (const auto existing = entryInfo(resolved); existing && existing->isDirectory) {
            return true;
        }

        QString error;
        const GDriveCreateTarget target = createTargetForPath(normalized, &error);
        if (!target.valid()) {
            setLastError(error.isEmpty()
                             ? QStringLiteral("Google Drive folder target is invalid")
                             : error);
            return false;
        }

        QString createdPath;
        if (!createDriveFolder(target.parentPath, target.name, &createdPath)) {
            return false;
        }
        if (!createdPath.isEmpty()) {
            m_createdPaths.insert(normalized, createdPath);
        }
        return true;
    }

    bool removePath(const QString &path) const override
    {
        clearLastError();
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        if (isServiceReadOnlyPath(normalized)) {
            setLastError(QStringLiteral("This Google Drive virtual folder is read-only"));
            return false;
        }
        const QString id = GDrivePath::idForItemPath(normalized);
        if (id.isEmpty()) {
            setLastError(QStringLiteral("Google Drive item path is invalid"));
            return false;
        }

        const auto entry = entryInfo(normalized);
        const QString parent = parentPath(normalized);
        if (entry) {
            m_removedTargets.insert(normalized, {parent, GDrivePath::driveParentIdForPath(parent), entry->name});
        }

        const std::optional<GDriveItemCapabilities> capabilities = sharedCapabilities(normalized);
        if (capabilities && !capabilities->canTrash) {
            setLastError(QStringLiteral("Google Drive does not allow moving this item to Trash"));
            return false;
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            setLastError(authError);
            return false;
        }

        QJsonObject trashedObject;
        QString error;
        if (!trashFileBlocking(m_network, id, accessToken, &trashedObject, &error)) {
            setLastError(error);
            return false;
        }

        removeCachedPath(normalized);
        markStorageQuotaRefreshPending();
        return true;
    }

    QStringList childPaths(const QString &path, bool includeHidden = true) const override
    {
        Q_UNUSED(includeHidden)
        const QString normalized = normalizedPath(path);
        const auto it = m_children.constFind(normalized);
        if (it != m_children.constEnd()) {
            return it.value();
        }
        const auto cachedChildren = sharedChildrenIfCached(normalized);
        if (cachedChildren) {
            return *cachedChildren;
        }

        QString error;
        const QStringList listedChildren = listChildrenBlocking(m_network, normalized, &error);
        if (!error.isEmpty()) {
            setLastError(error);
        }
        return listedChildren;
    }

    bool movePath(const QString &, const QString &) const override
    {
        setLastError(QStringLiteral("Google Drive move is not supported"));
        return false;
    }

    std::unique_ptr<QIODevice> openRead(const QString &) const override
    {
        setLastError(QStringLiteral("Google Drive download is not implemented yet"));
        return nullptr;
    }

    bool prepareDownloadItem(const LocalFileMaterializeItem &item,
                             GDrivePreparedDownloadItem *prepared,
                             QString *error) const
    {
        const QString normalized = normalizedPath(item.sourcePath);
        if (isTrashReadOnlyPath(normalized)) {
            if (error) {
                *error = QStringLiteral("Google Drive Trash is read-only; files cannot be downloaded from there.");
            }
            return false;
        }

        std::optional<FileEntry> entry = entryInfo(normalized);
        if (!entry) {
            if (error) {
                *error = QStringLiteral("Google Drive file metadata is not available. Open the folder first.");
            }
            return false;
        }
        if (entry->isShortcut && !entry->shortcutTargetIsDirectory && !entry->shortcutTargetPath.isEmpty()) {
            QString targetError;
            if (const std::optional<FileEntry> targetEntry = resolveFileShortcutTarget(*entry, &targetError)) {
                entry = shortcutEntryWithTargetMetadata(*entry, *targetEntry);
            } else if (entry->shortcutTargetMimeType.isEmpty()) {
                if (error) {
                    *error = targetError.trimmed().isEmpty()
                        ? QStringLiteral("Google Drive shortcut target metadata is not available.")
                        : targetError.trimmed();
                }
                return false;
            }
        }
        if (entry->isDirectory) {
            if (error) {
                *error = QStringLiteral("Google Drive folder download is not implemented yet");
            }
            return false;
        }

        const bool fileShortcut = entry->isShortcut
            && !entry->shortcutTargetIsDirectory
            && !entry->shortcutTargetPath.isEmpty();
        const QString downloadPath = fileShortcut ? entry->shortcutTargetPath : normalized;
        const QString resourceKey = fileShortcut ? entry->shortcutTargetResourceKey : QString{};
        const std::optional<GDriveItemCapabilities> capabilities = sharedCapabilities(normalized);
        if (!fileShortcut && capabilities && !capabilities->canDownload) {
            if (error) {
                *error = QStringLiteral("Google Drive does not allow downloading this file.");
            }
            return false;
        }

        QString mimeType = fileShortcut ? entry->shortcutTargetMimeType : QString{};
        if (mimeType.isEmpty()) {
            mimeType = m_mimeTypes.value(normalized);
        }
        if (mimeType.isEmpty()) {
            mimeType = sharedMimeType(normalized);
        }
        if (mimeType.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Google Drive file type is not available. Open the folder first.");
            }
            return false;
        }

        const QFileInfo destinationInfo(item.destinationFilePath);
        if (destinationInfo.absolutePath().isEmpty() || !QDir().mkpath(destinationInfo.absolutePath())) {
            if (error) {
                *error = QStringLiteral("Cannot create Google Drive download destination folder");
            }
            return false;
        }

        if (prepared) {
            LocalFileMaterializeItem normalizedItem = item;
            normalizedItem.sourcePath = normalized;
            normalizedItem.size = (std::max<qint64>)(0, item.size > 0 ? item.size : entry->size);
            prepared->item = normalizedItem;
            prepared->progressName = entry->name.isEmpty() ? GDrivePath::fallbackFileNameForPath(normalized) : entry->name;
            prepared->downloadPath = downloadPath;
            prepared->mimeType = mimeType;
            prepared->resourceKey = resourceKey;
            prepared->partialPath = item.destinationFilePath + QStringLiteral(".part");
        }
        if (error) {
            error->clear();
        }
        return true;
    }

    bool copyToLocalFile(const QString &sourcePath,
                         const QString &destinationFilePath,
                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                         QString *error) const override
    {
        clearLastError();

        GDrivePreparedDownloadItem prepared;
        QString prepareError;
        if (!prepareDownloadItem(LocalFileMaterializeItem{sourcePath, destinationFilePath, 0}, &prepared, &prepareError)) {
            setLastError(prepareError);
            if (error) {
                *error = prepareError;
            }
            return false;
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            setLastError(authError);
            if (error) {
                *error = authError;
            }
            return false;
        }

        QString downloadError;
        if (!downloadFileToLocalFile(m_network,
                                          prepared.downloadPath,
                                          prepared.mimeType,
                                          destinationFilePath,
                                          accessToken,
                                          progress,
                                          &downloadError,
                                          prepared.resourceKey)) {
            setLastError(downloadError);
            if (error) {
                *error = downloadError;
            }
            return false;
        }

        if (error) {
            error->clear();
        }
        return true;
    }

    bool supportsLocalFileBatchMaterialize() const override { return true; }

    bool copyToLocalFiles(const QVector<LocalFileMaterializeItem> &items,
                          const std::function<bool(const QString &currentSourcePath, qint64 processedBytes, qint64 totalBytes)> &progress,
                          QString *error) const override
    {
        clearLastError();
        if (items.isEmpty()) {
            if (error) {
                error->clear();
            }
            return true;
        }

        QString authError;
        QElapsedTimer authTimer;
        authTimer.start();
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            setLastError(authError);
            if (error) {
                *error = authError;
            }
            return false;
        }

        QVector<GDrivePreparedDownloadItem> prepared;
        prepared.reserve(items.size());
        QElapsedTimer prepareTimer;
        prepareTimer.start();
        for (const LocalFileMaterializeItem &item : items) {
            GDrivePreparedDownloadItem preparedItem;
            QString prepareError;
            if (!prepareDownloadItem(item, &preparedItem, &prepareError)) {
                setLastError(prepareError);
                if (error) {
                    *error = prepareError;
                }
                return false;
            }
            QFile::remove(preparedItem.partialPath);
            prepared.push_back(preparedItem);
        }

        QString transferError;
        if (!GDriveTransferClient::downloadFiles(prepared,
                                                 accessToken,
                                                 authTimer.elapsed(),
                                                 prepareTimer.elapsed(),
                                                 progress,
                                                 &transferError)) {
            setLastError(transferError);
            if (error) {
                *error = transferError;
            }
            return false;
        }
        if (error) {
            error->clear();
        }
        return true;
    }

    bool supportsLocalFileBatchCopy() const override { return true; }

    bool copyFromLocalFiles(const QVector<LocalFileCopyItem> &items,
                            const std::function<bool(const QString &currentFilePath, qint64 processedBytes, qint64 totalBytes)> &progress,
                            QString *error) const override
    {
        clearLastError();
        if (items.isEmpty()) {
            if (error) {
                error->clear();
            }
            return true;
        }

        const bool logging = uploadLoggingEnabled();
        QElapsedTimer prepareTimer;
        prepareTimer.start();
        if (logging) {
            qInfo() << "GDrive parallel upload scheduler preparing"
                    << "files" << items.size();
        }

        QString authError;
        QElapsedTimer authTimer;
        authTimer.start();
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            setLastError(authError);
            if (error) {
                *error = authError;
            }
            if (logging) {
                qInfo() << "GDrive parallel upload scheduler prepare failed"
                        << "phase" << "auth"
                        << "ms" << authTimer.elapsed()
                        << "message" << authError.left(180);
            }
            return false;
        }
        if (logging) {
            qInfo() << "GDrive parallel upload scheduler auth ready"
                    << "ms" << authTimer.elapsed();
        }

        QVector<GDriveTransferClient::BatchUploadItem> prepared;
        prepared.reserve(items.size());
        QElapsedTimer targetTimer;
        targetTimer.start();
        for (const LocalFileCopyItem &item : items) {
            const QFileInfo sourceInfo(item.sourceFilePath);
            if (!sourceInfo.isFile()) {
                const QString message = QStringLiteral("Google Drive upload source is not a regular file");
                setLastError(message);
                if (error) {
                    *error = message;
                }
                if (logging) {
                    qInfo() << "GDrive parallel upload scheduler prepare failed"
                            << "phase" << "source"
                            << "ms" << prepareTimer.elapsed()
                            << "source" << item.sourceFilePath
                            << "message" << message;
                }
                return false;
            }
            QString targetError;
            const GDriveCreateTarget target = createTargetForPath(normalizedPath(item.destinationPath), &targetError);
            if (!target.valid()) {
                const QString message = targetError.isEmpty()
                    ? QStringLiteral("Google Drive upload target is invalid")
                    : targetError;
                setLastError(message);
                if (error) {
                    *error = message;
                }
                if (logging) {
                    qInfo() << "GDrive parallel upload scheduler prepare failed"
                            << "phase" << "target"
                            << "ms" << prepareTimer.elapsed()
                            << "destination" << item.destinationPath
                            << "message" << message.left(180);
                }
                return false;
            }

            LocalFileCopyItem normalizedItem = item;
            normalizedItem.size = sourceInfo.size();
            prepared.push_back(GDriveTransferClient::BatchUploadItem{
                normalizedItem,
                target.parentPath,
                target.parentId,
                target.name
            });
        }

        QString transferError;
        const bool uploaded = GDriveTransferClient::uploadFiles(
            prepared,
            accessToken,
            targetTimer.elapsed(),
            prepareTimer.elapsed(),
            progress,
            [this, &prepared](qsizetype index, const QJsonObject &createdObject) {
                const GDriveTransferClient::BatchUploadItem uploadItem = prepared.at(index);
                const FileEntry entry = cacheDriveFileObject(createdObject, uploadItem.parentPath);
                if (!entry.path.isEmpty()) {
                    m_createdPaths.insert(normalizedPath(uploadItem.item.destinationPath), entry.path);
                }
            },
            &transferError);
        if (!uploaded) {
            setLastError(transferError);
            if (error) {
                *error = transferError;
            }
            return false;
        }
        markStorageQuotaRefreshPending();
        if (error) {
            error->clear();
        }
        return true;
    }

    bool copyFromLocalFile(const QString &sourceFilePath,
                           const QString &destinationPath,
                           const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                           QString *error) const override
    {
        clearLastError();

        const QFileInfo sourceInfo(sourceFilePath);
        if (!sourceInfo.isFile()) {
            const QString message = QStringLiteral("Google Drive upload source is not a regular file");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }

        QString targetError;
        const GDriveCreateTarget target = createTargetForPath(normalizedPath(destinationPath), &targetError);
        if (!target.valid()) {
            const QString message = targetError.isEmpty()
                ? QStringLiteral("Google Drive upload target is invalid")
                : targetError;
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            setLastError(authError);
            if (error) {
                *error = authError;
            }
            return false;
        }

        QJsonObject createdObject;
        QString uploadError;
        int retryCount = 0;
        if (!uploadFileBlockingWithRetry(m_network,
                                                     sourceFilePath,
                                                     target.parentId,
                                                     target.name,
                                                     accessToken,
                                                     progress,
                                                     &createdObject,
                                                     &uploadError,
                                                     &retryCount)) {
            setLastError(uploadError);
            if (error) {
                *error = uploadError;
            }
            return false;
        }

        const FileEntry entry = cacheDriveFileObject(createdObject, target.parentPath);
        if (!entry.path.isEmpty()) {
            m_createdPaths.insert(normalizedPath(destinationPath), entry.path);
        }
        markStorageQuotaRefreshPending();
        if (error) {
            error->clear();
        }
        return true;
    }

    std::unique_ptr<QIODevice> openWrite(const QString &, bool truncate = true) const override
    {
        Q_UNUSED(truncate)
        setLastError(QStringLiteral("Google Drive upload uses direct local file transfer"));
        return nullptr;
    }

    bool renamePath(const QString &, const QString &) override
    {
        setLastError(QStringLiteral("Google Drive rename is not supported"));
        return false;
    }

    bool createFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        return createDriveFolder(parentPath, name, createdPath);
    }

    bool createFile(const QString &, const QString &, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        setLastError(QStringLiteral("Google Drive creates files by uploading local files"));
        return false;
    }

    void flushPendingStorageInfoRefresh() const override
    {
        if (!m_storageQuotaRefreshPending) {
            return;
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            return;
        }

        if (refreshStorageQuotaBlocking(m_network, accessToken, nullptr)) {
            m_storageQuotaRefreshPending = false;
        }
    }

    QVariantMap storageInfo(const QString &) const override
    {
        flushPendingStorageInfoRefresh();
        auto quota = sharedQuota();
        if (!quota) {
            QString authError;
            const QString accessToken = accessTokenForBlockingRequest(&authError);
            if (!accessToken.isEmpty()) {
                refreshStorageQuotaBlocking(m_network, accessToken, nullptr);
                quota = sharedQuota();
            }
        }
        return quota ? storageInfoForQuota(*quota) : QVariantMap{};
    }

    QString lastErrorString() const override { return m_lastError; }
    void clearLastError() const override { m_lastError.clear(); }

private:
    void markStorageQuotaRefreshPending() const
    {
        m_storageQuotaRefreshPending = true;
    }

    QString resolveCreatedPath(const QString &path) const
    {
        QString current = path;
        QSet<QString> seen;
        while (m_createdPaths.contains(current) && !seen.contains(current)) {
            seen.insert(current);
            current = m_createdPaths.value(current);
        }
        return current;
    }

    bool isShortcutsReadOnlyPath(const QString &path) const
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        if (GDrivePath::isShortcutsViewPath(normalized)) {
            return true;
        }
        const QString parent = m_parents.value(normalized, sharedParent(normalized));
        return GDrivePath::isShortcutsViewPath(parent);
    }

    bool isTrashReadOnlyPath(const QString &path) const
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        if (normalized == GDrivePath::Trash) {
            return true;
        }

        QString current = normalized;
        QSet<QString> seen;
        while (!current.isEmpty() && !seen.contains(current)) {
            seen.insert(current);
            const QString parent = m_parents.value(current, sharedParent(current));
            if (parent == GDrivePath::Trash) {
                return true;
            }
            current = parent;
        }
        return false;
    }

    bool isServiceReadOnlyPath(const QString &path) const
    {
        return isShortcutsReadOnlyPath(path) || isTrashReadOnlyPath(path);
    }

    bool canCreateInFolder(const QString &folderPath) const
    {
        const QString normalized = resolveCreatedPath(normalizedPath(folderPath));
        if (GDrivePath::isShortcutsViewPath(normalized) || isTrashReadOnlyPath(normalized)) {
            return false;
        }
        if (normalized == GDrivePath::MyDrive) {
            return true;
        }
        if (normalized == GDrivePath::Root || normalized == GDrivePath::SharedWithMe || normalized.isEmpty()) {
            return false;
        }

        const auto localCapabilities = m_itemCapabilities.constFind(normalized);
        if (localCapabilities != m_itemCapabilities.constEnd()) {
            return localCapabilities->canAddChildren;
        }
        const std::optional<GDriveItemCapabilities> capabilities = sharedCapabilities(normalized);
        return capabilities && capabilities->canAddChildren;
    }

    GDriveCreateTarget createTargetForPath(const QString &path, QString *error) const
    {
        const QString normalized = normalizedPath(path);
        const GDrivePath::PendingPath pending = GDrivePath::pendingPathInfo(normalized);
        if (pending.valid()) {
            const QString parentPath = GDrivePath::parentPathForDriveParentId(pending.parentId);
            if (!canCreateInFolder(parentPath)) {
                if (error) {
                    *error = QStringLiteral("Google Drive does not allow creating items in this folder");
                }
                return {};
            }
            return {parentPath, pending.parentId, pending.name};
        }

        const auto removed = m_removedTargets.constFind(normalized);
        if (removed != m_removedTargets.constEnd() && removed->valid()) {
            if (!canCreateInFolder(removed->parentPath)) {
                if (error) {
                    *error = QStringLiteral("Google Drive does not allow recreating this item");
                }
                return {};
            }
            return removed.value();
        }

        if (error) {
            *error = QStringLiteral("Google Drive destination must be a new item path");
        }
        return {};
    }

    void cacheShortcutAliasEntry(const FileEntry &shortcutEntry, const QString &parentPath) const
    {
        const std::optional<FileEntry> aliasEntry = shortcutAliasEntryFor(shortcutEntry);
        if (!aliasEntry) {
            return;
        }

        const GDriveItemCapabilities capabilities = shortcutAliasCapabilities();
        m_entries.insert(aliasEntry->path, *aliasEntry);
        m_parents.insert(aliasEntry->path, parentPath);
        m_mimeTypes.insert(aliasEntry->path, QString(GoogleDriveFolderMime));
        m_itemCapabilities.insert(aliasEntry->path, capabilities);
        cacheSharedEntry(*aliasEntry, parentPath, QString(GoogleDriveFolderMime), capabilities);
    }

    void cacheShortcutEntryInRoot(const FileEntry &shortcutEntry, const GDriveItemCapabilities &capabilities) const
    {
        if (!shortcutEntry.isShortcut || shortcutEntry.path.isEmpty()) {
            return;
        }

        const GDriveItemCapabilities viewCapabilities = shortcutViewCapabilities(capabilities);
        const FileEntry viewEntry = shortcutViewEntry(shortcutEntry, capabilities);
        m_entries.insert(viewEntry.path, viewEntry);
        m_parents.insert(viewEntry.path, QString(GDrivePath::ShortcutsRoot));
        m_mimeTypes.insert(viewEntry.path, QString(GoogleDriveShortcutMime));
        m_itemCapabilities.insert(viewEntry.path, viewCapabilities);
        QStringList shortcutChildren = m_children.value(QString(GDrivePath::ShortcutsRoot));
        if (!shortcutChildren.contains(viewEntry.path)) {
            shortcutChildren.append(viewEntry.path);
            m_children.insert(QString(GDrivePath::ShortcutsRoot), shortcutChildren);
        }
        cacheSharedShortcutInRoot(viewEntry, capabilities);
    }

    FileEntry cacheDriveFileObject(const QJsonObject &fileObject, const QString &parentPath) const
    {
        const FileEntry entry = entryFromDriveFileObject(fileObject);
        if (entry.path.isEmpty()) {
            return {};
        }

        const QString mimeType = fileObject.value(QStringLiteral("mimeType")).toString();
        const GDriveItemCapabilities itemCapabilities = driveCapabilitiesFromDriveFileObject(fileObject);
        const QString thumbnailLink = fileObject.value(QStringLiteral("thumbnailLink")).toString().trimmed();
        m_entries.insert(entry.path, entry);
        m_parents.insert(entry.path, parentPath);
        m_mimeTypes.insert(entry.path, mimeType);
        m_itemCapabilities.insert(entry.path, itemCapabilities);

        QStringList children = m_children.value(parentPath);
        if (!children.contains(entry.path)) {
            children.append(entry.path);
        }
        m_children.insert(parentPath, children);
        cacheSharedEntry(entry, parentPath, mimeType, itemCapabilities);
        cacheSharedThumbnailLink(entry.path, thumbnailLink);
        cacheShortcutAliasEntry(entry, parentPath);
        cacheSharedChildren(parentPath, children);
        return entry;
    }

    void removeCachedPath(const QString &path) const
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        const QString parent = m_parents.value(normalized, sharedParent(normalized));
        const std::optional<FileEntry> entry = entryInfo(normalized);
        const QString shortcutAliasPath = entry && entry->isShortcut ? entry->shortcutOpenPath : QString{};
        m_entries.remove(normalized);
        m_parents.remove(normalized);
        m_mimeTypes.remove(normalized);
        m_itemCapabilities.remove(normalized);
        if (!parent.isEmpty()) {
            QStringList children = m_children.value(parent);
            if (children.isEmpty()) {
                children = sharedChildren(parent);
            }
            children.removeAll(normalized);
            m_children.insert(parent, children);
            cacheSharedChildren(parent, children);
        }
        m_children.remove(normalized);
        removeSharedPath(normalized, parent);
        if (!shortcutAliasPath.isEmpty()) {
            m_entries.remove(shortcutAliasPath);
            m_parents.remove(shortcutAliasPath);
            m_mimeTypes.remove(shortcutAliasPath);
            m_itemCapabilities.remove(shortcutAliasPath);
            m_children.remove(shortcutAliasPath);
            removeSharedPath(shortcutAliasPath, parent);
        }
    }

    bool createDriveFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) const
    {
        clearLastError();
        if (createdPath) {
            createdPath->clear();
        }

        const QString normalizedParent = resolveCreatedPath(normalizedPath(parentPath));
        const QString cleanName = name.trimmed();
        const QString parentId = GDrivePath::driveParentIdForPath(normalizedParent);
        if (parentId.isEmpty() || cleanName.isEmpty()) {
            setLastError(QStringLiteral("Google Drive folder target is invalid"));
            return false;
        }
        if (!canCreateInFolder(normalizedParent)) {
            setLastError(QStringLiteral("Google Drive does not allow creating folders here"));
            return false;
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            setLastError(authError);
            return false;
        }

        QJsonObject createdObject;
        QString error;
        if (!createMetadataBlocking(m_network,
                                         parentId,
                                         cleanName,
                                         QString(GoogleDriveFolderMime),
                                         accessToken,
                                         &createdObject,
                                         &error)) {
            setLastError(error);
            return false;
        }

        const FileEntry entry = cacheDriveFileObject(createdObject, normalizedParent);
        if (entry.path.isEmpty()) {
            setLastError(QStringLiteral("Google Drive create folder response is invalid"));
            return false;
        }
        if (createdPath) {
            *createdPath = entry.path;
        }
        markStorageQuotaRefreshPending();
        return true;
    }

    void cacheRootEntries()
    {
        GDriveItemCapabilities myDriveCapabilities;
        myDriveCapabilities.canListChildren = true;
        myDriveCapabilities.canAddChildren = true;

        GDriveItemCapabilities sharedWithMeCapabilities;
        sharedWithMeCapabilities.canListChildren = true;
        const GDriveItemCapabilities shortcutsCapabilities = shortcutsRootCapabilities();
        const GDriveItemCapabilities trashCapabilities = trashRootCapabilities();

        QList<FileEntry> entries;
        entries.append(virtualDirectoryEntry(QStringLiteral("My Drive"), QString(GDrivePath::MyDrive), myDriveCapabilities));
        entries.append(virtualDirectoryEntry(QStringLiteral("Shared with me"), QString(GDrivePath::SharedWithMe), sharedWithMeCapabilities));
        entries.append(virtualDirectoryEntry(QStringLiteral("Shortcuts"), QString(GDrivePath::ShortcutsRoot), shortcutsCapabilities));
        entries.append(virtualDirectoryEntry(QStringLiteral("Trash"), QString(GDrivePath::Trash), trashCapabilities));

        QStringList paths;
        paths.reserve(entries.size());
        for (const FileEntry &entry : entries) {
            m_entries.insert(entry.path, entry);
            m_parents.insert(entry.path, QString(GDrivePath::Root));
            m_mimeTypes.insert(entry.path, QString(GoogleDriveFolderMime));
            GDriveItemCapabilities capabilities = shortcutsCapabilities;
            if (entry.path == GDrivePath::MyDrive) {
                capabilities = myDriveCapabilities;
            } else if (entry.path == GDrivePath::SharedWithMe) {
                capabilities = sharedWithMeCapabilities;
            } else if (entry.path == GDrivePath::Trash) {
                capabilities = trashCapabilities;
            }
            m_itemCapabilities.insert(entry.path, capabilities);
            cacheSharedEntry(entry, QString(GDrivePath::Root), QString(GoogleDriveFolderMime), capabilities);
            paths.append(entry.path);
        }
        m_children.insert(QString(GDrivePath::Root), paths);
        cacheSharedChildren(QString(GDrivePath::Root), paths);
    }

    void emitRootEntries(int generation)
    {
        if (generation != currentGeneration()) {
            return;
        }
        QList<FileEntry> entries;
        const QStringList paths = m_children.value(QString(GDrivePath::Root));
        entries.reserve(paths.size());
        for (const QString &path : paths) {
            entries.append(m_entries.value(path));
        }
        emit batchReady(entries, generation);
    }

    void ensureAuthorizedAndList(int generation, const QString &path, const QString &pageToken)
    {
        if (generation != currentGeneration()) {
            return;
        }

        if (!validSessionAccessToken().isEmpty()) {
            requestFileList(generation, path, pageToken);
            return;
        }

        if (m_oauth && m_oauth->status() != QAbstractOAuth::Status::NotAuthenticated) {
            return;
        }

        OAuthClientConfig config = loadOAuthClientConfig();
        if (!config.valid()) {
            finish(generation, false, config.error);
            return;
        }

        const QString refreshToken = sessionRefreshToken();
        if (!refreshToken.isEmpty()) {
            startTokenRefresh(generation, path, pageToken, config, refreshToken);
            return;
        }

        startBrowserAuthorization(generation, path, pageToken, config);
    }

    void configureOAuth(const OAuthClientConfig &config)
    {
        m_authCompletionGeneration = -1;
        m_oauth = std::make_unique<QOAuth2AuthorizationCodeFlow>(&m_network);
        m_oauth->setAuthorizationUrl(config.authorizationUrl);
        m_oauth->setTokenUrl(config.tokenUrl);
        m_oauth->setClientIdentifier(config.clientId);
        if (!config.clientSecret.isEmpty()) {
            m_oauth->setClientIdentifierSharedKey(config.clientSecret);
        }
        m_oauth->setRequestedScopeTokens({QByteArray(GDriveAuth::GoogleDriveScope.data(), GDriveAuth::GoogleDriveScope.size())});
        m_oauth->setPkceMethod(QOAuth2AuthorizationCodeFlow::PkceMethod::S256);
        m_oauth->setModifyParametersFunction([](QAbstractOAuth::Stage stage, QMultiMap<QString, QVariant> *parameters) {
            if (stage != QAbstractOAuth::Stage::RequestingAuthorization || !parameters) {
                return;
            }
            parameters->insert(QStringLiteral("access_type"), QStringLiteral("offline"));
            parameters->insert(QStringLiteral("include_granted_scopes"), QStringLiteral("true"));
            parameters->insert(QStringLiteral("prompt"), QStringLiteral("consent"));
        });
    }

    void connectOAuthCompletion(int generation,
                                const QString &path,
                                const QString &pageToken,
                                bool completeOnTokenChanged)
    {
        QObject::connect(m_oauth.get(), &QAbstractOAuth::granted, this, [this, generation, path, pageToken]() {
            handleOAuthTokenReady(generation, path, pageToken);
        });

        if (completeOnTokenChanged) {
            QObject::connect(m_oauth.get(), &QAbstractOAuth::tokenChanged, this, [this, generation, path, pageToken](const QString &) {
                handleOAuthTokenReady(generation, path, pageToken);
            });
        }

        QObject::connect(m_oauth.get(), &QAbstractOAuth::requestFailed, this, [this, generation](QAbstractOAuth::Error) {
            if (generation != currentGeneration()) {
                return;
            }
            finish(generation, false, QStringLiteral("Google OAuth request failed"));
        });

        QObject::connect(m_oauth.get(),
                         &QAbstractOAuth2::serverReportedErrorOccurred,
                         this,
                         [this, generation](const QString &error, const QString &description, const QUrl &) {
                             if (generation != currentGeneration()) {
                                 return;
                             }
                             const QString message = description.trimmed().isEmpty()
                                 ? QStringLiteral("Google OAuth failed: %1").arg(error)
                                 : QStringLiteral("Google OAuth failed: %1").arg(description.trimmed());
                             finish(generation, false, message);
                         });
    }

    void startBrowserAuthorization(int generation,
                                   const QString &path,
                                   const QString &pageToken,
                                   const OAuthClientConfig &config)
    {
        configureOAuth(config);

        auto *replyHandler = new QOAuthHttpServerReplyHandler(QHostAddress::LocalHost, 0, m_oauth.get());
        replyHandler->setCallbackText(QStringLiteral("FMQml Google Drive authorization completed. You can close this window."));
        if (!replyHandler->isListening()) {
            finish(generation, false, QStringLiteral("Cannot start Google OAuth loopback listener"));
            return;
        }
        m_oauth->setReplyHandler(replyHandler);

        QObject::connect(m_oauth.get(), &QAbstractOAuth::authorizeWithBrowser, this, [this, generation](const QUrl &url) {
            if (generation != currentGeneration()) {
                return;
            }
            if (!QDesktopServices::openUrl(url)) {
                finish(generation, false, QStringLiteral("Cannot open Google OAuth browser"));
            }
        });

        connectOAuthCompletion(generation, path, pageToken, false);
        m_oauth->grant();
    }

    void startTokenRefresh(int generation,
                           const QString &path,
                           const QString &pageToken,
                           const OAuthClientConfig &config,
                           const QString &refreshToken)
    {
        configureOAuth(config);
        connectOAuthCompletion(generation, path, pageToken, true);
        m_oauth->setRefreshToken(refreshToken);
        m_oauth->refreshTokens();
    }

    void handleOAuthTokenReady(int generation, const QString &path, const QString &pageToken)
    {
        if (!m_oauth || generation != currentGeneration() || m_oauth->token().isEmpty()) {
            return;
        }
        if (m_authCompletionGeneration == generation) {
            return;
        }

        rememberAccessToken(m_oauth->token(), m_oauth->expirationAt());
        const QString refreshToken = m_oauth->refreshToken().trimmed().isEmpty()
            ? m_oauth->extraTokens().value(QStringLiteral("refresh_token")).toString().trimmed()
            : m_oauth->refreshToken().trimmed();
        if (!rememberRefreshToken(refreshToken)) {
            finish(generation, false, QStringLiteral("Cannot save Google Drive authorization"));
            return;
        }

        m_authCompletionGeneration = generation;
        requestFileList(generation, path, pageToken);
    }

    void requestFileList(int generation, const QString &path, const QString &pageToken)
    {
        const QString accessToken = validSessionAccessToken();
        if (accessToken.isEmpty() || generation != currentGeneration()) {
            return;
        }

        const QString query = driveQueryForPath(path);
        if (query.isEmpty()) {
            finish(generation, false, QStringLiteral("Google Drive folder is not available"));
            return;
        }

        QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/files"));
        QUrlQuery urlQuery;
        urlQuery.addQueryItem(QStringLiteral("q"), query);
        urlQuery.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("200"));
        urlQuery.addQueryItem(QStringLiteral("fields"), QString(DriveListFields));
        urlQuery.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
        urlQuery.addQueryItem(QStringLiteral("includeItemsFromAllDrives"), QStringLiteral("true"));
        if (!pageToken.isEmpty()) {
            urlQuery.addQueryItem(QStringLiteral("pageToken"), pageToken);
        }
        url.setQuery(urlQuery);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
        QNetworkReply *reply = m_network.get(request);
        m_activeReply = reply;

        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, generation, path, pageToken]() {
            if (m_activeReply == reply) {
                m_activeReply.clear();
            }
            if (generation != currentGeneration()) {
                reply->deleteLater();
                return;
            }

            const QByteArray body = safeReadAll(reply);
            const QNetworkReply::NetworkError networkError = reply->error();
            const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            reply->deleteLater();

            if (networkError != QNetworkReply::NoError) {
                const QString fallback = status > 0
                    ? QStringLiteral("Google Drive request failed with HTTP %1").arg(status)
                    : QStringLiteral("Google Drive request failed");
                finish(generation, false, errorMessage(body, fallback));
                return;
            }

            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
                finish(generation, false, QStringLiteral("Google Drive response is invalid"));
                return;
            }

            if (pageToken.isEmpty()) {
                m_children.insert(path, {});
                cacheSharedChildren(path, {});
            }

            const QJsonObject root = document.object();
            QList<FileEntry> entries;
            const QJsonArray files = root.value(QStringLiteral("files")).toArray();
            entries.reserve(files.size());
            QStringList childPaths = m_children.value(path);
            for (const QJsonValue &value : files) {
                const QJsonObject fileObject = value.toObject();
                const FileEntry entry = entryFromDriveFile(fileObject, path);
                if (entry.path.isEmpty()) {
                    continue;
                }
                const QString mimeType = fileObject.value(QStringLiteral("mimeType")).toString();
                const GDriveItemCapabilities itemCapabilities = driveCapabilitiesFromDriveFileObject(fileObject);
                const bool trashContext = isSharedTrashViewPath(path);
                if (!trashContext && entry.isShortcut && path != GDrivePath::ShortcutsRoot) {
                    cacheShortcutEntryInRoot(entry, itemCapabilities);
                    continue;
                }
                const GDriveItemCapabilities effectiveCapabilities = trashContext
                    ? trashViewCapabilities(itemCapabilities)
                    : entry.isShortcut
                    ? shortcutViewCapabilities(itemCapabilities)
                    : itemCapabilities;
                const FileEntry effectiveEntry = trashContext
                    ? trashViewEntry(entry, itemCapabilities)
                    : entry.isShortcut
                    ? shortcutViewEntry(entry, itemCapabilities)
                    : entry;
                const QString parentPath = !trashContext && entry.isShortcut ? QString(GDrivePath::ShortcutsRoot) : path;
                m_entries.insert(effectiveEntry.path, effectiveEntry);
                m_parents.insert(effectiveEntry.path, parentPath);
                m_mimeTypes.insert(effectiveEntry.path, mimeType);
                m_itemCapabilities.insert(effectiveEntry.path, effectiveCapabilities);
                cacheSharedEntry(effectiveEntry, parentPath, mimeType, effectiveCapabilities);
                cacheSharedThumbnailLink(effectiveEntry.path,
                                         fileObject.value(QStringLiteral("thumbnailLink")).toString().trimmed());
                if (!trashContext) {
                    cacheShortcutAliasEntry(effectiveEntry, parentPath);
                }
                childPaths.append(effectiveEntry.path);
                entries.append(effectiveEntry);
            }
            m_children.insert(path, childPaths);
            cacheSharedChildren(path, childPaths);

            const QString nextPageToken = root.value(QStringLiteral("nextPageToken")).toString();

            if (!entries.isEmpty()) {
                emit batchReady(entries, generation);
            }

            if (!nextPageToken.isEmpty()) {
                requestFileList(generation, path, nextPageToken);
                return;
            }

            requestStorageQuotaInBackground(generation);
            finish(generation, true, {});
        });
    }

    bool requestStorageQuotaInBackground(int generation)
    {
        const QString accessToken = validSessionAccessToken();
        if (accessToken.isEmpty() || generation != currentGeneration()) {
            return false;
        }

        QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/about"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("fields"), QString(DriveAboutFields));
        url.setQuery(query);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());

        QNetworkReply *reply = m_network.get(request);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
            if (generation != currentGeneration()) {
                reply->deleteLater();
                return;
            }

            const QByteArray body = safeReadAll(reply);
            const QNetworkReply::NetworkError networkError = reply->error();
            reply->deleteLater();

            if (networkError == QNetworkReply::NoError) {
                QJsonParseError parseError;
                const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
                if (parseError.error == QJsonParseError::NoError && document.isObject()) {
                    const QJsonObject aboutObject = document.object();
                    rememberAccountInfoFromAboutObject(aboutObject);
                    cacheSharedQuota(quotaFromAboutObject(aboutObject));
                }
            }
        });
        return true;
    }

    FileEntry entryFromDriveFile(const QJsonObject &object, const QString &parentPath) const
    {
        Q_UNUSED(parentPath)
        return entryFromDriveFileObject(object);
    }

    std::optional<FileEntry> resolveFileShortcutTarget(const FileEntry &shortcutEntry, QString *error) const
    {
        if (!shortcutEntry.isShortcut
            || shortcutEntry.shortcutTargetIsDirectory
            || shortcutEntry.shortcutTargetPath.isEmpty()) {
            return std::nullopt;
        }

        if (const std::optional<FileEntry> cachedTarget = sharedEntry(shortcutEntry.shortcutTargetPath)) {
            if (error) {
                error->clear();
            }
            return cachedTarget;
        }

        const QString targetId = GDrivePath::idForItemPath(shortcutEntry.shortcutTargetPath);
        if (targetId.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Google Drive shortcut target path is invalid");
            }
            return std::nullopt;
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            if (error) {
                *error = authError;
            }
            return std::nullopt;
        }

        QJsonObject targetObject;
        if (!fetchFileMetadataBlocking(m_network,
                                            targetId,
                                            accessToken,
                                            shortcutEntry.shortcutTargetResourceKey,
                                            &targetObject,
                                            error)) {
            return std::nullopt;
        }

        FileEntry targetEntry = entryFromDriveFileObject(targetObject);
        if (targetEntry.path.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Google Drive shortcut target metadata is invalid");
            }
            return std::nullopt;
        }

        const QString mimeType = targetObject.value(QStringLiteral("mimeType")).toString();
        const GDriveItemCapabilities capabilities = driveCapabilitiesFromDriveFileObject(targetObject);
        const QString thumbnailLink = targetObject.value(QStringLiteral("thumbnailLink")).toString().trimmed();
        QString parentPath = sharedParent(targetEntry.path);
        const QJsonArray parents = targetObject.value(QStringLiteral("parents")).toArray();
        if (parentPath.isEmpty() && !parents.isEmpty()) {
            parentPath = GDrivePath::parentPathForDriveParentId(parents.first().toString());
        }
        cacheSharedEntry(targetEntry, parentPath, mimeType, capabilities);
        cacheSharedThumbnailLink(targetEntry.path, thumbnailLink);
        m_entries.insert(targetEntry.path, targetEntry);
        if (!parentPath.isEmpty()) {
            m_parents.insert(targetEntry.path, parentPath);
        }
        m_mimeTypes.insert(targetEntry.path, mimeType);
        m_itemCapabilities.insert(targetEntry.path, capabilities);

        if (error) {
            error->clear();
        }
        return targetEntry;
    }

    void finish(int generation, bool success, const QString &error)
    {
        if (generation != currentGeneration()) {
            return;
        }
        if (!success) {
            setLastError(error);
        }
        m_running.store(false);
        emit finished(m_currentPath, success, generation, error);
    }

    bool failReadOnly() const
    {
        setLastError(QStringLiteral("Google Drive operation is not supported"));
        return false;
    }

    void setLastError(const QString &error) const
    {
        m_lastError = error;
    }

    QString m_currentPath = QString(GDrivePath::Root);
    std::atomic<int> m_generation{0};
    std::atomic_bool m_running{false};
    bool m_showHidden = false;
    mutable QString m_lastError;
    mutable QNetworkAccessManager m_network;
    std::unique_ptr<QOAuth2AuthorizationCodeFlow> m_oauth;
    QPointer<QNetworkReply> m_activeReply;
    mutable QHash<QString, FileEntry> m_entries;
    mutable QHash<QString, QStringList> m_children;
    mutable QHash<QString, QString> m_parents;
    mutable QHash<QString, QString> m_mimeTypes;
    mutable QHash<QString, GDriveItemCapabilities> m_itemCapabilities;
    mutable QHash<QString, QString> m_createdPaths;
    mutable QHash<QString, GDriveCreateTarget> m_removedTargets;
    mutable bool m_storageQuotaRefreshPending = false;
    int m_authCompletionGeneration = -1;
};

} // namespace

namespace GDriveFileProviderInternal {

std::unique_ptr<FileProvider> createProvider()
{
    return std::make_unique<GDriveFileProvider>();
}

bool isTrashViewPath(const QString &path)
{
    return isSharedTrashViewPath(path);
}

std::optional<ExportTarget> exportTargetForPath(const QString &path)
{
    const std::optional<GDriveEntryMapper::ExportTarget> target = googleAppsExportTargetForPath(path);
    if (!target) {
        return std::nullopt;
    }
    return ExportTarget{target->sourcePath, target->displayName, target->mimeType};
}

QVariantList capabilitiesProperties(const GDriveItemCapabilities &capabilities)
{
    return driveCapabilitiesProperties(capabilities);
}

bool downloadFileToLocalFile(QNetworkAccessManager &network,
                             const QString &sourcePath,
                             const QString &mimeType,
                             const QString &destinationFilePath,
                             const QString &accessToken,
                             const std::function<bool(qint64, qint64)> &progress,
                             QString *error,
                             const QString &resourceKey)
{
    return GDriveTransferClient::downloadFileToLocalFile(network,
                                                         sourcePath,
                                                         mimeType,
                                                         destinationFilePath,
                                                         accessToken,
                                                         progress,
                                                         error,
                                                         resourceKey);
}

bool restoreFileBlocking(QNetworkAccessManager &network,
                         const QString &fileId,
                         const QString &accessToken,
                         QJsonObject *restoredObject,
                         QString *error)
{
    return GDriveApiClient::restoreFileBlocking(network, fileId, accessToken, restoredObject, error);
}

} // namespace GDriveFileProviderInternal
