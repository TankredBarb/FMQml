#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>

struct DiskUsageEntry {
    QString path;
    QString name;
    qint64 size = 0;
    bool isDirectory = false;
    int fileCount = 0;
    int folderCount = 0;
};

Q_DECLARE_METATYPE(DiskUsageEntry)
Q_DECLARE_METATYPE(QList<DiskUsageEntry>)

class DiskUsageModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int sortKey READ sortKey NOTIFY sortChanged)
    Q_PROPERTY(bool sortAscending READ sortAscending NOTIFY sortChanged)

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        NameRole,
        SizeRole,
        SizeTextRole,
        SizeDetailTextRole,
        ExactSizeTextRole,
        PercentOfLargestRole,
        PercentOfRootRole,
        PercentOfRootTextRole,
        IsDirectoryRole,
        FileCountRole,
        FolderCountRole
    };
    Q_ENUM(Role)

    explicit DiskUsageModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    int sortKey() const;
    bool sortAscending() const;
    Q_INVOKABLE void setSort(int key, bool ascending);
    void setEntries(const QList<DiskUsageEntry> &entries, qint64 rootTotalBytes = 0);
    void clear();

private:
    void applySort();

    QList<DiskUsageEntry> m_entries;
    qint64 m_largestSize = 0;
    qint64 m_rootTotalBytes = 0;
    int m_sortKey = 0;
    bool m_sortAscending = false;

signals:
    void countChanged();
    void sortChanged();
};
