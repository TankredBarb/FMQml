#include "GDriveApiClient.h"

#include "GDriveCache.h"
#include "GDriveEntryMapper.h"
#include "GDrivePath.h"
#include "GDriveRequestPolicy.h"

#include <QDateTime>
#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMimeDatabase>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

namespace GDriveApiClient {
namespace {
constexpr QLatin1StringView DriveListFields{
    "nextPageToken,files(id,name,mimeType,size,modifiedTime,createdTime,parents,webViewLink,ownedByMe,shared,"
    "thumbnailLink,shortcutDetails(targetId,targetMimeType,targetResourceKey),"
    "capabilities(canDownload,canEdit,canAddChildren,canListChildren,canRename,canTrash,canDelete,canCopy))"};
constexpr QLatin1StringView DriveFileFields{
    "id,name,mimeType,size,modifiedTime,createdTime,parents,webViewLink,ownedByMe,shared,"
    "thumbnailLink,"
    "shortcutDetails(targetId,targetMimeType,targetResourceKey),"
    "capabilities(canDownload,canEdit,canAddChildren,canListChildren,canRename,canTrash,canDelete,canCopy)"};
constexpr QLatin1StringView DriveAboutFields{"storageQuota(limit,usage),user(displayName,emailAddress)"};
constexpr int DriveApiMaxAttempts = 3;
using GDriveAuth::AccountInfo;
using GDriveAuth::accessTokenForBlockingRequest;
using GDriveAuth::rememberAccountInfo;
using GDriveCache::cacheSharedChildren;
using GDriveCache::cacheSharedEntry;
using GDriveCache::cacheSharedQuota;
using GDriveCache::cacheSharedThumbnailLink;
using GDriveEntryMapper::cacheSharedShortcutAlias;
using GDriveEntryMapper::cacheSharedShortcutInRoot;
using GDriveEntryMapper::driveQueryForPath;
using GDriveEntryMapper::driveCapabilitiesFromDriveFileObject;
using GDriveEntryMapper::entryFromDriveFileObject;
using GDriveEntryMapper::isSharedTrashViewPath;
using GDriveEntryMapper::shortcutViewCapabilities;
using GDriveEntryMapper::shortcutViewEntry;
using GDriveEntryMapper::trashViewCapabilities;
using GDriveEntryMapper::trashViewEntry;

QString byteSizeText(qint64 size)
{
    return QLocale().formattedDataSize(size);
}

QByteArray resourceKeyHeaderValue(const QString &path, const QString &resourceKey)
{
    const QString cleanResourceKey = resourceKey.trimmed();
    if (cleanResourceKey.isEmpty()) {
        return {};
    }
    const QString fileId = GDrivePath::idForItemPath(path).trimmed();
    return fileId.isEmpty() ? QByteArray{} : QStringLiteral("%1/%2").arg(fileId, cleanResourceKey).toUtf8();
}

void applyResourceKeyHeader(QNetworkRequest *request, const QString &path, const QString &resourceKey)
{
    const QByteArray value = resourceKeyHeaderValue(path, resourceKey);
    if (request && !value.isEmpty()) {
        request->setRawHeader("X-Goog-Drive-Resource-Keys", value);
    }
}
} // namespace

QString errorMessage(const QByteArray &body, const QString &fallback)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const QJsonObject errorObject = document.object().value(QStringLiteral("error")).toObject();
    const QString message = errorObject.value(QStringLiteral("message")).toString().trimmed();
    if (!message.isEmpty()) {
        return message;
    }
    return fallback;
}

QByteArray safeReadAll(QIODevice *device)
{
    return device && device->isOpen() ? device->readAll() : QByteArray{};
}

QUrl driveFileMetadataUrl(const QString &fileId = {})
{
    QUrl url(fileId.isEmpty()
                 ? QStringLiteral("https://www.googleapis.com/drive/v3/files")
                 : QStringLiteral("https://www.googleapis.com/drive/v3/files/%1")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(fileId))));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("fields"), QString(DriveFileFields));
    url.setQuery(query);
    return url;
}

QNetworkRequest authorizedJsonRequest(const QUrl &url, const QString &accessToken)
{
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json; charset=utf-8"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

bool waitForReply(QNetworkReply *reply,
                  int timeoutMs,
                  QString timeoutMessage,
                  QByteArray *body,
                  QString *error,
                  QHash<QByteArray, QByteArray> *rawHeaders)
{
    QEventLoop loop;
    QTimer timeout;
    bool timedOut = false;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    if (timeoutMs > 0) {
        timeout.start(timeoutMs);
    }
    loop.exec();
    timeout.stop();

    if (body) {
        *body = safeReadAll(reply);
    }
    if (rawHeaders) {
        rawHeaders->clear();
        for (const QByteArray &header : reply->rawHeaderList()) {
            const QByteArray value = reply->rawHeader(header);
            rawHeaders->insert(header, value);
            rawHeaders->insert(header.toLower(), value);
        }
        const QVariant locationHeader = reply->header(QNetworkRequest::LocationHeader);
        if (locationHeader.isValid()) {
            const QUrl locationUrl = locationHeader.toUrl();
            const QByteArray value = locationUrl.isEmpty()
                ? locationHeader.toString().toUtf8()
                : locationUrl.toEncoded();
            if (!value.isEmpty()) {
                rawHeaders->insert(QByteArrayLiteral("location"), value);
            }
        }
    }
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorString = reply->errorString();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    delete reply;

    if (timedOut) {
        if (error) {
            *error = timeoutMessage;
        }
        return false;
    }
    if (networkError != QNetworkReply::NoError) {
        if (error) {
            const QString fallback = status > 0
                ? QStringLiteral("Google Drive request failed with HTTP %1").arg(status)
                : QStringLiteral("Google Drive request failed: %1").arg(networkErrorString);
            *error = errorMessage(body ? *body : QByteArray{}, fallback);
        }
        return false;
    }
    if (status >= 400) {
        if (error) {
            *error = errorMessage(body ? *body : QByteArray{},
                                       QStringLiteral("Google Drive request failed with HTTP %1").arg(status));
        }
        return false;
    }
    return true;
}

bool parseFileResponse(const QByteArray &body, QJsonObject *fileObject, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Google Drive file response is invalid");
        }
        return false;
    }
    if (fileObject) {
        *fileObject = document.object();
    }
    return true;
}

bool fetchFileMetadataBlocking(QNetworkAccessManager &network,
                                    const QString &fileId,
                                    const QString &accessToken,
                                    const QString &resourceKey,
                                    QJsonObject *fileObject,
                                    QString *error)
{
    if (fileId.trimmed().isEmpty()) {
        if (error) {
            *error = QStringLiteral("Google Drive target file id is empty");
        }
        return false;
    }

    QByteArray body;
    QNetworkRequest request = authorizedJsonRequest(driveFileMetadataUrl(fileId), accessToken);
    applyResourceKeyHeader(&request, GDrivePath::itemPathForId(fileId), resourceKey);
    QNetworkReply *reply = network.get(request);
    if (!waitForReply(reply, 30000, QStringLiteral("Google Drive file metadata request timed out"), &body, error)) {
        return false;
    }
    return parseFileResponse(body, fileObject, error);
}

GDriveStorageQuota quotaFromAboutObject(const QJsonObject &object)
{
    const QJsonObject quotaObject = object.value(QStringLiteral("storageQuota")).toObject();
    bool usageOk = false;
    bool limitOk = false;
    const qint64 used = quotaObject.value(QStringLiteral("usage")).toString().toLongLong(&usageOk);
    const qint64 total = quotaObject.value(QStringLiteral("limit")).toString().toLongLong(&limitOk);

    GDriveStorageQuota quota;
    quota.used = usageOk ? used : -1;
    quota.total = limitOk ? total : -1;
    quota.free = quota.total >= 0 && quota.used >= 0 ? (std::max<qint64>)(0, quota.total - quota.used) : -1;
    quota.valid = usageOk || limitOk;
    quota.cachedAt = QDateTime::currentDateTimeUtc();
    return quota;
}

AccountInfo accountInfoFromAboutObject(const QJsonObject &object)
{
    const QJsonObject userObject = object.value(QStringLiteral("user")).toObject();
    return {
        userObject.value(QStringLiteral("displayName")).toString().trimmed(),
        userObject.value(QStringLiteral("emailAddress")).toString().trimmed(),
    };
}

void rememberAccountInfoFromAboutObject(const QJsonObject &object)
{
    const AccountInfo accountInfo = accountInfoFromAboutObject(object);
    if (accountInfo.valid()) {
        rememberAccountInfo(accountInfo);
    }
}

bool refreshStorageQuotaBlocking(QNetworkAccessManager &network, const QString &accessToken, QString *error)
{
    QUrl url(QStringLiteral("https://www.googleapis.com/drive/v3/about"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fields"), QString(DriveAboutFields));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());

    QByteArray body;
    QNetworkReply *reply = network.get(request);
    if (!waitForReply(reply, 30000, QStringLiteral("Google Drive storage quota request timed out"), &body, error)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Google Drive storage quota response is invalid");
        }
        return false;
    }

    const QJsonObject aboutObject = document.object();
    rememberAccountInfoFromAboutObject(aboutObject);
    const GDriveStorageQuota quota = quotaFromAboutObject(aboutObject);
    cacheSharedQuota(quota);
    return quota.valid;
}

QVariantMap storageInfoForQuota(const GDriveStorageQuota &quota)
{
    if (!quota.valid) {
        return {};
    }

    const double percent = quota.total > 0 && quota.used >= 0
        ? static_cast<double>(quota.used) / static_cast<double>(quota.total)
        : 0.0;
    return {
        {QStringLiteral("valid"), true},
        {QStringLiteral("total"), quota.total},
        {QStringLiteral("free"), quota.free},
        {QStringLiteral("used"), quota.used},
        {QStringLiteral("percent"), percent},
        {QStringLiteral("totalStr"), quota.total >= 0 ? byteSizeText(quota.total) : QStringLiteral("unlimited or unknown")},
        {QStringLiteral("freeStr"), quota.free >= 0 ? byteSizeText(quota.free) : QStringLiteral("unknown")},
        {QStringLiteral("usedStr"), quota.used >= 0 ? byteSizeText(quota.used) : QStringLiteral("unknown")},
        {QStringLiteral("isCritical"), quota.total > 0 && quota.free >= 0 && (static_cast<double>(quota.free) / static_cast<double>(quota.total)) < 0.10},
    };
}

bool createMetadataBlocking(QNetworkAccessManager &network,
                                 const QString &parentId,
                                 const QString &name,
                                 const QString &mimeType,
                                 const QString &accessToken,
                                 QJsonObject *createdObject,
                                 QString *error)
{
    QString lastError;
    for (int attempt = 1; attempt <= DriveApiMaxAttempts; ++attempt) {
        GDriveRequestPolicy::waitForCooldown(QLatin1StringView("create"));
        QJsonObject metadata;
        metadata.insert(QStringLiteral("name"), name);
        metadata.insert(QStringLiteral("mimeType"), mimeType);
        metadata.insert(QStringLiteral("parents"), QJsonArray{parentId});

        QByteArray body;
        QHash<QByteArray, QByteArray> headers;
        QString attemptError;
        QNetworkReply *reply = network.post(authorizedJsonRequest(driveFileMetadataUrl(), accessToken),
                                            QJsonDocument(metadata).toJson(QJsonDocument::Compact));
        if (waitForReply(reply, 60000, QStringLiteral("Google Drive create request timed out"), &body, &attemptError, &headers)) {
            GDriveRequestPolicy::noteSuccess();
            return parseFileResponse(body, createdObject, error);
        }
        lastError = attemptError;
        if (attempt >= DriveApiMaxAttempts || !GDriveRequestPolicy::isRetryableError(attemptError)) {
            break;
        }
        GDriveRequestPolicy::noteThrottle(QLatin1StringView("create"), headers, attempt, attemptError);
    }
    if (error) {
        *error = lastError;
    }
    return false;
}

bool trashFileBlocking(QNetworkAccessManager &network,
                            const QString &fileId,
                            const QString &accessToken,
                            QJsonObject *trashedObject,
                            QString *error)
{
    QString lastError;
    for (int attempt = 1; attempt <= DriveApiMaxAttempts; ++attempt) {
        GDriveRequestPolicy::waitForCooldown(QLatin1StringView("trash"));
        QJsonObject metadata;
        metadata.insert(QStringLiteral("trashed"), true);

        QByteArray body;
        QHash<QByteArray, QByteArray> headers;
        QString attemptError;
        QNetworkReply *reply = network.sendCustomRequest(
            authorizedJsonRequest(driveFileMetadataUrl(fileId), accessToken),
            QByteArrayLiteral("PATCH"),
            QJsonDocument(metadata).toJson(QJsonDocument::Compact));
        if (waitForReply(reply, 60000, QStringLiteral("Google Drive delete request timed out"), &body, &attemptError, &headers)) {
            GDriveRequestPolicy::noteSuccess();
            return parseFileResponse(body, trashedObject, error);
        }
        lastError = attemptError;
        if (attempt >= DriveApiMaxAttempts || !GDriveRequestPolicy::isRetryableError(attemptError)) {
            break;
        }
        GDriveRequestPolicy::noteThrottle(QLatin1StringView("trash"), headers, attempt, attemptError);
    }
    if (error) {
        *error = lastError;
    }
    return false;
}

bool restoreFileBlocking(QNetworkAccessManager &network,
                              const QString &fileId,
                              const QString &accessToken,
                              QJsonObject *restoredObject,
                              QString *error)
{
    QString lastError;
    for (int attempt = 1; attempt <= DriveApiMaxAttempts; ++attempt) {
        GDriveRequestPolicy::waitForCooldown(QLatin1StringView("restore"));
        QJsonObject metadata;
        metadata.insert(QStringLiteral("trashed"), false);

        QByteArray body;
        QHash<QByteArray, QByteArray> headers;
        QString attemptError;
        QNetworkReply *reply = network.sendCustomRequest(
            authorizedJsonRequest(driveFileMetadataUrl(fileId), accessToken),
            QByteArrayLiteral("PATCH"),
            QJsonDocument(metadata).toJson(QJsonDocument::Compact));
        if (waitForReply(reply, 60000, QStringLiteral("Google Drive restore request timed out"), &body, &attemptError, &headers)) {
            GDriveRequestPolicy::noteSuccess();
            return parseFileResponse(body, restoredObject, error);
        }
        lastError = attemptError;
        if (attempt >= DriveApiMaxAttempts || !GDriveRequestPolicy::isRetryableError(attemptError)) {
            break;
        }
        GDriveRequestPolicy::noteThrottle(QLatin1StringView("restore"), headers, attempt, attemptError);
    }
    if (error) {
        *error = lastError;
    }
    return false;
}


QStringList listChildrenBlocking(QNetworkAccessManager &network, const QString &path, QString *error)
{
    const QString query = driveQueryForPath(path);
    if (query.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Google Drive folder is not available");
        }
        return {};
    }

    QString authError;
    const QString accessToken = accessTokenForBlockingRequest(&authError);
    if (accessToken.isEmpty()) {
        if (error) {
            *error = authError;
        }
        return {};
    }

    QStringList children;
    QString pageToken;

    do {
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

        QByteArray body;
        QString requestError;
        bool requestOk = false;
        for (int attempt = 1; attempt <= DriveApiMaxAttempts; ++attempt) {
            GDriveRequestPolicy::waitForCooldown(QLatin1StringView("list"));
            QNetworkRequest request(url);
            request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());

            QHash<QByteArray, QByteArray> headers;
            QNetworkReply *reply = network.get(request);
            requestOk = waitForReply(reply,
                                     60000,
                                     QStringLiteral("Google Drive folder listing timed out"),
                                     &body,
                                     &requestError,
                                     &headers);
            if (requestOk) {
                GDriveRequestPolicy::noteSuccess();
                break;
            }
            if (attempt >= DriveApiMaxAttempts || !GDriveRequestPolicy::isRetryableError(requestError)) {
                break;
            }
            GDriveRequestPolicy::noteThrottle(QLatin1StringView("list"), headers, attempt, requestError);
        }

        if (!requestOk) {
            if (error) {
                *error = requestError;
            }
            return {};
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (error) {
                *error = QStringLiteral("Google Drive folder listing response is invalid");
            }
            return {};
        }

        const QJsonObject root = document.object();
        const QJsonArray files = root.value(QStringLiteral("files")).toArray();
        for (const QJsonValue &value : files) {
            const QJsonObject fileObject = value.toObject();
            const FileEntry entry = entryFromDriveFileObject(fileObject);
            if (entry.path.isEmpty()) {
                continue;
            }
            const QString mimeType = fileObject.value(QStringLiteral("mimeType")).toString();
            const GDriveItemCapabilities itemCapabilities = driveCapabilitiesFromDriveFileObject(fileObject);
            const QString thumbnailLink = fileObject.value(QStringLiteral("thumbnailLink")).toString().trimmed();
            const bool trashContext = isSharedTrashViewPath(path);
            if (!trashContext && entry.isShortcut && path != GDrivePath::ShortcutsRoot) {
                cacheSharedShortcutInRoot(entry, itemCapabilities);
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
            cacheSharedEntry(effectiveEntry, parentPath, mimeType, effectiveCapabilities);
            cacheSharedThumbnailLink(effectiveEntry.path, thumbnailLink);
            if (!trashContext) {
                cacheSharedShortcutAlias(effectiveEntry, parentPath);
            }
            children.append(effectiveEntry.path);
        }

        pageToken = root.value(QStringLiteral("nextPageToken")).toString();
    } while (!pageToken.isEmpty());

    cacheSharedChildren(path, children);
    if (error) {
        error->clear();
    }
    return children;
}


} // namespace GDriveApiClient
