#pragma once

#include <QAbstractListModel>
#include <QVariantList>
#include <QVariantMap>

class AudioTagEditModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int dirtyCount READ dirtyCount NOTIFY dirtyCountChanged)

public:
    enum Role { RecordRole = Qt::UserRole + 1, PathRole, NameRole, OkRole, DirtyRole, ErrorRole };

    explicit AudioTagEditModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRecords(const QVariantList &records);
    QVariantMap record(int index) const;
    bool updateField(int index, const QString &field, const QVariant &value);
    bool setCover(int index, const QString &coverPath, const QString &previewSource, bool removeCover);
    bool clearTags(int index);
    int applyCoverToAll(int sourceIndex);
    int applyLookupFields(int index, const QVariantMap &fields);
    QVariantList dirtyRecords(int onlyIndex = -1) const;
    void reconcileApplyResults(const QVariantList &results);
    bool hasPendingCover() const;
    int dirtyCount() const;

signals:
    void countChanged();
    void dirtyCountChanged();
    void recordChanged(int index);

private:
    struct Row { QVariantMap current; QVariantMap original; };
    static bool isDirty(const Row &row);
    void emitRowChanged(int index, int previousDirtyCount);

    QList<Row> m_rows;
};
