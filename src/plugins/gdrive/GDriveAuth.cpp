#include "GDriveAuth.h"

#include <algorithm>

#include <QEventLoop>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrlQuery>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincred.h>
#elif defined(HAS_LIBSECRET)
#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")
#endif

namespace {

constexpr QLatin1StringView RefreshTokenCredentialTarget{"FMQml/GoogleDrive/OAuthRefreshToken"};
constexpr QLatin1StringView AccountInfoCredentialTarget{"FMQml/GoogleDrive/AccountInfo"};

struct GDriveAuthSession {
    QString accessToken;
    QDateTime accessTokenExpiresAt;
    QString refreshToken;
    bool refreshTokenLoaded = false;
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

#ifdef Q_OS_WIN
QString readCredentialText(QLatin1StringView targetName)
{
    PCREDENTIALW credential = nullptr;
    const std::wstring target = QString(targetName).toStdWString();
    if (!CredReadW(target.c_str(), CRED_TYPE_GENERIC, 0, &credential) || !credential) {
        return {};
    }

    const QByteArray bytes(reinterpret_cast<const char *>(credential->CredentialBlob),
                           static_cast<int>(credential->CredentialBlobSize));
    const QString text = QString::fromUtf8(bytes);
    CredFree(credential);
    return text;
}

bool writeCredentialText(QLatin1StringView targetName, const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    if (bytes.isEmpty()) {
        return false;
    }

    const std::wstring target = QString(targetName).toStdWString();
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

bool deleteCredentialText(QLatin1StringView targetName)
{
    const std::wstring target = QString(targetName).toStdWString();
    if (CredDeleteW(target.c_str(), CRED_TYPE_GENERIC, 0)) {
        return true;
    }
    const DWORD error = GetLastError();
    return error == ERROR_NOT_FOUND || error == ERROR_NO_SUCH_LOGON_SESSION;
}
#elif defined(HAS_LIBSECRET)
const SecretSchema *gdriveCredentialSchema()
{
    static const SecretSchema schema = {
        "org.fmqml.GoogleDrive",
        SECRET_SCHEMA_NONE,
        {
            {"target", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
        },
    };
    return &schema;
}

QString readCredentialText(QLatin1StringView targetName)
{
    const QByteArray target = QString(targetName).toUtf8();
    GError *error = nullptr;
    gchar *password = secret_password_lookup_sync(gdriveCredentialSchema(),
                                                  nullptr,
                                                  &error,
                                                  "target",
                                                  target.constData(),
                                                  nullptr);
    if (error) {
        g_error_free(error);
        return {};
    }
    if (!password) {
        return {};
    }

    const QString text = QString::fromUtf8(password);
    secret_password_free(password);
    return text;
}

bool writeCredentialText(QLatin1StringView targetName, const QString &text)
{
    const QByteArray bytes = text.toUtf8();
    if (bytes.isEmpty()) {
        return false;
    }

    const QString targetText = QString(targetName);
    const QByteArray target = targetText.toUtf8();
    const QByteArray label = QStringLiteral("FMQml Google Drive %1").arg(targetText).toUtf8();
    GError *error = nullptr;
    const gboolean stored = secret_password_store_sync(gdriveCredentialSchema(),
                                                       SECRET_COLLECTION_DEFAULT,
                                                       label.constData(),
                                                       bytes.constData(),
                                                       nullptr,
                                                       &error,
                                                       "target",
                                                       target.constData(),
                                                       nullptr);
    if (error) {
        g_error_free(error);
        return false;
    }
    return stored;
}

bool deleteCredentialText(QLatin1StringView targetName)
{
    const QByteArray target = QString(targetName).toUtf8();
    GError *error = nullptr;
    secret_password_clear_sync(gdriveCredentialSchema(),
                               nullptr,
                               &error,
                               "target",
                               target.constData(),
                               nullptr);
    if (error) {
        g_error_free(error);
        return false;
    }
    return true;
}
#else
QString readCredentialText(QLatin1StringView)
{
    return {};
}

bool writeCredentialText(QLatin1StringView, const QString &)
{
    return false;
}

bool deleteCredentialText(QLatin1StringView)
{
    return true;
}
#endif

QString readCredentialRefreshToken()
{
    return readCredentialText(RefreshTokenCredentialTarget);
}

bool writeCredentialRefreshToken(const QString &refreshToken)
{
    return writeCredentialText(RefreshTokenCredentialTarget, refreshToken);
}

bool deleteCredentialRefreshToken()
{
    return deleteCredentialText(RefreshTokenCredentialTarget);
}

GDriveAuth::AccountInfo accountInfoFromJson(const QString &json)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return {};
    }

    const QJsonObject object = document.object();
    return {
        object.value(QStringLiteral("displayName")).toString().trimmed(),
        object.value(QStringLiteral("email")).toString().trimmed(),
    };
}

QString accountInfoToJson(const GDriveAuth::AccountInfo &accountInfo)
{
    QJsonObject object;
    object.insert(QStringLiteral("displayName"), accountInfo.displayName.trimmed());
    object.insert(QStringLiteral("email"), accountInfo.email.trimmed());
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QByteArray readAllFromDevice(QIODevice *device)
{
    return device && device->isOpen() ? device->readAll() : QByteArray{};
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

bool refreshAccessTokenBlocking(const GDriveAuth::OAuthClientConfig &config,
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

    const QByteArray body = readAllFromDevice(reply);
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
            GDriveAuth::clearSavedAuthorization();
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
    GDriveAuth::rememberAccessToken(token, QDateTime::currentDateTimeUtc().addSecs((std::max)(60, expiresIn)));
    if (accessToken) {
        *accessToken = token;
    }
    return true;
}

} // namespace

namespace GDriveAuth {

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

bool hasSavedAuthorization()
{
    return !sessionRefreshToken().trimmed().isEmpty();
}

AccountInfo savedAccountInfo()
{
    const QString json = readCredentialText(AccountInfoCredentialTarget);
    return accountInfoFromJson(json);
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

bool rememberAccountInfo(const AccountInfo &accountInfo)
{
    if (!accountInfo.valid()) {
        return true;
    }
    return writeCredentialText(AccountInfoCredentialTarget, accountInfoToJson(accountInfo));
}

bool clearSavedAuthorization()
{
    const bool deleted = deleteCredentialRefreshToken();
    const bool accountInfoDeleted = deleteCredentialText(AccountInfoCredentialTarget);
    QMutexLocker locker(&authSessionMutex());
    GDriveAuthSession &session = authSession();
    session.accessToken.clear();
    session.accessTokenExpiresAt = {};
    session.refreshToken.clear();
    session.refreshTokenLoaded = true;
    return deleted && accountInfoDeleted;
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

} // namespace GDriveAuth
