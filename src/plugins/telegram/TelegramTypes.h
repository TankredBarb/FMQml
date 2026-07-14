#pragma once

#include <QDateTime>
#include <QByteArray>
#include <QList>
#include <QString>

namespace TelegramProviderInternal {

inline constexpr const char *TelegramLoadMoreItemName = "__load_more__";

enum class TelegramPathKind {
    Invalid,
    Root,
    Saved,
    Chats,
    Chat,
    Channel,
    Downloads,
    Status
};

struct ParsedTelegramPath {
    bool valid = false;
    TelegramPathKind kind = TelegramPathKind::Invalid;
    QString normalized;
    QString id;
    QString itemName;
    bool loadMore = false;
};

inline constexpr const char *TelegramStatusPath = "telegram://status/telegram-provider-status.txt";

struct TelegramEntry {
    QString name;
    QString path;
    QString mimeType;
    QString iconName;
    QString providerLabel;
    QString localPath;
    QString openUrl;
    QString thumbnailLocalPath;
    QByteArray virtualContent;
    QByteArray thumbnailData;
    qint64 chatId = 0;
    qint64 messageId = 0;
    int fileId = 0;
    int thumbnailFileId = 0;
    qint64 size = 0;
    QDateTime date;
    bool directory = false;
    bool downloaded = false;
    bool hasThumbnail = false;
    bool loadMore = false;
};

struct TelegramUploadFile {
    QString localFilePath;
    QString mimeType;
    qint64 size = 0;
};

struct TelegramSavedMessagesPage {
    QList<TelegramEntry> entries;
    qint64 nextFromMessageId = 0;
    bool hasMore = false;
};

using TelegramFilesPage = TelegramSavedMessagesPage;

} // namespace TelegramProviderInternal
