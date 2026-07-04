#include "InstagramAuth.h"

#include <QFile>
#include <QIODevice>
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

constexpr QLatin1StringView SessionCookieCredentialTarget{"FMQml/Instagram/SessionCookieHeader"};

struct InstagramAuthSession {
    QString cookieHeader;
    bool cookieHeaderLoaded = false;
};

QMutex &authSessionMutex()
{
    static QMutex mutex;
    return mutex;
}

InstagramAuthSession &authSession()
{
    static InstagramAuthSession session;
    return session;
}

QString normalizeCookieHeader(QString cookieHeader)
{
    cookieHeader.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    const QStringList lines = cookieHeader.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QStringList parts;
    for (QString line : lines) {
        line = line.trimmed();
        if (line.startsWith(QStringLiteral("Cookie:"), Qt::CaseInsensitive)) {
            line = line.mid(7).trimmed();
        }
        const QStringList lineParts = line.split(QLatin1Char(';'), Qt::SkipEmptyParts);
        for (QString part : lineParts) {
            part = part.trimmed();
            if (!part.isEmpty()) {
                parts.append(part);
            }
        }
    }
    return parts.join(QStringLiteral("; "));
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
const SecretSchema *instagramCredentialSchema()
{
    static const SecretSchema schema = {
        "org.fmqml.Instagram",
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
    gchar *password = secret_password_lookup_sync(instagramCredentialSchema(),
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
    const QByteArray label = QStringLiteral("FMQml Instagram %1").arg(targetText).toUtf8();
    GError *error = nullptr;
    const gboolean stored = secret_password_store_sync(instagramCredentialSchema(),
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
    secret_password_clear_sync(instagramCredentialSchema(),
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

namespace InstagramAuth {

QByteArray cookieValue(const QByteArray &cookie, const QByteArray &name)
{
    const QList<QByteArray> parts = cookie.split(';');
    const QByteArray prefix = name + '=';
    for (QByteArray part : parts) {
        part = part.trimmed();
        if (part.startsWith(prefix)) {
            return part.mid(prefix.size()).trimmed();
        }
    }
    return {};
}

bool hasRequiredSessionMarkers(const QByteArray &cookie)
{
    return !cookieValue(cookie, "sessionid").isEmpty()
        && !cookieValue(cookie, "ds_user_id").isEmpty()
        && !cookieValue(cookie, "csrftoken").isEmpty();
}

QString savedSessionCookieHeader()
{
    QMutexLocker locker(&authSessionMutex());
    InstagramAuthSession &session = authSession();
    if (!session.cookieHeaderLoaded) {
        session.cookieHeader = readCredentialText(SessionCookieCredentialTarget);
        session.cookieHeaderLoaded = true;
    }
    return session.cookieHeader;
}

QByteArray sessionCookieHeader()
{
    QByteArray cookie = qgetenv("FM_INSTAGRAM_COOKIE").trimmed();
    if (!cookie.isEmpty()) {
        return cookie;
    }

    const QByteArray cookieFilePath = qgetenv("FM_INSTAGRAM_COOKIE_FILE").trimmed();
    if (!cookieFilePath.isEmpty()) {
        QFile file(QString::fromLocal8Bit(cookieFilePath));
        if (file.open(QIODevice::ReadOnly)) {
            return normalizeCookieHeader(QString::fromUtf8(file.readAll())).toUtf8();
        }
    }

    return savedSessionCookieHeader().trimmed().toUtf8();
}

bool hasSavedSession()
{
    return !savedSessionCookieHeader().trimmed().isEmpty();
}

bool rememberSessionCookieHeader(const QString &cookieHeader)
{
    const QString cleanCookie = normalizeCookieHeader(cookieHeader);
    if (cleanCookie.isEmpty() || !hasRequiredSessionMarkers(cleanCookie.toUtf8())) {
        return false;
    }
    if (!writeCredentialText(SessionCookieCredentialTarget, cleanCookie)) {
        return false;
    }

    QMutexLocker locker(&authSessionMutex());
    InstagramAuthSession &session = authSession();
    session.cookieHeader = cleanCookie;
    session.cookieHeaderLoaded = true;
    return true;
}

bool clearSavedSession()
{
    const bool deleted = deleteCredentialText(SessionCookieCredentialTarget);
    QMutexLocker locker(&authSessionMutex());
    InstagramAuthSession &session = authSession();
    session.cookieHeader.clear();
    session.cookieHeaderLoaded = true;
    return deleted;
}

} // namespace InstagramAuth
