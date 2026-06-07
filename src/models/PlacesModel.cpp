#include "PlacesModel.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QStandardPaths>
#include <QStorageInfo>

#include "../core/DriveUtils.h"
#include "../core/FileProviderFactory.h"
#include "../core/IsoMountManager.h"

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    refresh();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000);
    connect(m_refreshTimer, &QTimer::timeout, this, &PlacesModel::refreshDriveInfo);
    m_refreshTimer->start();
}

void PlacesModel::setIsoMountManager(IsoMountManager *manager)
{
    if (m_isoMountManager == manager) {
        return;
    }
    if (m_isoMountManager) {
        disconnect(m_isoMountManager, nullptr, this, nullptr);
    }
    m_isoMountManager = manager;
    if (m_isoMountManager) {
        connect(m_isoMountManager, &IsoMountManager::mountsChanged, this, &PlacesModel::refresh);
    }
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
    case NameRole:         return item.name;
    case PathRole:         return item.path;
    case IconRole:         return item.icon;
    case IsDriveRole:      return item.isDrive;
    case TotalSpaceRole:   return item.totalBytes;
    case FreeSpaceRole:    return item.freeBytes;
    case UsedSpaceRole:    return item.totalBytes - item.freeBytes;
    case UsagePercentRole: return item.totalBytes > 0
                                  ? static_cast<double>(item.totalBytes - item.freeBytes) / static_cast<double>(item.totalBytes)
                                  : 0.0;
    case FileSystemRole:   return item.fileSystem;
    case DriveTypeRole:    return item.driveType;
    case IsReadyRole:      return item.isReady;
    case IsCriticalRole:   return item.isCritical;
    case IsVirtualDriveRole: return item.isVirtualDrive;
    case CanEjectRole:     return item.canEject;
    case SourcePathRole:   return item.sourcePath;
    case MountIdRole:      return item.mountId;
    default:               return {};
    }
}

QHash<int, QByteArray> PlacesModel::roleNames() const
{
    return {
        {NameRole,         "name"},
        {PathRole,         "path"},
        {IconRole,         "icon"},
        {IsDriveRole,      "isDrive"},
        {TotalSpaceRole,   "totalSpace"},
        {FreeSpaceRole,    "freeSpace"},
        {UsedSpaceRole,    "usedSpace"},
        {UsagePercentRole, "usagePercent"},
        {FileSystemRole,   "fileSystem"},
        {DriveTypeRole,    "driveType"},
        {IsReadyRole,      "isReady"},
        {IsCriticalRole,   "isCritical"},
        {IsVirtualDriveRole, "isVirtualDrive"},
        {CanEjectRole,     "canEject"},
        {SourcePathRole,   "sourcePath"},
        {MountIdRole,      "mountId"},
    };
}

// Fills storage info fields of a PlaceItem from QStorageInfo.
static void fillStorageInfo(PlaceItem &item, const QStorageInfo &storage)
{
    item.isReady     = storage.isReady();
    item.totalBytes  = storage.bytesTotal();
    item.freeBytes   = storage.bytesFree();
    item.fileSystem  = QString::fromLatin1(storage.fileSystemType());
    item.driveType   = DriveUtils::detectDriveType(storage);
    item.isCritical  = item.totalBytes > 0
                       && (static_cast<double>(item.freeBytes) / static_cast<double>(item.totalBytes)) < 0.10;
}

static QString normalizedRootPath(const QString &rootPath)
{
    QString path = QDir::fromNativeSeparators(rootPath).trimmed();
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':')) {
        path = path.left(2).toUpper() + QLatin1Char('/');
    }
    return path;
}

static void applyIsoMountInfo(PlaceItem &item, const IsoMountManager::Mount &mount)
{
    if (mount.rootPath.isEmpty()) {
        return;
    }
    item.name = QFileInfo(mount.imagePath).completeBaseName();
    if (item.name.isEmpty()) {
        item.name = QFileInfo(mount.imagePath).fileName();
    }
    if (!mount.letter.isNull()) {
        item.name += QStringLiteral(" (%1:)").arg(mount.letter);
    }
    item.icon = QStringLiteral("drive");
    item.isDrive = true;
    item.isReady = true;
    item.isVirtualDrive = true;
    item.canEject = true;
    item.sourcePath = mount.imagePath;
    item.mountId = mount.rootPath;
    item.driveType = QStringLiteral("iso");
    if (item.fileSystem.isEmpty()) {
        item.fileSystem = QStringLiteral("ISO");
    }
}

static bool googleDriveProviderAvailable()
{
    return FileProviderFactory::hasPluginProviderForPath(QStringLiteral("gdrive://"));
}

void PlacesModel::refresh()
{
    beginResetModel();
    m_items.clear();

    m_items.append({QStringLiteral("Favorites"), QStringLiteral("favorites://"), QStringLiteral("star"), false});
    if (googleDriveProviderAvailable()) {
        m_items.append({QStringLiteral("Google Drive"), QStringLiteral("gdrive://"), QStringLiteral("gdrive"), false});
    }

    // Standard Places
    struct PathInfo {
        QStandardPaths::StandardLocation loc;
        QString name;
        QString icon;
    };

    const QList<PathInfo> standard = {
        {QStandardPaths::HomeLocation,      QStringLiteral("Home"),      QStringLiteral("home")},
        {QStandardPaths::DesktopLocation,   QStringLiteral("Desktop"),   QStringLiteral("desktop")},
        {QStandardPaths::DownloadLocation,  QStringLiteral("Downloads"), QStringLiteral("download")},
        {QStandardPaths::DocumentsLocation, QStringLiteral("Documents"), QStringLiteral("document")},
        {QStandardPaths::PicturesLocation,  QStringLiteral("Pictures"),  QStringLiteral("image")},
        {QStandardPaths::MusicLocation,     QStringLiteral("Music"),     QStringLiteral("music")},
        {QStandardPaths::MoviesLocation,    QStringLiteral("Videos"),    QStringLiteral("video")}
    };

    for (const auto &info : standard) {
        const QString path = QStandardPaths::writableLocation(info.loc);
        if (!path.isEmpty() && QDir(path).exists()) {
            m_items.append({info.name, QDir(path).absolutePath(), info.icon, false});
        }
    }

    QHash<QString, IsoMountManager::Mount> isoMountsByRoot;
    if (m_isoMountManager) {
        for (const IsoMountManager::Mount &mount : m_isoMountManager->mounts()) {
            isoMountsByRoot.insert(normalizedRootPath(mount.rootPath), mount);
        }
    }

    // System Drives
    for (QStorageInfo storage : QStorageInfo::mountedVolumes()) {
        storage.refresh();
        if (storage.isValid()) {
            PlaceItem item;
            item.name    = DriveUtils::rootDisplayName(storage.rootPath());
            if (item.name.isEmpty()) {
                item.name = storage.displayName().isEmpty() ? storage.rootPath() : storage.displayName();
            }
            item.path    = storage.rootPath();
            item.icon    = QStringLiteral("drive");
            item.isDrive = true;
            fillStorageInfo(item, storage);
            const QString root = normalizedRootPath(item.path);
            if (isoMountsByRoot.contains(root)) {
                applyIsoMountInfo(item, isoMountsByRoot.take(root));
            }
            m_items.append(item);
        }
    }

    for (auto it = isoMountsByRoot.cbegin(); it != isoMountsByRoot.cend(); ++it) {
        const IsoMountManager::Mount &mount = it.value();
        PlaceItem item;
        item.path = mount.rootPath;
        QStorageInfo storage(item.path);
        storage.refresh();
        if (storage.isValid()) {
            fillStorageInfo(item, storage);
        }
        applyIsoMountInfo(item, mount);
        m_items.append(item);
    }

    endResetModel();
}

void PlacesModel::refreshDriveInfo()
{
    // Update only storage data for drive items, no full model reset.
    for (int i = 0; i < m_items.size(); ++i) {
        PlaceItem &item = m_items[i];
        if (!item.isDrive) continue;

        QStorageInfo storage(item.path);
        storage.refresh();
        if (!storage.isValid()) continue;

        const bool wasReady    = item.isReady;
        const qint64 oldTotal  = item.totalBytes;
        const qint64 oldFree   = item.freeBytes;
        const bool wasCritical = item.isCritical;

        fillStorageInfo(item, storage);
        if (item.isVirtualDrive) {
            item.driveType = QStringLiteral("iso");
            item.canEject = true;
        }

        // Notify QML if anything changed
        if (item.isReady != wasReady || item.totalBytes != oldTotal || item.freeBytes != oldFree || item.isCritical != wasCritical) {
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {
                TotalSpaceRole,
                FreeSpaceRole,
                UsedSpaceRole,
                UsagePercentRole,
                IsReadyRole,
                IsCriticalRole
            });

            // Emit low disk space warning once the drive goes critical
            if (item.isCritical && !wasCritical) {
                emit lowDiskSpaceWarning(item.name, item.freeBytes);
            }
        }
    }
}
