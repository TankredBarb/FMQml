#include "MegaAuth.h"

#include <QByteArray>
#include <QLatin1StringView>
#include <QMutex>
#include <QMutexLocker>

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

constexpr QLatin1StringView SessionCredentialTarget{"FMQml/MEGA/Session"};
constexpr QLatin1StringView EmailCredentialTarget{"FMQml/MEGA/AccountEmail"};

struct MegaAuthSession {
    QString session;
    QString email;
    bool sessionLoaded = false;
    bool emailLoaded = false;
};

QMutex &authSessionMutex()
{
    static QMutex mutex;
    return mutex;
}

MegaAuthSession &authSession()
{
    static MegaAuthSession session;
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
const SecretSchema *megaCredentialSchema()
{
    static const SecretSchema schema = {
        "org.fmqml.MEGA",
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
    gchar *password = secret_password_lookup_sync(megaCredentialSchema(),
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
    const QByteArray label = QStringLiteral("FMQml MEGA %1").arg(targetText).toUtf8();
    GError *error = nullptr;
    const gboolean stored = secret_password_store_sync(megaCredentialSchema(),
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
    secret_password_clear_sync(megaCredentialSchema(),
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

} // namespace

namespace MegaAuth {

QString savedSession()
{
    QMutexLocker locker(&authSessionMutex());
    MegaAuthSession &session = authSession();
    if (!session.sessionLoaded) {
        session.session = readCredentialText(SessionCredentialTarget);
        session.sessionLoaded = true;
    }
    return session.session;
}

QString savedEmail()
{
    QMutexLocker locker(&authSessionMutex());
    MegaAuthSession &session = authSession();
    if (!session.emailLoaded) {
        session.email = readCredentialText(EmailCredentialTarget);
        session.emailLoaded = true;
    }
    return session.email;
}

bool hasSavedAuthorization()
{
    return !savedSession().trimmed().isEmpty();
}

bool rememberAuthorization(const QString &sessionToken, const QString &email)
{
    const QString cleanSession = sessionToken.trimmed();
    if (cleanSession.isEmpty()) {
        return false;
    }
    if (!writeCredentialText(SessionCredentialTarget, cleanSession)) {
        return false;
    }
    const QString cleanEmail = email.trimmed();
    const bool emailStored = cleanEmail.isEmpty() || writeCredentialText(EmailCredentialTarget, cleanEmail);

    QMutexLocker locker(&authSessionMutex());
    MegaAuthSession &session = authSession();
    session.session = cleanSession;
    if (!cleanEmail.isEmpty()) {
        session.email = cleanEmail;
        session.emailLoaded = true;
    }
    session.sessionLoaded = true;
    return emailStored;
}

bool clearSavedAuthorization()
{
    const bool sessionDeleted = deleteCredentialText(SessionCredentialTarget);
    const bool emailDeleted = deleteCredentialText(EmailCredentialTarget);
    QMutexLocker locker(&authSessionMutex());
    MegaAuthSession &session = authSession();
    session.session.clear();
    session.email.clear();
    session.sessionLoaded = true;
    session.emailLoaded = true;
    return sessionDeleted && emailDeleted;
}

} // namespace MegaAuth
