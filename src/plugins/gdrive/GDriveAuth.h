#pragma once

#include <QDateTime>
#include <QLatin1StringView>
#include <QString>
#include <QUrl>

namespace GDriveAuth {

constexpr QLatin1StringView GoogleDriveScope{"https://www.googleapis.com/auth/drive"};

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

struct AccountInfo {
    QString displayName;
    QString email;

    bool valid() const
    {
        return !displayName.trimmed().isEmpty() || !email.trimmed().isEmpty();
    }

    QString label() const
    {
        const QString cleanName = displayName.trimmed();
        const QString cleanEmail = email.trimmed();
        if (cleanName.isEmpty()) {
            return cleanEmail;
        }
        if (cleanEmail.isEmpty() || cleanName.compare(cleanEmail, Qt::CaseInsensitive) == 0) {
            return cleanName;
        }
        return cleanName + QLatin1Char('\n') + cleanEmail;
    }
};

OAuthClientConfig loadOAuthClientConfig();
QString validSessionAccessToken();
QString sessionRefreshToken();
bool hasSavedAuthorization();
AccountInfo savedAccountInfo();
void rememberAccessToken(const QString &accessToken, QDateTime expiresAt);
bool rememberRefreshToken(const QString &refreshToken);
bool rememberAccountInfo(const AccountInfo &accountInfo);
bool clearSavedAuthorization();
QString accessTokenForBlockingRequest(QString *error);

} // namespace GDriveAuth
