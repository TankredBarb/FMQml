#include "PlacesModel.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QMetaObject>
#include <QPointer>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrentRun>

#include "../core/DriveUtils.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileProviderPluginRegistry.h"
#include "../core/IsoMountManager.h"
#include "../core/VolumeMonitor.h"

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    refresh();

    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000);
    connect(m_refreshTimer, &QTimer::timeout, this, [this]() {
        refreshDriveInfo();
        refreshGoogleDriveAccountInfo();
        refreshProviderPlacesAsync();
    });
    m_refreshTimer->start();
    refreshProviderPlacesAsync();
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

void PlacesModel::setVolumeMonitor(VolumeMonitor *monitor)
{
    if (m_volumeMonitor == monitor) {
        return;
    }
    if (m_volumeMonitor) {
        disconnect(m_volumeMonitor, nullptr, this, nullptr);
    }
    m_volumeMonitor = monitor;
    if (m_volumeMonitor) {
        connect(m_volumeMonitor, &VolumeMonitor::volumesChanged, this, &PlacesModel::refresh);
        connect(m_volumeMonitor, &VolumeMonitor::volumeChanged, this, [this]() {
            refreshDriveInfo();
        });
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
    case SectionRole:      return item.section;
    case SubtitleRole:     return item.subtitle;
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
        {SectionRole,      "section"},
        {SubtitleRole,     "subtitle"},
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

static void fillStorageInfo(PlaceItem &item, const VolumeInfo &volume)
{
    item.isReady = volume.isReady;
    item.totalBytes = volume.totalBytes;
    item.freeBytes = volume.freeBytes;
    item.fileSystem = volume.fileSystem;
    item.driveType = volume.driveType;
    item.isCritical = volume.isCritical;
    item.canEject = volume.isEjectable;
}

static QString normalizedRootPath(const QString &rootPath)
{
    QString path = QDir::fromNativeSeparators(rootPath).trimmed();
    if (path.size() >= 2 && path.at(1) == QLatin1Char(':')) {
        path = path.left(2).toUpper() + QLatin1Char('/');
    }
    return path;
}

static bool displayNamesEqual(const QString &lhs, const QString &rhs)
{
#ifdef Q_OS_WIN
    return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
#else
    return lhs == rhs;
#endif
}

static QString placesIsoMountDisplayName(const IsoMountManager::Mount &mount)
{
    QString name = QFileInfo(mount.imagePath).completeBaseName();
    if (name.isEmpty()) {
        name = QFileInfo(mount.imagePath).fileName();
    }
    if (mount.letter.isNull()) {
        return name;
    }

    const QString rootName = QStringLiteral("%1:").arg(mount.letter.toUpper());
    return name.isEmpty() ? rootName : QStringLiteral("%1 %2").arg(rootName, name);
}

static void applyIsoMountInfo(PlaceItem &item, const IsoMountManager::Mount &mount)
{
    if (mount.rootPath.isEmpty()) {
        return;
    }
    const QString rootName = DriveUtils::rootDisplayName(item.path);
    if (item.name.isEmpty()
        || displayNamesEqual(item.name, rootName)
        || displayNamesEqual(item.name, item.path)) {
        item.name = placesIsoMountDisplayName(mount);
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

static QString standardPlacePath(QStandardPaths::StandardLocation location)
{
    if (location == QStandardPaths::HomeLocation) {
        return QDir::homePath();
    }
    return QStandardPaths::writableLocation(location);
}

static QString googleDriveAccountLabel()
{
    const QVariantMap status = FileProviderPluginRegistry::instance().triggerAction(
        QStringLiteral("fm.gdrive-provider::authStatus"),
        {});
    if (!status.value(QStringLiteral("signedIn")).toBool()) {
        return {};
    }
    return status.value(QStringLiteral("accountLabel")).toString().trimmed();
}

static PlaceItem placeFromProviderPlace(const ProviderPlaceItem &providerPlace)
{
    PlaceItem item;
    item.name = providerPlace.name;
    item.path = providerPlace.path;
    item.icon = providerPlace.icon.isEmpty() ? QStringLiteral("drive") : providerPlace.icon;
    item.section = providerPlace.section.isEmpty() ? QStringLiteral("place") : providerPlace.section;
    item.subtitle = providerPlace.subtitle;
    item.isDrive = false;
    item.isReady = providerPlace.isReady;
    item.canEject = providerPlace.canEject;
    item.driveType = providerPlace.driveType;
    return item;
}

static QHash<QString, ProviderPlaceItem> providerPlacesByPath(const QList<ProviderPlaceItem> &places)
{
    QHash<QString, ProviderPlaceItem> result;
    for (const ProviderPlaceItem &place : places) {
        const QString key = place.path.trimmed();
        if (!key.isEmpty()) {
            result.insert(key, place);
        }
    }
    return result;
}

static QList<ProviderPlaceItem> removedProviderPlaces(const QList<ProviderPlaceItem> &previous,
                                                      const QList<ProviderPlaceItem> &next)
{
    const QHash<QString, ProviderPlaceItem> nextByPath = providerPlacesByPath(next);
    QList<ProviderPlaceItem> removed;
    for (const ProviderPlaceItem &place : previous) {
        const QString key = place.path.trimmed();
        if (!key.isEmpty() && !nextByPath.contains(key)) {
            removed.append(place);
        }
    }
    return removed;
}

void PlacesModel::refresh()
{
    beginResetModel();
    m_items.clear();

    PlaceItem favoritesItem;
    favoritesItem.name = QStringLiteral("Favorites");
    favoritesItem.path = QStringLiteral("favorites://");
    favoritesItem.icon = QStringLiteral("star");
    favoritesItem.section = QStringLiteral("place");
    m_items.append(favoritesItem);

    QList<PlaceItem> standardItems;
    if (googleDriveProviderAvailable()) {
        PlaceItem gdriveItem;
        gdriveItem.name = QStringLiteral("Google Drive");
        gdriveItem.path = QStringLiteral("gdrive://");
        gdriveItem.icon = QStringLiteral("gdrive");
        gdriveItem.section = QStringLiteral("place");
        gdriveItem.subtitle = googleDriveAccountLabel();
        standardItems.append(gdriveItem);
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
        const QString path = standardPlacePath(info.loc);
        if (!path.isEmpty() && QDir(path).exists()) {
            PlaceItem item;
            item.name = info.name;
            item.path = QDir(path).absolutePath();
            item.icon = info.icon;
            item.section = QStringLiteral("place");
            item.subtitle = QDir::toNativeSeparators(item.path);
            standardItems.append(item);
        }
    }

    QList<PlaceItem> driveItems;
    QHash<QString, IsoMountManager::Mount> isoMountsByRoot;
    if (m_isoMountManager) {
        for (const IsoMountManager::Mount &mount : m_isoMountManager->mounts()) {
            isoMountsByRoot.insert(normalizedRootPath(mount.rootPath), mount);
        }
    }

    if (m_volumeMonitor) {
        for (const VolumeInfo &volume : m_volumeMonitor->volumes()) {
            PlaceItem item;
            item.name    = volume.displayName;
            if (item.name.isEmpty()) {
                item.name = DriveUtils::rootDisplayName(volume.rootPath);
            }
            item.path    = volume.rootPath;
            item.icon    = QStringLiteral("drive");
            item.section = QStringLiteral("drive");
            item.isDrive = true;
            fillStorageInfo(item, volume);
            const QString root = normalizedRootPath(item.path);
            if (isoMountsByRoot.contains(root)) {
                applyIsoMountInfo(item, isoMountsByRoot.take(root));
            }
            driveItems.append(item);
        }
    } else {
        // System Drives
        for (QStorageInfo storage : QStorageInfo::mountedVolumes()) {
            storage.refresh();
            if (storage.isValid()) {
                PlaceItem item;
                item.name    = DriveUtils::volumeDisplayName(storage);
                if (item.name.isEmpty()) {
                    item.name = storage.rootPath();
                }
                item.path    = storage.rootPath();
                item.icon    = QStringLiteral("drive");
                item.section = QStringLiteral("drive");
                item.isDrive = true;
                fillStorageInfo(item, storage);
                const QString root = normalizedRootPath(item.path);
                if (isoMountsByRoot.contains(root)) {
                    applyIsoMountInfo(item, isoMountsByRoot.take(root));
                }
                driveItems.append(item);
            }
        }
    }

    for (auto it = isoMountsByRoot.cbegin(); it != isoMountsByRoot.cend(); ++it) {
        const IsoMountManager::Mount &mount = it.value();
        PlaceItem item;
        item.path = mount.rootPath;
        QStorageInfo storage(item.path);
        storage.refresh();
        if (storage.isValid()) {
            item.name = DriveUtils::volumeDisplayName(storage);
            fillStorageInfo(item, storage);
        }
        applyIsoMountInfo(item, mount);
        item.section = QStringLiteral("drive");
        driveItems.append(item);
    }

    QList<PlaceItem> portableItems;
    QList<PlaceItem> otherProviderItems;
    QStringList providerSignatureParts;
    providerSignatureParts.reserve(m_cachedProviderPlaces.size());
    for (const ProviderPlaceItem &place : m_cachedProviderPlaces) {
        PlaceItem item = placeFromProviderPlace(place);
        if (item.path.isEmpty() || item.name.isEmpty()) {
            continue;
        }
        providerSignatureParts.append(QStringList{
            item.name,
            item.path,
            item.icon,
            item.section,
            item.driveType,
            item.subtitle,
            item.isReady ? QStringLiteral("1") : QStringLiteral("0"),
            item.canEject ? QStringLiteral("1") : QStringLiteral("0"),
        }.join(QLatin1Char('\t')));
        if (item.section == QLatin1String("portable")) {
            portableItems.append(item);
        } else {
            otherProviderItems.append(item);
        }
    }
    m_providerPlacesSignature = providerSignatureParts.join(QLatin1Char('\n'));

    m_items.append(driveItems);
    m_items.append(portableItems);
    m_items.append(standardItems);
    m_items.append(otherProviderItems);

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

void PlacesModel::refreshGoogleDriveAccountInfo()
{
    const QString accountLabel = googleDriveAccountLabel();
    for (int i = 0; i < m_items.size(); ++i) {
        PlaceItem &item = m_items[i];
        if (item.path != QLatin1String("gdrive://") || item.subtitle == accountLabel) {
            continue;
        }
        item.subtitle = accountLabel;
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, {SubtitleRole});
        return;
    }
}

void PlacesModel::refreshProviderPlacesAsync()
{
    if (m_providerPlacesRefreshPending) {
        m_providerPlacesRefreshQueued = true;
        return;
    }

    m_providerPlacesRefreshPending = true;
    const int generation = ++m_providerPlacesRefreshGeneration;
    QPointer<PlacesModel> self(this);
    auto future = QtConcurrent::run([self, generation]() {
        QList<ProviderPlaceItem> places = FileProviderPluginRegistry::instance().providerPlaces();
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, generation, places = std::move(places)]() mutable {
            if (!self || generation != self->m_providerPlacesRefreshGeneration) {
                return;
            }

            self->m_providerPlacesRefreshPending = false;
            const bool refreshQueued = self->m_providerPlacesRefreshQueued;
            self->m_providerPlacesRefreshQueued = false;
            const QString signature = self->providerPlacesSignature(places);
            if (signature != self->m_providerPlacesSignature) {
                const QList<ProviderPlaceItem> removed = removedProviderPlaces(self->m_cachedProviderPlaces, places);
                self->m_cachedProviderPlaces = std::move(places);
                self->m_providerPlacesSignature = signature;
                self->refresh();
                for (const ProviderPlaceItem &place : removed) {
                    emit self->providerPlaceRemoved(place.path, place.name, place.section);
                }
            }
            if (refreshQueued) {
                self->refreshProviderPlacesAsync();
            }
        }, Qt::QueuedConnection);
    });
    Q_UNUSED(future)
}

QString PlacesModel::providerPlacesSignature(const QList<ProviderPlaceItem> &providerPlaces) const
{
    QStringList parts;
    parts.reserve(providerPlaces.size());
    for (const ProviderPlaceItem &place : providerPlaces) {
        PlaceItem item = placeFromProviderPlace(place);
        if (item.path.isEmpty() || item.name.isEmpty()) {
            continue;
        }
        parts.append(QStringList{
            item.name,
            item.path,
            item.icon,
            item.section,
            item.driveType,
            item.subtitle,
            item.isReady ? QStringLiteral("1") : QStringLiteral("0"),
            item.canEject ? QStringLiteral("1") : QStringLiteral("0"),
        }.join(QLatin1Char('\t')));
    }
    return parts.join(QLatin1Char('\n'));
}
