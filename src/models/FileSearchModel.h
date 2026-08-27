#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>

struct FileSearchResult {
    QString path;
    QString name;
    QString parentPath;
    qint64 size = 0;
    QDateTime modified;
    bool isDirectory = false;
    QString matchKind;
    int lineNumber = 0;
    QString lineText;
    int lineMatchStart = -1;
    int lineMatchLength = 0;
    int nameMatchStart = -1;
    int nameMatchLength = 0;
    int relevanceScore = 0;
    int discoveryOrder = 0;
};

Q_DECLARE_METATYPE(FileSearchResult)
Q_DECLARE_METATYPE(QList<FileSearchResult>)

class FileSearchModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role {
        PathRole = Qt::UserRole + 1,
        NameRole,
        ParentPathRole,
        DisplayPathRole,
        DisplayParentPathRole,
        SizeRole,
        SizeTextRole,
        ModifiedTextRole,
        IsDirectoryRole,
        MatchKindRole,
        LineNumberRole,
        LineTextRole,
        LineMatchStartRole,
        LineMatchLengthRole,
        NameMatchStartRole,
        NameMatchLengthRole,
        RelevanceScoreRole
    };
    Q_ENUM(Role)

    enum SortMode { RelevanceSort = 0, NameSort, PathSort, SizeSort, ModifiedSort };
    Q_ENUM(SortMode)

    explicit FileSearchModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirectoryAt(int row) const;
    Q_INVOKABLE int indexOfResult(const QString &path, const QString &matchKind, int lineNumber) const;
    void sort(int mode);
    void appendResults(const QList<FileSearchResult> &results);
    void clear();

private:
    QList<FileSearchResult> m_results;

signals:
    void countChanged();
};
