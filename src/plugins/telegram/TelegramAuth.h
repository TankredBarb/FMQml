#pragma once

#include <QString>

namespace TelegramProviderInternal {

class TelegramAuthState final
{
public:
    bool signedIn = false;
    bool hasApiCredentials = false;
    QString accountLabel;
    QString statusLabel = QStringLiteral("Not signed in");
};

struct TelegramApiCredentials {
    int apiId = 0;
    QString apiHash;
    QString accountLabel;
};

TelegramAuthState currentAuthState();
TelegramApiCredentials savedApiCredentials();
bool hasSavedApiCredentials();
bool rememberApiCredentials(int apiId, const QString &apiHash, const QString &accountLabel = {});
bool clearSavedApiCredentials();
bool forgetLocalTelegramData(QString *error = nullptr);

} // namespace TelegramProviderInternal
