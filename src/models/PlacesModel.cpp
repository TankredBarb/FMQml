#include "PlacesModel.h"

#include <QDir>
#include <QStandardPaths>
#include <QStorageInfo>

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    refresh();
}

int PlacesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return m_items.size();
}

QVariant PlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }

    const PlaceItem &item = m_items.at(index.row());
    switch (role) {
    case NameRole: return item.name;
    case PathRole: return item.path;
    case IconRole: return item.icon;
    case IsDriveRole: return item.isDrive;
    default: return {};
    }
}

QHash<int, QByteArray> PlacesModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {IconRole, "icon"},
        {IsDriveRole, "isDrive"}
    };
}

void PlacesModel::refresh()
{
    beginResetModel();
    m_items.clear();

    // Standard Places
    struct PathInfo {
        QStandardPaths::StandardLocation loc;
        QString name;
        QString icon;
    };

    const QList<PathInfo> standard = {
        {QStandardPaths::HomeLocation, "Home", "home"},
        {QStandardPaths::DesktopLocation, "Desktop", "desktop"},
        {QStandardPaths::DownloadLocation, "Downloads", "download"},
        {QStandardPaths::DocumentsLocation, "Documents", "document"},
        {QStandardPaths::PicturesLocation, "Pictures", "image"},
        {QStandardPaths::MusicLocation, "Music", "music"},
        {QStandardPaths::MoviesLocation, "Videos", "video"}
    };

    for (const auto &info : standard) {
        const QString path = QStandardPaths::writableLocation(info.loc);
        if (!path.isEmpty() && QDir(path).exists()) {
            m_items.append({info.name, QDir(path).absolutePath(), info.icon, false});
        }
    }

    // System Drives
    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (storage.isValid() && storage.isReady()) {
            QString name = storage.displayName();
            if (name.isEmpty()) name = storage.rootPath();
            m_items.append({name, storage.rootPath(), "drive", true});
        }
    }

    endResetModel();
}
