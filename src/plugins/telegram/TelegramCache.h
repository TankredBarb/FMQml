#pragma once

#include <optional>

#include <QHash>
#include <QList>
#include <QMutex>
#include <QString>
#include <QStringList>

#include "TelegramTypes.h"

namespace TelegramProviderInternal {

QMutex &cacheMutex();
void clearCache();
void storeEntry(const TelegramEntry &entry);
std::optional<TelegramEntry> cachedEntry(const QString &path);
void storeChildren(const QString &parentPath, const QList<TelegramEntry> &entries);
void appendChildren(const QString &parentPath, const QList<TelegramEntry> &entries);
QStringList cachedChildren(const QString &parentPath);
void storePagination(const QString &parentPath, qint64 nextFromMessageId, bool hasMore);
std::optional<TelegramFilesPage> pagination(const QString &parentPath);
void storeSavedPagination(const QString &parentPath, qint64 nextFromMessageId, bool hasMore);
std::optional<TelegramSavedMessagesPage> savedPagination(const QString &parentPath);

} // namespace TelegramProviderInternal
