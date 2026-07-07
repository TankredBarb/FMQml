#include "GDriveFileProviderPlugin.h"

#include "GDriveAuth.h"
#include "GDriveCache.h"
#include "GDrivePath.h"
#include "GDriveTypes.h"

#include <algorithm>
#include <atomic>
#include <optional>

#include <QDateTime>
#include <QCache>
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
constexpr QLatin1StringView GoogleDriveSignOutAction{"signOut"};
constexpr QLatin1StringView GoogleDriveAuthStatusAction{"authStatus"};
constexpr QLatin1StringView GoogleDriveRawCapabilitiesAction{"rawCapabilities"};
constexpr QLatin1StringView GoogleDriveDownloadPdfAction{"downloadAsPdf"};
constexpr QLatin1StringView GoogleDriveRestoreAction{"restore"};
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
constexpr qint64 kProviderThumbnailMaxBytes = 2 * 1024 * 1024;
constexpr int kProviderThumbnailTimeoutMs = 5000;
constexpr int kGDriveThumbnailByteCacheLimitKb = 32 * 1024;
constexpr int kGDriveThumbnailWorkerCount = 4;

struct GDriveThumbnailDownloadResult {
    int httpStatus = 0;
    QByteArray body;
    bool timedOut = false;
    bool oversize = false;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
};

class GDriveThumbnailNetworkWorker final : public QObject
{
public:
    GDriveThumbnailDownloadResult download(const QUrl &url, const QString &accessToken)
    {
        GDriveThumbnailDownloadResult result;
        if (!url.isValid()) {
            return result;
        }

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("Mozilla/5.0 FMQml/1.0 GoogleDriveNativeThumbnail"));
        request.setRawHeader("Accept", "image/jpeg,image/png,image/*;q=0.8,*/*;q=0.5");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        // Keep the network manager local to the worker-thread method. A member
        // QNetworkAccessManager may be constructed in the caller thread before
        // the worker is moved, which makes QNetworkReply creation warn/fail
        // when thumbnails are requested from QQuickPixmapReader.
        QNetworkAccessManager network;
        QNetworkReply *reply = network.get(request);
        if (!reply) {
            return result;
        }

        bool oversize = false;
        QObject::connect(reply, &QNetworkReply::downloadProgress,
                         [reply, &oversize](qint64 processed, qint64 total) {
                             if (processed > kProviderThumbnailMaxBytes
                                 || (total > 0 && total > kProviderThumbnailMaxBytes)) {
                                 oversize = true;
                                 reply->abort();
                             }
                         });

        QEventLoop loop;
        QTimer timeout;
        bool timedOut = false;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            reply->abort();
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timeout.start(kProviderThumbnailTimeoutMs);
        loop.exec();
        timeout.stop();

        result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.networkError = reply->error();
        result.body = reply->readAll();
        result.timedOut = timedOut;
        result.oversize = oversize;
        delete reply;
        return result;
    }

};

const QVector<GDriveThumbnailNetworkWorker *> &gdriveThumbnailNetworkWorkers()
{
    static QVector<GDriveThumbnailNetworkWorker *> workers = [] {
        QVector<GDriveThumbnailNetworkWorker *> result;
        result.reserve(kGDriveThumbnailWorkerCount);
        for (int i = 0; i < kGDriveThumbnailWorkerCount; ++i) {
            auto *workerThread = new QThread;
            workerThread->setObjectName(QStringLiteral("GDriveThumbnailNetwork%1").arg(i + 1));
            workerThread->start();

            auto *networkWorker = new GDriveThumbnailNetworkWorker;
            networkWorker->moveToThread(workerThread);
            result.append(networkWorker);
        }
        return result;
    }();
    return workers;
}

GDriveThumbnailNetworkWorker *gdriveThumbnailNetworkWorker()
{
    static std::atomic<int> nextWorker{0};
    const QVector<GDriveThumbnailNetworkWorker *> &workers = gdriveThumbnailNetworkWorkers();
    if (workers.isEmpty()) {
        return nullptr;
    }
    const int index = nextWorker.fetch_add(1, std::memory_order_relaxed);
    const int slot = int(static_cast<unsigned int>(index) % static_cast<unsigned int>(workers.size()));
    return workers.at(slot);
}

GDriveThumbnailDownloadResult downloadGDriveThumbnailBytes(const QUrl &url, const QString &accessToken)
{
    GDriveThumbnailDownloadResult result;
    if (!url.isValid() || accessToken.isEmpty()) {
        return result;
    }

    GDriveThumbnailNetworkWorker *worker = gdriveThumbnailNetworkWorker();
    if (!worker) {
        return result;
    }
    if (QThread::currentThread() == worker->thread()) {
        result = worker->download(url, accessToken);
    } else {
        QMetaObject::invokeMethod(worker,
                                  [&result, worker, url, accessToken]() {
                                      result = worker->download(url, accessToken);
                                  },
                                  Qt::BlockingQueuedConnection);
    }
    return result;
}

QCache<QString, QByteArray> &gdriveThumbnailByteCache()
{
    static QCache<QString, QByteArray> cache(kGDriveThumbnailByteCacheLimitKb);
    return cache;
}

QMutex &gdriveThumbnailByteCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

QByteArray cachedGDriveThumbnailBytes(const QString &cacheIdentity)
{
    if (cacheIdentity.isEmpty()) {
        return {};
    }
    QMutexLocker locker(&gdriveThumbnailByteCacheMutex());
    if (const QByteArray *bytes = gdriveThumbnailByteCache().object(cacheIdentity)) {
        return *bytes;
    }
    return {};
}

void cacheGDriveThumbnailBytes(const QString &cacheIdentity, const QByteArray &bytes)
{
    if (cacheIdentity.isEmpty() || bytes.isEmpty()) {
        return;
    }
    const int costKb = qMax(1, int((bytes.size() + 1023) / 1024));
    QMutexLocker locker(&gdriveThumbnailByteCacheMutex());
    gdriveThumbnailByteCache().insert(cacheIdentity, new QByteArray(bytes), costKb);
}

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

QMutex s_driveApiCooldownMutex;
qint64 s_driveApiCooldownUntilMs = 0;
int s_driveApiThrottleCount = 0;

QString suffixForName(const QString &name)
{
    if (name.endsWith(QStringLiteral(".fb2.zip"), Qt::CaseInsensitive)) {
        return QStringLiteral("fb2.zip");
    }
    const int dotIndex = name.lastIndexOf(QLatin1Char('.'));
    if (dotIndex <= 0 || dotIndex == name.size() - 1) {
        return {};
    }
    return name.mid(dotIndex + 1).toLower();
}

QString byteSizeText(qint64 size)
{
    return QLocale().formattedDataSize(size);
}

QString isoDateText(const QDateTime &dateTime)
{
    return dateTime.isValid() ? dateTime.toLocalTime().toString(Qt::ISODate) : QString{};
}

bool isImageMimeType(const QString &mimeType)
{
    return mimeType.startsWith(QStringLiteral("image/"), Qt::CaseInsensitive);
}

bool isGoogleAppsMimeType(const QString &mimeType)
{
    return mimeType.startsWith(GoogleDriveAppsMimePrefix, Qt::CaseInsensitive)
        && mimeType != GoogleDriveFolderMime
        && mimeType != GoogleDriveShortcutMime;
}

struct GDriveExportFormat
{
    QString mimeType;
    QString suffix;
};

GDriveExportFormat pdfExportFormat()
{
    return {QStringLiteral("application/pdf"), QStringLiteral("pdf")};
}

GDriveExportFormat defaultExportFormatForGoogleAppsMimeType(QString mimeType)
{
    mimeType = mimeType.trimmed().toLower();
    if (mimeType == QLatin1String("application/vnd.google-apps.document")) {
        return {
            QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document"),
            QStringLiteral("docx")
        };
    }
    if (mimeType == QLatin1String("application/vnd.google-apps.spreadsheet")) {
        return {
            QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"),
            QStringLiteral("xlsx")
        };
    }
    if (mimeType == QLatin1String("application/vnd.google-apps.presentation")) {
        return {
            QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation"),
            QStringLiteral("pptx")
        };
    }
    if (mimeType == QLatin1String("application/vnd.google-apps.drawing")) {
        return {QStringLiteral("image/png"), QStringLiteral("png")};
    }
    return pdfExportFormat();
}

QString exportSuffixForDestinationFile(QString destinationFilePath)
{
    destinationFilePath = destinationFilePath.trimmed();
    if (destinationFilePath.endsWith(QStringLiteral(".part"), Qt::CaseInsensitive)) {
        destinationFilePath.chop(QStringLiteral(".part").size());
    }
    return QFileInfo(destinationFilePath).suffix().toLower();
}

GDriveExportFormat exportFormatForGoogleAppsDownload(const QString &mimeType, const QString &destinationFilePath)
{
    const GDriveExportFormat defaultFormat = defaultExportFormatForGoogleAppsMimeType(mimeType);
    const QString requestedSuffix = exportSuffixForDestinationFile(destinationFilePath);
    if (requestedSuffix == QLatin1String("pdf")) {
        return pdfExportFormat();
    }
    if (!requestedSuffix.isEmpty() && requestedSuffix == defaultFormat.suffix) {
        return defaultFormat;
    }
    return defaultFormat;
}

QString withExportSuffix(QString name, const QString &suffix)
{
    name = name.trimmed();
    if (name.isEmpty() || suffix.isEmpty()) {
        return name;
    }
    const QString dottedSuffix = QLatin1Char('.') + suffix;
    return name.endsWith(dottedSuffix, Qt::CaseInsensitive) ? name : name + dottedSuffix;
}

QString safeLocalExportFileName(QString name)
{
    name = name.trimmed();
    static const QString invalidCharacters = QStringLiteral("<>:\"/\\|?*");
    for (QChar &ch : name) {
        const ushort code = ch.unicode();
        if (code < 0x20 || code == 0x7f || invalidCharacters.contains(ch)) {
            ch = QLatin1Char('_');
        }
    }
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) {
        name.chop(1);
    }
    return name.isEmpty() ? QStringLiteral("download") : name;
}

QString uniqueLocalFilePath(const QString &path)
{
    if (!QFileInfo::exists(path)) {
        return path;
    }

    const QFileInfo info(path);
    const QDir dir = info.dir();
    const QString baseName = info.completeBaseName().isEmpty()
        ? info.fileName()
        : info.completeBaseName();
    const QString suffix = info.suffix();
    for (int i = 1; i < 10000; ++i) {
        const QString candidateName = suffix.isEmpty()
            ? QStringLiteral("%1 copy %2").arg(baseName).arg(i)
            : QStringLiteral("%1 copy %2.%3").arg(baseName).arg(i).arg(suffix);
        const QString candidate = dir.filePath(candidateName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return path;
}

QString iconSuffixForMimeType(QString mimeType)
{
    mimeType = mimeType.trimmed().toLower();
    if (mimeType.isEmpty()) {
        return {};
    }

    static const QHash<QString, QString> exactSuffixes = {
        {QStringLiteral("application/vnd.google-apps.document"), QStringLiteral("docx")},
        {QStringLiteral("application/vnd.google-apps.spreadsheet"), QStringLiteral("xlsx")},
        {QStringLiteral("application/vnd.google-apps.presentation"), QStringLiteral("pptx")},
        {QStringLiteral("application/vnd.google-apps.drawing"), QStringLiteral("png")},
        {QStringLiteral("application/vnd.google-apps.script"), QStringLiteral("js")},
        {QStringLiteral("application/vnd.google-apps.form"), QStringLiteral("docx")},
        {QStringLiteral("application/vnd.google-apps.site"), QStringLiteral("html")},
        {QStringLiteral("application/vnd.google-apps.fusiontable"), QStringLiteral("xlsx")},
        {QStringLiteral("application/pdf"), QStringLiteral("pdf")},
        {QStringLiteral("text/plain"), QStringLiteral("txt")},
        {QStringLiteral("text/markdown"), QStringLiteral("md")},
        {QStringLiteral("text/csv"), QStringLiteral("csv")},
        {QStringLiteral("text/tab-separated-values"), QStringLiteral("tsv")},
        {QStringLiteral("text/html"), QStringLiteral("html")},
        {QStringLiteral("text/css"), QStringLiteral("css")},
        {QStringLiteral("application/json"), QStringLiteral("json")},
        {QStringLiteral("application/ld+json"), QStringLiteral("json")},
        {QStringLiteral("application/javascript"), QStringLiteral("js")},
        {QStringLiteral("text/javascript"), QStringLiteral("js")},
        {QStringLiteral("application/xml"), QStringLiteral("xml")},
        {QStringLiteral("text/xml"), QStringLiteral("xml")},
        {QStringLiteral("application/x-yaml"), QStringLiteral("yaml")},
        {QStringLiteral("application/yaml"), QStringLiteral("yaml")},
        {QStringLiteral("application/zip"), QStringLiteral("zip")},
        {QStringLiteral("application/x-zip-compressed"), QStringLiteral("zip")},
        {QStringLiteral("application/x-rar-compressed"), QStringLiteral("rar")},
        {QStringLiteral("application/vnd.rar"), QStringLiteral("rar")},
        {QStringLiteral("application/x-7z-compressed"), QStringLiteral("7z")},
        {QStringLiteral("application/x-tar"), QStringLiteral("tar")},
        {QStringLiteral("application/gzip"), QStringLiteral("gz")},
        {QStringLiteral("application/x-gzip"), QStringLiteral("gz")},
        {QStringLiteral("application/x-xz"), QStringLiteral("xz")},
        {QStringLiteral("application/x-bzip2"), QStringLiteral("bz2")},
        {QStringLiteral("application/vnd.android.package-archive"), QStringLiteral("apk")},
        {QStringLiteral("application/msword"), QStringLiteral("doc")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document"), QStringLiteral("docx")},
        {QStringLiteral("application/vnd.oasis.opendocument.text"), QStringLiteral("odt")},
        {QStringLiteral("application/vnd.ms-excel"), QStringLiteral("xls")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"), QStringLiteral("xlsx")},
        {QStringLiteral("application/vnd.oasis.opendocument.spreadsheet"), QStringLiteral("ods")},
        {QStringLiteral("application/vnd.ms-powerpoint"), QStringLiteral("ppt")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation"), QStringLiteral("pptx")},
        {QStringLiteral("application/vnd.oasis.opendocument.presentation"), QStringLiteral("odp")},
        {QStringLiteral("font/ttf"), QStringLiteral("ttf")},
        {QStringLiteral("font/otf"), QStringLiteral("otf")},
        {QStringLiteral("font/woff"), QStringLiteral("woff")},
        {QStringLiteral("font/woff2"), QStringLiteral("woff2")},
        {QStringLiteral("application/font-woff"), QStringLiteral("woff")},
        {QStringLiteral("application/x-font-ttf"), QStringLiteral("ttf")},
        {QStringLiteral("application/x-font-otf"), QStringLiteral("otf")},
        {QStringLiteral("application/vnd.ms-fontobject"), QStringLiteral("eot")},
        {QStringLiteral("application/x-msdownload"), QStringLiteral("exe")},
        {QStringLiteral("application/x-msi"), QStringLiteral("msi")},
        {QStringLiteral("application/java-archive"), QStringLiteral("jar")},
    };

    const QString exact = exactSuffixes.value(mimeType);
    if (!exact.isEmpty()) {
        return exact;
    }
    if (mimeType.startsWith(QStringLiteral("image/"))) {
        return QStringLiteral("png");
    }
    if (mimeType.startsWith(QStringLiteral("audio/"))) {
        return QStringLiteral("mp3");
    }
    if (mimeType.startsWith(QStringLiteral("video/"))) {
        return QStringLiteral("mp4");
    }
    if (mimeType.startsWith(GoogleDriveAppsMimePrefix, Qt::CaseInsensitive)) {
        return QStringLiteral("docx");
    }
    if (mimeType.startsWith(QStringLiteral("text/"))) {
        return QStringLiteral("txt");
    }
    return {};
}

struct GDriveCreateTarget {
    QString parentPath;
    QString parentId;
    QString name;

    bool valid() const
    {
        return !parentPath.isEmpty() && !parentId.isEmpty() && !name.trimmed().isEmpty();
    }
};

QString driveCapabilitiesText(const GDriveItemCapabilities &capabilities);

GDriveItemCapabilities shortcutAliasCapabilities()
{
    GDriveItemCapabilities capabilities;
    capabilities.canListChildren = true;
    return capabilities;
}

GDriveItemCapabilities shortcutsRootCapabilities()
{
    GDriveItemCapabilities capabilities;
    capabilities.canListChildren = true;
    return capabilities;
}

GDriveItemCapabilities trashRootCapabilities()
{
    GDriveItemCapabilities capabilities;
    capabilities.canListChildren = true;
    return capabilities;
}

GDriveItemCapabilities shortcutViewCapabilities(const GDriveItemCapabilities &capabilities)
{
    GDriveItemCapabilities result;
    result.canDownload = capabilities.canDownload;
    result.canListChildren = capabilities.canListChildren;
    result.canCopy = capabilities.canCopy;
    return result;
}

FileEntry shortcutViewEntry(FileEntry entry, const GDriveItemCapabilities &capabilities)
{
    entry.isReadOnly = true;
    entry.providerCapabilitiesText = driveCapabilitiesText(shortcutViewCapabilities(capabilities));
    return entry;
}

GDriveItemCapabilities trashViewCapabilities(const GDriveItemCapabilities &capabilities)
{
    GDriveItemCapabilities result;
    result.canListChildren = capabilities.canListChildren;
    return result;
}

FileEntry trashViewEntry(FileEntry entry, const GDriveItemCapabilities &capabilities)
{
    entry.isReadOnly = true;
    entry.providerCapabilitiesText = driveCapabilitiesText(trashViewCapabilities(capabilities));
    return entry;
}

std::optional<FileEntry> shortcutAliasEntryFor(const FileEntry &shortcutEntry)
{
    if (!shortcutEntry.isShortcut
        || !shortcutEntry.shortcutTargetIsDirectory
        || shortcutEntry.shortcutOpenPath.isEmpty()
        || shortcutEntry.shortcutTargetPath.isEmpty()) {
        return std::nullopt;
    }

    FileEntry aliasEntry = shortcutEntry;
    aliasEntry.path = shortcutEntry.shortcutOpenPath;
    aliasEntry.mimeType = QString(GoogleDriveFolderMime);
    aliasEntry.suffix.clear();
    aliasEntry.isDirectory = true;
    aliasEntry.isImage = false;
    aliasEntry.hasThumbnail = false;
    aliasEntry.isReadOnly = true;
    aliasEntry.providerCapabilitiesText = driveCapabilitiesText(shortcutAliasCapabilities());
    return aliasEntry;
}

void cacheSharedShortcutAlias(const FileEntry &shortcutEntry, const QString &parentPath)
{
    const std::optional<FileEntry> aliasEntry = shortcutAliasEntryFor(shortcutEntry);
    if (!aliasEntry) {
        return;
    }

    cacheSharedEntry(*aliasEntry,
                     parentPath,
                     QString(GoogleDriveFolderMime),
                     shortcutAliasCapabilities());
}

void cacheSharedShortcutInRoot(const FileEntry &shortcutEntry, const GDriveItemCapabilities &capabilities)
{
    if (!shortcutEntry.isShortcut || shortcutEntry.path.isEmpty()) {
        return;
    }

    const GDriveItemCapabilities viewCapabilities = shortcutViewCapabilities(capabilities);
    const FileEntry viewEntry = shortcutViewEntry(shortcutEntry, capabilities);
    cacheSharedEntry(viewEntry, QString(GDrivePath::ShortcutsRoot), QString(GoogleDriveShortcutMime), viewCapabilities);
    cacheSharedShortcutAlias(viewEntry, QString(GDrivePath::ShortcutsRoot));

    QStringList children = sharedChildren(QString(GDrivePath::ShortcutsRoot));
    if (!children.contains(viewEntry.path)) {
        children.append(viewEntry.path);
        cacheSharedChildren(QString(GDrivePath::ShortcutsRoot), children);
    }
}

bool isSharedTrashViewPath(const QString &path)
{
    QString current = GDrivePath::normalizedPath(path);
    QSet<QString> seen;
    while (!current.isEmpty() && !seen.contains(current)) {
        if (current == GDrivePath::Trash) {
            return true;
        }
        seen.insert(current);
        current = sharedParent(current);
    }
    return false;
}

struct GDriveGoogleAppsExportTarget
{
    QString sourcePath;
    QString displayName;
    QString mimeType;
};

std::optional<GDriveGoogleAppsExportTarget> googleAppsExportTargetForPath(const QString &path)
{
    const QString normalized = GDrivePath::normalizedPath(path);
    if (normalized.isEmpty()) {
        return std::nullopt;
    }

    QString sourcePath = normalized;
    QString displayName = GDrivePath::fallbackFileNameForPath(normalized);
    QString mimeType = sharedMimeType(normalized);
    if (const std::optional<FileEntry> entry = sharedEntry(normalized)) {
        displayName = entry->name;
        if (entry->isDirectory) {
            return std::nullopt;
        }
        if (entry->isShortcut) {
            sourcePath = entry->shortcutTargetPath;
            mimeType = entry->shortcutTargetMimeType;
        } else {
            mimeType = entry->mimeType;
        }
    }

    if (sourcePath.isEmpty() || !isGoogleAppsMimeType(mimeType)) {
        return std::nullopt;
    }
    return GDriveGoogleAppsExportTarget{sourcePath, displayName, mimeType};
}

QString driveContentIdForPath(const QString &path)
{
    const QString shortcutId = GDrivePath::idForShortcutPath(path);
    if (!shortcutId.isEmpty()) {
        const std::optional<FileEntry> aliasEntry = sharedEntry(path);
        if (!aliasEntry || !aliasEntry->shortcutTargetIsDirectory) {
            return {};
        }
        return GDrivePath::idForItemPath(aliasEntry->shortcutTargetPath);
    }
    return GDrivePath::driveParentIdForPath(path);
}

QString boolText(bool value)
{
    return value ? QStringLiteral("true") : QStringLiteral("false");
}

QString driveCapabilitiesText(const GDriveItemCapabilities &capabilities)
{
    return QStringLiteral(
               "canDownload: %1\n"
               "canEdit: %2\n"
               "canAddChildren: %3\n"
               "canListChildren: %4\n"
               "canRename: %5\n"
               "canTrash: %6\n"
               "canDelete: %7\n"
               "canCopy: %8")
        .arg(boolText(capabilities.canDownload),
             boolText(capabilities.canEdit),
             boolText(capabilities.canAddChildren),
             boolText(capabilities.canListChildren),
             boolText(capabilities.canRename),
             boolText(capabilities.canTrash),
             boolText(capabilities.canDelete),
             boolText(capabilities.canCopy));
}

QVariantMap capabilityProperty(const QString &label, bool value)
{
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("value"), value ? QStringLiteral("Allowed") : QStringLiteral("Unavailable")},
        {QStringLiteral("active"), value},
    };
}

QVariantList driveCapabilitiesProperties(const GDriveItemCapabilities &capabilities)
{
    return {
        capabilityProperty(QStringLiteral("Download"), capabilities.canDownload),
        capabilityProperty(QStringLiteral("Edit"), capabilities.canEdit),
        capabilityProperty(QStringLiteral("Create inside"), capabilities.canAddChildren),
        capabilityProperty(QStringLiteral("Browse / traverse"), capabilities.canListChildren),
        capabilityProperty(QStringLiteral("Rename"), capabilities.canRename),
        capabilityProperty(QStringLiteral("Trash"), capabilities.canTrash),
        capabilityProperty(QStringLiteral("Delete permanently"), capabilities.canDelete),
        capabilityProperty(QStringLiteral("Copy"), capabilities.canCopy),
    };
}

FileEntry virtualDirectoryEntry(const QString &name, const QString &path, const GDriveItemCapabilities &capabilities = {})
{
    FileEntry entry;
    entry.name = name;
    entry.path = path;
    entry.providerCapabilitiesText = driveCapabilitiesText(capabilities);
    entry.iconName = GDrivePath::virtualIconNameForPath(path);
    entry.modified = QDateTime::currentDateTime();
    entry.created = entry.modified;
    entry.modifiedText = isoDateText(entry.modified);
    entry.createdText = entry.modifiedText;
    entry.isDirectory = true;
    entry.isReadOnly = true;
    return entry;
}

GDriveItemCapabilities driveCapabilitiesFromDriveFileObject(const QJsonObject &object)
{
    GDriveItemCapabilities result;
    const QJsonObject capabilities = object.value(QStringLiteral("capabilities")).toObject();
    result.canDownload = capabilities.value(QStringLiteral("canDownload")).toBool(false);
    result.canEdit = capabilities.value(QStringLiteral("canEdit")).toBool(false);
    result.canAddChildren = capabilities.value(QStringLiteral("canAddChildren")).toBool(false);
    result.canListChildren = capabilities.value(QStringLiteral("canListChildren")).toBool(false);
    result.canRename = capabilities.value(QStringLiteral("canRename")).toBool(false);
    result.canTrash = capabilities.value(QStringLiteral("canTrash")).toBool(false);
    result.canDelete = capabilities.value(QStringLiteral("canDelete")).toBool(false);
    result.canCopy = capabilities.value(QStringLiteral("canCopy")).toBool(false);
    return result;
}

QString driveQueryForPath(const QString &path)
{
    if (path == GDrivePath::MyDrive) {
        return QStringLiteral("'root' in parents and trashed = false");
    }
    if (path == GDrivePath::SharedWithMe) {
        return QStringLiteral("sharedWithMe = true and trashed = false");
    }
    if (path == GDrivePath::ShortcutsRoot) {
        return QStringLiteral("mimeType = '%1' and trashed = false").arg(QString(GoogleDriveShortcutMime));
    }
    if (path == GDrivePath::Trash) {
        return QStringLiteral("trashed = true");
    }

    const QString folderId = driveContentIdForPath(path);
    if (!folderId.isEmpty()) {
        const QString trashed = isSharedTrashViewPath(path) ? QStringLiteral("true") : QStringLiteral("false");
        return QStringLiteral("'%1' in parents and trashed = %2").arg(folderId, trashed);
    }

    return {};
}

QString driveErrorMessage(const QByteArray &body, const QString &fallback)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const QJsonObject errorObject = document.object().value(QStringLiteral("error")).toObject();
    const QString message = errorObject.value(QStringLiteral("message")).toString().trimmed();
    if (!message.isEmpty()) {
        return message;
    }
    return fallback;
}

struct GDriveUploadLogContext {
    QString batchId;
    qsizetype batchIndex = -1;
    qsizetype batchCount = 0;
    qsizetype waveStart = -1;
    qsizetype waveEnd = -1;
    int attempt = 1;
};

struct GDriveUploadReplyResult {
    QByteArray body;
    QHash<QByteArray, QByteArray> headers;
    QString error;
    int status = 0;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
    qint64 elapsedMs = 0;
    bool timedOut = false;
    bool canceled = false;
};

QString uploadContextText(const GDriveUploadLogContext &context)
{
    if (context.batchId.isEmpty()) {
        return QStringLiteral("single");
    }
    return QStringLiteral("batch=%1 index=%2/%3 wave=%4-%5 attempt=%6")
        .arg(context.batchId)
        .arg(context.batchIndex + 1)
        .arg(context.batchCount)
        .arg(context.waveStart + 1)
        .arg(context.waveEnd)
        .arg(context.attempt);
}

bool isRetryableDriveUploadError(const QString &message);
bool isRetryableDriveUploadStatus(int status);
bool isRetryableDriveUploadNetworkError(QNetworkReply::NetworkError error);
bool isDriveUploadRateLimitError(const QString &message);
int retryAfterDelayMs(const QHash<QByteArray, QByteArray> &headers, int fallbackAttempt);
bool waitForDriveApiCooldown(QLatin1StringView operation,
                             const std::function<bool()> &shouldCancel = {});
void noteDriveApiSuccess();
void noteDriveApiThrottle(QLatin1StringView operation,
                          const QHash<QByteArray, QByteArray> &headers,
                          int attempt,
                          const QString &message);
qint64 acknowledgedResumableOffset(const QHash<QByteArray, QByteArray> &headers);
GDriveUploadReplyResult waitForUploadReply(QNetworkReply *reply,
                                           int timeoutMs,
                                           const QString &timeoutMessage,
                                           const std::function<bool(qint64 sent, qint64 total)> &progress);
bool queryResumableUploadOffset(QNetworkAccessManager &network,
                                const QUrl &sessionUrl,
                                qint64 fileSize,
                                const QString &accessToken,
                                qint64 *offset,
                                QString *error);

FileEntry entryFromDriveFileObject(const QJsonObject &object)
{
    const QString id = object.value(QStringLiteral("id")).toString().trimmed();
    const QString name = object.value(QStringLiteral("name")).toString().trimmed();
    const QString mimeType = object.value(QStringLiteral("mimeType")).toString();
    if (id.isEmpty() || name.isEmpty()) {
        return {};
    }

    const bool directory = mimeType == GoogleDriveFolderMime;
    const bool shortcut = mimeType == GoogleDriveShortcutMime;
    const QJsonObject shortcutDetails = object.value(QStringLiteral("shortcutDetails")).toObject();
    const QString shortcutTargetId = shortcutDetails.value(QStringLiteral("targetId")).toString().trimmed();
    const QString shortcutTargetMimeType = shortcutDetails.value(QStringLiteral("targetMimeType")).toString();
    const QString shortcutTargetResourceKey = shortcutDetails.value(QStringLiteral("targetResourceKey")).toString().trimmed();
    const GDriveItemCapabilities capabilities = driveCapabilitiesFromDriveFileObject(object);
    const QString thumbnailLink = object.value(QStringLiteral("thumbnailLink")).toString().trimmed();
    FileEntry entry;
    entry.name = name;
    entry.path = GDrivePath::itemPathForId(id);
    entry.mimeType = mimeType;
    entry.suffix = shortcut ? QStringLiteral("shortcut") : (directory ? QString{} : suffixForName(name));
    if (entry.suffix.isEmpty()) {
        entry.suffix = iconSuffixForMimeType(mimeType);
    }
    entry.isDirectory = directory;
    entry.isShortcut = shortcut;
    entry.shortcutTargetPath = shortcutTargetId.isEmpty() ? QString{} : GDrivePath::itemPathForId(shortcutTargetId);
    entry.shortcutTargetMimeType = shortcutTargetMimeType;
    entry.shortcutTargetResourceKey = shortcutTargetResourceKey;
    entry.shortcutTargetIsDirectory = shortcutTargetMimeType == GoogleDriveFolderMime;
    if (entry.isShortcut && entry.shortcutTargetIsDirectory && !entry.shortcutTargetPath.isEmpty()) {
        entry.shortcutOpenPath = GDrivePath::shortcutPathForId(id);
    }
    if (entry.isShortcut) {
        if (entry.shortcutTargetIsDirectory) {
            entry.iconName = QStringLiteral("gdrive-shortcut");
        } else {
            entry.iconName = QStringLiteral("gdrive-file-shortcut");
        }
    }
    entry.isReadOnly = true;
    entry.isImage = isImageMimeType(mimeType);
    entry.hasThumbnail = !directory && !thumbnailLink.isEmpty();
    entry.providerCapabilitiesText = driveCapabilitiesText(capabilities);
    cacheSharedThumbnailLink(entry.path, thumbnailLink);

    bool ok = false;
    const qint64 size = object.value(QStringLiteral("size")).toString().toLongLong(&ok);
    if (ok) {
        entry.size = size;
        entry.sizeText = byteSizeText(size);
    }

    entry.modified = QDateTime::fromString(object.value(QStringLiteral("modifiedTime")).toString(), Qt::ISODateWithMs);
    if (!entry.modified.isValid()) {
        entry.modified = QDateTime::fromString(object.value(QStringLiteral("modifiedTime")).toString(), Qt::ISODate);
    }
    entry.created = QDateTime::fromString(object.value(QStringLiteral("createdTime")).toString(), Qt::ISODateWithMs);
    if (!entry.created.isValid()) {
        entry.created = QDateTime::fromString(object.value(QStringLiteral("createdTime")).toString(), Qt::ISODate);
    }
    entry.modifiedText = isoDateText(entry.modified);
    entry.createdText = isoDateText(entry.created);
    return entry;
}

QByteArray safeReadAll(QIODevice *device)
{
    return device && device->isOpen() ? device->readAll() : QByteArray{};
}

QByteArray resourceKeyHeaderValue(const QString &path, const QString &resourceKey)
{
    const QString cleanResourceKey = resourceKey.trimmed();
    if (cleanResourceKey.isEmpty()) {
        return {};
    }
    const QString fileId = GDrivePath::idForItemPath(path).trimmed();
    if (fileId.isEmpty()) {
        return {};
    }
    return QStringLiteral("%1/%2").arg(fileId, cleanResourceKey).toUtf8();
}

void applyResourceKeyHeader(QNetworkRequest *request, const QString &path, const QString &resourceKey)
{
    if (!request) {
        return;
    }
    const QByteArray headerValue = resourceKeyHeaderValue(path, resourceKey);
    if (!headerValue.isEmpty()) {
        request->setRawHeader("X-Goog-Drive-Resource-Keys", headerValue);
    }
}

QUrl driveDownloadUrl(const QString &path,
                      const QString &mimeType,
                      const QString &destinationFilePath)
{
    const QString id = GDrivePath::idForItemPath(path);
    if (id.isEmpty()) {
        return {};
    }

    QUrl url;
    QUrlQuery query;
    if (isGoogleAppsMimeType(mimeType)) {
        url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1/export")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(id))));
        query.addQueryItem(QStringLiteral("mimeType"),
                           exportFormatForGoogleAppsDownload(mimeType, destinationFilePath).mimeType);
    } else {
        url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(id))));
        query.addQueryItem(QStringLiteral("alt"), QStringLiteral("media"));
        query.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
    }
    url.setQuery(query);
    return url;
}

bool downloadDriveFileToLocalFile(QNetworkAccessManager &network,
                                  const QString &sourcePath,
                                  const QString &mimeType,
                                  const QString &destinationFilePath,
                                  const QString &accessToken,
                                  const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                  QString *error,
                                  const QString &resourceKey = {})
{
    const QUrl url = driveDownloadUrl(sourcePath, mimeType, destinationFilePath);
    if (!url.isValid()) {
        if (error) {
            *error = QStringLiteral("Google Drive file path is invalid");
        }
        return false;
    }

    QFileInfo destinationInfo(destinationFilePath);
    if (!destinationInfo.absoluteDir().exists()) {
        if (error) {
            *error = QStringLiteral("Google Drive download destination does not exist: %1")
                .arg(destinationInfo.absolutePath());
        }
        return false;
    }

    QFile output(destinationFilePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Cannot open Google Drive destination: %1").arg(output.errorString());
        }
        return false;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    applyResourceKeyHeader(&request, sourcePath, resourceKey);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = network.get(request);
    QEventLoop loop;
    QByteArray errorBody;
    QString writeError;
    bool canceled = false;
    bool writeFailed = false;
    bool timedOut = false;
    QTimer idleTimeout;
    idleTimeout.setSingleShot(true);

    auto consumeReadyRead = [&]() {
        if (!reply->isOpen()) {
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = safeReadAll(reply);
        if (data.isEmpty()) {
            return;
        }
        if (status >= 400) {
            errorBody.append(data);
            return;
        }
        if (writeFailed) {
            return;
        }
        if (output.write(data) != data.size()) {
            writeFailed = true;
            writeError = output.errorString();
            reply->abort();
        }
    };

    QObject::connect(&idleTimeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QIODevice::readyRead, &loop, [&]() {
        idleTimeout.start(TransferIdleTimeoutMs);
        consumeReadyRead();
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, &loop, [&](qint64 received, qint64 total) {
        idleTimeout.start(TransferIdleTimeoutMs);
        if (progress && !progress(received, total)) {
            canceled = true;
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    idleTimeout.start(TransferIdleTimeoutMs);
    loop.exec();
    idleTimeout.stop();
    consumeReadyRead();

    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorString = reply->errorString();
    delete reply;

    const bool flushed = output.flush();
    const QString flushError = output.errorString();
    output.close();

    if (writeFailed) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = QStringLiteral("Google Drive download write failed: %1").arg(writeError);
        }
        return false;
    }

    if (!flushed) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = QStringLiteral("Google Drive download flush failed: %1").arg(flushError);
        }
        return false;
    }

    if (timedOut) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = QStringLiteral("Google Drive download timed out");
        }
        return false;
    }

    if (canceled || networkError == QNetworkReply::OperationCanceledError) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = QStringLiteral("Google Drive download canceled");
        }
        return false;
    }

    if (networkError != QNetworkReply::NoError) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = driveErrorMessage(errorBody,
                                       QStringLiteral("Google Drive download failed: %1").arg(networkErrorString));
        }
        return false;
    }

    return true;
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

void disableHttp2ForUpload(QNetworkRequest *request)
{
    if (!request) {
        return;
    }
    request->setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
}

bool waitForReply(QNetworkReply *reply,
                  int timeoutMs,
                  QString timeoutMessage,
                  QByteArray *body,
                  QString *error,
                  QHash<QByteArray, QByteArray> *rawHeaders = nullptr)
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
            *error = driveErrorMessage(body ? *body : QByteArray{}, fallback);
        }
        return false;
    }
    if (status >= 400) {
        if (error) {
            *error = driveErrorMessage(body ? *body : QByteArray{},
                                       QStringLiteral("Google Drive request failed with HTTP %1").arg(status));
        }
        return false;
    }
    return true;
}

bool parseDriveFileResponse(const QByteArray &body, QJsonObject *fileObject, QString *error)
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

bool fetchDriveFileMetadataBlocking(QNetworkAccessManager &network,
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
    return parseDriveFileResponse(body, fileObject, error);
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

bool refreshDriveStorageQuotaBlocking(QNetworkAccessManager &network, const QString &accessToken, QString *error)
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

QString mimeTypeForLocalUpload(const QString &sourceFilePath)
{
    const QString mimeType = QMimeDatabase().mimeTypeForFile(sourceFilePath, QMimeDatabase::MatchExtension).name();
    return mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType;
}

bool createDriveMetadataBlocking(QNetworkAccessManager &network,
                                 const QString &parentId,
                                 const QString &name,
                                 const QString &mimeType,
                                 const QString &accessToken,
                                 QJsonObject *createdObject,
                                 QString *error)
{
    QString lastError;
    for (int attempt = 1; attempt <= DriveApiMaxAttempts; ++attempt) {
        waitForDriveApiCooldown(QLatin1StringView("create"));
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
            noteDriveApiSuccess();
            return parseDriveFileResponse(body, createdObject, error);
        }
        lastError = attemptError;
        if (attempt >= DriveApiMaxAttempts || !isRetryableDriveUploadError(attemptError)) {
            break;
        }
        noteDriveApiThrottle(QLatin1StringView("create"), headers, attempt, attemptError);
    }
    if (error) {
        *error = lastError;
    }
    return false;
}

bool trashDriveFileBlocking(QNetworkAccessManager &network,
                            const QString &fileId,
                            const QString &accessToken,
                            QJsonObject *trashedObject,
                            QString *error)
{
    QString lastError;
    for (int attempt = 1; attempt <= DriveApiMaxAttempts; ++attempt) {
        waitForDriveApiCooldown(QLatin1StringView("trash"));
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
            noteDriveApiSuccess();
            return parseDriveFileResponse(body, trashedObject, error);
        }
        lastError = attemptError;
        if (attempt >= DriveApiMaxAttempts || !isRetryableDriveUploadError(attemptError)) {
            break;
        }
        noteDriveApiThrottle(QLatin1StringView("trash"), headers, attempt, attemptError);
    }
    if (error) {
        *error = lastError;
    }
    return false;
}

bool restoreDriveFileBlocking(QNetworkAccessManager &network,
                              const QString &fileId,
                              const QString &accessToken,
                              QJsonObject *restoredObject,
                              QString *error)
{
    QString lastError;
    for (int attempt = 1; attempt <= DriveApiMaxAttempts; ++attempt) {
        waitForDriveApiCooldown(QLatin1StringView("restore"));
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
            noteDriveApiSuccess();
            return parseDriveFileResponse(body, restoredObject, error);
        }
        lastError = attemptError;
        if (attempt >= DriveApiMaxAttempts || !isRetryableDriveUploadError(attemptError)) {
            break;
        }
        noteDriveApiThrottle(QLatin1StringView("restore"), headers, attempt, attemptError);
    }
    if (error) {
        *error = lastError;
    }
    return false;
}

QUrl driveMultipartUploadUrl()
{
    QUrl url(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("multipart"));
    query.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("fields"), QString(DriveFileFields));
    url.setQuery(query);
    return url;
}

QUrl driveUploadSessionUrl()
{
    QUrl url(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("resumable"));
    query.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("fields"), QString(DriveFileFields));
    url.setQuery(query);
    return url;
}

bool gdriveUploadLoggingEnabled();
bool gdriveDownloadLoggingEnabled();
bool gdriveDownloadRangeLoggingEnabled();
int gdriveUploadStallLogIntervalMs();
int gdriveDownloadConcurrency();

bool uploadSmallLocalFileToDriveBlocking(QNetworkAccessManager &network,
                                         QFile &file,
                                         const QString &parentId,
                                         const QString &name,
                                         const QString &mimeType,
                                         const QString &accessToken,
                                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                         QJsonObject *createdObject,
                                         QString *error,
                                         const GDriveUploadLogContext &logContext)
{
    if (!file.seek(0)) {
        if (error) {
            *error = QStringLiteral("Cannot rewind local file for Google Drive upload: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray fileBytes = file.readAll();
    if (file.error() != QFile::NoError) {
        if (error) {
            *error = QStringLiteral("Cannot read local file for Google Drive upload: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("name"), name);
    metadata.insert(QStringLiteral("parents"), QJsonArray{parentId});

    const QByteArray boundary = QByteArrayLiteral("fmqml-gdrive-") + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    QByteArray body;
    body.reserve(QJsonDocument(metadata).toJson(QJsonDocument::Compact).size() + fileBytes.size() + 512);
    body += QByteArrayLiteral("--") + boundary + QByteArrayLiteral("\r\n");
    body += QByteArrayLiteral("Content-Type: application/json; charset=UTF-8\r\n\r\n");
    body += QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    body += QByteArrayLiteral("\r\n--") + boundary + QByteArrayLiteral("\r\n");
    body += QByteArrayLiteral("Content-Type: ") + mimeType.toUtf8() + QByteArrayLiteral("\r\n\r\n");
    body += fileBytes;
    body += QByteArrayLiteral("\r\n--") + boundary + QByteArrayLiteral("--\r\n");

    QNetworkRequest uploadRequest = authorizedJsonRequest(driveMultipartUploadUrl(), accessToken);
    uploadRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                            QStringLiteral("multipart/related; boundary=%1").arg(QString::fromLatin1(boundary)));
    uploadRequest.setHeader(QNetworkRequest::ContentLengthHeader, body.size());

    if (!waitForDriveApiCooldown(QLatin1StringView("multipartUpload"), [&]() {
            return progress && !progress(0, file.size());
        })) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }
    QNetworkReply *uploadReply = network.post(uploadRequest, body);
    QEventLoop uploadLoop;
    QTimer uploadIdleTimeout;
    QTimer uploadStallLogTimer;
    QElapsedTimer uploadTimer;
    QElapsedTimer uploadProgressTimer;
    bool canceled = false;
    bool timedOut = false;
    qint64 lastSent = 0;
    qint64 lastTotal = 0;
    int stallLogCount = 0;
    const bool logging = gdriveUploadLoggingEnabled();
    const int stallLogMs = logging ? gdriveUploadStallLogIntervalMs() : 0;
    uploadTimer.start();
    uploadProgressTimer.start();
    if (logging) {
        qInfo() << "GDrive upload attempt started"
                << "mode" << "multipart"
                << "context" << uploadContextText(logContext)
                << "name" << name
                << "bytes" << file.size();
    }
    uploadIdleTimeout.setSingleShot(true);
    QObject::connect(&uploadIdleTimeout, &QTimer::timeout, &uploadLoop, [&]() {
        timedOut = true;
        uploadReply->abort();
    });
    if (stallLogMs > 0) {
        QObject::connect(&uploadStallLogTimer, &QTimer::timeout, &uploadLoop, [&]() {
            ++stallLogCount;
            if (progress && !progress(file.size(), file.size())) {
                canceled = true;
                uploadReply->abort();
                return;
            }
            qInfo() << "GDrive upload no progress"
                    << "mode" << "multipart"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "sent" << lastSent
                    << "total" << lastTotal
                    << "idleMs" << uploadProgressTimer.elapsed()
                    << "elapsedMs" << uploadTimer.elapsed()
                    << "count" << stallLogCount;
        });
        uploadStallLogTimer.start(stallLogMs);
    }
    QObject::connect(uploadReply, &QNetworkReply::uploadProgress, &uploadLoop, [&](qint64 sent, qint64 total) {
        uploadIdleTimeout.start(TransferIdleTimeoutMs);
        lastSent = sent;
        lastTotal = total;
        uploadProgressTimer.restart();
        stallLogCount = 0;
        const qint64 fileSize = file.size();
        qint64 processed = fileSize;
        if (total > 0 && fileSize > 0) {
            processed = std::clamp<qint64>((sent * fileSize) / total, 0, fileSize);
        } else if (fileSize > 0) {
            processed = std::clamp<qint64>(sent, 0, fileSize);
        }
        if (progress && !progress(processed, fileSize)) {
            canceled = true;
            uploadReply->abort();
        }
    });
    QObject::connect(uploadReply, &QNetworkReply::finished, &uploadLoop, &QEventLoop::quit);
    uploadIdleTimeout.start(TransferIdleTimeoutMs);
    uploadLoop.exec();
    uploadIdleTimeout.stop();
    uploadStallLogTimer.stop();

    const QByteArray uploadBody = safeReadAll(uploadReply);
    QHash<QByteArray, QByteArray> uploadHeaders;
    for (const QByteArray &header : uploadReply->rawHeaderList()) {
        const QByteArray value = uploadReply->rawHeader(header);
        uploadHeaders.insert(header, value);
        uploadHeaders.insert(header.toLower(), value);
    }
    const QNetworkReply::NetworkError networkError = uploadReply->error();
    const QString networkErrorString = uploadReply->errorString();
    const int status = uploadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    delete uploadReply;

    if (timedOut) {
        if (error) {
            *error = QStringLiteral("Google Drive upload timed out");
        }
        if (logging) {
            qInfo() << "GDrive upload attempt failed"
                    << "mode" << "multipart"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "elapsedMs" << uploadTimer.elapsed()
                    << "reason" << "timeout";
        }
        return false;
    }
    if (canceled || networkError == QNetworkReply::OperationCanceledError) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        if (logging) {
            qInfo() << "GDrive upload attempt failed"
                    << "mode" << "multipart"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "elapsedMs" << uploadTimer.elapsed()
                    << "reason" << "canceled";
        }
        return false;
    }
    if (networkError != QNetworkReply::NoError || status >= 400) {
        const QString message = status > 0
            ? driveErrorMessage(uploadBody, QStringLiteral("Google Drive upload failed with HTTP %1").arg(status))
            : driveErrorMessage(uploadBody, QStringLiteral("Google Drive upload failed: %1").arg(networkErrorString));
        if (error) {
            *error = message;
        }
        if (logging) {
            qInfo() << "GDrive upload attempt failed"
                    << "mode" << "multipart"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "elapsedMs" << uploadTimer.elapsed()
                    << "httpStatus" << status
                    << "networkError" << networkError
                    << "message" << message.left(180);
        }
        if (isRetryableDriveUploadError(message)) {
            noteDriveApiThrottle(QLatin1StringView("multipartUpload"), uploadHeaders, 1, message);
        }
        return false;
    }

    if (progress && !progress(file.size(), file.size())) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }

    if (logging) {
        qInfo() << "GDrive upload attempt finished"
                << "mode" << "multipart"
                << "context" << uploadContextText(logContext)
                << "name" << name
                << "bytes" << file.size()
                << "elapsedMs" << uploadTimer.elapsed();
    }
    noteDriveApiSuccess();
    return parseDriveFileResponse(uploadBody, createdObject, error);
}

bool uploadLocalFileToDriveBlocking(QNetworkAccessManager &network,
                                    const QString &sourceFilePath,
                                    const QString &parentId,
                                    const QString &name,
                                    const QString &accessToken,
                                    const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                    QJsonObject *createdObject,
                                    QString *error,
                                    const GDriveUploadLogContext &logContext = {})
{
    QFile file(sourceFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot read local file for Google Drive upload: %1").arg(file.errorString());
        }
        return false;
    }

    const QString mimeType = mimeTypeForLocalUpload(sourceFilePath);
    if (file.size() <= SmallMultipartUploadThresholdBytes) {
        return uploadSmallLocalFileToDriveBlocking(network,
                                                   file,
                                                   parentId,
                                                   name,
                                                   mimeType,
                                                   accessToken,
                                                   progress,
                                                   createdObject,
                                                   error,
                                                   logContext);
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("name"), name);
    metadata.insert(QStringLiteral("parents"), QJsonArray{parentId});

    QNetworkRequest sessionRequest = authorizedJsonRequest(driveUploadSessionUrl(), accessToken);
    sessionRequest.setRawHeader("X-Upload-Content-Type", mimeType.toUtf8());
    sessionRequest.setRawHeader("X-Upload-Content-Length", QByteArray::number(file.size()));
    disableHttp2ForUpload(&sessionRequest);

    QByteArray sessionBody;
    QHash<QByteArray, QByteArray> sessionHeaders;
    if (!waitForDriveApiCooldown(QLatin1StringView("uploadSession"), [&]() {
            return progress && !progress(0, file.size());
        })) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }
    QNetworkReply *sessionReply = network.post(sessionRequest, QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    if (!waitForReply(sessionReply,
                      60000,
                      QStringLiteral("Google Drive upload session timed out"),
                      &sessionBody,
                      error,
                      &sessionHeaders)) {
        if (gdriveUploadLoggingEnabled()) {
            qInfo() << "GDrive upload session failed"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "message" << (error ? error->left(180) : QString{});
        }
        if (error && isRetryableDriveUploadError(*error)) {
            noteDriveApiThrottle(QLatin1StringView("uploadSession"), sessionHeaders, 1, *error);
        }
        return false;
    }
    noteDriveApiSuccess();

    const QByteArray sessionUrlBytes = sessionHeaders.value(QByteArrayLiteral("location")).trimmed();
    const QUrl sessionUrl(QString::fromUtf8(sessionUrlBytes));
    if (sessionUrlBytes.isEmpty() || !sessionUrl.isValid() || sessionUrl.isRelative()) {
        if (error) {
            *error = QStringLiteral("Google Drive upload session response has no upload URL");
        }
        return false;
    }

    const bool logging = gdriveUploadLoggingEnabled();
    QElapsedTimer uploadTimer;
    uploadTimer.start();
    if (logging) {
        qInfo() << "GDrive upload attempt started"
                << "mode" << "resumable"
                << "context" << uploadContextText(logContext)
                << "name" << name
                << "bytes" << file.size()
                << "chunkBytes" << ResumableUploadChunkBytes;
    }

    QByteArray finalBody;
    qint64 offset = 0;
    int consecutiveRetry = 0;

    while (offset < file.size()) {
        if (!file.seek(offset)) {
            if (error) {
                *error = QStringLiteral("Cannot seek local file for Google Drive upload: %1").arg(file.errorString());
            }
            return false;
        }

        const qint64 chunkSize = (std::min<qint64>)(ResumableUploadChunkBytes, file.size() - offset);
        const QByteArray chunk = file.read(chunkSize);
        if (chunk.size() != chunkSize) {
            if (error) {
                *error = QStringLiteral("Cannot read local file chunk for Google Drive upload: %1").arg(file.errorString());
            }
            return false;
        }

        QNetworkRequest uploadRequest(sessionUrl);
        uploadRequest.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
        uploadRequest.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
        uploadRequest.setHeader(QNetworkRequest::ContentLengthHeader, chunk.size());
        uploadRequest.setRawHeader("Content-Range",
                                   QByteArrayLiteral("bytes ")
                                       + QByteArray::number(offset)
                                       + QByteArrayLiteral("-")
                                       + QByteArray::number(offset + chunk.size() - 1)
                                       + QByteArrayLiteral("/")
                                       + QByteArray::number(file.size()));
        uploadRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        disableHttp2ForUpload(&uploadRequest);

        const qint64 chunkOffset = offset;
        if (logging) {
            qInfo() << "GDrive upload chunk started"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "offset" << chunkOffset
                    << "bytes" << chunk.size()
                    << "attempt" << (consecutiveRetry + 1);
        }

        if (!waitForDriveApiCooldown(QLatin1StringView("uploadChunk"), [&]() {
                return progress && !progress(offset, file.size());
            })) {
            if (error) {
                *error = QStringLiteral("Google Drive upload canceled");
            }
            return false;
        }
        const GDriveUploadReplyResult reply = waitForUploadReply(
            network.put(uploadRequest, chunk),
            TransferIdleTimeoutMs,
            QStringLiteral("Google Drive upload timed out"),
            [&, chunkOffset](qint64 sent, qint64 total) -> bool {
                Q_UNUSED(total)
                return !progress || progress(chunkOffset + sent, file.size());
            });

        if (reply.canceled) {
            if (error) {
                *error = reply.error;
            }
            if (logging) {
                qInfo() << "GDrive upload attempt failed"
                        << "mode" << "resumable"
                        << "context" << uploadContextText(logContext)
                        << "name" << name
                        << "bytes" << file.size()
                        << "elapsedMs" << uploadTimer.elapsed()
                        << "reason" << "canceled";
            }
            return false;
        }

        if (reply.status == 308) {
            const qint64 nextOffset = std::clamp<qint64>(acknowledgedResumableOffset(reply.headers), offset, file.size());
            offset = nextOffset > offset ? nextOffset : offset + chunk.size();
            consecutiveRetry = 0;
            if (logging) {
                qInfo() << "GDrive upload chunk accepted"
                        << "context" << uploadContextText(logContext)
                        << "name" << name
                        << "nextOffset" << offset
                        << "elapsedMs" << reply.elapsedMs;
            }
            continue;
        }

        if ((reply.status == 200 || reply.status == 201) && reply.error.isEmpty()) {
            finalBody = reply.body;
            offset = file.size();
            if (progress && !progress(file.size(), file.size())) {
                if (error) {
                    *error = QStringLiteral("Google Drive upload canceled");
                }
                return false;
            }
            break;
        }

        const QString failureMessage = reply.error.trimmed().isEmpty()
            ? QStringLiteral("Google Drive upload failed with HTTP %1").arg(reply.status)
            : reply.error;
        const bool rateLimited = isDriveUploadRateLimitError(failureMessage);
        if (rateLimited) {
            if (error) {
                *error = failureMessage;
            }
            if (logging) {
                qInfo() << "GDrive upload attempt failed"
                        << "mode" << "resumable"
                        << "context" << uploadContextText(logContext)
                        << "name" << name
                        << "bytes" << file.size()
                        << "elapsedMs" << uploadTimer.elapsed()
                        << "httpStatus" << reply.status
                        << "networkError" << reply.networkError
                        << "message" << failureMessage.left(180)
                        << "reason" << "rate-limit-abort";
            }
            noteDriveApiThrottle(QLatin1StringView("uploadChunk"), reply.headers, consecutiveRetry + 1, failureMessage);
            return false;
        }

        const bool retryable = reply.timedOut
            || isRetryableDriveUploadStatus(reply.status)
            || isRetryableDriveUploadNetworkError(reply.networkError)
            || isRetryableDriveUploadError(reply.error);
        if (!retryable || consecutiveRetry >= MaxResumableChunkAttempts - 1) {
            if (error) {
                *error = failureMessage;
            }
            if (logging) {
                qInfo() << "GDrive upload attempt failed"
                        << "mode" << "resumable"
                        << "context" << uploadContextText(logContext)
                        << "name" << name
                        << "bytes" << file.size()
                        << "elapsedMs" << uploadTimer.elapsed()
                        << "httpStatus" << reply.status
                        << "networkError" << reply.networkError
                        << "message" << failureMessage.left(180);
            }
            if (retryable) {
                noteDriveApiThrottle(QLatin1StringView("uploadChunk"), reply.headers, consecutiveRetry + 1, failureMessage);
            }
            return false;
        }

        ++consecutiveRetry;
        qint64 confirmedOffset = offset;
        QString queryError;
        queryResumableUploadOffset(network, sessionUrl, file.size(), accessToken, &confirmedOffset, &queryError);
        offset = std::clamp<qint64>(confirmedOffset, 0, file.size());
        const int delayMs = retryAfterDelayMs(reply.headers, consecutiveRetry);
        noteDriveApiThrottle(QLatin1StringView("uploadChunk"), reply.headers, consecutiveRetry, reply.error);
        if (logging) {
            qInfo() << "GDrive upload chunk retry scheduled"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "offset" << offset
                    << "delayMs" << delayMs
                    << "httpStatus" << reply.status
                    << "message" << reply.error.left(180);
        }
        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    if (logging) {
        qInfo() << "GDrive upload attempt finished"
                << "mode" << "resumable"
                << "context" << uploadContextText(logContext)
                << "name" << name
                << "bytes" << file.size()
                << "elapsedMs" << uploadTimer.elapsed();
    }
    noteDriveApiSuccess();
    return parseDriveFileResponse(finalBody, createdObject, error);
}


bool gdriveUploadLoggingEnabled()
{
    return qEnvironmentVariableIntValue("FMQML_GDRIVE_UPLOAD_LOG") > 0;
}

bool gdriveDownloadLoggingEnabled()
{
    return qEnvironmentVariableIntValue("FMQML_GDRIVE_DOWNLOAD_LOG") > 0;
}

bool gdriveDownloadRangeLoggingEnabled()
{
    return qEnvironmentVariableIntValue("FMQML_GDRIVE_DOWNLOAD_RANGE_LOG") > 0;
}

int gdriveUploadStallLogIntervalMs()
{
    bool ok = false;
    const int requested = qEnvironmentVariableIntValue("FMQML_GDRIVE_UPLOAD_STALL_LOG_MS", &ok);
    if (!ok) {
        return DefaultUploadStallLogMs;
    }
    if (requested <= 0) {
        return 0;
    }
    return std::clamp(requested, 1000, TransferIdleTimeoutMs);
}

int gdriveSmallUploadConcurrency()
{
    bool ok = false;
    const int requested = qEnvironmentVariableIntValue("FMQML_GDRIVE_UPLOAD_CONCURRENCY", &ok);
    if (!ok) {
        return DefaultSmallUploadConcurrency;
    }
    return std::clamp(requested, 1, MaxSmallUploadConcurrency);
}

int gdriveDownloadConcurrency()
{
    bool ok = false;
    const int requested = qEnvironmentVariableIntValue("FMQML_GDRIVE_DOWNLOAD_CONCURRENCY", &ok);
    if (!ok) {
        return DefaultDownloadConcurrency;
    }
    return std::clamp(requested, 1, MaxDownloadConcurrency);
}

bool isRetryableDriveUploadError(const QString &message)
{
    const QString lower = message.toLower();
    return lower.contains(QStringLiteral("429"))
        || lower.contains(QStringLiteral("too many requests"))
        || lower.contains(QStringLiteral("ratelimit"))
        || lower.contains(QStringLiteral("rate limit"))
        || lower.contains(QStringLiteral("user rate limit"))
        || lower.contains(QStringLiteral("http 500"))
        || lower.contains(QStringLiteral("http 502"))
        || lower.contains(QStringLiteral("http 503"))
        || lower.contains(QStringLiteral("http 504"))
        || lower.contains(QStringLiteral("temporarily unavailable"))
        || lower.contains(QStringLiteral("timed out"))
        || lower.contains(QStringLiteral("unknown protocol specified"))
        || lower.contains(QStringLiteral("protocolunknownerror"))
        || lower.contains(QStringLiteral("connection closed"))
        || lower.contains(QStringLiteral("remote host closed"));
}

bool isDriveUploadRateLimitError(const QString &message)
{
    const QString lower = message.toLower();
    return lower.contains(QStringLiteral("429"))
        || lower.contains(QStringLiteral("too many requests"))
        || lower.contains(QStringLiteral("ratelimit"))
        || lower.contains(QStringLiteral("rate limit"))
        || lower.contains(QStringLiteral("user rate limit"));
}

bool isRetryableDriveUploadStatus(int status)
{
    return status == 403
        || status == 408
        || status == 429
        || status == 500
        || status == 502
        || status == 503
        || status == 504;
}

bool isRetryableDriveUploadNetworkError(QNetworkReply::NetworkError error)
{
    return error == QNetworkReply::RemoteHostClosedError
        || error == QNetworkReply::TimeoutError
        || error == QNetworkReply::TemporaryNetworkFailureError
        || error == QNetworkReply::NetworkSessionFailedError
        || error == QNetworkReply::ProtocolFailure
        || error == QNetworkReply::ProtocolUnknownError
        || error == QNetworkReply::UnknownNetworkError;
}

int retryAfterDelayMs(const QHash<QByteArray, QByteArray> &headers, int fallbackAttempt)
{
    bool ok = false;
    const int retryAfterSeconds = QString::fromLatin1(headers.value(QByteArrayLiteral("retry-after")).trimmed()).toInt(&ok);
    if (ok && retryAfterSeconds > 0) {
        return std::clamp(retryAfterSeconds * 1000, 1000, 120000);
    }

    const int cappedAttempt = std::clamp(fallbackAttempt, 1, 8);
    const int baseMs = 1000 << (cappedAttempt - 1);
    const int jitterMs = QRandomGenerator::global()->bounded(500);
    return std::clamp(baseMs + jitterMs, 1000, 120000);
}

qint64 monotonicNowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

bool waitForDriveApiCooldown(QLatin1StringView operation,
                             const std::function<bool()> &shouldCancel)
{
    while (true) {
        if (shouldCancel && shouldCancel()) {
            return false;
        }
        qint64 waitMs = 0;
        int throttleCount = 0;
        {
            QMutexLocker locker(&s_driveApiCooldownMutex);
            waitMs = s_driveApiCooldownUntilMs - monotonicNowMs();
            throttleCount = s_driveApiThrottleCount;
        }
        if (waitMs <= 0) {
            return true;
        }
        const int sleepMs = static_cast<int>(std::clamp<qint64>(waitMs, 250, 5000));
        if (gdriveUploadLoggingEnabled()) {
            qInfo() << "GDrive API cooldown wait"
                    << "operation" << QString(operation)
                    << "delayMs" << sleepMs
                    << "remainingMs" << waitMs
                    << "throttleCount" << throttleCount;
        }
        QThread::msleep(static_cast<unsigned long>(sleepMs));
    }
}

void noteDriveApiSuccess()
{
    QMutexLocker locker(&s_driveApiCooldownMutex);
    if (s_driveApiThrottleCount > 0 && s_driveApiCooldownUntilMs <= monotonicNowMs()) {
        --s_driveApiThrottleCount;
    }
}

void noteDriveApiThrottle(QLatin1StringView operation,
                          const QHash<QByteArray, QByteArray> &headers,
                          int attempt,
                          const QString &message)
{
    const bool rateLimit = isDriveUploadRateLimitError(message);
    const int backoffAttempt = rateLimit ? attempt + 4 : attempt + 1;
    const int delayMs = std::clamp(retryAfterDelayMs(headers, backoffAttempt), 1000, DriveApiCooldownMaxMs);
    qint64 cooldownUntil = 0;
    int throttleCount = 0;
    {
        QMutexLocker locker(&s_driveApiCooldownMutex);
        s_driveApiThrottleCount = std::clamp(s_driveApiThrottleCount + (rateLimit ? 2 : 1), 1, 12);
        const qint64 now = monotonicNowMs();
        s_driveApiCooldownUntilMs = (std::max)(s_driveApiCooldownUntilMs, now + delayMs);
        cooldownUntil = s_driveApiCooldownUntilMs;
        throttleCount = s_driveApiThrottleCount;
    }
    if (gdriveUploadLoggingEnabled()) {
        qInfo() << "GDrive API cooldown set"
                << "operation" << QString(operation)
                << "delayMs" << delayMs
                << "untilMs" << cooldownUntil
                << "throttleCount" << throttleCount
                << "message" << message.left(180);
    }
}

qint64 acknowledgedResumableOffset(const QHash<QByteArray, QByteArray> &headers)
{
    const QString range = QString::fromLatin1(headers.value(QByteArrayLiteral("range")).trimmed());
    if (range.isEmpty()) {
        return 0;
    }

    static const QRegularExpression expression(QStringLiteral("^bytes=\\d+-(\\d+)$"));
    const QRegularExpressionMatch match = expression.match(range);
    if (!match.hasMatch()) {
        return 0;
    }

    bool ok = false;
    const qint64 lastByte = match.captured(1).toLongLong(&ok);
    return ok && lastByte >= 0 ? lastByte + 1 : 0;
}

GDriveUploadReplyResult waitForUploadReply(QNetworkReply *reply,
                                           int timeoutMs,
                                           const QString &timeoutMessage,
                                           const std::function<bool(qint64 sent, qint64 total)> &progress)
{
    GDriveUploadReplyResult result;
    QEventLoop loop;
    QTimer idleTimeout;
    QTimer cancelPollTimer;
    QElapsedTimer timer;
    qint64 lastSent = 0;
    qint64 lastTotal = 0;
    timer.start();
    idleTimeout.setSingleShot(true);
    cancelPollTimer.setInterval(250);

    QObject::connect(&idleTimeout, &QTimer::timeout, &loop, [&]() {
        result.timedOut = true;
        reply->abort();
    });
    QObject::connect(&cancelPollTimer, &QTimer::timeout, &loop, [&]() {
        if (progress && !progress(lastSent, lastTotal)) {
            result.canceled = true;
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::uploadProgress, &loop, [&](qint64 sent, qint64 total) {
        idleTimeout.start(timeoutMs);
        lastSent = sent;
        lastTotal = total;
        if (progress && !progress(sent, total)) {
            result.canceled = true;
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    idleTimeout.start(timeoutMs);
    cancelPollTimer.start();
    loop.exec();
    cancelPollTimer.stop();
    idleTimeout.stop();

    result.elapsedMs = timer.elapsed();
    result.body = safeReadAll(reply);
    for (const QByteArray &header : reply->rawHeaderList()) {
        const QByteArray value = reply->rawHeader(header);
        result.headers.insert(header, value);
        result.headers.insert(header.toLower(), value);
    }
    result.networkError = reply->error();
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString networkErrorString = reply->errorString();
    delete reply;

    if (result.timedOut) {
        result.error = timeoutMessage;
    } else if (result.canceled || result.networkError == QNetworkReply::OperationCanceledError) {
        result.canceled = true;
        result.error = QStringLiteral("Google Drive upload canceled");
    } else if (result.networkError != QNetworkReply::NoError) {
        const QString fallback = result.status > 0
            ? QStringLiteral("Google Drive upload failed with HTTP %1").arg(result.status)
            : QStringLiteral("Google Drive upload failed: %1").arg(networkErrorString);
        result.error = driveErrorMessage(result.body, fallback);
    } else if (result.status >= 400) {
        result.error = driveErrorMessage(result.body, QStringLiteral("Google Drive upload failed with HTTP %1").arg(result.status));
    }

    return result;
}

bool queryResumableUploadOffset(QNetworkAccessManager &network,
                                const QUrl &sessionUrl,
                                qint64 fileSize,
                                const QString &accessToken,
                                qint64 *offset,
                                QString *error)
{
    QNetworkRequest request(sessionUrl);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentLengthHeader, 0);
    request.setRawHeader("Content-Range", QByteArrayLiteral("bytes */") + QByteArray::number(fileSize));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    disableHttp2ForUpload(&request);

    waitForDriveApiCooldown(QLatin1StringView("uploadStatus"));
    const GDriveUploadReplyResult reply = waitForUploadReply(
        network.put(request, QByteArray{}),
        TransferIdleTimeoutMs,
        QStringLiteral("Google Drive upload status query timed out"),
        nullptr);
    if (reply.canceled) {
        if (error) {
            *error = reply.error;
        }
        return false;
    }
    if (reply.status == 308) {
        if (offset) {
            *offset = std::clamp<qint64>(acknowledgedResumableOffset(reply.headers), 0, fileSize);
        }
        noteDriveApiSuccess();
        return true;
    }
    if (reply.status == 200 || reply.status == 201) {
        if (offset) {
            *offset = fileSize;
        }
        noteDriveApiSuccess();
        return true;
    }
    if (error) {
        *error = reply.error.trimmed().isEmpty()
            ? QStringLiteral("Google Drive upload status query failed with HTTP %1").arg(reply.status)
            : reply.error;
    }
    if (isRetryableDriveUploadStatus(reply.status) || isRetryableDriveUploadError(reply.error)) {
        noteDriveApiThrottle(QLatin1StringView("uploadStatus"), reply.headers, 1, reply.error);
    }
    return false;
}

bool uploadLocalFileToDriveBlockingWithRetry(QNetworkAccessManager &network,
                                             const QString &sourceFilePath,
                                             const QString &parentId,
                                             const QString &name,
                                             const QString &accessToken,
                                             const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                             QJsonObject *createdObject,
                                             QString *error,
                                             int *retryCount,
                                             const GDriveUploadLogContext &logContext = {})
{
    constexpr int maxAttempts = 3;
    QString lastError;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        QJsonObject attemptObject;
        QString attemptError;
        GDriveUploadLogContext attemptContext = logContext;
        attemptContext.attempt = attempt;
        if (uploadLocalFileToDriveBlocking(network,
                                           sourceFilePath,
                                           parentId,
                                           name,
                                           accessToken,
                                           progress,
                                           &attemptObject,
                                           &attemptError,
                                           attemptContext)) {
            if (createdObject) {
                *createdObject = attemptObject;
            }
            if (error) {
                error->clear();
            }
            return true;
        }

        lastError = attemptError;
        if (isDriveUploadRateLimitError(attemptError)) {
            if (retryCount) {
                ++(*retryCount);
            }
            if (gdriveUploadLoggingEnabled()) {
                qInfo() << "GDrive upload retry suppressed"
                        << "context" << uploadContextText(attemptContext)
                        << "name" << name
                        << "message" << attemptError.left(180)
                        << "reason" << "rate-limit-abort";
            }
            break;
        }
        if (retryCount && attempt < maxAttempts && isRetryableDriveUploadError(attemptError)) {
            ++(*retryCount);
        }
        if (attempt >= maxAttempts || !isRetryableDriveUploadError(attemptError)) {
            break;
        }

        if (gdriveUploadLoggingEnabled()) {
            qInfo() << "GDrive upload retry scheduled"
                    << "context" << uploadContextText(attemptContext)
                    << "name" << name
                    << "message" << attemptError.left(180);
        }
        const int delayMs = isDriveUploadRateLimitError(attemptError)
            ? retryAfterDelayMs({}, attempt + 3)
            : retryAfterDelayMs({}, attempt);
        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    if (error) {
        *error = lastError;
    }
    return false;
}

struct GDrivePreparedUploadItem {
    LocalFileCopyItem item;
    GDriveCreateTarget target;
};

struct GDriveBatchUploadResult {
    qsizetype index = -1;
    bool success = false;
    QJsonObject createdObject;
    QString error;
    int retries = 0;
};

struct GDrivePreparedDownloadItem {
    LocalFileMaterializeItem item;
    QString progressName;
    QString downloadPath;
    QString mimeType;
    QString resourceKey;
    QString partialPath;
};

struct GDriveBatchDownloadResult {
    qsizetype index = -1;
    bool success = false;
    QString error;
};

QStringList listDriveChildrenBlocking(QNetworkAccessManager &network, const QString &path, QString *error)
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
            waitForDriveApiCooldown(QLatin1StringView("list"));
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
                noteDriveApiSuccess();
                break;
            }
            if (attempt >= DriveApiMaxAttempts || !isRetryableDriveUploadError(requestError)) {
                break;
            }
            noteDriveApiThrottle(QLatin1StringView("list"), headers, attempt, requestError);
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

FileEntry shortcutEntryWithTargetMetadata(FileEntry shortcutEntry, const FileEntry &targetEntry)
{
    shortcutEntry.size = targetEntry.size;
    shortcutEntry.modified = targetEntry.modified;
    shortcutEntry.created = targetEntry.created;
    shortcutEntry.modifiedText = targetEntry.modifiedText;
    shortcutEntry.createdText = targetEntry.createdText;
    shortcutEntry.mimeType = targetEntry.mimeType;
    shortcutEntry.isImage = targetEntry.isImage;
    shortcutEntry.hasThumbnail = targetEntry.hasThumbnail;
    shortcutEntry.providerCapabilitiesText = targetEntry.providerCapabilitiesText;
    if (!targetEntry.suffix.isEmpty()) {
        shortcutEntry.suffix = targetEntry.suffix;
    }
    return shortcutEntry;
}

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
            children = listDriveChildrenBlocking(m_network, normalizedParent, &error);
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

        const QByteArray cachedBytes = cachedGDriveThumbnailBytes(cacheIdentity);
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

        const GDriveThumbnailDownloadResult download = downloadGDriveThumbnailBytes(QUrl(thumbnailLink), accessToken);

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

        cacheGDriveThumbnailBytes(cacheIdentity, download.body);
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
        if (!trashDriveFileBlocking(m_network, id, accessToken, &trashedObject, &error)) {
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
        const QStringList listedChildren = listDriveChildrenBlocking(m_network, normalized, &error);
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
        if (!downloadDriveFileToLocalFile(m_network,
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
        qint64 totalBytes = 0;
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
            totalBytes += preparedItem.item.size;
            prepared.push_back(preparedItem);
        }

        const int concurrency = gdriveDownloadConcurrency();
        const bool logging = gdriveDownloadLoggingEnabled();
        const QString batchId = logging
            ? QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
            : QString{};
        QElapsedTimer timer;
        timer.start();
        if (logging) {
            qInfo() << "GDrive parallel download scheduler started"
                    << "batch" << batchId
                    << "files" << prepared.size()
                    << "bytes" << totalBytes
                    << "concurrency" << concurrency
                    << "authMs" << authTimer.elapsed()
                    << "prepareMs" << prepareTimer.elapsed();
        }

        QThreadPool pool;
        pool.setMaxThreadCount(concurrency);

        QVector<qint64> itemProgress(prepared.size(), 0);
        QMutex progressMutex;
        QMutex progressCallbackMutex;
        qint64 aggregateProgress = 0;
        std::atomic_bool canceled{false};

        QVector<QFuture<GDriveBatchDownloadResult>> futures;
        futures.reserve(prepared.size());
        const bool rangeLogging = gdriveDownloadRangeLoggingEnabled();
        for (qsizetype offset = 0; offset < prepared.size(); offset += concurrency) {
            const qsizetype waveEnd = (std::min)(prepared.size(), offset + concurrency);
            if (rangeLogging) {
                qInfo() << "GDrive parallel download scheduler queue range"
                        << "batch" << batchId
                        << "range" << QStringLiteral("%1-%2").arg(offset + 1).arg(waveEnd)
                        << "files" << (waveEnd - offset);
            }
            for (qsizetype i = offset; i < waveEnd; ++i) {
                futures.push_back(QtConcurrent::run(&pool, [&, i]() -> GDriveBatchDownloadResult {
                    const GDrivePreparedDownloadItem downloadItem = prepared.at(i);
                    GDriveBatchDownloadResult result;
                    result.index = i;
                    if (canceled.load()) {
                        result.error = QStringLiteral("Google Drive download canceled");
                        return result;
                    }

                    thread_local QNetworkAccessManager network;
                    QString downloadError;
                    const bool downloaded = downloadDriveFileToLocalFile(
                        network,
                        downloadItem.downloadPath,
                        downloadItem.mimeType,
                        downloadItem.partialPath,
                        accessToken,
                        [&, i, downloadItem](qint64 processed, qint64 total) -> bool {
                            Q_UNUSED(total)
                            if (canceled.load()) {
                                return false;
                            }
                            qint64 aggregate = 0;
                            {
                                QMutexLocker locker(&progressMutex);
                                const qint64 itemSize = downloadItem.item.size > 0 ? downloadItem.item.size : processed;
                                const qint64 boundedProcessed = std::clamp<qint64>(processed, 0, (std::max<qint64>)(0, itemSize));
                                aggregateProgress += boundedProcessed - itemProgress[i];
                                itemProgress[i] = boundedProcessed;
                                aggregate = aggregateProgress;
                            }
                            QMutexLocker callbackLocker(&progressCallbackMutex);
                            return !progress || progress(downloadItem.progressName, aggregate, totalBytes);
                        },
                        &downloadError,
                        downloadItem.resourceKey);
                    result.success = downloaded;
                    result.error = downloadError;
                    if (!downloaded) {
                        canceled = true;
                    }
                    return result;
                }));
            }
        }

        for (QFuture<GDriveBatchDownloadResult> &future : futures) {
            future.waitForFinished();
            const GDriveBatchDownloadResult result = future.result();
            if (!result.success) {
                for (const GDrivePreparedDownloadItem &item : std::as_const(prepared)) {
                    QFile::remove(item.partialPath);
                }
                const QString message = result.error.trimmed().isEmpty()
                    ? QStringLiteral("Google Drive download scheduler failed")
                    : result.error.trimmed();
                setLastError(message);
                if (error) {
                    *error = message;
                }
                return false;
            }

            const GDrivePreparedDownloadItem downloadItem = prepared.at(result.index);
            {
                QMutexLocker locker(&progressMutex);
                const qint64 finalProgress = downloadItem.item.size > 0
                    ? downloadItem.item.size
                    : itemProgress[result.index];
                aggregateProgress += finalProgress - itemProgress[result.index];
                itemProgress[result.index] = finalProgress;
            }
        }

        if (canceled.load()) {
            for (const GDrivePreparedDownloadItem &item : std::as_const(prepared)) {
                QFile::remove(item.partialPath);
            }
            const QString message = QStringLiteral("Google Drive download canceled");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }

        for (const GDrivePreparedDownloadItem &item : std::as_const(prepared)) {
            QFile::remove(item.item.destinationFilePath);
            if (!QFile::rename(item.partialPath, item.item.destinationFilePath)) {
                for (const GDrivePreparedDownloadItem &cleanupItem : std::as_const(prepared)) {
                    QFile::remove(cleanupItem.partialPath);
                }
                const QString message = QStringLiteral("Could not move Google Drive download into place");
                setLastError(message);
                if (error) {
                    *error = message;
                }
                return false;
            }
        }

        if (progress && !progress(QString{}, totalBytes, totalBytes)) {
            const QString message = QStringLiteral("Google Drive download canceled");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }
        if (logging) {
            qInfo() << "GDrive parallel download scheduler finished"
                    << "batch" << batchId
                    << "files" << prepared.size()
                    << "bytes" << totalBytes
                    << "ms" << timer.elapsed();
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

        const bool logging = gdriveUploadLoggingEnabled();
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

        QVector<GDrivePreparedUploadItem> prepared;
        prepared.reserve(items.size());
        qint64 totalBytes = 0;
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
            totalBytes += normalizedItem.size;
            prepared.push_back(GDrivePreparedUploadItem{normalizedItem, target});
        }

        const int concurrency = gdriveSmallUploadConcurrency();
        const QString batchId = logging
            ? QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
            : QString{};
        QElapsedTimer timer;
        timer.start();
        if (logging) {
            qInfo() << "GDrive parallel upload scheduler prepared"
                    << "files" << prepared.size()
                    << "bytes" << totalBytes
                    << "targetMs" << targetTimer.elapsed()
                    << "prepareMs" << prepareTimer.elapsed();
            qInfo() << "GDrive parallel upload scheduler started"
                    << "batch" << batchId
                    << "files" << prepared.size()
                    << "bytes" << totalBytes
                    << "concurrency" << concurrency;
        }

        QThreadPool pool;
        pool.setMaxThreadCount(concurrency);

        QVector<qint64> itemProgress(prepared.size(), 0);
        QMutex progressMutex;
        QMutex progressCallbackMutex;
        qint64 aggregateProgress = 0;
        std::atomic_bool canceled{false};
        int totalRetries = 0;

        QVector<QFuture<GDriveBatchUploadResult>> futures;
        futures.reserve(prepared.size());
        for (qsizetype offset = 0; offset < prepared.size(); offset += concurrency) {
            const qsizetype waveEnd = (std::min)(prepared.size(), offset + concurrency);
            if (logging) {
                qInfo() << "GDrive parallel upload scheduler queue range"
                        << "batch" << batchId
                        << "range" << QStringLiteral("%1-%2").arg(offset + 1).arg(waveEnd)
                        << "files" << (waveEnd - offset);
            }
            for (qsizetype i = offset; i < waveEnd; ++i) {
                futures.push_back(QtConcurrent::run(&pool, [&, i]() -> GDriveBatchUploadResult {
                    const GDrivePreparedUploadItem uploadItem = prepared.at(i);
                    GDriveBatchUploadResult result;
                    result.index = i;
                    if (canceled.load()) {
                        result.error = QStringLiteral("Google Drive upload canceled");
                        return result;
                    }

                    thread_local QNetworkAccessManager network;
                    QString uploadError;
                    const qsizetype rangeStart = (i / concurrency) * concurrency;
                    const qsizetype rangeEnd = (std::min)(prepared.size(), rangeStart + concurrency);
                    const bool uploaded = uploadLocalFileToDriveBlockingWithRetry(
                        network,
                        uploadItem.item.sourceFilePath,
                        uploadItem.target.parentId,
                        uploadItem.target.name,
                        accessToken,
                        [&, i](qint64 processed, qint64 total) -> bool {
                            Q_UNUSED(total)
                            if (canceled.load()) {
                                return false;
                            }
                            qint64 aggregate = 0;
                            {
                                QMutexLocker locker(&progressMutex);
                                const qint64 boundedProcessed = std::clamp<qint64>(processed, 0, prepared.at(i).item.size);
                                aggregateProgress += boundedProcessed - itemProgress[i];
                                itemProgress[i] = boundedProcessed;
                                aggregate = aggregateProgress;
                            }
                            QMutexLocker callbackLocker(&progressCallbackMutex);
                            return !progress || progress(uploadItem.item.sourceFilePath, aggregate, totalBytes);
                        },
                        &result.createdObject,
                        &uploadError,
                        &result.retries,
                        GDriveUploadLogContext{
                            batchId,
                            i,
                            prepared.size(),
                            rangeStart,
                            rangeEnd,
                            1
                        });
                    result.success = uploaded;
                    result.error = uploadError;
                    if (!uploaded) {
                        canceled = true;
                    }
                    return result;
                }));
            }
        }

        for (QFuture<GDriveBatchUploadResult> &future : futures) {
            future.waitForFinished();
            const GDriveBatchUploadResult result = future.result();
            totalRetries += result.retries;
            if (!result.success) {
                const QString message = result.error.trimmed().isEmpty()
                    ? QStringLiteral("Google Drive upload scheduler failed")
                    : result.error.trimmed();
                setLastError(message);
                if (error) {
                    *error = message;
                }
                return false;
            }

            const GDrivePreparedUploadItem uploadItem = prepared.at(result.index);
            const FileEntry entry = cacheDriveFileObject(result.createdObject, uploadItem.target.parentPath);
            if (!entry.path.isEmpty()) {
                m_createdPaths.insert(normalizedPath(uploadItem.item.destinationPath), entry.path);
            }
            {
                QMutexLocker locker(&progressMutex);
                aggregateProgress += uploadItem.item.size - itemProgress[result.index];
                itemProgress[result.index] = uploadItem.item.size;
            }
        }

        if (canceled.load()) {
            const QString message = QStringLiteral("Google Drive upload canceled");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }

        markStorageQuotaRefreshPending();
        if (progress && !progress(QString{}, totalBytes, totalBytes)) {
            const QString message = QStringLiteral("Google Drive upload canceled");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }
        if (logging) {
            qInfo() << "GDrive parallel upload scheduler finished"
                    << "batch" << batchId
                    << "files" << prepared.size()
                    << "bytes" << totalBytes
                    << "ms" << timer.elapsed()
                    << "retries" << totalRetries;
        }
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
        if (!uploadLocalFileToDriveBlockingWithRetry(m_network,
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

        if (refreshDriveStorageQuotaBlocking(m_network, accessToken, nullptr)) {
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
                refreshDriveStorageQuotaBlocking(m_network, accessToken, nullptr);
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
        if (!createDriveMetadataBlocking(m_network,
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
                finish(generation, false, driveErrorMessage(body, fallback));
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
        if (!fetchDriveFileMetadataBlocking(m_network,
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

int GDriveFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}

QString GDriveFileProviderPlugin::pluginId() const
{
    return QStringLiteral("fm.gdrive-provider");
}

QString GDriveFileProviderPlugin::displayName() const
{
    return QStringLiteral("Google Drive Provider");
}

QStringList GDriveFileProviderPlugin::schemes() const
{
    return {QStringLiteral("gdrive")};
}

bool GDriveFileProviderPlugin::canHandle(const QString &path) const
{
    return !GDrivePath::normalizedPath(path).isEmpty();
}

std::unique_ptr<FileProvider> GDriveFileProviderPlugin::createProvider()
{
    return std::make_unique<GDriveFileProvider>();
}

int GDriveFileProviderPlugin::actionApiVersion() const
{
    return FM_FILE_ACTION_PLUGIN_API_VERSION;
}

QString GDriveFileProviderPlugin::actionPluginId() const
{
    return pluginId();
}

QString GDriveFileProviderPlugin::actionDisplayName() const
{
    return displayName();
}

QList<FileActionDescriptor> GDriveFileProviderPlugin::actionsForContext(const FileActionContext &context) const
{
    const QString targetPath = GDrivePath::normalizedPath(context.targetPath);
    if (targetPath.isEmpty()) {
        return {};
    }

    QList<FileActionDescriptor> actions;
    const bool trashTarget = targetPath != GDrivePath::Trash && isSharedTrashViewPath(targetPath);
    if (trashTarget) {
        FileActionDescriptor restore;
        restore.id = QString(GoogleDriveRestoreAction);
        restore.text = QStringLiteral("Restore");
        restore.iconSource = QStringLiteral("../assets/icons/refresh.svg");
        restore.order = 80;
        actions.append(restore);
    }

    const bool hasDestinationPanel = !context.destinationPath.trimmed().isEmpty();
    if (!trashTarget && hasDestinationPanel && googleAppsExportTargetForPath(targetPath)) {
        FileActionDescriptor downloadPdf;
        downloadPdf.id = QString(GoogleDriveDownloadPdfAction);
        downloadPdf.text = QStringLiteral("Download as PDF");
        downloadPdf.iconSource = QStringLiteral("../assets/icons/download.svg");
        downloadPdf.order = 100;
        actions.append(downloadPdf);
    }

    FileActionDescriptor rawCapabilities;
    rawCapabilities.id = QString(GoogleDriveRawCapabilitiesAction);
    rawCapabilities.text = QStringLiteral("Raw capabilities");
    rawCapabilities.iconSource = QStringLiteral("../assets/icons/info.svg");
    rawCapabilities.order = 900;
    actions.append(rawCapabilities);
    return actions;
}

QVariantMap GDriveFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    if (actionId == GoogleDriveSignOutAction) {
        const bool ok = clearSavedAuthorization();
        if (ok) {
            clearSharedMetadata();
        }
        return {
            {QStringLiteral("ok"), ok},
            {QStringLiteral("title"), QStringLiteral("Google Drive")},
            {QStringLiteral("message"),
             ok
                 ? QStringLiteral("Google Drive authorization was removed.")
                 : QStringLiteral("Google Drive authorization could not be removed.")},
        };
    }

    if (actionId == GoogleDriveAuthStatusAction) {
        const AccountInfo accountInfo = savedAccountInfo();
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Google Drive")},
            {QStringLiteral("signedIn"), hasSavedAuthorization()},
            {QStringLiteral("accountName"), accountInfo.displayName},
            {QStringLiteral("accountEmail"), accountInfo.email},
            {QStringLiteral("accountLabel"), accountInfo.label()},
        };
    }

    if (actionId == GoogleDriveDownloadPdfAction) {
        const QString targetPath = GDrivePath::normalizedPath(context.targetPath);
        const std::optional<GDriveGoogleAppsExportTarget> exportTarget = googleAppsExportTargetForPath(targetPath);
        if (!exportTarget) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"), QStringLiteral("This action is available only for Google-native Drive files.")},
            };
        }

        const std::optional<GDriveItemCapabilities> downloadCapabilities =
            sharedCapabilities(exportTarget->sourcePath);
        if (downloadCapabilities && !downloadCapabilities->canDownload) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"), QStringLiteral("Google Drive does not allow downloading this file.")},
            };
        }

        const QString destinationFolder = QDir::fromNativeSeparators(context.destinationPath.trimmed());
        const QFileInfo destinationInfo(destinationFolder);
        if (destinationFolder.isEmpty()
            || destinationFolder.contains(QStringLiteral("://"))
            || !destinationInfo.exists()
            || !destinationInfo.isDir()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"),
                 QStringLiteral("Open a local folder in the opposite panel before downloading a Drive file as PDF.")},
            };
        }
        if (!destinationInfo.isWritable()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"), QStringLiteral("The opposite panel folder is not writable.")},
            };
        }

        const QString fileName = safeLocalExportFileName(withExportSuffix(exportTarget->displayName, QStringLiteral("pdf")));
        const QString destinationFilePath = uniqueLocalFilePath(QDir(destinationFolder).filePath(fileName));

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"), authError.isEmpty() ? QStringLiteral("Google Drive authorization failed.") : authError},
            };
        }

        QNetworkAccessManager network;
        QString downloadError;
        const bool ok = downloadDriveFileToLocalFile(network,
                                                     exportTarget->sourcePath,
                                                     exportTarget->mimeType,
                                                     destinationFilePath,
                                                     accessToken,
                                                     {},
                                                     &downloadError);
        if (!ok) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"),
                 downloadError.isEmpty() ? QStringLiteral("Google Drive PDF download failed.") : downloadError},
            };
        }

        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Download as PDF")},
            {QStringLiteral("subtitle"), QStringLiteral("Google Drive")},
            {QStringLiteral("message"), QStringLiteral("Saved PDF to the opposite panel folder.")},
            {QStringLiteral("statusMessage"), QStringLiteral("\"%1\" saved as PDF").arg(QFileInfo(destinationFilePath).fileName())},
            {QStringLiteral("properties"), QVariantList{
                 QVariantMap{
                     {QStringLiteral("label"), QStringLiteral("File")},
                     {QStringLiteral("value"), QDir::toNativeSeparators(destinationFilePath)},
                 },
             }},
        };
    }

    if (actionId == GoogleDriveRestoreAction) {
        const QString targetPath = GDrivePath::normalizedPath(context.targetPath);
        if (targetPath.isEmpty() || targetPath == GDrivePath::Trash || !isSharedTrashViewPath(targetPath)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Restore")},
                {QStringLiteral("message"), QStringLiteral("This action is available only for items in Google Drive Trash.")},
            };
        }

        const QString id = GDrivePath::idForItemPath(targetPath);
        if (id.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Restore")},
                {QStringLiteral("message"), QStringLiteral("Google Drive item path is invalid.")},
            };
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Restore")},
                {QStringLiteral("message"), authError.isEmpty() ? QStringLiteral("Google Drive authorization failed.") : authError},
            };
        }

        QNetworkAccessManager network;
        QJsonObject restoredObject;
        QString restoreError;
        if (!restoreDriveFileBlocking(network, id, accessToken, &restoredObject, &restoreError)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Restore")},
                {QStringLiteral("message"),
                 restoreError.isEmpty() ? QStringLiteral("Google Drive restore failed.") : restoreError},
            };
        }

        const std::optional<FileEntry> restoredEntry = sharedEntry(targetPath);
        const QString restoredName = restoredEntry ? restoredEntry->name : GDrivePath::fallbackFileNameForPath(targetPath);
        const QString parent = sharedParent(targetPath);
        removeSharedPath(targetPath, parent);
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Restore")},
            {QStringLiteral("subtitle"), QStringLiteral("Google Drive Trash")},
            {QStringLiteral("message"), QStringLiteral("Item was restored from Trash.")},
            {QStringLiteral("refreshCurrentPath"), true},
            {QStringLiteral("statusMessage"), QStringLiteral("\"%1\" restored").arg(restoredName)},
        };
    }

    if (actionId == GoogleDriveRawCapabilitiesAction) {
        const QString targetPath = GDrivePath::normalizedPath(context.targetPath);
        if (targetPath.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Google Drive raw capabilities")},
                {QStringLiteral("message"), QStringLiteral("This action is available only for Google Drive paths.")},
            };
        }

        const std::optional<GDriveItemCapabilities> capabilities = sharedCapabilities(targetPath);
        if (!capabilities) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Google Drive raw capabilities")},
                {QStringLiteral("subtitle"), QStringLiteral("Provider diagnostics")},
                {QStringLiteral("message"),
                 QStringLiteral("No cached capabilities for this item.\nOpen or refresh its parent folder first.\n\nPath: %1")
                     .arg(targetPath)},
            };
        }

        QVariantList properties;
        properties.append(QVariantMap{
            {QStringLiteral("label"), QStringLiteral("Path")},
            {QStringLiteral("value"), targetPath},
        });
        properties.append(driveCapabilitiesProperties(*capabilities));

        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Google Drive raw capabilities")},
            {QStringLiteral("subtitle"), QStringLiteral("Provider diagnostics")},
            {QStringLiteral("message"), QStringLiteral("Cached Google Drive capabilities reported by the API.")},
            {QStringLiteral("properties"), properties},
        };
    }

    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("title"), QStringLiteral("Google Drive")},
        {QStringLiteral("message"), QStringLiteral("Unknown Google Drive action.")},
    };
}
