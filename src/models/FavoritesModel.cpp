#include "FavoritesModel.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>

namespace {
bool hasAnySuffix(const QString &suffix, const QSet<QString> &suffixes)
{
    return suffixes.contains(suffix.toLower());
}

bool isImageSuffix(const QString &suffix)
{
    static const QSet<QString> imageSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp"),
        QStringLiteral("ico"), QStringLiteral("svg"), QStringLiteral("svgz"),
        QStringLiteral("avif"), QStringLiteral("heic"), QStringLiteral("tif"),
        QStringLiteral("tiff")
    };
    return hasAnySuffix(suffix, imageSuffixes);
}

bool hasThumbnailSuffix(const QString &suffix)
{
    static const QSet<QString> mediaSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp"),
        QStringLiteral("ico"), QStringLiteral("svg"), QStringLiteral("svgz"),
        QStringLiteral("avif"), QStringLiteral("heic"), QStringLiteral("tif"),
        QStringLiteral("tiff"), QStringLiteral("mp3"), QStringLiteral("flac"),
        QStringLiteral("ogg"), QStringLiteral("m4a"), QStringLiteral("m4b"),
        QStringLiteral("wav"), QStringLiteral("wma"), QStringLiteral("mp4"),
        QStringLiteral("avi"), QStringLiteral("mkv"), QStringLiteral("mov"),
        QStringLiteral("wmv"), QStringLiteral("pdf"), QStringLiteral("ttf"),
        QStringLiteral("otf"), QStringLiteral("woff"), QStringLiteral("woff2")
    };
    return hasAnySuffix(suffix, mediaSuffixes);
}

QString defaultLabelForPath(const QString &path)
{
    QString normalized = QDir::fromNativeSeparators(path);
    while (normalized.size() > 1 && normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }

    const int slash = normalized.lastIndexOf(QLatin1Char('/'));
    QString name = slash >= 0 ? normalized.mid(slash + 1) : normalized;
    if (!name.isEmpty()) {
        return name;
    }

    return normalized.isEmpty() ? path : normalized;
}

QString suffixForPath(const QString &path)
{
    const QString name = defaultLabelForPath(path);
    const int dot = name.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0 || dot == name.size() - 1) {
        return {};
    }
    return name.mid(dot + 1).toLower();
}

bool looksLikeDirectoryPath(const QString &path)
{
    const QString normalized = QDir::fromNativeSeparators(path);
    return normalized.endsWith(QLatin1Char('/')) || suffixForPath(path).isEmpty();
}
}

FavoritesModel::FavoritesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int FavoritesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_pinnedEntries.size() + m_frequentEntries.size();
}

QVariant FavoritesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const bool pinned = index.row() < m_pinnedEntries.size();
    const FavoritePinnedEntry pinnedEntry = pinned ? m_pinnedEntries.at(index.row()) : FavoritePinnedEntry{};
    const FavoriteUsageEntry usageEntry = pinned ? FavoriteUsageEntry{} : m_frequentEntries.at(index.row() - m_pinnedEntries.size());
    const QString targetPath = pinned ? pinnedEntry.targetPath : usageEntry.targetPath;
    const QString label = pinned ? pinnedEntry.label : usageEntry.label;
    const QString id = pinned
        ? pinnedEntry.id
        : QStringLiteral("freq-%1").arg(FavoritesStore::normalizedPathKey(targetPath));
    const QString lastUsedAt = pinned ? pinnedEntry.lastUsedAt : usageEntry.lastVisitedAt;
    const double usageScore = pinned ? 0.0 : usageEntry.score;
    const double usageProgress = !pinned && m_maxVisitCount > 0 ? double(usageEntry.visitCount) / double(m_maxVisitCount) : 0.0;
    const PathInfo cachedInfo = m_pathInfo.value(targetPath);
    const QString suffix = cachedInfo.loaded ? cachedInfo.suffix : suffixForPath(targetPath);
    const bool exists = cachedInfo.loaded ? cachedInfo.exists : true;
    const bool isDirectory = cachedInfo.loaded ? cachedInfo.isDirectory : (!pinned || looksLikeDirectoryPath(targetPath));
    switch (role) {
    case SectionRole: return pinned ? QStringLiteral("Pinned") : QStringLiteral("Frequent");
    case IdRole: return id;
    case NameRole: return label;
    case TargetPathRole: return targetPath;
    case DisplayPathRole: return QDir::toNativeSeparators(targetPath);
    case IconRole: return isDirectory ? QStringLiteral("folder") : QStringLiteral("document");
    case SuffixRole: return suffix;
    case IsDirectoryRole: return isDirectory;
    case ExistsRole: return exists;
    case IsImageRole: return exists && !isDirectory && isImageSuffix(suffix);
    case HasThumbnailRole: return exists && !isDirectory && hasThumbnailSuffix(suffix);
    case TagsRole: return pinned ? pinnedEntry.tags : QStringList{};
    case LastUsedAtRole: return lastUsedAt;
    case UsageScoreRole: return usageScore;
    case UsageProgressRole: return qBound(0.0, usageProgress, 1.0);
    case VisitCountRole: return pinned ? 0 : usageEntry.visitCount;
    case IsPinnedRole: return pinned;
    case HasCustomLabelRole: return pinned && label != defaultLabelForPath(targetPath);
    default: return {};
    }
}

QHash<int, QByteArray> FavoritesModel::roleNames() const
{
    return {
        {SectionRole, "section"},
        {IdRole, "id"},
        {NameRole, "name"},
        {TargetPathRole, "targetPath"},
        {DisplayPathRole, "displayPath"},
        {IconRole, "icon"},
        {SuffixRole, "suffix"},
        {IsDirectoryRole, "isDirectory"},
        {ExistsRole, "exists"},
        {IsImageRole, "isImage"},
        {HasThumbnailRole, "hasThumbnail"},
        {TagsRole, "tags"},
        {LastUsedAtRole, "lastUsedAt"},
        {UsageScoreRole, "usageScore"},
        {UsageProgressRole, "usageProgress"},
        {VisitCountRole, "visitCount"},
        {IsPinnedRole, "isPinned"},
        {HasCustomLabelRole, "hasCustomLabel"},
    };
}

void FavoritesModel::setEntries(const QList<FavoritePinnedEntry> &pinnedEntries,
                                const QList<FavoriteUsageEntry> &frequentEntries)
{
    QStringList paths;
    QSet<QString> seenPaths;
    auto appendPath = [&paths, &seenPaths](const QString &path) {
        if (path.isEmpty() || seenPaths.contains(path)) {
            return;
        }
        seenPaths.insert(path);
        paths.append(path);
    };
    for (const FavoritePinnedEntry &entry : pinnedEntries) {
        appendPath(entry.targetPath);
    }
    for (const FavoriteUsageEntry &entry : frequentEntries) {
        appendPath(entry.targetPath);
    }

    const int generation = ++m_pathInfoGeneration;

    beginResetModel();
    m_pinnedEntries = pinnedEntries;
    m_frequentEntries = frequentEntries;
    m_pathInfo.clear();
    m_maxVisitCount = 0;
    for (const FavoriteUsageEntry &entry : m_frequentEntries) {
        m_maxVisitCount = std::max(m_maxVisitCount, entry.visitCount);
    }
    endResetModel();

    refreshPathInfoAsync(paths, generation);
}

void FavoritesModel::refreshPathInfoAsync(const QStringList &paths, int generation)
{
    if (paths.isEmpty()) {
        return;
    }

    QPointer<FavoritesModel> self(this);
    (void)QtConcurrent::run([self, paths, generation]() {
        QHash<QString, FavoritesModel::PathInfo> infoByPath;
        for (const QString &path : paths) {
            QFileInfo fileInfo(path);
            FavoritesModel::PathInfo info;
            info.loaded = true;
            info.exists = fileInfo.exists();
            info.isDirectory = info.exists && fileInfo.isDir();
            info.suffix = fileInfo.suffix().toLower();
            infoByPath.insert(path, info);
        }

        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, generation, infoByPath = std::move(infoByPath)]() mutable {
            if (!self || generation != self->m_pathInfoGeneration) {
                return;
            }

            self->m_pathInfo = std::move(infoByPath);
            const int rows = self->rowCount();
            if (rows <= 0) {
                return;
            }
            emit self->dataChanged(self->index(0), self->index(rows - 1), {
                FavoritesModel::IconRole,
                FavoritesModel::SuffixRole,
                FavoritesModel::IsDirectoryRole,
                FavoritesModel::ExistsRole,
                FavoritesModel::IsImageRole,
                FavoritesModel::HasThumbnailRole,
                FavoritesModel::HasCustomLabelRole
            });
        }, Qt::QueuedConnection);
    });
}
