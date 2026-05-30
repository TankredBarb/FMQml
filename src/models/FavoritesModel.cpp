#include "FavoritesModel.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

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
    const QFileInfo info(path);
    QString name = info.fileName();
    if (!name.isEmpty()) {
        return name;
    }

    QString normalized = QDir::fromNativeSeparators(path);
    if (normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized.isEmpty() ? path : normalized;
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
    const QFileInfo info(targetPath);
    const QString suffix = info.suffix();
    switch (role) {
    case SectionRole: return pinned ? QStringLiteral("Pinned") : QStringLiteral("Frequent");
    case IdRole: return id;
    case NameRole: return label;
    case TargetPathRole: return targetPath;
    case DisplayPathRole: return QDir::toNativeSeparators(targetPath);
    case IconRole: return info.isDir() ? QStringLiteral("folder") : QStringLiteral("document");
    case SuffixRole: return suffix;
    case IsDirectoryRole: return info.isDir();
    case ExistsRole: return info.exists();
    case IsImageRole: return info.exists() && !info.isDir() && isImageSuffix(suffix);
    case HasThumbnailRole: return info.exists() && !info.isDir() && hasThumbnailSuffix(suffix);
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
    beginResetModel();
    m_pinnedEntries = pinnedEntries;
    m_frequentEntries = frequentEntries;
    m_maxVisitCount = 0;
    for (const FavoriteUsageEntry &entry : m_frequentEntries) {
        m_maxVisitCount = std::max(m_maxVisitCount, entry.visitCount);
    }
    endResetModel();
}
