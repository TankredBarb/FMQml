#include "GDriveFileProviderPlugin.h"

#include <algorithm>
#include <atomic>
#include <optional>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMimeDatabase>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QOAuth2AuthorizationCodeFlow>
#include <QOAuthHttpServerReplyHandler>
#include <QPointer>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantList>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincred.h>
#endif

namespace {

constexpr QLatin1StringView GDriveRoot{"gdrive://"};
constexpr QLatin1StringView GDriveMyDrive{"gdrive://my-drive"};
constexpr QLatin1StringView GDriveSharedWithMe{"gdrive://shared-with-me"};
constexpr QLatin1StringView GDriveItemPrefix{"gdrive://item/"};
constexpr QLatin1StringView GDriveNewPrefix{"gdrive://new/"};
constexpr QLatin1StringView GoogleDriveFolderMime{"application/vnd.google-apps.folder"};
constexpr QLatin1StringView GoogleDriveAppsMimePrefix{"application/vnd.google-apps."};
constexpr QLatin1StringView GoogleDriveScope{"https://www.googleapis.com/auth/drive"};
constexpr QLatin1StringView GoogleDriveSignOutAction{"signOut"};
constexpr QLatin1StringView GoogleDriveRawCapabilitiesAction{"rawCapabilities"};
constexpr QLatin1StringView DriveListFields{
    "nextPageToken,files(id,name,mimeType,size,modifiedTime,createdTime,parents,webViewLink,ownedByMe,shared,"
    "capabilities(canDownload,canEdit,canAddChildren,canListChildren,canRename,canTrash,canDelete,canCopy))"};
constexpr QLatin1StringView DriveFileFields{
    "id,name,mimeType,size,modifiedTime,createdTime,parents,webViewLink,ownedByMe,shared,"
    "capabilities(canDownload,canEdit,canAddChildren,canListChildren,canRename,canTrash,canDelete,canCopy)"};
constexpr QLatin1StringView DriveAboutFields{"storageQuota(limit,usage)"};
constexpr QLatin1StringView CredentialTarget{"FMQml/GoogleDrive/OAuthRefreshToken"};

bool isGDriveSchemePath(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return false;
    }
    return trimmed.left(separatorIndex).compare(QStringLiteral("gdrive"), Qt::CaseInsensitive) == 0;
}

QString itemPathForId(const QString &id)
{
    const QString encodedId = QString::fromLatin1(QUrl::toPercentEncoding(id));
    return QString(GDriveItemPrefix) + encodedId;
}

QString idForItemPath(const QString &path)
{
    if (!path.startsWith(GDriveItemPrefix)) {
        return {};
    }
    return QUrl::fromPercentEncoding(path.mid(QString(GDriveItemPrefix).size()).toUtf8());
}

struct GDrivePendingPath {
    QString parentId;
    QString name;

    bool valid() const
    {
        return !parentId.trimmed().isEmpty() && !name.trimmed().isEmpty();
    }
};

QString pendingPathForParentIdAndName(const QString &parentId, const QString &name)
{
    const QString cleanParentId = parentId.trimmed();
    const QString cleanName = name.trimmed();
    if (cleanParentId.isEmpty() || cleanName.isEmpty()) {
        return {};
    }
    return QString(GDriveNewPrefix)
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanParentId))
        + QLatin1Char('/')
        + QString::fromLatin1(QUrl::toPercentEncoding(cleanName));
}

GDrivePendingPath pendingPathInfo(const QString &path)
{
    if (!path.startsWith(GDriveNewPrefix)) {
        return {};
    }

    const QString tail = path.mid(QString(GDriveNewPrefix).size());
    const int slash = tail.indexOf(QLatin1Char('/'));
    if (slash <= 0 || slash == tail.size() - 1) {
        return {};
    }

    GDrivePendingPath result;
    result.parentId = QUrl::fromPercentEncoding(tail.left(slash).toUtf8());
    result.name = QUrl::fromPercentEncoding(tail.mid(slash + 1).toUtf8());
    return result;
}

QString parentPathForDriveParentId(const QString &parentId)
{
    if (parentId == QLatin1String("root")) {
        return QString(GDriveMyDrive);
    }
    return itemPathForId(parentId);
}

QString driveParentIdForPath(const QString &path)
{
    if (path == GDriveMyDrive) {
        return QStringLiteral("root");
    }
    return idForItemPath(path);
}

QString normalizedGDrivePath(QString path)
{
    path = path.trimmed();
    if (path.compare(QStringLiteral("gdrive:"), Qt::CaseInsensitive) == 0
        || path.compare(QStringLiteral("gdrive:/"), Qt::CaseInsensitive) == 0
        || path.compare(QStringLiteral("gdrive://"), Qt::CaseInsensitive) == 0) {
        return QString(GDriveRoot);
    }

    if (!isGDriveSchemePath(path)) {
        return {};
    }

    QString tail = path.mid(path.indexOf(QStringLiteral("://")) + 3);
    tail.replace(QLatin1Char('\\'), QLatin1Char('/'));
    while (tail.startsWith(QLatin1Char('/'))) {
        tail.remove(0, 1);
    }
    while (tail.endsWith(QLatin1Char('/'))) {
        tail.chop(1);
    }

    if (tail.isEmpty()) {
        return QString(GDriveRoot);
    }

    const QString lowerTail = tail.toLower();
    if (lowerTail == QStringLiteral("my-drive")) {
        return QString(GDriveMyDrive);
    }
    if (lowerTail == QStringLiteral("shared-with-me")) {
        return QString(GDriveSharedWithMe);
    }
    if (lowerTail.startsWith(QStringLiteral("item/")) && lowerTail.size() > 5) {
        return QString(GDriveItemPrefix) + tail.mid(5);
    }
    if (lowerTail.startsWith(QStringLiteral("new/")) && lowerTail.size() > 4) {
        const GDrivePendingPath pending = pendingPathInfo(QString(GDriveRoot) + tail);
        if (pending.valid()) {
            return pendingPathForParentIdAndName(pending.parentId, pending.name);
        }
    }

    return {};
}

QString parentGDrivePath(const QString &path)
{
    const QString normalized = normalizedGDrivePath(path);
    if (normalized == GDriveMyDrive || normalized == GDriveSharedWithMe) {
        return QString(GDriveRoot);
    }
    const GDrivePendingPath pending = pendingPathInfo(normalized);
    if (pending.valid()) {
        return parentPathForDriveParentId(pending.parentId);
    }
    return {};
}

QString fallbackFileNameForPath(const QString &path)
{
    const QString normalized = normalizedGDrivePath(path);
    if (normalized == GDriveRoot) {
        return QStringLiteral("Google Drive");
    }
    if (normalized == GDriveMyDrive) {
        return QStringLiteral("My Drive");
    }
    if (normalized == GDriveSharedWithMe) {
        return QStringLiteral("Shared with me");
    }
    const QString id = idForItemPath(normalized);
    const GDrivePendingPath pending = pendingPathInfo(normalized);
    if (pending.valid()) {
        return pending.name;
    }
    return id.isEmpty() ? QStringLiteral("Google Drive") : id;
}

QString childGDrivePath(const QString &parentPath, const QString &name)
{
    const QString normalizedParent = normalizedGDrivePath(parentPath);
    const QString cleanName = name.trimmed();
    if (normalizedParent == GDriveRoot) {
        if (cleanName.compare(QStringLiteral("My Drive"), Qt::CaseInsensitive) == 0) {
            return QString(GDriveMyDrive);
        }
        if (cleanName.compare(QStringLiteral("Shared with me"), Qt::CaseInsensitive) == 0) {
            return QString(GDriveSharedWithMe);
        }
    }
    return {};
}

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
        && mimeType != GoogleDriveFolderMime;
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

struct OAuthClientConfig {
    QString filePath;
    QString clientId;
    QString clientSecret;
    QUrl authorizationUrl;
    QUrl tokenUrl;
    QString error;

    bool valid() const
    {
        return !clientId.isEmpty() && authorizationUrl.isValid() && tokenUrl.isValid();
    }
};

struct GDriveAuthSession {
    QString accessToken;
    QDateTime accessTokenExpiresAt;
    QString refreshToken;
    bool refreshTokenLoaded = false;
};

struct GDriveItemCapabilities {
    bool canDownload = false;
    bool canEdit = false;
    bool canAddChildren = false;
    bool canListChildren = false;
    bool canRename = false;
    bool canTrash = false;
    bool canDelete = false;
    bool canCopy = false;
};

struct GDriveStorageQuota {
    qint64 total = -1;
    qint64 used = -1;
    qint64 free = -1;
    bool valid = false;
    QDateTime cachedAt;
};

struct GDriveCreateTarget {
    QString parentPath;
    QString parentId;
    QString name;

    bool valid() const
    {
        return !parentPath.isEmpty() && !parentId.isEmpty() && !name.trimmed().isEmpty();
    }
};

struct GDriveSharedMetadata {
    QHash<QString, FileEntry> entries;
    QHash<QString, QStringList> children;
    QHash<QString, QString> parents;
    QHash<QString, QString> mimeTypes;
    QHash<QString, GDriveItemCapabilities> capabilities;
    GDriveStorageQuota quota;
};

QMutex &authSessionMutex()
{
    static QMutex mutex;
    return mutex;
}

GDriveAuthSession &authSession()
{
    static GDriveAuthSession session;
    return session;
}

QMutex &sharedMetadataMutex()
{
    static QMutex mutex;
    return mutex;
}

GDriveSharedMetadata &sharedMetadata()
{
    static GDriveSharedMetadata metadata;
    return metadata;
}

#ifdef Q_OS_WIN
QString readCredentialRefreshToken()
{
    PCREDENTIALW credential = nullptr;
    const std::wstring target = QString(CredentialTarget).toStdWString();
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential) || !credential) {
        return {};
    }

    const QByteArray bytes(reinterpret_cast<const char *>(credential->CredentialBlob),
                           static_cast<int>(credential->CredentialBlobSize));
    const QString token = QString::fromUtf8(bytes);
    CredFree(credential);
    return token;
}

bool writeCredentialRefreshToken(const QString &refreshToken)
{
    const QByteArray bytes = refreshToken.toUtf8();
    if (bytes.isEmpty()) {
        return false;
    }

    const std::wstring target = QString(CredentialTarget).toStdWString();
    const std::wstring userName = QStringLiteral("default").toStdWString();

    CREDENTIALW credential;
    ZeroMemory(&credential, sizeof(credential));
    credential.Type = CRED_TYPE_GENERIC;
    credential.TargetName = const_cast<LPWSTR>(target.c_str());
    credential.CredentialBlobSize = static_cast<DWORD>(bytes.size());
    credential.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char *>(bytes.constData()));
    credential.Persist = CRED_PERSIST_LOCAL_MACHINE;
    credential.UserName = const_cast<LPWSTR>(userName.c_str());

    return CredWriteW(&credential, 0);
}

bool deleteCredentialRefreshToken()
{
    const std::wstring target = QString(CredentialTarget).toStdWString();
    if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        return true;
    }
    const DWORD error = GetLastError();
    return error == ERROR_NOT_FOUND || error == ERROR_NO_SUCH_LOGON_SESSION;
}
#else
QString readCredentialRefreshToken()
{
    return {};
}

bool writeCredentialRefreshToken(const QString &)
{
    return false;
}

bool deleteCredentialRefreshToken()
{
    return true;
}
#endif

QString validSessionAccessToken()
{
    QMutexLocker locker(&authSessionMutex());
    const GDriveAuthSession &session = authSession();
    if (session.accessToken.isEmpty()) {
        return {};
    }
    if (session.accessTokenExpiresAt.isValid()
        && QDateTime::currentDateTimeUtc().secsTo(session.accessTokenExpiresAt.toUTC()) <= 60) {
        return {};
    }
    return session.accessToken;
}

QString sessionRefreshToken()
{
    QMutexLocker locker(&authSessionMutex());
    GDriveAuthSession &session = authSession();
    if (!session.refreshTokenLoaded) {
        session.refreshToken = readCredentialRefreshToken();
        session.refreshTokenLoaded = true;
    }
    return session.refreshToken;
}

void rememberAccessToken(const QString &accessToken, QDateTime expiresAt)
{
    if (accessToken.isEmpty()) {
        return;
    }
    if (!expiresAt.isValid()) {
        expiresAt = QDateTime::currentDateTimeUtc().addSecs(3600);
    }

    QMutexLocker locker(&authSessionMutex());
    GDriveAuthSession &session = authSession();
    session.accessToken = accessToken;
    session.accessTokenExpiresAt = expiresAt.toUTC();
}

bool rememberRefreshToken(const QString &refreshToken)
{
    if (refreshToken.trimmed().isEmpty()) {
        return true;
    }
    if (!writeCredentialRefreshToken(refreshToken)) {
        return false;
    }

    QMutexLocker locker(&authSessionMutex());
    GDriveAuthSession &session = authSession();
    session.refreshToken = refreshToken;
    session.refreshTokenLoaded = true;
    return true;
}

bool clearSavedAuthorization()
{
    const bool deleted = deleteCredentialRefreshToken();
    QMutexLocker locker(&authSessionMutex());
    GDriveAuthSession &session = authSession();
    session.accessToken.clear();
    session.accessTokenExpiresAt = {};
    session.refreshToken.clear();
    session.refreshTokenLoaded = true;
    return deleted;
}

void cacheSharedEntry(const FileEntry &entry,
                      const QString &parentPath,
                      const QString &mimeType,
                      const GDriveItemCapabilities &capabilities = {})
{
    QMutexLocker locker(&sharedMetadataMutex());
    GDriveSharedMetadata &metadata = sharedMetadata();
    metadata.entries.insert(entry.path, entry);
    metadata.parents.insert(entry.path, parentPath);
    metadata.mimeTypes.insert(entry.path, mimeType);
    metadata.capabilities.insert(entry.path, capabilities);
}

void cacheSharedChildren(const QString &parentPath, const QStringList &children)
{
    QMutexLocker locker(&sharedMetadataMutex());
    sharedMetadata().children.insert(parentPath, children);
}

void removeSharedPath(const QString &path, const QString &parentPath)
{
    QMutexLocker locker(&sharedMetadataMutex());
    GDriveSharedMetadata &metadata = sharedMetadata();
    metadata.entries.remove(path);
    metadata.parents.remove(path);
    metadata.mimeTypes.remove(path);
    metadata.capabilities.remove(path);
    if (!parentPath.isEmpty()) {
        QStringList children = metadata.children.value(parentPath);
        children.removeAll(path);
        metadata.children.insert(parentPath, children);
    }
    metadata.children.remove(path);
}

std::optional<FileEntry> sharedEntry(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    const auto it = sharedMetadata().entries.constFind(path);
    if (it == sharedMetadata().entries.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

QStringList sharedChildren(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    return sharedMetadata().children.value(path);
}

std::optional<QStringList> sharedChildrenIfCached(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    const auto it = sharedMetadata().children.constFind(path);
    if (it == sharedMetadata().children.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

QString sharedParent(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    return sharedMetadata().parents.value(path);
}

QString sharedMimeType(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    return sharedMetadata().mimeTypes.value(path);
}

std::optional<GDriveItemCapabilities> sharedCapabilities(const QString &path)
{
    QMutexLocker locker(&sharedMetadataMutex());
    const auto it = sharedMetadata().capabilities.constFind(path);
    if (it == sharedMetadata().capabilities.constEnd()) {
        return std::nullopt;
    }
    return it.value();
}

void cacheSharedQuota(const GDriveStorageQuota &quota)
{
    if (!quota.valid) {
        return;
    }

    QMutexLocker locker(&sharedMetadataMutex());
    sharedMetadata().quota = quota;
}

std::optional<GDriveStorageQuota> sharedQuota()
{
    QMutexLocker locker(&sharedMetadataMutex());
    const GDriveStorageQuota quota = sharedMetadata().quota;
    if (!quota.valid) {
        return std::nullopt;
    }
    return quota;
}

OAuthClientConfig loadOAuthClientConfig()
{
    OAuthClientConfig config;
    const QString envPath = QString::fromLocal8Bit(qgetenv("FM_GOOGLE_OAUTH_CLIENT_JSON")).trimmed();
    if (envPath.isEmpty()) {
        config.error = QStringLiteral("Google OAuth client JSON not configured. Set FM_GOOGLE_OAUTH_CLIENT_JSON.");
        return config;
    }
    config.filePath = envPath;

    QFile file(config.filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        config.error = QStringLiteral("Google OAuth client JSON not found. Set FM_GOOGLE_OAUTH_CLIENT_JSON.");
        return config;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        config.error = QStringLiteral("Google OAuth client JSON is invalid.");
        return config;
    }

    const QJsonObject root = document.object();
    const QJsonObject installed = root.value(QStringLiteral("installed")).toObject();
    if (installed.isEmpty()) {
        config.error = QStringLiteral("Google OAuth client JSON must be a Desktop app client.");
        return config;
    }

    config.clientId = installed.value(QStringLiteral("client_id")).toString().trimmed();
    config.clientSecret = installed.value(QStringLiteral("client_secret")).toString();
    config.authorizationUrl = QUrl(installed.value(QStringLiteral("auth_uri")).toString());
    config.tokenUrl = QUrl(installed.value(QStringLiteral("token_uri")).toString());

    if (!config.valid()) {
        config.error = QStringLiteral("Google OAuth client JSON is missing required fields.");
    }
    return config;
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
    if (path == GDriveMyDrive) {
        return QStringLiteral("'root' in parents and trashed = false");
    }
    if (path == GDriveSharedWithMe) {
        return QStringLiteral("sharedWithMe = true and trashed = false");
    }

    const QString folderId = idForItemPath(path);
    if (!folderId.isEmpty()) {
        return QStringLiteral("'%1' in parents and trashed = false").arg(folderId);
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

FileEntry entryFromDriveFileObject(const QJsonObject &object)
{
    const QString id = object.value(QStringLiteral("id")).toString().trimmed();
    const QString name = object.value(QStringLiteral("name")).toString().trimmed();
    const QString mimeType = object.value(QStringLiteral("mimeType")).toString();
    if (id.isEmpty() || name.isEmpty()) {
        return {};
    }

    const bool directory = mimeType == GoogleDriveFolderMime;
    const GDriveItemCapabilities capabilities = driveCapabilitiesFromDriveFileObject(object);
    FileEntry entry;
        entry.name = name;
        entry.path = itemPathForId(id);
        entry.mimeType = mimeType;
        entry.suffix = directory ? QString{} : suffixForName(name);
        if (entry.suffix.isEmpty()) {
            entry.suffix = iconSuffixForMimeType(mimeType);
        }
    entry.isDirectory = directory;
    entry.isReadOnly = true;
    entry.isImage = isImageMimeType(mimeType);
    entry.hasThumbnail = false;
    entry.providerCapabilitiesText = driveCapabilitiesText(capabilities);

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

QString tokenEndpointErrorMessage(const QByteArray &body, const QString &fallback)
{
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const QJsonObject object = document.object();
    const QString description = object.value(QStringLiteral("error_description")).toString().trimmed();
    if (!description.isEmpty()) {
        return description;
    }
    const QString error = object.value(QStringLiteral("error")).toString().trimmed();
    return error.isEmpty() ? fallback : error;
}

QByteArray safeReadAll(QIODevice *device)
{
    return device && device->isOpen() ? device->readAll() : QByteArray{};
}

bool refreshAccessTokenBlocking(const OAuthClientConfig &config,
                                const QString &refreshToken,
                                QString *accessToken,
                                QString *error)
{
    if (accessToken) {
        accessToken->clear();
    }

    QNetworkAccessManager network;
    QNetworkRequest request(config.tokenUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));

    QUrlQuery form;
    form.addQueryItem(QStringLiteral("client_id"), config.clientId);
    if (!config.clientSecret.isEmpty()) {
        form.addQueryItem(QStringLiteral("client_secret"), config.clientSecret);
    }
    form.addQueryItem(QStringLiteral("refresh_token"), refreshToken);
    form.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("refresh_token"));

    QNetworkReply *reply = network.post(request, form.query(QUrl::FullyEncoded).toUtf8());
    QEventLoop loop;
    QTimer timeout;
    bool timedOut = false;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timeout.start(30000);
    loop.exec();
    timeout.stop();

    const QByteArray body = safeReadAll(reply);
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorString = reply->errorString();
    delete reply;

    if (timedOut) {
        if (error) {
            *error = QStringLiteral("Google Drive authorization refresh timed out");
        }
        return false;
    }

    if (networkError != QNetworkReply::NoError) {
        const QJsonDocument errorDocument = QJsonDocument::fromJson(body);
        const QString errorCode = errorDocument.object().value(QStringLiteral("error")).toString().trimmed();
        const QString message = tokenEndpointErrorMessage(body, networkErrorString);
        if (errorCode == QLatin1String("invalid_grant")) {
            clearSavedAuthorization();
        }
        if (error) {
            *error = QStringLiteral("Google Drive authorization refresh failed: %1").arg(message);
        }
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) {
            *error = QStringLiteral("Google Drive authorization response is invalid");
        }
        return false;
    }

    const QJsonObject object = document.object();
    const QString token = object.value(QStringLiteral("access_token")).toString().trimmed();
    if (token.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Google Drive authorization response has no access token");
        }
        return false;
    }

    const int expiresIn = object.value(QStringLiteral("expires_in")).toInt(3600);
    rememberAccessToken(token, QDateTime::currentDateTimeUtc().addSecs((std::max)(60, expiresIn)));
    if (accessToken) {
        *accessToken = token;
    }
    return true;
}

QString accessTokenForBlockingRequest(QString *error)
{
    const QString cachedToken = validSessionAccessToken();
    if (!cachedToken.isEmpty()) {
        return cachedToken;
    }

    OAuthClientConfig config = loadOAuthClientConfig();
    if (!config.valid()) {
        if (error) {
            *error = config.error;
        }
        return {};
    }

    const QString refreshToken = sessionRefreshToken();
    if (refreshToken.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Google Drive authorization required. Open gdrive:// first.");
        }
        return {};
    }

    QString accessToken;
    if (!refreshAccessTokenBlocking(config, refreshToken, &accessToken, error)) {
        return {};
    }
    return accessToken;
}

QUrl driveDownloadUrl(const QString &path, const QString &mimeType)
{
    const QString id = idForItemPath(path);
    if (id.isEmpty()) {
        return {};
    }

    QUrl url;
    QUrlQuery query;
    if (isGoogleAppsMimeType(mimeType)) {
        url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1/export")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(id))));
        query.addQueryItem(QStringLiteral("mimeType"), QStringLiteral("application/pdf"));
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
                                  QString *error)
{
    const QUrl url = driveDownloadUrl(sourcePath, mimeType);
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
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = network.get(request);
    QEventLoop loop;
    QByteArray errorBody;
    QString writeError;
    bool canceled = false;
    bool writeFailed = false;

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

    QObject::connect(reply, &QIODevice::readyRead, &loop, consumeReadyRead);
    QObject::connect(reply, &QNetworkReply::downloadProgress, &loop, [&](qint64 received, qint64 total) {
        if (progress && !progress(received, total)) {
            canceled = true;
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();
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

    const GDriveStorageQuota quota = quotaFromAboutObject(document.object());
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
    QJsonObject metadata;
    metadata.insert(QStringLiteral("name"), name);
    metadata.insert(QStringLiteral("mimeType"), mimeType);
    metadata.insert(QStringLiteral("parents"), QJsonArray{parentId});

    QByteArray body;
    QNetworkReply *reply = network.post(authorizedJsonRequest(driveFileMetadataUrl(), accessToken),
                                        QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    if (!waitForReply(reply, 60000, QStringLiteral("Google Drive create request timed out"), &body, error)) {
        return false;
    }
    return parseDriveFileResponse(body, createdObject, error);
}

bool trashDriveFileBlocking(QNetworkAccessManager &network,
                            const QString &fileId,
                            const QString &accessToken,
                            QJsonObject *trashedObject,
                            QString *error)
{
    QJsonObject metadata;
    metadata.insert(QStringLiteral("trashed"), true);

    QByteArray body;
    QNetworkReply *reply = network.sendCustomRequest(
        authorizedJsonRequest(driveFileMetadataUrl(fileId), accessToken),
        QByteArrayLiteral("PATCH"),
        QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    if (!waitForReply(reply, 60000, QStringLiteral("Google Drive delete request timed out"), &body, error)) {
        return false;
    }
    return parseDriveFileResponse(body, trashedObject, error);
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

bool uploadLocalFileToDriveBlocking(QNetworkAccessManager &network,
                                    const QString &sourceFilePath,
                                    const QString &parentId,
                                    const QString &name,
                                    const QString &accessToken,
                                    const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                    QJsonObject *createdObject,
                                    QString *error)
{
    QFile file(sourceFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot read local file for Google Drive upload: %1").arg(file.errorString());
        }
        return false;
    }

    const QString mimeType = mimeTypeForLocalUpload(sourceFilePath);
    QJsonObject metadata;
    metadata.insert(QStringLiteral("name"), name);
    metadata.insert(QStringLiteral("parents"), QJsonArray{parentId});

    QNetworkRequest sessionRequest = authorizedJsonRequest(driveUploadSessionUrl(), accessToken);
    sessionRequest.setRawHeader("X-Upload-Content-Type", mimeType.toUtf8());
    sessionRequest.setRawHeader("X-Upload-Content-Length", QByteArray::number(file.size()));

    QByteArray sessionBody;
    QHash<QByteArray, QByteArray> sessionHeaders;
    QNetworkReply *sessionReply = network.post(sessionRequest, QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    if (!waitForReply(sessionReply,
                      60000,
                      QStringLiteral("Google Drive upload session timed out"),
                      &sessionBody,
                      error,
                      &sessionHeaders)) {
        return false;
    }

    const QByteArray sessionUrlBytes = sessionHeaders.value(QByteArrayLiteral("location")).trimmed();
    const QUrl sessionUrl(QString::fromUtf8(sessionUrlBytes));
    if (sessionUrlBytes.isEmpty() || !sessionUrl.isValid() || sessionUrl.isRelative()) {
        if (error) {
            *error = QStringLiteral("Google Drive upload session response has no upload URL");
        }
        return false;
    }

    QNetworkRequest uploadRequest(sessionUrl);
    uploadRequest.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    uploadRequest.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
    uploadRequest.setHeader(QNetworkRequest::ContentLengthHeader, file.size());
    uploadRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *uploadReply = network.put(uploadRequest, &file);
    QEventLoop uploadLoop;
    bool canceled = false;
    QObject::connect(uploadReply, &QNetworkReply::uploadProgress, &uploadLoop, [&](qint64 sent, qint64 total) {
        if (progress && !progress(sent, total)) {
            canceled = true;
            uploadReply->abort();
        }
    });
    QObject::connect(uploadReply, &QNetworkReply::finished, &uploadLoop, &QEventLoop::quit);
    uploadLoop.exec();

    const QByteArray uploadBody = safeReadAll(uploadReply);
    const QNetworkReply::NetworkError networkError = uploadReply->error();
    const QString networkErrorString = uploadReply->errorString();
    const int status = uploadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    delete uploadReply;

    if (canceled || networkError == QNetworkReply::OperationCanceledError) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }
    if (networkError != QNetworkReply::NoError || status >= 400) {
        if (error) {
            const QString fallback = status > 0
                ? QStringLiteral("Google Drive upload failed with HTTP %1").arg(status)
                : QStringLiteral("Google Drive upload failed: %1").arg(networkErrorString);
            *error = driveErrorMessage(uploadBody, fallback);
        }
        return false;
    }

    return parseDriveFileResponse(uploadBody, createdObject, error);
}

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
        urlQuery.addQueryItem(QStringLiteral("orderBy"), QStringLiteral("folder,name"));
        urlQuery.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
        urlQuery.addQueryItem(QStringLiteral("includeItemsFromAllDrives"), QStringLiteral("true"));
        if (!pageToken.isEmpty()) {
            urlQuery.addQueryItem(QStringLiteral("pageToken"), pageToken);
        }
        url.setQuery(urlQuery);

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());

        QNetworkReply *reply = network.get(request);
        QEventLoop loop;
        QTimer timeout;
        bool timedOut = false;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            reply->abort();
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timeout.start(60000);
        loop.exec();
        timeout.stop();

        const QByteArray body = safeReadAll(reply);
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorString = reply->errorString();
        delete reply;

        if (timedOut) {
            if (error) {
                *error = QStringLiteral("Google Drive folder listing timed out");
            }
            return {};
        }

        if (networkError != QNetworkReply::NoError) {
            if (error) {
                *error = driveErrorMessage(body,
                                           QStringLiteral("Google Drive folder listing failed: %1")
                                               .arg(networkErrorString));
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
            cacheSharedEntry(entry, path, mimeType, itemCapabilities);
            children.append(entry.path);
        }

        pageToken = root.value(QStringLiteral("nextPageToken")).toString();
    } while (!pageToken.isEmpty());

    cacheSharedChildren(path, children);
    if (error) {
        error->clear();
    }
    return children;
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
    bool canHandle(const QString &path) const override { return !normalizedGDrivePath(path).isEmpty(); }
    Capabilities capabilities() const override { return Browse | ReadMetadata | Create | Remove | Transfer; }

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

        if (normalized == GDriveRoot) {
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
        if (pendingPathInfo(normalized).valid()) {
            return false;
        }
        return normalized == GDriveRoot
            || normalized == GDriveMyDrive
            || normalized == GDriveSharedWithMe
            || m_entries.contains(normalized)
            || sharedEntry(normalized).has_value()
            || normalized.startsWith(GDriveItemPrefix);
    }

    bool isDirectory(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        if (normalized == GDriveRoot || normalized == GDriveMyDrive || normalized == GDriveSharedWithMe) {
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
    QString normalizedPath(const QString &path) const override { return normalizedGDrivePath(path); }

    QString fileName(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        const auto it = m_entries.constFind(normalized);
        if (it != m_entries.constEnd()) {
            return it->name;
        }
        const auto entry = sharedEntry(normalized);
        return entry ? entry->name : fallbackFileNameForPath(normalized);
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
        return parentGDrivePath(normalized);
    }

    QString childPath(const QString &parentPath, const QString &name) const override
    {
        const QString rootChild = childGDrivePath(parentPath, name);
        if (!rootChild.isEmpty()) {
            return rootChild;
        }

        const QString normalizedParent = resolveCreatedPath(normalizedPath(parentPath));
        const QString cleanName = name.trimmed();
        if (normalizedParent.isEmpty() || cleanName.isEmpty()
            || cleanName.contains(QLatin1Char('/')) || cleanName.contains(QLatin1Char('\\'))) {
            return {};
        }

        const QString parentId = driveParentIdForPath(normalizedParent);
        if (parentId.isEmpty()) {
            return {};
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
        return pendingPathForParentIdAndName(parentId, cleanName);
    }

    std::optional<FileEntry> entryInfo(const QString &path) const override
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        const auto it = m_entries.constFind(normalized);
        if (it != m_entries.constEnd()) {
            return it.value();
        }
        const auto cached = sharedEntry(normalized);
        if (cached) {
            return cached;
        }
        if (normalized == GDriveRoot) {
            GDriveItemCapabilities rootCapabilities;
            rootCapabilities.canListChildren = true;
            return virtualDirectoryEntry(QStringLiteral("Google Drive"), QString(GDriveRoot), rootCapabilities);
        }
        return std::nullopt;
    }

    bool ensureParentDirectory(const QString &path) const override
    {
        clearLastError();
        const QString parent = parentPath(path);
        if (parent.isEmpty() || parent == GDriveRoot || parent == GDriveSharedWithMe) {
            setLastError(QStringLiteral("Google Drive cannot create items in this location"));
            return false;
        }
        if (!canCreateInFolder(parent)) {
            setLastError(QStringLiteral("Google Drive does not allow creating items in this folder"));
            return false;
        }
        return true;
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
        const QString id = idForItemPath(normalized);
        if (id.isEmpty()) {
            setLastError(QStringLiteral("Google Drive item path is invalid"));
            return false;
        }

        const auto entry = entryInfo(normalized);
        const QString parent = parentPath(normalized);
        if (entry) {
            m_removedTargets.insert(normalized, {parent, driveParentIdForPath(parent), entry->name});
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
        refreshDriveStorageQuotaBlocking(m_network, accessToken, nullptr);
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

    bool copyToLocalFile(const QString &sourcePath,
                         const QString &destinationFilePath,
                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                         QString *error) const override
    {
        clearLastError();

        const QString normalized = normalizedPath(sourcePath);
        const std::optional<FileEntry> entry = entryInfo(normalized);
        if (!entry) {
            const QString message = QStringLiteral("Google Drive file metadata is not available. Open the folder first.");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }
        if (entry->isDirectory) {
            const QString message = QStringLiteral("Google Drive folder download is not implemented yet");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }

        const std::optional<GDriveItemCapabilities> capabilities = sharedCapabilities(normalized);
        if (capabilities && !capabilities->canDownload) {
            const QString message = QStringLiteral("Google Drive does not allow downloading this file.");
            setLastError(message);
            if (error) {
                *error = message;
            }
            return false;
        }

        QString mimeType = m_mimeTypes.value(normalized);
        if (mimeType.isEmpty()) {
            mimeType = sharedMimeType(normalized);
        }
        if (mimeType.isEmpty()) {
            const QString message = QStringLiteral("Google Drive file type is not available. Open the folder first.");
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

        QString downloadError;
        if (!downloadDriveFileToLocalFile(m_network, normalized, mimeType, destinationFilePath, accessToken, progress, &downloadError)) {
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
        if (!uploadLocalFileToDriveBlocking(m_network,
                                            sourceFilePath,
                                            target.parentId,
                                            target.name,
                                            accessToken,
                                            progress,
                                            &createdObject,
                                            &uploadError)) {
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
        refreshDriveStorageQuotaBlocking(m_network, accessToken, nullptr);
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

    QVariantMap storageInfo(const QString &) const override
    {
        const auto quota = sharedQuota();
        return quota ? storageInfoForQuota(*quota) : QVariantMap{};
    }

    QString lastErrorString() const override { return m_lastError; }
    void clearLastError() const override { m_lastError.clear(); }

private:
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

    bool canCreateInFolder(const QString &folderPath) const
    {
        const QString normalized = resolveCreatedPath(normalizedPath(folderPath));
        if (normalized == GDriveMyDrive) {
            return true;
        }
        if (normalized == GDriveRoot || normalized == GDriveSharedWithMe || normalized.isEmpty()) {
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
        const GDrivePendingPath pending = pendingPathInfo(normalized);
        if (pending.valid()) {
            const QString parentPath = parentPathForDriveParentId(pending.parentId);
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

    FileEntry cacheDriveFileObject(const QJsonObject &fileObject, const QString &parentPath) const
    {
        const FileEntry entry = entryFromDriveFileObject(fileObject);
        if (entry.path.isEmpty()) {
            return {};
        }

        const QString mimeType = fileObject.value(QStringLiteral("mimeType")).toString();
        const GDriveItemCapabilities itemCapabilities = driveCapabilitiesFromDriveFileObject(fileObject);
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
        cacheSharedChildren(parentPath, children);
        return entry;
    }

    void removeCachedPath(const QString &path) const
    {
        const QString normalized = resolveCreatedPath(normalizedPath(path));
        const QString parent = m_parents.value(normalized, sharedParent(normalized));
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
    }

    bool createDriveFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) const
    {
        clearLastError();
        if (createdPath) {
            createdPath->clear();
        }

        const QString normalizedParent = resolveCreatedPath(normalizedPath(parentPath));
        const QString cleanName = name.trimmed();
        const QString parentId = driveParentIdForPath(normalizedParent);
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
        refreshDriveStorageQuotaBlocking(m_network, accessToken, nullptr);
        return true;
    }

    void cacheRootEntries()
    {
        GDriveItemCapabilities myDriveCapabilities;
        myDriveCapabilities.canListChildren = true;
        myDriveCapabilities.canAddChildren = true;

        GDriveItemCapabilities sharedWithMeCapabilities;
        sharedWithMeCapabilities.canListChildren = true;

        QList<FileEntry> entries;
        entries.append(virtualDirectoryEntry(QStringLiteral("My Drive"), QString(GDriveMyDrive), myDriveCapabilities));
        entries.append(virtualDirectoryEntry(QStringLiteral("Shared with me"), QString(GDriveSharedWithMe), sharedWithMeCapabilities));

        QStringList paths;
        paths.reserve(entries.size());
        for (const FileEntry &entry : entries) {
            m_entries.insert(entry.path, entry);
            m_parents.insert(entry.path, QString(GDriveRoot));
            m_mimeTypes.insert(entry.path, QString(GoogleDriveFolderMime));
            const GDriveItemCapabilities capabilities = entry.path == GDriveMyDrive
                ? myDriveCapabilities
                : sharedWithMeCapabilities;
            m_itemCapabilities.insert(entry.path, capabilities);
            cacheSharedEntry(entry, QString(GDriveRoot), QString(GoogleDriveFolderMime), capabilities);
            paths.append(entry.path);
        }
        m_children.insert(QString(GDriveRoot), paths);
        cacheSharedChildren(QString(GDriveRoot), paths);
    }

    void emitRootEntries(int generation)
    {
        if (generation != currentGeneration()) {
            return;
        }
        QList<FileEntry> entries;
        const QStringList paths = m_children.value(QString(GDriveRoot));
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
        m_oauth->setRequestedScopeTokens({QByteArray(GoogleDriveScope.data(), GoogleDriveScope.size())});
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

        auto *replyHandler = new QOAuthHttpServerReplyHandler(QHostAddress::Any, 0, m_oauth.get());
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
        urlQuery.addQueryItem(QStringLiteral("orderBy"), QStringLiteral("folder,name"));
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
                m_entries.insert(entry.path, entry);
                m_parents.insert(entry.path, path);
                m_mimeTypes.insert(entry.path, mimeType);
                m_itemCapabilities.insert(entry.path, itemCapabilities);
                cacheSharedEntry(entry, path, mimeType, itemCapabilities);
                childPaths.append(entry.path);
                entries.append(entry);
            }
            m_children.insert(path, childPaths);
            cacheSharedChildren(path, childPaths);

            if (!entries.isEmpty()) {
                emit batchReady(entries, generation);
            }

            const QString nextPageToken = root.value(QStringLiteral("nextPageToken")).toString();
            if (!nextPageToken.isEmpty()) {
                requestFileList(generation, path, nextPageToken);
                return;
            }

            requestStorageQuotaAndFinish(generation);
        });
    }

    void requestStorageQuotaAndFinish(int generation)
    {
        const QString accessToken = validSessionAccessToken();
        if (accessToken.isEmpty() || generation != currentGeneration()) {
            finish(generation, true, {});
            return;
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
                    cacheSharedQuota(quotaFromAboutObject(document.object()));
                }
            }

            finish(generation, true, {});
        });
    }

    FileEntry entryFromDriveFile(const QJsonObject &object, const QString &parentPath) const
    {
        Q_UNUSED(parentPath)
        return entryFromDriveFileObject(object);
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

    QString m_currentPath = QString(GDriveRoot);
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
    return !normalizedGDrivePath(path).isEmpty();
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
    const QString targetPath = normalizedGDrivePath(context.targetPath);
    if (targetPath.isEmpty()) {
        return {};
    }

    FileActionDescriptor rawCapabilities;
    rawCapabilities.id = QString(GoogleDriveRawCapabilitiesAction);
    rawCapabilities.text = QStringLiteral("Raw capabilities");
    rawCapabilities.iconSource = QStringLiteral("../assets/icons/info.svg");
    rawCapabilities.order = 900;
    return {rawCapabilities};
}

QVariantMap GDriveFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    if (actionId == GoogleDriveSignOutAction) {
        const bool ok = clearSavedAuthorization();
        return {
            {QStringLiteral("ok"), ok},
            {QStringLiteral("title"), QStringLiteral("Google Drive")},
            {QStringLiteral("message"),
             ok
                 ? QStringLiteral("Google Drive authorization was removed.")
                 : QStringLiteral("Google Drive authorization could not be removed.")},
        };
    }

    if (actionId == GoogleDriveRawCapabilitiesAction) {
        const QString targetPath = normalizedGDrivePath(context.targetPath);
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
