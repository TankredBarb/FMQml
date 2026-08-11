#include "FolderPreviewController.h"
#include "FolderPreviewWarmupRegistry.h"
#include "../core/FileProvider.h"
#include "../core/FileEntrySortPolicy.h"
#ifndef FM_FOLDER_PREVIEW_CONTROLLER_TEST
#include "../core/FileProviderFactory.h"
#endif

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QPointer>
#include <QUrl>
#include <QThread>

#include <algorithm>
#include <utility>

namespace {
constexpr int MaxPresentationEntries = 9;
constexpr int MaxScannedEntries = 1000;
constexpr int MaxTextLength = 1024;
#ifdef FM_FOLDER_PREVIEW_CONTROLLER_TEST
constexpr bool ProviderRequestsSupported = false;
#else
constexpr bool ProviderRequestsSupported = true;
#endif

QString boundedText(const QString &value)
{
    return value.left(MaxTextLength);
}

bool isLocalPath(const QString &path)
{
    const QUrl url(path);
    return !url.isValid() || url.scheme().isEmpty() || url.isLocalFile();
}

QVariantMap unavailableSnapshot(quint64 requestId, const QString &path)
{
    return {{QStringLiteral("requestId"), requestId},
            {QStringLiteral("path"), boundedText(path)},
            {QStringLiteral("state"), QStringLiteral("unavailable")},
            {QStringLiteral("entries"), QVariantList{}},
            {QStringLiteral("hasMore"), false},
            {QStringLiteral("displayedCount"), 0},
            {QStringLiteral("errorText"), QString{}}};
}

QVariantMap previewPresentationEntry(const FileEntry &entry)
{
    return {{QStringLiteral("name"), boundedText(entry.name)},
            {QStringLiteral("path"), boundedText(entry.path)},
            {QStringLiteral("isDirectory"), entry.isDirectory},
            {QStringLiteral("suffix"), boundedText(entry.suffix)},
            {QStringLiteral("iconName"), entry.iconName.isEmpty()
                                                   ? (entry.isDirectory ? QStringLiteral("folder.svg")
                                                                        : QStringLiteral("document.svg"))
                                                   : boundedText(entry.iconName)},
            {QStringLiteral("mimeType"), boundedText(entry.mimeType)},
            {QStringLiteral("isImage"), entry.isImage},
            {QStringLiteral("hasThumbnail"), entry.hasThumbnail},
            {QStringLiteral("thumbnailIdentity"), QString{}}};
}
}

FolderPreviewController::FolderPreviewController(QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
    m_pool.setExpiryTimeout(30000);
    m_warmPool.setMaxThreadCount(1);
    m_warmPool.setExpiryTimeout(30000);
    m_snapshot = unavailableSnapshot(0, {});
    m_snapshot[QStringLiteral("state")] = QStringLiteral("idle");
}

FolderPreviewController::~FolderPreviewController()
{
    ++m_generation;
    m_pool.clear();
    m_warmPool.clear();
    m_pool.waitForDone();
    m_warmPool.waitForDone();
}

QVariantMap FolderPreviewController::snapshot() const { return m_snapshot; }
bool FolderPreviewController::loading() const { return m_snapshot.value(QStringLiteral("state")) == QLatin1String("loading"); }

quint64 FolderPreviewController::request(const QString &path, bool showHidden, int maxEntries)
{
    return requestWithSort(path, showHidden, maxEntries, 0, int(Qt::AscendingOrder), false);
}

quint64 FolderPreviewController::requestWithSort(const QString &path, bool showHidden, int maxEntries,
                                                 int sortRole, int sortOrder, bool mixFilesAndFolders)
{
    if (loading()) ++m_cancellationCount;
    ++m_requestCount;
    emit statisticsChanged();
    const quint64 requestId = ++m_generation;
    maxEntries = qBound(1, maxEntries, MaxPresentationEntries);
    if (!isLocalPath(path) && !ProviderRequestsSupported) {
        publish(requestId, unavailableSnapshot(requestId, path));
        return requestId;
    }

    const bool local = isLocalPath(path);
    sortRole = qBound(0, sortRole, 5);
    const Qt::SortOrder normalizedSortOrder = sortOrder == int(Qt::DescendingOrder)
            ? Qt::DescendingOrder : Qt::AscendingOrder;
    const QString localPath = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    m_snapshot = {{QStringLiteral("requestId"), requestId},
                  {QStringLiteral("path"), boundedText(localPath)},
                  {QStringLiteral("state"), QStringLiteral("loading")},
                  {QStringLiteral("entries"), QVariantList{}},
                  {QStringLiteral("hasMore"), false},
                  {QStringLiteral("displayedCount"), 0},
                  {QStringLiteral("errorText"), QString{}}};
    emit snapshotChanged();

    QPointer<FolderPreviewController> self(this);
    m_pool.start([self, requestId, path, local, localPath, showHidden, maxEntries,
                  sortRole, normalizedSortOrder, mixFilesAndFolders]() {
        QVariantList entries;
        bool hasMore = false;
        QString state;
        QString errorText;
        if (!local) {
#ifndef FM_FOLDER_PREVIEW_CONTROLLER_TEST
            const std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
            if (!provider) {
                state = QStringLiteral("unavailable");
            } else {
                const auto cancelled = [self, requestId]() {
                    return !self || self->m_generation.load() != requestId;
                };
                const BoundedFolderPreviewResult result = provider->boundedFolderPreview(
                    path, showHidden, MaxScannedEntries, cancelled);
                if (cancelled()) return;
                hasMore = result.hasMore;
                if (result.status == BoundedFolderPreviewResult::Status::Unsupported) {
                    state = QStringLiteral("unavailable");
                } else if (result.status == BoundedFolderPreviewResult::Status::Error) {
                    state = QStringLiteral("error");
                    errorText = QStringLiteral("Folder preview is unavailable");
                } else {
                    QList<FileEntry> sortedEntries = result.entries;
                    std::stable_sort(sortedEntries.begin(), sortedEntries.end(),
                                     [mixFilesAndFolders, sortRole, normalizedSortOrder](const FileEntry &a, const FileEntry &b) {
                        return FileEntrySortPolicy::lessThan(a, b, mixFilesAndFolders,
                                                             sortRole, normalizedSortOrder);
                    });
                    hasMore = result.hasMore || sortedEntries.size() > maxEntries;
                    for (const FileEntry &entry : std::as_const(sortedEntries)) {
                        if (entries.size() >= maxEntries) break;
                        entries.push_back(previewPresentationEntry(entry));
                    }
                    state = entries.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
                }
            }
#endif
            QVariantMap result{{QStringLiteral("requestId"), requestId},
                               {QStringLiteral("path"), boundedText(path)},
                               {QStringLiteral("state"), state},
                               {QStringLiteral("entries"), entries},
                               {QStringLiteral("hasMore"), hasMore},
                               {QStringLiteral("displayedCount"), entries.size()},
                               {QStringLiteral("errorText"), errorText}};
            if (!self) return;
            QMetaObject::invokeMethod(self, [self, requestId, result]() {
                if (self) self->publish(requestId, result);
            }, Qt::QueuedConnection);
            if ((hasMore || state == QLatin1String("unavailable")) && self) {
                QMetaObject::invokeMethod(self, [self, requestId, path, showHidden, maxEntries,
                                                 sortRole, normalizedSortOrder, mixFilesAndFolders]() {
                    if (self) self->startRemoteWarmup(requestId, path, showHidden, maxEntries,
                                                      sortRole, normalizedSortOrder, mixFilesAndFolders);
                }, Qt::QueuedConnection);
            }
            return;
        }

        const QFileInfo rootInfo(localPath);
        if (!rootInfo.exists() || !rootInfo.isDir() || !rootInfo.isReadable()) {
            state = QStringLiteral("error");
            errorText = QStringLiteral("Folder is not available");
        } else {
            const QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot
                                          | (showHidden ? QDir::Hidden : QDir::Filter(0));
            QDirIterator iterator(localPath, filters, QDirIterator::NoIteratorFlags);
            QMimeDatabase mimeDatabase;
            QList<FileEntry> scannedEntries;
            while (iterator.hasNext()) {
                if (!self || self->m_generation.load() != requestId) return;
                iterator.next();
                if (scannedEntries.size() >= MaxScannedEntries) {
                    hasMore = true;
                    break;
                }
                const QFileInfo info = iterator.fileInfo();
                const bool directory = info.isDir();
                const QString suffix = boundedText(info.suffix().toLower());
                const QString mimeType = directory ? QStringLiteral("inode/directory")
                                                   : mimeDatabase.mimeTypeForFile(info, QMimeDatabase::MatchExtension).name();
                const bool image = mimeType.startsWith(QLatin1String("image/"));
                FileEntry entry;
                entry.name = info.fileName();
                entry.path = info.absoluteFilePath();
                entry.suffix = suffix;
                entry.size = info.size();
                entry.modified = info.lastModified();
                entry.created = info.birthTime();
                entry.isDirectory = directory;
                entry.mimeType = mimeType;
                entry.isImage = image;
                entry.hasThumbnail = image;
                entry.iconName = directory ? QStringLiteral("folder.svg")
                                           : (image ? QStringLiteral("image.svg") : QStringLiteral("document.svg"));
                scannedEntries.push_back(std::move(entry));
            }
            std::stable_sort(scannedEntries.begin(), scannedEntries.end(),
                             [mixFilesAndFolders, sortRole, normalizedSortOrder](const FileEntry &a, const FileEntry &b) {
                return FileEntrySortPolicy::lessThan(a, b, mixFilesAndFolders,
                                                     sortRole, normalizedSortOrder);
            });
            hasMore = hasMore || scannedEntries.size() > maxEntries;
            for (const FileEntry &entry : std::as_const(scannedEntries)) {
                if (entries.size() >= maxEntries) break;
                entries.push_back(previewPresentationEntry(entry));
            }
            state = scannedEntries.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
        }

        QVariantMap result{{QStringLiteral("requestId"), requestId},
                           {QStringLiteral("path"), boundedText(localPath)},
                           {QStringLiteral("state"), state},
                           {QStringLiteral("entries"), entries},
                           {QStringLiteral("hasMore"), hasMore},
                           {QStringLiteral("displayedCount"), entries.size()},
                           {QStringLiteral("errorText"), errorText}};
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, requestId, result]() {
            if (self) self->publish(requestId, result);
        }, Qt::QueuedConnection);
    });
    return requestId;
}

void FolderPreviewController::cancel()
{
    if (loading()) {
        ++m_cancellationCount;
        emit statisticsChanged();
    }
    ++m_generation;
    m_pool.clear();
    m_warmPool.clear();
    m_snapshot = unavailableSnapshot(m_generation.load(), {});
    m_snapshot[QStringLiteral("state")] = QStringLiteral("idle");
    emit snapshotChanged();
}

void FolderPreviewController::startRemoteWarmup(quint64 requestId, const QString &path,
                                                bool showHidden, int maxEntries, int sortRole,
                                                Qt::SortOrder sortOrder, bool mixFilesAndFolders)
{
#ifndef FM_FOLDER_PREVIEW_CONTROLLER_TEST
    if (m_generation.load() != requestId) return;
    QPointer<FolderPreviewController> self(this);
    m_warmPool.start([self, requestId, path, showHidden, maxEntries,
                      sortRole, sortOrder, mixFilesAndFolders]() {
        const std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
        if (!provider) return;
        const auto cancelled = [self, requestId]() {
            return !self || self->m_generation.load() != requestId;
        };
        const bool owner = FolderPreviewWarmupRegistry::tryAcquire(path);
        if (owner) {
            provider->warmFolderPreviewCache(path, MaxScannedEntries, cancelled);
            FolderPreviewWarmupRegistry::release(path);
        } else {
            while (FolderPreviewWarmupRegistry::isRunning(path) && !cancelled()) QThread::msleep(100);
        }
        if (cancelled()) return;
        const BoundedFolderPreviewResult warmed = provider->boundedFolderPreview(
            path, showHidden, MaxScannedEntries, cancelled);
        if (cancelled() || warmed.status != BoundedFolderPreviewResult::Status::Ready) return;
        QList<FileEntry> sortedEntries = warmed.entries;
        std::stable_sort(sortedEntries.begin(), sortedEntries.end(),
                         [mixFilesAndFolders, sortRole, sortOrder](const FileEntry &a, const FileEntry &b) {
            return FileEntrySortPolicy::lessThan(a, b, mixFilesAndFolders, sortRole, sortOrder);
        });
        QVariantList entries;
        for (const FileEntry &entry : std::as_const(sortedEntries)) {
            if (entries.size() >= maxEntries) break;
            entries.push_back(previewPresentationEntry(entry));
        }
        const bool hasMore = warmed.hasMore || sortedEntries.size() > maxEntries;
        QVariantMap snapshot{{QStringLiteral("requestId"), requestId},
                             {QStringLiteral("path"), boundedText(path)},
                             {QStringLiteral("state"), entries.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready")},
                             {QStringLiteral("entries"), entries},
                             {QStringLiteral("hasMore"), hasMore},
                             {QStringLiteral("displayedCount"), entries.size()},
                             {QStringLiteral("errorText"), QString{}}};
        QMetaObject::invokeMethod(self, [self, requestId, snapshot]() {
            if (self) self->publish(requestId, snapshot);
        }, Qt::QueuedConnection);
    });
#else
    Q_UNUSED(requestId)
    Q_UNUSED(path)
    Q_UNUSED(showHidden)
    Q_UNUSED(maxEntries)
    Q_UNUSED(sortRole)
    Q_UNUSED(sortOrder)
    Q_UNUSED(mixFilesAndFolders)
#endif
}

void FolderPreviewController::publish(quint64 requestId, const QVariantMap &snapshot)
{
    if (m_generation.load() != requestId) return;
    m_snapshot = snapshot;
    if (snapshot.value(QStringLiteral("state")) == QLatin1String("error")) {
        ++m_failureCount;
        emit statisticsChanged();
    }
    emit snapshotChanged();
}
