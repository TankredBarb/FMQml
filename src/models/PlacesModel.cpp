#include "PlacesModel.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QLoggingCategory>
#include <QMetaObject>
#include <QElapsedTimer>
#include <QPointer>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QtConcurrent/QtConcurrentRun>

#include "../core/DriveUtils.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileProviderPluginRegistry.h"
#include "../core/IsoMountManager.h"
#include "../core/VolumeMonitor.h"

namespace {

Q_LOGGING_CATEGORY(placesTrace, "fm.places.trace")

bool placesTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIntValue("FM_PLACES_TRACE") > 0;
    return enabled;
}

void tracePlaces(const QString &message)
{
    if (placesTraceEnabled()) {
        static QElapsedTimer traceTimer = [] {
            QElapsedTimer timer;
            timer.start();
            return timer;
        }();
        qCInfo(placesTrace).noquote() << QStringLiteral("t=%1 ").arg(traceTimer.elapsed()) + message;
    }
}

QString placesRoleTraceName(int role)
{
    switch (role) {
    case PlacesModel::NameRole: return QStringLiteral("name");
    case PlacesModel::PathRole: return QStringLiteral("path");
    case PlacesModel::IconRole: return QStringLiteral("icon");
    case PlacesModel::IsDriveRole: return QStringLiteral("isDrive");
    case PlacesModel::TotalSpaceRole: return QStringLiteral("totalSpace");
    case PlacesModel::FreeSpaceRole: return QStringLiteral("freeSpace");
    case PlacesModel::UsedSpaceRole: return QStringLiteral("usedSpace");
    case PlacesModel::UsagePercentRole: return QStringLiteral("usagePercent");
    case PlacesModel::FileSystemRole: return QStringLiteral("fileSystem");
    case PlacesModel::DriveTypeRole: return QStringLiteral("driveType");
    case PlacesModel::IsReadyRole: return QStringLiteral("isReady");
    case PlacesModel::IsCriticalRole: return QStringLiteral("isCritical");
    case PlacesModel::IsVirtualDriveRole: return QStringLiteral("isVirtualDrive");
    case PlacesModel::CanEjectRole: return QStringLiteral("canEject");
    case PlacesModel::SourcePathRole: return QStringLiteral("sourcePath");
    case PlacesModel::MountIdRole: return QStringLiteral("mountId");
    case PlacesModel::SectionRole: return QStringLiteral("section");
    case PlacesModel::SubtitleRole: return QStringLiteral("subtitle");
    case PlacesModel::VisualSectionRole: return QStringLiteral("visualSection");
    case PlacesModel::ShowSectionHeaderRole: return QStringLiteral("showSectionHeader");
    default: return QString::number(role);
    }
}

void tracePlacesDataAccess(int role)
{
    if (!placesTraceEnabled()) {
        return;
    }

    static QElapsedTimer timer = [] {
        QElapsedTimer elapsed;
        elapsed.start();
        return elapsed;
    }();
    static QHash<int, int> roleHits;
    static int totalHits = 0;

    ++roleHits[role];
    ++totalHits;

    if (timer.elapsed() < 1000) {
        return;
    }

    QStringList parts;
    const auto names = roleHits.keys();
    for (int hitRole : names) {
        parts.append(QStringLiteral("%1=%2").arg(placesRoleTraceName(hitRole)).arg(roleHits.value(hitRole)));
    }
    parts.sort();
    tracePlaces(QStringLiteral("dataAccess total=%1 roles=%2")
                    .arg(totalHits)
                    .arg(parts.join(QLatin1Char(','))));
    roleHits.clear();
    totalHits = 0;
    timer.restart();
}

} // namespace

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

static QString visualSectionForPlace(const PlaceItem &item)
{
    if (item.isDrive || item.section == QLatin1String("drive")) {
        return QStringLiteral("drives");
    }
    if (item.section == QLatin1String("portable")) {
        return QStringLiteral("portable");
    }
    if (item.path == QLatin1String("favorites://") || item.icon == QLatin1String("star")) {
        return QStringLiteral("pinned");
    }
    if (item.section == QLatin1String("cloud")
            || item.path == QLatin1String("gdrive://") || item.icon == QLatin1String("gdrive")
            || item.path == QLatin1String("mega:///") || item.icon == QLatin1String("mega")) {
        return QStringLiteral("cloud");
    }
    return QStringLiteral("folders");
}

QVariant PlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_items.size()) {
        return {};
    }
    tracePlacesDataAccess(role);

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
    case VisualSectionRole:
        return visualSectionForPlace(item);
    case ShowSectionHeaderRole:
        return index.row() == 0 || visualSectionForPlace(m_items.at(index.row() - 1)) != visualSectionForPlace(item);
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
        {VisualSectionRole, "visualSection"},
        {ShowSectionHeaderRole, "showSectionHeader"},
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

static bool megaProviderAvailable()
{
    return FileProviderFactory::hasPluginProviderForPath(QStringLiteral("mega:///"));
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
    const QString email = status.value(QStringLiteral("accountEmail")).toString().trimmed();
    if (!email.isEmpty()) {
        return email;
    }
    return status.value(QStringLiteral("accountLabel")).toString().trimmed();
}

static QString megaAccountLabel()
{
    const QVariantMap status = FileProviderPluginRegistry::instance().triggerAction(
        QStringLiteral("mega::authStatus"),
        {});
    return status.value(QStringLiteral("accountEmail")).toString().trimmed();
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
    QElapsedTimer timer;
    if (placesTraceEnabled()) {
        timer.start();
        tracePlaces(QStringLiteral("refresh begin rows=%1 cachedProviderPlaces=%2")
                        .arg(m_items.size())
                        .arg(m_cachedProviderPlaces.size()));
    }
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
        gdriveItem.section = QStringLiteral("cloud");
        gdriveItem.subtitle = googleDriveAccountLabel();
        standardItems.append(gdriveItem);
    }
    if (megaProviderAvailable()) {
        PlaceItem megaItem;
        megaItem.name = QStringLiteral("MEGA NZ");
        megaItem.path = QStringLiteral("mega:///");
        megaItem.icon = QStringLiteral("mega");
        megaItem.section = QStringLiteral("cloud");
        megaItem.subtitle = megaAccountLabel();
        standardItems.append(megaItem);
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
    if (placesTraceEnabled()) {
        tracePlaces(QStringLiteral("refresh end rows=%1 elapsedMs=%2 providerSignatureLen=%3")
                        .arg(m_items.size())
                        .arg(timer.elapsed())
                        .arg(m_providerPlacesSignature.size()));
    }
}

void PlacesModel::refreshDriveInfo()
{
    QElapsedTimer timer;
    if (placesTraceEnabled()) {
        timer.start();
    }
    int changedRows = 0;
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
            ++changedRows;
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
    if (placesTraceEnabled()) {
        tracePlaces(QStringLiteral("refreshDriveInfo rows=%1 changed=%2 elapsedMs=%3")
                        .arg(m_items.size())
                        .arg(changedRows)
                        .arg(timer.elapsed()));
    }
}

void PlacesModel::refreshGoogleDriveAccountInfo()
{
    QElapsedTimer timer;
    if (placesTraceEnabled()) {
        timer.start();
    }
    int changedRows = 0;
    const QHash<QString, QString> accountLabels{
        {QStringLiteral("gdrive://"), googleDriveAccountLabel()},
    };
    for (int i = 0; i < m_items.size(); ++i) {
        PlaceItem &item = m_items[i];
        if (item.path == QLatin1String("mega:///")) {
            const QString accountLabel = megaAccountLabel();
            const QString name = QStringLiteral("MEGA NZ");
            if (item.name == name && item.subtitle == accountLabel) {
                continue;
            }
            item.name = name;
            item.subtitle = accountLabel;
            ++changedRows;
            const QModelIndex idx = index(i);
            emit dataChanged(idx, idx, {NameRole, SubtitleRole});
            continue;
        }
        const auto labelIt = accountLabels.constFind(item.path);
        if (labelIt == accountLabels.constEnd() || item.subtitle == *labelIt) {
            continue;
        }
        item.subtitle = *labelIt;
        ++changedRows;
        const QModelIndex idx = index(i);
        emit dataChanged(idx, idx, {SubtitleRole});
    }
    if (placesTraceEnabled()) {
        tracePlaces(QStringLiteral("refreshCloudAccountInfo rows=%1 changed=%2 elapsedMs=%3")
                        .arg(m_items.size())
                        .arg(changedRows)
                        .arg(timer.elapsed()));
    }
}

void PlacesModel::refreshProviderPlacesAsync()
{
    if (m_providerPlacesRefreshPending) {
        m_providerPlacesRefreshQueued = true;
        tracePlaces(QStringLiteral("providerPlaces queued generation=%1").arg(m_providerPlacesRefreshGeneration));
        return;
    }

    m_providerPlacesRefreshPending = true;
    const int generation = ++m_providerPlacesRefreshGeneration;
    tracePlaces(QStringLiteral("providerPlaces start generation=%1 cached=%2")
                    .arg(generation)
                    .arg(m_cachedProviderPlaces.size()));
    QPointer<PlacesModel> self(this);
    auto future = QtConcurrent::run([self, generation]() {
        QElapsedTimer workerTimer;
        if (placesTraceEnabled()) {
            workerTimer.start();
        }
        QList<ProviderPlaceItem> places = FileProviderPluginRegistry::instance().providerPlaces();
        const qint64 workerElapsed = placesTraceEnabled() ? workerTimer.elapsed() : 0;
        if (!self) {
            return;
        }

        QMetaObject::invokeMethod(self.data(), [self, generation, workerElapsed, places = std::move(places)]() mutable {
            if (!self || generation != self->m_providerPlacesRefreshGeneration) {
                if (placesTraceEnabled()) {
                    tracePlaces(QStringLiteral("providerPlaces stale generation=%1 workerMs=%2")
                                    .arg(generation)
                                    .arg(workerElapsed));
                }
                return;
            }

            self->m_providerPlacesRefreshPending = false;
            const bool refreshQueued = self->m_providerPlacesRefreshQueued;
            self->m_providerPlacesRefreshQueued = false;
            const QString signature = self->providerPlacesSignature(places);
            const bool changed = signature != self->m_providerPlacesSignature;
            tracePlaces(QStringLiteral("providerPlaces finish generation=%1 count=%2 workerMs=%3 changed=%4 queued=%5 oldSig=%6 newSig=%7")
                            .arg(generation)
                            .arg(places.size())
                            .arg(workerElapsed)
                            .arg(changed ? 1 : 0)
                            .arg(refreshQueued ? 1 : 0)
                            .arg(self->m_providerPlacesSignature.size())
                            .arg(signature.size()));
            if (signature != self->m_providerPlacesSignature) {
                const QList<ProviderPlaceItem> removed = removedProviderPlaces(self->m_cachedProviderPlaces, places);
                tracePlaces(QStringLiteral("providerPlaces apply generation=%1 removed=%2")
                                .arg(generation)
                                .arg(removed.size()));
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
