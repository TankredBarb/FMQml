#pragma once

#include <QAbstractListModel>
#include <QStringList>

struct PlaceItem {
    QString name;
    QString path;
    QString icon;
    bool isDrive = false;
};

class PlacesModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IconRole,
        IsDriveRole
    };

    explicit PlacesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

private:
    QList<PlaceItem> m_items;
};
