#include "TelegramAuth.h"

#include "TelegramCache.h"
#include "TelegramClient.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1StringView>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

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

constexpr QLatin1StringView ApiCredentialsTarget{"FMQml/Telegram/ApiCredentials"};

struct TelegramAuthSession {
    TelegramProviderInternal::TelegramApiCredentials credentials;
    bool credentialsLoaded = false;
};

QMutex &authSessionMutex()
{
    static QMutex mutex;
    return mutex;
}

TelegramAuthSession &authSession()
{
    static TelegramAuthSession session;
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
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
const SecretSchema *telegramCredentialSchema()
{
    static const SecretSchema schema = {
        "org.fmqml.Telegram",
        SECRET_SCHEMA_NONE,
        {
            {"target", SECRET_SCHEMA_ATTRIBUTE_STRING},
            {nullptr, SECRET_SCHEMA_ATTRIBUTE_STRING},
        },
    };
    return &schema;
}
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

QString readCredentialText(QLatin1StringView targetName)
{
    const QByteArray target = QString(targetName).toUtf8();
    GError *error = nullptr;
    gchar *password = secret_password_lookup_sync(telegramCredentialSchema(),
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
    const QByteArray label = QStringLiteral("FMQml Telegram %1").arg(targetText).toUtf8();
    GError *error = nullptr;
    const gboolean stored = secret_password_store_sync(telegramCredentialSchema(),
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
    secret_password_clear_sync(telegramCredentialSchema(),
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

TelegramProviderInternal::TelegramApiCredentials credentialsFromJson(const QString &json)
{
    TelegramProviderInternal::TelegramApiCredentials credentials;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject()) {
        return credentials;
    }

    const QJsonObject object = document.object();
    credentials.apiId = object.value(QStringLiteral("apiId")).toInt();
    credentials.apiHash = object.value(QStringLiteral("apiHash")).toString().trimmed();
    credentials.accountLabel = object.value(QStringLiteral("accountLabel")).toString().trimmed();
    return credentials;
}

QString credentialsToJson(const TelegramProviderInternal::TelegramApiCredentials &credentials)
{
    QJsonObject object;
    object.insert(QStringLiteral("apiId"), credentials.apiId);
    object.insert(QStringLiteral("apiHash"), credentials.apiHash.trimmed());
    if (!credentials.accountLabel.trimmed().isEmpty()) {
        object.insert(QStringLiteral("accountLabel"), credentials.accountLabel.trimmed());
    }
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

QString telegramDataRoot()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return base.isEmpty() ? QString{} : QDir(base).filePath(QStringLiteral("telegram"));
}

} // namespace

namespace TelegramProviderInternal {

TelegramAuthState currentAuthState()
{
    QString error;
    TelegramClient &client = sharedTelegramClient();

    TelegramAuthState state;
    const TelegramApiCredentials savedCredentials = savedApiCredentials();
    state.hasApiCredentials = savedCredentials.apiId > 0 && !savedCredentials.apiHash.trimmed().isEmpty();
    state.accountLabel = savedCredentials.accountLabel;
    if (client.state() == TelegramClient::State::NotStarted
        || client.state() == TelegramClient::State::Closed) {
        state.signedIn = state.hasApiCredentials;
        state.statusLabel = state.hasApiCredentials
            ? QStringLiteral("Telegram client is idle. Open a Telegram source to resume the saved session.")
            : QStringLiteral("Enter Telegram API ID, API hash, and phone number to continue authorization.");
        return state;
    }

    client.configureFromEnvironment(&error);
    client.poll(100, &error);

    state.signedIn = client.state() == TelegramClient::State::Ready;
    state.statusLabel = error.isEmpty() ? client.sanitizedStatus() : error;
    return state;
}

TelegramApiCredentials savedApiCredentials()
{
    QMutexLocker locker(&authSessionMutex());
    TelegramAuthSession &session = authSession();
    if (!session.credentialsLoaded) {
        session.credentials = credentialsFromJson(readCredentialText(ApiCredentialsTarget));
        session.credentialsLoaded = true;
    }
    return session.credentials;
}

bool hasSavedApiCredentials()
{
    const TelegramApiCredentials credentials = savedApiCredentials();
    return credentials.apiId > 0 && !credentials.apiHash.trimmed().isEmpty();
}

bool rememberApiCredentials(int apiId, const QString &apiHash, const QString &accountLabel)
{
    TelegramApiCredentials credentials;
    credentials.apiId = apiId;
    credentials.apiHash = apiHash.trimmed();
    credentials.accountLabel = accountLabel.trimmed();
    if (credentials.apiId <= 0 || credentials.apiHash.isEmpty()) {
        return false;
    }
    if (!writeCredentialText(ApiCredentialsTarget, credentialsToJson(credentials))) {
        return false;
    }

    QMutexLocker locker(&authSessionMutex());
    TelegramAuthSession &session = authSession();
    session.credentials = credentials;
    session.credentialsLoaded = true;
    return true;
}

bool clearSavedApiCredentials()
{
    const bool deleted = deleteCredentialText(ApiCredentialsTarget);
    QMutexLocker locker(&authSessionMutex());
    TelegramAuthSession &session = authSession();
    session.credentials = {};
    session.credentialsLoaded = true;
    return deleted;
}

bool forgetLocalTelegramData(QString *error)
{
    if (error) {
        error->clear();
    }

    QString ignoredLogoutError;
    TelegramClient &client = sharedTelegramClient();
    if (client.state() != TelegramClient::State::NotStarted
        && client.state() != TelegramClient::State::Closed) {
        client.logOut(&ignoredLogoutError);
    }
    client.close();
    clearCache();

    bool ok = clearSavedApiCredentials();
    const QString dataRoot = telegramDataRoot();
    if (dataRoot.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Telegram local data directory is unavailable.");
        }
        return false;
    }

    const QFileInfo info(dataRoot);
    if (info.exists()) {
        ok = QDir(dataRoot).removeRecursively() && ok;
    }
    if (!ok && error) {
        *error = QStringLiteral("Telegram local data could not be fully removed.");
    }
    return ok;
}

} // namespace TelegramProviderInternal
