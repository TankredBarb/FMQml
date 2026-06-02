#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>

#include "../core/FavoritesStore.h"

class FavoritesModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SectionRole = Qt::UserRole + 1,
        IdRole,
        NameRole,
        TargetPathRole,
        DisplayPathRole,
        IconRole,
        SuffixRole,
        IsDirectoryRole,
        ExistsRole,
        IsImageRole,
        HasThumbnailRole,
        TagsRole,
        LastUsedAtRole,
        UsageScoreRole,
        UsageProgressRole,
        VisitCountRole,
        IsPinnedRole,
        HasCustomLabelRole
    };

    explicit FavoritesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(const QList<FavoritePinnedEntry> &pinnedEntries,
                    const QList<FavoriteUsageEntry> &frequentEntries);

private:
    struct PathInfo {
        bool loaded = false;
        bool exists = true;
        bool isDirectory = false;
        QString suffix;
    };

    void refreshPathInfoAsync(const QStringList &paths, int generation);

    QList<FavoritePinnedEntry> m_pinnedEntries;
    QList<FavoriteUsageEntry> m_frequentEntries;
    QHash<QString, PathInfo> m_pathInfo;
    int m_maxVisitCount = 0;
    int m_pathInfoGeneration = 0;
};
