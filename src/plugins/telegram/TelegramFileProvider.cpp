#include "TelegramFileProvider.h"

#include "TelegramCache.h"
#include "TelegramClient.h"
#include "TelegramPath.h"
#include "TelegramPresentation.h"

#include <algorithm>
#include <atomic>
#include <memory>
#include <optional>

#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QIODevice>
#include <QList>
#include <QMimeDatabase>
#include <QMimeType>
#include <QSaveFile>
#include <QStandardPaths>

namespace TelegramProviderInternal {

namespace {

QList<TelegramEntry> rootEntries()
{
    TelegramEntry saved = rootEntry(QStringLiteral("Saved Messages"), QStringLiteral("telegram://saved"), QStringLiteral("Read-only"));
    saved.iconName = QStringLiteral("telegram-saved");

    TelegramEntry chats = rootEntry(QStringLiteral("Chats and Channels"), QStringLiteral("telegram://chats"), QStringLiteral("Read-only"));
    chats.iconName = QStringLiteral("telegram-chats");

    TelegramEntry downloads = rootEntry(QStringLiteral("Downloads"), QStringLiteral("telegram://downloads"), QStringLiteral("Local TDLib files"));
    downloads.iconName = QStringLiteral("telegram-downloads");

    return { saved, chats, downloads };
}

TelegramEntry statusEntry()
{
    TelegramEntry entry;
    entry.name = QStringLiteral("telegram-provider-status.txt");
    entry.path = QString::fromLatin1(TelegramStatusPath);
    entry.providerLabel = QStringLiteral("Diagnostics");
    entry.mimeType = QStringLiteral("text/plain");
    entry.iconName = QStringLiteral("text");
    entry.size = 128;
    entry.directory = false;
    return entry;
}

QString savedRootPath()
{
    return QStringLiteral("telegram://saved");
}

QString chatRootPath(const QString &chatId)
{
    return QStringLiteral("telegram://chat/%1").arg(chatId);
}

QString containerPathForParsed(const ParsedTelegramPath &parsed)
{
    if (parsed.kind == TelegramPathKind::Saved) {
        return savedRootPath();
    }
    if (parsed.kind == TelegramPathKind::Chat) {
        return chatRootPath(parsed.id);
    }
    if (parsed.kind == TelegramPathKind::Channel) {
        return QStringLiteral("telegram://channel/%1").arg(parsed.id);
    }
    return parsed.normalized;
}

bool isUploadContainer(const ParsedTelegramPath &parsed)
{
    return parsed.valid
        && parsed.itemName.isEmpty()
        && !parsed.loadMore
        && (parsed.kind == TelegramPathKind::Saved
            || parsed.kind == TelegramPathKind::Chat
            || parsed.kind == TelegramPathKind::Channel);
}

int previewDownloadTimeoutMs()
{
    bool ok = false;
    const int seconds = qEnvironmentVariableIntValue("FM_TELEGRAM_PREVIEW_DOWNLOAD_TIMEOUT_SECONDS", &ok);
    if (ok) {
        return seconds > 0 ? seconds * 1000 : 1000;
    }
    return 15000;
}

ParsedTelegramPath uploadContainerForPath(const QString &path)
{
    ParsedTelegramPath parsed = parseTelegramPath(path);
    if (isUploadContainer(parsed)) {
        return parsed;
    }
    const QString parent = parentTelegramPath(path);
    return parent.isEmpty() ? ParsedTelegramPath{} : parseTelegramPath(parent);
}

bool sameUploadContainer(const ParsedTelegramPath &lhs, const ParsedTelegramPath &rhs)
{
    return isUploadContainer(lhs)
        && isUploadContainer(rhs)
        && lhs.kind == rhs.kind
        && lhs.id == rhs.id
        && lhs.normalized == rhs.normalized;
}

constexpr qint64 TelegramProviderPhotoUploadLimit = 10 * 1024 * 1024;

bool isAlbumCompatibleMime(QString mimeType, qint64 size)
{
    mimeType = mimeType.trimmed().toLower();
    return (mimeType.startsWith(QStringLiteral("image/")) && (size <= 0 || size <= TelegramProviderPhotoUploadLimit))
        || mimeType.startsWith(QStringLiteral("video/"));
}

QString loadMorePathForParent(const QString &parentPath)
{
    return childTelegramPath(parentPath, QString::fromLatin1(TelegramLoadMoreItemName));
}

QString telegramDownloadsRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/telegram/tdlib-files");
}

QString localDownloadPathForRelativePath(const QString &relativePath)
{
    const QDir root(telegramDownloadsRoot());
    const QString absoluteRoot = root.absolutePath();
    const QString absolutePath = relativePath.isEmpty()
        ? absoluteRoot
        : QDir::cleanPath(root.filePath(relativePath));
    if (absolutePath != absoluteRoot && !absolutePath.startsWith(absoluteRoot + QLatin1Char('/'))) {
        return {};
    }
    return absolutePath;
}

QList<TelegramEntry> localDownloadEntries(const QString &parentPath, const QString &relativePath, QString *error)
{
    const QString localPath = localDownloadPathForRelativePath(relativePath);
    QFileInfo parentInfo(localPath);
    if (localPath.isEmpty() || (!relativePath.isEmpty() && !parentInfo.exists())) {
        if (error) {
            *error = QStringLiteral("Telegram downloads folder is unavailable.");
        }
        return {};
    }
    if (!parentInfo.exists()) {
        if (error) {
            error->clear();
        }
        return {};
    }
    if (!parentInfo.isDir()) {
        if (error) {
            *error = QStringLiteral("Telegram downloads path is not a folder.");
        }
        return {};
    }

    QMimeDatabase mimeDatabase;
    QDir dir(parentInfo.absoluteFilePath());
    const QFileInfoList infos = dir.entryInfoList(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot,
                                                  QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);
    QList<TelegramEntry> entries;
    entries.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        TelegramEntry entry;
        entry.name = info.fileName();
        entry.path = childTelegramPath(parentPath, info.fileName());
        entry.localPath = info.absoluteFilePath();
        entry.directory = info.isDir();
        entry.size = info.isDir() ? 0 : info.size();
        entry.date = info.lastModified();
        entry.providerLabel = QStringLiteral("Local TDLib file");
        entry.iconName = info.isDir() ? QStringLiteral("folder") : QStringLiteral("text");
        entry.mimeType = info.isDir() ? QString{} : mimeDatabase.mimeTypeForFile(info).name();
        entry.downloaded = !info.isDir();
        entries.append(entry);
    }
    if (error) {
        error->clear();
    }
    return entries;
}

TelegramEntry loadMoreEntryForParent(const QString &parentPath)
{
    TelegramEntry entry;
    entry.name = QStringLiteral("Load more...");
    entry.path = loadMorePathForParent(parentPath);
    entry.providerLabel = QStringLiteral("Telegram next batch");
    entry.iconName = QStringLiteral("telegram-badge-load-more");
    entry.directory = true;
    entry.loadMore = true;
    return entry;
}

bool telegramTraceEnabled()
{
    return qEnvironmentVariableIntValue("FM_TELEGRAM_TRACE") != 0;
}

void appendLoadMoreIfNeeded(QList<TelegramEntry> &entries, const QString &parentPath)
{
    const std::optional<TelegramFilesPage> page = pagination(parentPath);
    if (page && page->hasMore) {
        entries.append(loadMoreEntryForParent(parentPath));
    }
}

QList<TelegramEntry> cachedEntriesForChildren(const QString &parentPath)
{
    QList<TelegramEntry> entries;
    const QStringList paths = cachedChildren(parentPath);
    entries.reserve(paths.size() + 1);
    for (const QString &childPath : paths) {
        const std::optional<TelegramEntry> entry = cachedEntry(childPath);
        if (entry) {
            entries.append(*entry);
        }
    }
    appendLoadMoreIfNeeded(entries, parentPath);
    return entries;
}

class TelegramFileProvider final : public FileProvider
{
public:
    explicit TelegramFileProvider(QObject *parent = nullptr)
        : FileProvider(parent)
    {
    }

    QString scheme() const override { return QStringLiteral("telegram"); }
    bool canHandle(const QString &path) const override { return parseTelegramPath(path).valid; }
    Capabilities capabilities() const override { return Browse | ReadMetadata | Create | Transfer; }
    bool canCreateChildren(const QString &path) const override
    {
        return isUploadContainer(parseTelegramPath(path));
    }
    bool canRemovePath(const QString &path) const override
    {
        Q_UNUSED(path)
        return false;
    }
    bool canCopyPath(const QString &path) const override
    {
        const std::optional<FileEntry> entry = entryInfo(path);
        return entry && !entry->isDirectory;
    }
    bool isReadOnlyContainer(const QString &path) const override
    {
        return !isUploadContainer(parseTelegramPath(path));
    }

    void scan(const QString &path) override
    {
        clearLastError();
        const ParsedTelegramPath parsed = parseTelegramPath(path);
        m_currentPath = parsed.loadMore ? containerPathForParsed(parsed) : parsed.normalized;
        m_running.store(true);
        const int generation = m_generation.fetch_add(1) + 1;
        emit started();

        if (!parsed.valid) {
            finishWithError(path, generation, QStringLiteral("Invalid telegram:// path"));
            return;
        }

        QList<TelegramEntry> entries;
        if (parsed.kind == TelegramPathKind::Root) {
            entries = rootEntries();
        } else if (parsed.kind == TelegramPathKind::Status) {
            entries = {statusEntry()};
        } else if (parsed.kind == TelegramPathKind::Downloads && !parsed.loadMore) {
            QString error;
            entries = localDownloadEntries(parsed.normalized, parsed.itemName, &error);
            if (!error.isEmpty()) {
                finishWithError(parsed.normalized, generation, error);
                return;
            }
            storeChildren(parsed.normalized, entries);
        } else if (parsed.kind == TelegramPathKind::Chats) {
            QString error;
            entries = sharedTelegramClient().chats(&error);
            if (!error.isEmpty()) {
                finishWithError(parsed.normalized, generation, error);
                return;
            }
            storeChildren(parsed.normalized, entries);
        } else if (parsed.kind == TelegramPathKind::Saved && (parsed.itemName.isEmpty() || parsed.loadMore)) {
            QString error;
            const qint64 fromMessageId = parsed.loadMore
                ? pagination(savedRootPath()).value_or(TelegramFilesPage{}).nextFromMessageId
                : 0;
            const TelegramSavedMessagesPage page = sharedTelegramClient().savedMessageFiles(fromMessageId, &error);
            if (!error.isEmpty()) {
                finishWithError(parsed.normalized, generation, error);
                return;
            }
            if (parsed.loadMore) {
                appendChildren(savedRootPath(), page.entries);
            } else {
                storeChildren(savedRootPath(), page.entries);
            }
            storePagination(savedRootPath(), page.nextFromMessageId, page.hasMore);
            entries = cachedEntriesForChildren(savedRootPath());
            if (telegramTraceEnabled()) {
                qInfo().noquote() << "[TelegramProvider]"
                                  << QStringLiteral("saved scan loadMore=%1 cursor=%2 pageEntries=%3 nextCursor=%4 hasMore=%5 visibleEntries=%6")
                                         .arg(parsed.loadMore ? QStringLiteral("true") : QStringLiteral("false"),
                                              fromMessageId != 0 ? QStringLiteral("set") : QStringLiteral("initial"),
                                              QString::number(page.entries.size()),
                                              page.nextFromMessageId != 0 ? QStringLiteral("set") : QStringLiteral("empty"),
                                              page.hasMore ? QStringLiteral("true") : QStringLiteral("false"),
                                              QString::number(entries.size()));
            }
        } else if ((parsed.kind == TelegramPathKind::Chat || parsed.kind == TelegramPathKind::Channel)
                   && (parsed.itemName.isEmpty() || parsed.loadMore)) {
            bool ok = false;
            qint64 chatId = parsed.id.toLongLong(&ok);
            QString error;
            if (!ok && parsed.kind == TelegramPathKind::Channel) {
                chatId = sharedTelegramClient().publicChatId(parsed.id, &error);
                ok = chatId != 0;
            }
            if (!ok || chatId == 0) {
                finishWithError(parsed.normalized, generation, error.isEmpty() ? QStringLiteral("Invalid Telegram chat id") : error);
                return;
            }

            const QString parentPath = containerPathForParsed(parsed);
            const qint64 fromMessageId = parsed.loadMore
                ? pagination(parentPath).value_or(TelegramFilesPage{}).nextFromMessageId
                : 0;
            const TelegramFilesPage page = sharedTelegramClient().chatMessageFiles(chatId, parentPath, fromMessageId, &error);
            if (!error.isEmpty()) {
                finishWithError(parsed.normalized, generation, error);
                return;
            }
            if (parsed.loadMore) {
                appendChildren(parentPath, page.entries);
            } else {
                storeChildren(parentPath, page.entries);
            }
            storePagination(parentPath, page.nextFromMessageId, page.hasMore);
            entries = cachedEntriesForChildren(parentPath);
            if (telegramTraceEnabled()) {
                qInfo().noquote() << "[TelegramProvider]"
                                  << QStringLiteral("chat scan kind=%1 loadMore=%2 cursor=%3 pageEntries=%4 nextCursor=%5 hasMore=%6 visibleEntries=%7")
                                         .arg(parsed.kind == TelegramPathKind::Channel ? QStringLiteral("channel") : QStringLiteral("chat"),
                                              parsed.loadMore ? QStringLiteral("true") : QStringLiteral("false"),
                                              fromMessageId != 0 ? QStringLiteral("set") : QStringLiteral("initial"),
                                              QString::number(page.entries.size()),
                                              page.nextFromMessageId != 0 ? QStringLiteral("set") : QStringLiteral("empty"),
                                              page.hasMore ? QStringLiteral("true") : QStringLiteral("false"),
                                              QString::number(entries.size()));
            }
        } else {
            finishWithError(parsed.normalized, generation, QStringLiteral("This Telegram path is not a browsable folder yet"));
            return;
        }

        if (parsed.kind != TelegramPathKind::Saved
            && parsed.kind != TelegramPathKind::Chat
            && parsed.kind != TelegramPathKind::Channel
            && parsed.kind != TelegramPathKind::Chats) {
            storeChildren(parsed.normalized, entries);
        }
        QList<FileEntry> fileEntries;
        fileEntries.reserve(entries.size());
        for (const TelegramEntry &entry : entries) {
            fileEntries.append(fileEntryFromTelegramEntry(entry));
        }
        if (!fileEntries.isEmpty()) {
            emit batchReady(fileEntries, generation);
        }
        m_running.store(false);
        emit finished(parsed.loadMore ? containerPathForParsed(parsed) : parsed.normalized, true, generation, {});
    }

    void refresh(const QString &path) override { scan(path); }
    void cancel() override
    {
        m_generation.fetch_add(1);
        m_running.store(false);
    }

    void setShowHidden(bool show) override { m_showHidden = show; }
    bool isRunning() const override { return m_running.load(); }
    QString currentPath() const override { return m_currentPath; }
    int currentGeneration() const override { return m_generation.load(); }
    bool pathExists(const QString &path) const override { return entryInfo(path).has_value(); }
    bool isDirectory(const QString &path) const override
    {
        const std::optional<FileEntry> entry = entryInfo(path);
        return entry && entry->isDirectory;
    }
    bool isSymLink(const QString &path) const override
    {
        Q_UNUSED(path)
        return false;
    }
    QString normalizedPath(const QString &path) const override
    {
        const QString converted = telegramPathFromUserInput(path);
        return converted.isEmpty() ? normalizedTelegramPath(path) : converted;
    }
    QString fileName(const QString &path) const override
    {
        const QString normalized = normalizedTelegramPath(path);
        if (const std::optional<TelegramEntry> cached = cachedEntry(normalized)) {
            if (!cached->name.isEmpty()) {
                return cached->name;
            }
        }
        return fileNameForTelegramPath(path);
    }
    QString absolutePath(const QString &path) const override { return normalizedTelegramPath(path); }
    QString parentPath(const QString &path) const override { return parentTelegramPath(path); }
    QString childPath(const QString &parentPath, const QString &name) const override { return childTelegramPath(parentPath, name); }

    std::optional<FileEntry> entryInfo(const QString &path) const override
    {
        const ParsedTelegramPath parsed = parseTelegramPath(path);
        if (!parsed.valid) {
            return std::nullopt;
        }
        if (parsed.kind == TelegramPathKind::Root) {
            TelegramEntry entry = rootEntry(QStringLiteral("Telegram"), QStringLiteral("telegram:///"), QStringLiteral("Read-only"));
            entry.iconName = QStringLiteral("telegram");
            return fileEntryFromTelegramEntry(entry);
        }
        if (parsed.kind == TelegramPathKind::Saved && parsed.itemName.isEmpty()) {
            TelegramEntry entry = rootEntry(QStringLiteral("Saved Messages"), QStringLiteral("telegram://saved"), QStringLiteral("Read-only"));
            entry.iconName = QStringLiteral("telegram-saved");
            return fileEntryFromTelegramEntry(entry);
        }
        if (parsed.kind == TelegramPathKind::Chats) {
            TelegramEntry entry = rootEntry(QStringLiteral("Chats and Channels"), QStringLiteral("telegram://chats"), QStringLiteral("Read-only"));
            entry.iconName = QStringLiteral("telegram-chats");
            return fileEntryFromTelegramEntry(entry);
        }
        if (parsed.kind == TelegramPathKind::Downloads && parsed.itemName.isEmpty()) {
            TelegramEntry entry = rootEntry(QStringLiteral("Downloads"), QStringLiteral("telegram://downloads"), QStringLiteral("Local TDLib files"));
            entry.iconName = QStringLiteral("telegram-downloads");
            return fileEntryFromTelegramEntry(entry);
        }
        if (parsed.normalized == QString::fromLatin1(TelegramStatusPath)) {
            return fileEntryFromTelegramEntry(statusEntry());
        }
        if ((parsed.kind == TelegramPathKind::Saved
             || parsed.kind == TelegramPathKind::Chat
             || parsed.kind == TelegramPathKind::Channel)
            && parsed.loadMore) {
            return fileEntryFromTelegramEntry(loadMoreEntryForParent(containerPathForParsed(parsed)));
        }
        const std::optional<TelegramEntry> cached = cachedEntry(parsed.normalized);
        return cached ? std::optional<FileEntry>(fileEntryFromTelegramEntry(*cached)) : std::nullopt;
    }

    bool ensureParentDirectory(const QString &path) const override
    {
        return isUploadContainer(parseTelegramPath(parentTelegramPath(path)));
    }
    bool makePath(const QString &path) const override
    {
        Q_UNUSED(path)
        return failReadOnly();
    }
    bool removePath(const QString &path) const override
    {
        Q_UNUSED(path)
        return failReadOnly();
    }

    QStringList childPaths(const QString &path, bool includeHidden = true) const override
    {
        Q_UNUSED(includeHidden)
        const ParsedTelegramPath parsed = parseTelegramPath(path);
        if (!parsed.valid) {
            return {};
        }
        if (parsed.kind == TelegramPathKind::Root) {
            QList<TelegramEntry> entries = rootEntries();
            storeChildren(parsed.normalized, entries);
        } else if (parsed.kind == TelegramPathKind::Status) {
            storeChildren(parsed.normalized, {statusEntry()});
        }
        QStringList paths = cachedChildren(parsed.normalized);
        if ((parsed.kind == TelegramPathKind::Saved
             || parsed.kind == TelegramPathKind::Chat
             || parsed.kind == TelegramPathKind::Channel)
            && parsed.itemName.isEmpty()) {
            const std::optional<TelegramFilesPage> page = pagination(parsed.normalized);
            const QString loadMorePath = loadMorePathForParent(parsed.normalized);
            if (page && page->hasMore && !paths.contains(loadMorePath)) {
                paths.append(loadMorePath);
            }
        }
        return paths;
    }

    bool movePath(const QString &sourcePath, const QString &destinationPath) const override
    {
        Q_UNUSED(sourcePath)
        Q_UNUSED(destinationPath)
        return failReadOnly();
    }

    std::unique_ptr<QIODevice> openRead(const QString &path) const override
    {
        clearLastError();
        if (normalizedTelegramPath(path) != QString::fromLatin1(TelegramStatusPath)) {
            const std::optional<TelegramEntry> entry = cachedEntry(normalizedTelegramPath(path));
            if (entry && !entry->virtualContent.isEmpty()) {
                auto buffer = std::make_unique<QBuffer>();
                buffer->setData(entry->virtualContent);
                if (!buffer->open(QIODevice::ReadOnly)) {
                    setLastError(QStringLiteral("Could not open Telegram virtual file"));
                    return nullptr;
                }
                return buffer;
            }
            if (entry && entry->downloaded && !entry->localPath.isEmpty()) {
                auto file = std::make_unique<QFile>(entry->localPath);
                if (!file->open(QIODevice::ReadOnly)) {
                    setLastError(QStringLiteral("Cannot open downloaded Telegram file"));
                    return nullptr;
                }
                return file;
            }
            if (!entry || entry->fileId <= 0) {
                setLastError(QStringLiteral("Telegram file metadata is not available. Refresh the Telegram folder first."));
                return nullptr;
            }
            QString error;
            const QString localPath = sharedTelegramClient().downloadFile(entry->fileId, &error);
            if (localPath.isEmpty()) {
                setLastError(error.isEmpty() ? QStringLiteral("Telegram file download failed") : error);
                return nullptr;
            }
            auto file = std::make_unique<QFile>(localPath);
            if (!file->open(QIODevice::ReadOnly)) {
                setLastError(QStringLiteral("Cannot open downloaded Telegram file"));
                return nullptr;
            }
            return file;
        }

        auto buffer = std::make_unique<QBuffer>();
        const QByteArray data("Telegram provider is available.\nSaved Messages and chat file browsing are read-only.\n");
        buffer->setData(data);
        if (!buffer->open(QIODevice::ReadOnly)) {
            setLastError(QStringLiteral("Could not open Telegram status buffer"));
            return nullptr;
        }
        return buffer;
    }

    bool copyToLocalFileWithTimeout(const QString &sourcePath,
                                    const QString &destinationFilePath,
                                    const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                    QString *error,
                                    int downloadTimeoutMs) const
    {
        const QString normalized = normalizedTelegramPath(sourcePath);
        if (normalized != QString::fromLatin1(TelegramStatusPath)) {
            const std::optional<TelegramEntry> entry = cachedEntry(normalized);
            QString localPath;
            if (entry && !entry->virtualContent.isEmpty()) {
                QSaveFile destination(destinationFilePath);
                if (!destination.open(QIODevice::WriteOnly)) {
                    const QString message = QStringLiteral("Cannot write %1").arg(QDir::toNativeSeparators(destinationFilePath));
                    if (error) {
                        *error = message;
                    }
                    setLastError(message);
                    return false;
                }
                if (destination.write(entry->virtualContent) != entry->virtualContent.size() || !destination.commit()) {
                    const QString message = QStringLiteral("Cannot finalize %1").arg(QDir::toNativeSeparators(destinationFilePath));
                    if (error) {
                        *error = message;
                    }
                    setLastError(message);
                    return false;
                }
                if (progress) {
                    progress(entry->virtualContent.size(), entry->virtualContent.size());
                }
                if (error) {
                    error->clear();
                }
                return true;
            } else if (entry && entry->downloaded && !entry->localPath.isEmpty()) {
                localPath = entry->localPath;
            } else if (entry && entry->fileId > 0) {
                QString downloadError;
                localPath = sharedTelegramClient().downloadFile(
                    entry->fileId,
                    [entry, &progress](qint64 processed, qint64 total) {
                        if (!progress) {
                            return true;
                        }
                        const qint64 effectiveTotal = total > 0 ? total : entry->size;
                        const qint64 effectiveProcessed = effectiveTotal > 0
                            ? std::clamp<qint64>((std::max<qint64>)(0, processed) * 9 / 10, 0, effectiveTotal)
                            : (std::max<qint64>)(0, processed);
                        return progress(effectiveProcessed, effectiveTotal);
                    },
                    &downloadError,
                    downloadTimeoutMs);
                if (localPath.isEmpty()) {
                    const QString message = downloadError.isEmpty() ? QStringLiteral("Telegram file download failed") : downloadError;
                    if (error) {
                        *error = message;
                    }
                    setLastError(message);
                    return false;
                }
            } else {
                const QString message = QStringLiteral("Telegram file metadata is not available. Refresh the Telegram folder first.");
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
            QFile source(localPath);
            if (!source.open(QIODevice::ReadOnly)) {
                const QString message = QStringLiteral("Cannot read downloaded Telegram file");
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
            QSaveFile destination(destinationFilePath);
            if (!destination.open(QIODevice::WriteOnly)) {
                const QString message = QStringLiteral("Cannot write %1").arg(QDir::toNativeSeparators(destinationFilePath));
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
            qint64 copied = 0;
            const qint64 total = entry->size > 0 ? entry->size : source.size();
            while (!source.atEnd()) {
                const QByteArray chunk = source.read(1024 * 1024);
                if (chunk.isEmpty() && source.error() != QFile::NoError) {
                    const QString message = QStringLiteral("Cannot read downloaded Telegram file");
                    if (error) {
                        *error = message;
                    }
                    setLastError(message);
                    return false;
                }
                if (destination.write(chunk) != chunk.size()) {
                    const QString message = QStringLiteral("Cannot write %1").arg(QDir::toNativeSeparators(destinationFilePath));
                    if (error) {
                        *error = message;
                    }
                    setLastError(message);
                    return false;
                }
                copied += chunk.size();
                const qint64 reportedCopied = entry && entry->fileId > 0 && !entry->downloaded && total > 0
                    ? std::clamp<qint64>(total * 9 / 10 + copied / 10, 0, total)
                    : copied;
                if (progress && !progress(reportedCopied, total)) {
                    const QString message = QStringLiteral("Telegram file copy cancelled. Retry will reuse the TDLib cache when possible.");
                    if (error) {
                        *error = message;
                    }
                    setLastError(message);
                    return false;
                }
            }
            if (!destination.commit()) {
                const QString message = QStringLiteral("Cannot finalize %1").arg(QDir::toNativeSeparators(destinationFilePath));
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
            if (progress) {
                progress(total, total);
            }
            if (error) {
                error->clear();
            }
            return true;
        }
        const QString message = QStringLiteral("Telegram status file is virtual and cannot be copied yet");
        if (error) {
            *error = message;
        }
        setLastError(message);
        return false;
    }

    bool copyToLocalFile(const QString &sourcePath,
                         const QString &destinationFilePath,
                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                         QString *error) const override
    {
        return copyToLocalFileWithTimeout(sourcePath, destinationFilePath, progress, error, 600000);
    }

    bool copyToLocalFileForPreview(const QString &sourcePath,
                                   const QString &destinationFilePath,
                                   const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                   QString *error) const override
    {
        return copyToLocalFileWithTimeout(sourcePath, destinationFilePath, progress, error, previewDownloadTimeoutMs());
    }

    bool supportsLocalFileBatchMaterialize() const override { return true; }

    bool copyToLocalFiles(const QVector<LocalFileMaterializeItem> &items,
                          const std::function<bool(const QString &currentSourcePath, qint64 processedBytes, qint64 totalBytes)> &progress,
                          QString *error) const override
    {
        qint64 totalBytes = 0;
        for (const LocalFileMaterializeItem &item : items) {
            if (item.size > 0) {
                totalBytes += item.size;
                continue;
            }
            if (const std::optional<TelegramEntry> entry = cachedEntry(normalizedTelegramPath(item.sourcePath)); entry && entry->size > 0) {
                totalBytes += entry->size;
            }
        }

        qint64 completedBytes = 0;
        for (const LocalFileMaterializeItem &item : items) {
            const std::optional<TelegramEntry> entry = cachedEntry(normalizedTelegramPath(item.sourcePath));
            const qint64 itemSize = item.size > 0
                ? item.size
                : (entry && entry->size > 0 ? entry->size : 0);
            qint64 lastItemProcessed = 0;
            QString itemError;
            const bool ok = copyToLocalFile(
                item.sourcePath,
                item.destinationFilePath,
                [&](qint64 processed, qint64 total) {
                    const qint64 effectiveItemTotal = total > 0 ? total : itemSize;
                    lastItemProcessed = effectiveItemTotal > 0
                        ? std::clamp<qint64>(processed, 0, effectiveItemTotal)
                        : (std::max<qint64>)(0, processed);
                    const qint64 aggregate = completedBytes + lastItemProcessed;
                    const qint64 aggregateTotal = totalBytes > 0 ? totalBytes : aggregate;
                    return !progress || progress(item.sourcePath, aggregate, aggregateTotal);
                },
                &itemError);

            if (!ok) {
                const QString message = itemError.trimmed().isEmpty()
                    ? QStringLiteral("Telegram batch download failed.")
                    : itemError;
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }

            completedBytes += itemSize > 0 ? itemSize : lastItemProcessed;
            if (progress && !progress(item.sourcePath, completedBytes, totalBytes > 0 ? totalBytes : completedBytes)) {
                const QString message = QStringLiteral("Telegram batch download cancelled. Retry will reuse the TDLib cache when possible.");
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
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
        const QFileInfo sourceInfo(sourceFilePath);
        if (!sourceInfo.exists() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
            const QString message = QStringLiteral("Telegram upload source file is unavailable.");
            if (error) {
                *error = message;
            }
            setLastError(message);
            return false;
        }

        const ParsedTelegramPath container = uploadContainerForPath(destinationPath);
        if (!isUploadContainer(container)) {
            const QString message = QStringLiteral("Copy files into Telegram Saved Messages, chat, or channel folders.");
            if (error) {
                *error = message;
            }
            setLastError(message);
            return false;
        }

        qint64 chatId = container.id.toLongLong();
        if (container.kind == TelegramPathKind::Saved) {
            QString savedError;
            chatId = sharedTelegramClient().savedMessagesChatId(&savedError);
            if (chatId == 0) {
                const QString message = savedError.isEmpty() ? QStringLiteral("Telegram Saved Messages chat is unavailable.") : savedError;
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
        }
        if (chatId == 0) {
            const QString message = QStringLiteral("Telegram chat id is unavailable.");
            if (error) {
                *error = message;
            }
            setLastError(message);
            return false;
        }

        const QString mimeType = QMimeDatabase().mimeTypeForFile(sourceInfo).name();
        const bool ok = sharedTelegramClient().sendFile(chatId, sourceInfo.absoluteFilePath(), mimeType, progress, error);
        if (!ok) {
            const QString message = error && !error->isEmpty() ? *error : QStringLiteral("Telegram upload failed.");
            setLastError(message);
            return false;
        }
        if (error) {
            error->clear();
        }
        clearLastError();
        return true;
    }

    bool supportsLocalFileBatchCopy() const override { return true; }

    bool copyFromLocalFiles(const QVector<LocalFileCopyItem> &items,
                            const std::function<bool(const QString &currentFilePath, qint64 processedBytes, qint64 totalBytes)> &progress,
                            QString *error) const override
    {
        if (items.isEmpty()) {
            if (error) {
                error->clear();
            }
            return true;
        }

        struct PreparedUpload {
            LocalFileCopyItem item;
            TelegramUploadFile file;
            ParsedTelegramPath container;
            bool albumCompatible = false;
        };

        QVector<PreparedUpload> prepared;
        prepared.reserve(items.size());
        ParsedTelegramPath commonContainer;
        qint64 totalBytes = 0;
        for (const LocalFileCopyItem &item : items) {
            const QFileInfo sourceInfo(item.sourceFilePath);
            if (!sourceInfo.exists() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
                const QString message = QStringLiteral("Telegram upload source file is unavailable.");
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
            const ParsedTelegramPath container = uploadContainerForPath(item.destinationPath);
            if (!isUploadContainer(container)) {
                const QString message = QStringLiteral("Copy files directly into Telegram Saved Messages, chat, or channel folders.");
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
            if (!commonContainer.valid) {
                commonContainer = container;
            } else if (!sameUploadContainer(commonContainer, container)) {
                const QString message = QStringLiteral("Telegram batch upload must target one chat folder.");
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }

            const QString mimeType = QMimeDatabase().mimeTypeForFile(sourceInfo).name();
            const qint64 size = item.size > 0 ? item.size : sourceInfo.size();
            totalBytes += size;
            prepared.push_back({item,
                                TelegramUploadFile{sourceInfo.absoluteFilePath(), mimeType, size},
                                container,
                                isAlbumCompatibleMime(mimeType, size)});
        }

        qint64 chatId = commonContainer.id.toLongLong();
        if (commonContainer.kind == TelegramPathKind::Saved) {
            QString savedError;
            chatId = sharedTelegramClient().savedMessagesChatId(&savedError);
            if (chatId == 0) {
                const QString message = savedError.isEmpty() ? QStringLiteral("Telegram Saved Messages chat is unavailable.") : savedError;
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
        }
        if (chatId == 0) {
            const QString message = QStringLiteral("Telegram chat id is unavailable.");
            if (error) {
                *error = message;
            }
            setLastError(message);
            return false;
        }

        qint64 completedBytes = 0;
        for (qsizetype i = 0; i < prepared.size();) {
            if (prepared.at(i).albumCompatible) {
                QList<TelegramUploadFile> album;
                qint64 albumBytes = 0;
                qsizetype j = i;
                while (j < prepared.size() && prepared.at(j).albumCompatible && album.size() < 8) {
                    album.append(prepared.at(j).file);
                    albumBytes += prepared.at(j).file.size;
                    ++j;
                }
                if (album.size() >= 2) {
                    QString albumError;
                    const QString currentPath = prepared.at(i).item.sourceFilePath;
                    const bool ok = sharedTelegramClient().sendFileAlbum(
                        chatId,
                        album,
                        [&](qint64 processed, qint64 total) {
                            Q_UNUSED(total)
                            const qint64 aggregate = completedBytes + std::clamp<qint64>(processed, 0, albumBytes);
                            return !progress || progress(currentPath, aggregate, totalBytes);
                        },
                        &albumError);
                    if (!ok) {
                        const QString message = albumError.isEmpty() ? QStringLiteral("Telegram album upload failed.") : albumError;
                        if (error) {
                            *error = message;
                        }
                        setLastError(message);
                        return false;
                    }
                    completedBytes += albumBytes;
                    if (progress && !progress(currentPath, completedBytes, totalBytes)) {
                        const QString message = QStringLiteral("Telegram upload cancelled.");
                        if (error) {
                            *error = message;
                        }
                        setLastError(message);
                        return false;
                    }
                    i = j;
                    continue;
                }
            }

            const PreparedUpload &upload = prepared.at(i);
            QString uploadError;
            const bool ok = sharedTelegramClient().sendFile(
                chatId,
                upload.file.localFilePath,
                upload.file.mimeType,
                [&](qint64 processed, qint64 total) {
                    Q_UNUSED(total)
                    const qint64 aggregate = completedBytes + std::clamp<qint64>(processed, 0, upload.file.size);
                    return !progress || progress(upload.item.sourceFilePath, aggregate, totalBytes);
                },
                &uploadError);
            if (!ok) {
                const QString message = uploadError.isEmpty() ? QStringLiteral("Telegram upload failed.") : uploadError;
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
            completedBytes += upload.file.size;
            if (progress && !progress(upload.item.sourceFilePath, completedBytes, totalBytes)) {
                const QString message = QStringLiteral("Telegram upload cancelled.");
                if (error) {
                    *error = message;
                }
                setLastError(message);
                return false;
            }
            ++i;
        }

        if (error) {
            error->clear();
        }
        clearLastError();
        return true;
    }

    std::unique_ptr<QIODevice> openWrite(const QString &path, bool truncate = true) const override
    {
        Q_UNUSED(path)
        Q_UNUSED(truncate)
        failReadOnly();
        return nullptr;
    }

    bool renamePath(const QString &oldPath, const QString &newName) override
    {
        Q_UNUSED(oldPath)
        Q_UNUSED(newName)
        return failReadOnly();
    }
    bool createFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override
    {
        Q_UNUSED(parentPath)
        Q_UNUSED(name)
        if (createdPath) {
            createdPath->clear();
        }
        return failReadOnly();
    }
    bool createFile(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override
    {
        Q_UNUSED(parentPath)
        Q_UNUSED(name)
        if (createdPath) {
            createdPath->clear();
        }
        return failReadOnly();
    }

    QString lastErrorString() const override { return m_lastError; }
    void clearLastError() const override { m_lastError.clear(); }

private:
    void finishWithError(const QString &path, int generation, const QString &error)
    {
        setLastError(error);
        m_running.store(false);
        emit finished(path, false, generation, error);
    }

    bool failReadOnly() const
    {
        setLastError(QStringLiteral("telegram:// is read-only"));
        return false;
    }

    void setLastError(const QString &error) const
    {
        m_lastError = error;
    }

    QString m_currentPath = QStringLiteral("telegram:///");
    std::atomic<int> m_generation{0};
    std::atomic_bool m_running{false};
    bool m_showHidden = false;
    mutable QString m_lastError;
};

} // namespace

std::unique_ptr<FileProvider> createTelegramFileProvider()
{
    return std::make_unique<TelegramFileProvider>();
}

} // namespace TelegramProviderInternal
