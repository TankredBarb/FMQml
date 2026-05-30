#pragma once

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

struct FavoritePinnedEntry {
    QString id;
    QString targetPath;
    QString label;
    QString createdAt;
    QString lastUsedAt;
    QStringList tags;
    int order = 0;
};

struct FavoriteUsageEntry {
    QString targetPath;
    QString label;
    QString lastVisitedAt;
    int visitCount = 0;
    double score = 0.0;
};

class FavoritesStore {
public:
    FavoritesStore();

    const QList<FavoritePinnedEntry> &pinnedEntries() const;
    const QList<FavoriteUsageEntry> &usageEntries() const;
    bool load();
    bool save() const;

    bool pinPath(const QString &path);
    bool unpinPath(const QString &path);
    bool movePinnedPath(const QString &path, int offset);
    bool setPinnedLabel(const QString &path, const QString &label);
    bool setPinnedTags(const QString &path, const QStringList &tags);
    QStringList tagsForPath(const QString &path) const;
    bool isPinned(const QString &path) const;
    bool recordVisit(const QString &path);
    bool forgetUsagePath(const QString &path);
    bool clearUsage();

    static QString normalizedPathKey(const QString &path);

private:
    QString storageFilePath() const;
    QString nextId() const;

    QList<FavoritePinnedEntry> m_pinnedEntries;
    QList<FavoriteUsageEntry> m_usageEntries;
};
