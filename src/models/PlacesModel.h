#pragma once

#include <QAbstractListModel>
#include <QStringList>
#include <QTimer>

class IsoMountManager;

struct PlaceItem {
    QString name;
    QString path;
    QString icon;
    bool    isDrive    = false;

    // Storage info (drives only)
    qint64  totalBytes = 0;
    qint64  freeBytes  = 0;
    QString fileSystem;   // "NTFS", "FAT32", "exFAT", …
    QString driveType;    // "hdd" | "ssd" | "usb" | "optical" | "network"
    bool    isReady    = false;
    bool    isCritical = false; // freeBytes < 10% totalBytes
    bool    isVirtualDrive = false;
    bool    canEject = false;
    QString sourcePath;
    QString mountId;
};

class PlacesModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IconRole,
        IsDriveRole,
        TotalSpaceRole,
        FreeSpaceRole,
        UsedSpaceRole,
        UsagePercentRole,
        FileSystemRole,
        DriveTypeRole,
        IsReadyRole,
        IsCriticalRole,
        IsVirtualDriveRole,
        CanEjectRole,
        SourcePathRole,
        MountIdRole
    };

    explicit PlacesModel(QObject *parent = nullptr);
    void setIsoMountManager(IsoMountManager *manager);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();

signals:
    void lowDiskSpaceWarning(const QString &driveName, qint64 freeBytes);

private slots:
    void refreshDriveInfo();

private:
    QList<PlaceItem> m_items;
    QTimer *m_refreshTimer = nullptr;
    IsoMountManager *m_isoMountManager = nullptr;
};
