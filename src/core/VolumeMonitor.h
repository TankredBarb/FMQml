#pragma once

#include <QAbstractNativeEventFilter>
#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QSet>
#include <memory>

class LinuxDeviceMonitor;

struct VolumeInfo {
    QString rootPath;
    QString displayName;
    QString deviceDescription;
    QString fileSystem;
    QString driveType;
    qint64 totalBytes = 0;
    qint64 freeBytes = 0;
    bool isReady = false;
    bool isCritical = false;
    bool isRemovable = false;
    bool isOptical = false;
    bool isNetwork = false;
    bool isEjectable = false;
    bool canUnmount = false;
    bool canSafelyRemove = false;
    QString stableDeviceId;
    QString blockDevice;
    QString driveObjectPath;
};

class VolumeMonitor final : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT

public:
    explicit VolumeMonitor(QObject *parent = nullptr);
    ~VolumeMonitor() override;

    const QList<VolumeInfo> &volumes() const;
    const QList<VolumeInfo> &unmountedVolumes() const;
    bool isKnownUnmountableRoot(const QString &rootPath) const;
    QStringList relatedMountedRoots(const QString &rootPath) const;
    bool isDeviceActionPending(const QString &stableDeviceId) const;
    QString displayNameForRoot(const QString &rootPath) const;
    QString unavailableRootForPath(const QString &path) const;
    bool pathBelongsToRoot(const QString &path, const QString &rootPath) const;
    static QString volumeKeyForRoot(const QString &rootPath);

    Q_INVOKABLE void requestEject(const QString &rootPath);
    Q_INVOKABLE void requestMount(const QString &stableDeviceId);

    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) override;

public slots:
    void refreshNow();
    void scheduleRefresh();

signals:
    void volumesChanged();
    void volumeRemoved(const QString &rootPath, const QString &displayName);
    void volumeChanged(const QString &rootPath);
    void deviceTopologyChanged();
    void ejectFinished(const QString &rootPath, bool success, const QString &message);
    void mountFinished(const QString &stableDeviceId, const QString &rootPath, bool success, const QString &message);
    void deviceActionStateChanged();

private:
    QList<VolumeInfo> enumerateVolumes() const;
    void applySnapshot(const QList<VolumeInfo> &volumes);
    void scheduleRefresh(int delayMs, int followUpCount);
    QString rootForPathInMap(const QString &path, const QHash<QString, VolumeInfo> &volumesByKey) const;
    static QString comparablePath(const QString &path);
    static bool volumeInfoChanged(const VolumeInfo &lhs, const VolumeInfo &rhs);

    struct PendingRemoval {
        VolumeInfo info;
        int missedSnapshots = 0;
    };

    QList<VolumeInfo> m_volumes;
    QList<VolumeInfo> m_unmountedVolumes;
    QHash<QString, VolumeInfo> m_volumesByKey;
    QHash<QString, VolumeInfo> m_recentlyRemovedByKey;
    QHash<QString, PendingRemoval> m_pendingRemovedByKey;
    QTimer m_refreshTimer;
    QTimer m_pollTimer;
    int m_followUpRefreshes = 0;
    QSet<QString> m_pendingDeviceIds;
#ifdef Q_OS_LINUX
    std::unique_ptr<LinuxDeviceMonitor> m_linuxDeviceMonitor;
#endif
};
