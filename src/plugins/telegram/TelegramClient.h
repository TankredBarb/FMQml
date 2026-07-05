#pragma once

#include <functional>
#include <memory>

#include <QList>
#include <QRecursiveMutex>
#include <QString>

#include <td/telegram/td_api.h>

#include "TelegramTypes.h"

namespace td {
class ClientManager;
}

namespace TelegramProviderInternal {

class TelegramClient final
{
public:
    enum class State {
        NotStarted,
        Starting,
        WaitTdlibParameters,
        WaitPhoneNumber,
        WaitCode,
        WaitPassword,
        Ready,
        Closing,
        Closed
    };

    TelegramClient() = default;
    ~TelegramClient();

    bool start(QString *error);
    void close();
    State state() const;
    QString sanitizedStatus() const;
    bool poll(int timeoutMs, QString *error);
    bool configureFromEnvironment(QString *error);
    bool configureWithCredentials(int apiId, const QString &apiHash, QString *error);
    bool setPhoneNumber(const QString &phoneNumber, int apiId, const QString &apiHash, QString *error);
    bool checkCode(const QString &code, QString *error);
    bool checkPassword(const QString &password, QString *error);
    bool logOut(QString *error);
    QString currentUserAccountLabel(QString *error);
    QList<TelegramEntry> chats(QString *error);
    qint64 publicChatId(const QString &username, QString *error);
    qint64 savedMessagesChatId(QString *error);
    TelegramSavedMessagesPage savedMessageFiles(qint64 fromMessageId, QString *error);
    TelegramFilesPage chatMessageFiles(qint64 chatId, const QString &parentPath, qint64 fromMessageId, QString *error);
    QString downloadFile(int fileId, QString *error);
    QString downloadFile(int fileId,
                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                         QString *error,
                         int timeoutMs = 600000);
    bool sendFile(qint64 chatId,
                  const QString &localFilePath,
                  const QString &mimeType,
                  const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                  QString *error);
    bool sendFileAlbum(qint64 chatId,
                       const QList<TelegramUploadFile> &files,
                       const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                       QString *error);

private:
    class ActivityScope;

    bool ensureStarted(QString *error);
    bool sendTdlibParameters(int apiId, const QString &apiHash, QString *error);
    bool rememberPendingApiCredentialsIfReady(QString *error);
    td::td_api::object_ptr<td::td_api::Object> sendBlocking(td::td_api::object_ptr<td::td_api::Function> &&request,
                                                             const QString &label,
                                                             QString *error,
                                                             int timeoutMs = 10000);
    void sendGetAuthorizationState();
    void handleObject(int objectId);
    void setState(State state);
    void pollBriefly(QString *error);
    bool pollUntilStateChanges(State previousState, int timeoutMs, QString *error);
    bool waitForMessageSendResults(const QList<qint64> &pendingMessageIds,
                                   qint64 totalBytes,
                                   const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                   QString *error);
    void beginActivity();
    void endActivity();
    void scheduleIdleCloseLocked();
    bool canIdleCloseLocked() const;

    std::unique_ptr<td::ClientManager> m_manager;
    mutable QRecursiveMutex m_clientMutex;
    int m_clientId = 0;
    quint64 m_nextRequestId = 1;
    State m_state = State::NotStarted;
    QString m_lastStatus = QStringLiteral("TDLib client not started");
    int m_pendingApiId = 0;
    QString m_pendingApiHash;
    quint64 m_idleGeneration = 0;
    int m_activeOperations = 0;
};

TelegramClient &sharedTelegramClient();

} // namespace TelegramProviderInternal
