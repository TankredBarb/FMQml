#include "InstagramInternal.h"

#include "InstagramAuth.h"

#include <atomic>

#include <QBuffer>
#include <QCryptographicHash>
#include <QDir>
#include <QFuture>
#include <QSaveFile>
#include <QtConcurrent>

namespace InstagramProviderInternal {

class InstagramFileProvider final : public FileProvider
{
public:
    explicit InstagramFileProvider(QObject *parent = nullptr)
        : FileProvider(parent)
    {
    }

    QString scheme() const override { return QStringLiteral("instagram"); }
    bool canHandle(const QString &path) const override { return parseInstagramPath(path).valid; }
    Capabilities capabilities() const override { return Browse | ReadMetadata | Transfer; }

    void scan(const QString &path) override
    {
        const ParsedPath parsed = parseInstagramPath(path);
        if (!parsed.valid) {
            emit finished(path, false, m_generation.fetch_add(1) + 1, QStringLiteral("Invalid Instagram path"));
            return;
        }
        m_currentPath = parsed.loadMore ? parsed.rootPath : parsed.normalized;
        m_running.store(true);
        const int generation = m_generation.fetch_add(1) + 1;
        emit started();

        m_scanFutures.append(QtConcurrent::run([this, parsed, generation]() {
            if (parsed.stories && !parsed.storyItemName.isEmpty()) {
                m_running.store(false);
                emit finished(parsed.normalized, false, generation, QStringLiteral("Instagram story media item is not a folder"));
                return;
            }
            if (!parsed.itemName.isEmpty() && !parsed.loadMore && !parsed.stories) {
                m_running.store(false);
                emit finished(parsed.normalized, false, generation, QStringLiteral("Instagram media item is not a folder"));
                return;
            }

            if (parsed.stories) {
                const InstagramPost storiesPost = fetchStoriesForProfile(parsed);
                if (generation != m_generation.load()) {
                    return;
                }
                if (!storiesPost.error.isEmpty()) {
                    m_running.store(false);
                    emit finished(parsed.storiesRootPath, false, generation, storiesPost.error);
                    return;
                }

                QList<FileEntry> entries;
                entries.reserve(storiesPost.items.size());
                for (const InstagramMediaItem &item : storiesPost.items) {
                    entries.append(entryFromMedia(item));
                }
                if (!entries.isEmpty()) {
                    emit batchReady(entries, generation);
                }
                m_running.store(false);
                emit finished(parsed.storiesRootPath, true, generation, {});
                return;
            }

            const bool allowExpandedCache = !(parsed.kind == QLatin1String("user") && parsed.itemName.isEmpty());
            const InstagramPost post = parsed.loadMore ? fetchNextProfileBatch(parsed) : fetchPost(parsed, allowExpandedCache);
            if (generation != m_generation.load()) {
                return;
            }
            if (!post.error.isEmpty()) {
                if (parsed.loadMore) {
                    emit statusMessage(post.error);
                    const InstagramPost cachedPost = fetchPost(parsed);
                    QList<FileEntry> entries;
                    entries.reserve(cachedPost.items.size() + 1);
                    for (const InstagramMediaItem &item : cachedPost.items) {
                        entries.append(entryFromMedia(item));
                    }
                    if (cachedPost.hasNextPage) {
                        entries.append(entryFromLoadMore(cachedPost));
                    }
                    if (!entries.isEmpty()) {
                        emit batchReady(entries, generation);
                    }
                    m_running.store(false);
                    emit finished(parsed.rootPath, true, generation, {});
                    return;
                }
                m_running.store(false);
                emit finished(parsed.rootPath, false, generation, post.error);
                return;
            }

            QList<FileEntry> entries;
            entries.reserve(post.items.size() + 2);
            for (const InstagramMediaItem &item : post.items) {
                entries.append(entryFromMedia(item));
            }
            if (parsed.kind == QLatin1String("user") && parsed.itemName.isEmpty()) {
                const InstagramPost storiesPost = fetchStoriesForProfile(parsed);
                if (generation != m_generation.load()) {
                    return;
                }
                if (!storiesPost.items.isEmpty()) {
                    entries.append(entryFromStories(post, storiesPost));
                }
            }
            if (post.hasNextPage) {
                entries.append(entryFromLoadMore(post));
            }
            if (!entries.isEmpty()) {
                emit batchReady(entries, generation);
            }
            m_running.store(false);
            emit finished(parsed.rootPath, true, generation, {});
        }));
    }

    void cancel() override
    {
        m_generation.fetch_add(1);
        m_running.store(false);
    }

    void setShowHidden(bool show) override { m_showHidden = show; }
    bool isRunning() const override { return m_running.load(); }
    QString currentPath() const override { return m_currentPath; }
    int currentGeneration() const override { return m_generation.load(); }
    bool pathExists(const QString &path) const override
    {
        const ParsedPath parsed = parseInstagramPath(path);
        if (!parsed.valid) {
            return false;
        }
        if (parsed.stories) {
            const InstagramPost storiesPost = fetchStoriesForProfile(parsed);
            if (parsed.storyItemName.isEmpty()) {
                return !storiesPost.items.isEmpty();
            }
            return mediaItemForPath(path).has_value();
        }
        if (parsed.itemName.isEmpty()) {
            return !fetchPost(parsed).items.isEmpty();
        }
        if (parsed.loadMore) {
            const InstagramPost post = fetchPost(parsed);
            return post.hasNextPage;
        }
        return mediaItemForPath(path).has_value();
    }
    bool isDirectory(const QString &path) const override
    {
        const ParsedPath parsed = parseInstagramPath(path);
        return parsed.valid && (parsed.itemName.isEmpty() || parsed.loadMore || (parsed.stories && parsed.storyItemName.isEmpty()));
    }
    bool isSymLink(const QString &) const override { return false; }
    QString normalizedPath(const QString &path) const override { return parseInstagramPath(path).normalized; }
    QString fileName(const QString &path) const override { return fileNameForInstagramPath(path); }
    QString localCopyFileName(const QString &path) const override { return fileName(path); }
    QString thumbnailUrl(const QString &path) const override
    {
        const std::optional<InstagramMediaItem> item = cachedMediaItemForPath(path);
        return item ? item->thumbnailUrl : QString{};
    }
    QString thumbnailCacheIdentity(const QString &path) const override
    {
        const std::optional<InstagramMediaItem> item = cachedMediaItemForPath(path);
        if (!item || item->thumbnailUrl.isEmpty()) {
            return {};
        }
        const QByteArray urlHash = QCryptographicHash::hash(item->thumbnailUrl.toUtf8(), QCryptographicHash::Sha1)
                                       .toHex();
        return QStringLiteral("instagram:%1:%2").arg(QString::fromLatin1(urlHash),
                                                       item->timestamp.toUTC().toString(Qt::ISODateWithMs));
    }
    ProviderThumbnailResult thumbnailForPath(const QString &path, const QSize &requestedSize,
                                             QString *error) const override
    {
        Q_UNUSED(requestedSize)
        constexpr qint64 kThumbnailLimit = 2 * 1024 * 1024;
        const std::optional<InstagramMediaItem> item = cachedMediaItemForPath(path);
        if (!item || item->thumbnailUrl.isEmpty()) {
            if (error) *error = QStringLiteral("Instagram entry has no thumbnail URL");
            return {};
        }
        QString contentType;
        QString fetchError;
        const QByteArray bytes = httpGetBytes(QUrl(item->thumbnailUrl),
            [](qint64 received, qint64 total) {
                return received <= kThumbnailLimit && (total <= 0 || total <= kThumbnailLimit);
            }, &contentType, &fetchError, true, QByteArray(InstagramBrowserUserAgent));
        if (bytes.isEmpty()) {
            if (error) *error = fetchError.isEmpty() ? QStringLiteral("Instagram thumbnail download failed") : fetchError;
            ProviderThumbnailResult result;
            if (fetchError.contains(QStringLiteral("timed out"), Qt::CaseInsensitive)
                || fetchError.contains(QStringLiteral("HTTP 429"))
                || fetchError.contains(QStringLiteral("HTTP 5"))) {
                result.kind = ProviderThumbnailResult::Kind::TemporaryUnavailable;
            }
            result.cacheIdentity = thumbnailCacheIdentity(path);
            return result;
        }
        ProviderThumbnailResult result;
        result.kind = ProviderThumbnailResult::Kind::EncodedBytes;
        result.encodedBytes = bytes;
        result.mimeType = contentType;
        result.cacheIdentity = thumbnailCacheIdentity(path);
        return result;
    }
    QString absolutePath(const QString &path) const override { return normalizedPath(path); }
    QString parentPath(const QString &path) const override { return parentInstagramPath(path); }
    QString childPath(const QString &parentPath, const QString &name) const override
    {
        const ParsedPath parsed = parseInstagramPath(parentPath);
        if (!parsed.valid || (!parsed.itemName.isEmpty() && !(parsed.stories && parsed.storyItemName.isEmpty()))) {
            return {};
        }
        if (parsed.stories) {
            return parsed.storiesRootPath + QLatin1Char('/') + name.trimmed();
        }
        if (parsed.kind == QLatin1String("user")
            && name.trimmed().compare(QString::fromLatin1(StoriesItemName), Qt::CaseInsensitive) == 0) {
            return storiesPathForRoot(parsed.rootPath);
        }
        return parsed.rootPath + QLatin1Char('/') + name.trimmed();
    }

    std::optional<FileEntry> entryInfo(const QString &path) const override
    {
        const ParsedPath parsed = parseInstagramPath(path);
        if (!parsed.valid) {
            return std::nullopt;
        }
        if (parsed.stories) {
            const InstagramPost storiesPost = fetchStoriesForProfile(parsed);
            if (parsed.storyItemName.isEmpty()) {
                if (storiesPost.items.isEmpty()) {
                    return std::nullopt;
                }
                InstagramPost profilePost;
                profilePost.rootPath = parsed.rootPath;
                profilePost.fetchedAt = storiesPost.fetchedAt;
                return entryFromStories(profilePost, storiesPost);
            }
            const std::optional<InstagramMediaItem> item = mediaItemForPath(path);
            if (!item) {
                return std::nullopt;
            }
            return entryFromMedia(*item);
        }
        if (parsed.itemName.isEmpty()) {
            const InstagramPost post = fetchPost(parsed);
            if (post.items.isEmpty()) {
                return std::nullopt;
            }
            return entryFromPost(post);
        }
        if (parsed.loadMore) {
            const InstagramPost post = fetchPost(parsed);
            if (!post.hasNextPage) {
                return std::nullopt;
            }
            return entryFromLoadMore(post);
        }
        const std::optional<InstagramMediaItem> item = mediaItemForPath(path);
        if (!item) {
            return std::nullopt;
        }
        return entryFromMedia(*item);
    }

    bool ensureParentDirectory(const QString &) const override { return failReadOnly(); }
    bool makePath(const QString &) const override { return failReadOnly(); }
    bool removePath(const QString &) const override { return failReadOnly(); }

    QStringList childPaths(const QString &path, bool includeHidden = true) const override
    {
        Q_UNUSED(includeHidden)
        const ParsedPath parsed = parseInstagramPath(path);
        if (!parsed.valid || (!parsed.itemName.isEmpty() && !(parsed.stories && parsed.storyItemName.isEmpty()))) {
            return {};
        }
        if (parsed.stories) {
            const InstagramPost storiesPost = fetchStoriesForProfile(parsed);
            QStringList paths;
            paths.reserve(storiesPost.items.size());
            for (const InstagramMediaItem &item : storiesPost.items) {
                paths.append(item.path);
            }
            return paths;
        }

        const InstagramPost post = fetchPost(parsed);
        QStringList paths;
        paths.reserve(post.items.size() + 2);
        for (const InstagramMediaItem &item : post.items) {
            paths.append(item.path);
        }
        if (parsed.kind == QLatin1String("user")) {
            const InstagramPost storiesPost = fetchStoriesForProfile(parsed);
            if (!storiesPost.items.isEmpty()) {
                paths.append(storiesPathForRoot(post.rootPath));
            }
        }
        if (post.hasNextPage) {
            paths.append(loadMorePathForRoot(post.rootPath));
        }
        return paths;
    }

    bool movePath(const QString &, const QString &) const override { return failReadOnly(); }

    std::unique_ptr<QIODevice> openRead(const QString &path) const override
    {
        clearLastError();
        const std::optional<InstagramMediaItem> item = mediaItemForPath(path);
        if (!item) {
            setLastError(QStringLiteral("Instagram media item is not available"));
            return nullptr;
        }
        QString contentType;
        QString error;
        const QByteArray bytes = httpGetBytes(QUrl(item->url), &contentType, &error);
        if (bytes.isEmpty()) {
            setLastError(error.isEmpty() ? QStringLiteral("Instagram media download failed") : error);
            return nullptr;
        }
        updateCachedMediaItemSize(item->path, bytes.size());
        auto buffer = std::make_unique<QBuffer>();
        buffer->setData(bytes);
        if (!buffer->open(QIODevice::ReadOnly)) {
            setLastError(QStringLiteral("Cannot open Instagram media buffer"));
            return nullptr;
        }
        return buffer;
    }

    bool copyToLocalFile(const QString &sourcePath,
                         const QString &destinationFilePath,
                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                         QString *error) const override
    {
        const std::optional<InstagramMediaItem> item = mediaItemForPath(sourcePath);
        if (!item) {
            if (error) {
                *error = QStringLiteral("Instagram media item is not available");
            }
            return false;
        }

        QString contentType;
        QString downloadError;
        const QByteArray bytes = httpGetBytes(QUrl(item->url), progress, &contentType, &downloadError);
        if (bytes.isEmpty()) {
            if (error) {
                *error = downloadError.isEmpty() ? QStringLiteral("Instagram media download failed") : downloadError;
            }
            return false;
        }
        updateCachedMediaItemSize(item->path, bytes.size());

        QSaveFile file(destinationFilePath);
        if (!file.open(QIODevice::WriteOnly)) {
            if (error) {
                *error = QStringLiteral("Cannot write %1").arg(QDir::toNativeSeparators(destinationFilePath));
            }
            return false;
        }
        if (file.write(bytes) != bytes.size() || !file.commit()) {
            if (error) {
                *error = QStringLiteral("Cannot finalize %1").arg(QDir::toNativeSeparators(destinationFilePath));
            }
            return false;
        }
        return true;
    }

    std::unique_ptr<QIODevice> openWrite(const QString &, bool truncate = true) const override
    {
        Q_UNUSED(truncate)
        failReadOnly();
        return nullptr;
    }

    bool renamePath(const QString &, const QString &) override { return failReadOnly(); }
    bool createFolder(const QString &, const QString &, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        return failReadOnly();
    }
    bool createFile(const QString &, const QString &, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        return failReadOnly();
    }

    QString lastErrorString() const override { return m_lastError; }
    void clearLastError() const override { m_lastError.clear(); }

private:
    bool failReadOnly() const
    {
        setLastError(QStringLiteral("instagram:// is read-only"));
        return false;
    }

    void setLastError(const QString &error) const
    {
        m_lastError = error;
    }

    QString m_currentPath;
    QList<QFuture<void>> m_scanFutures;
    std::atomic<int> m_generation{0};
    std::atomic_bool m_running{false};
    bool m_showHidden = false;
    mutable QString m_lastError;
};


std::unique_ptr<FileProvider> createInstagramFileProvider()
{
    return std::make_unique<InstagramFileProvider>();
}


} // namespace InstagramProviderInternal
