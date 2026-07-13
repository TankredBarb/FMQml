#pragma once

#include <QAbstractItemModel>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVector>
#include <atomic>
#include <memory>
#include <vector>

#include "../core/DirectoryChangeWatcher.h"
#include "../core/FileProvider.h"

class IsoMountManager;
class VolumeMonitor;

class TreeModel final : public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IconRole,
        IsDriveRole,
        LoadingRole
    };

    explicit TreeModel(QObject *parent = nullptr);
    void setIsoMountManager(IsoMountManager *manager);
    void setVolumeMonitor(VolumeMonitor *monitor);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QHash<int, QByteArray> roleNames() const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void refreshPath(const QString &path);
    Q_INVOKABLE QModelIndex indexForPath(const QString &path);
    Q_INVOKABLE QModelIndex nearestLoadedIndexForPath(const QString &path, int maxMissingLoads = 2);
    Q_INVOKABLE void revealPathAsync(const QString &path, int requestId);
    Q_INVOKABLE QModelIndex parentIndex(const QModelIndex &index) const;
    Q_INVOKABLE QString pathForIndex(const QModelIndex &index) const;

    bool showHidden() const;
    void setShowHidden(bool show);

signals:
    void showHiddenChanged();
    void pathRevealReady(int requestId, const QModelIndex &index, bool exact);

private:
    struct Node {
        Node *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        QString name;
        QString path;
        QString icon;
        bool isDrive = false;
        bool loaded = false;
        bool loading = false;
        bool canFetch = true;
        quint64 loadGeneration = 0;
        int loadRevealRequestId = -1;
        std::shared_ptr<std::atomic_bool> loadCancelled;
    };

    struct ChildEntry {
        QString name;
        QString path;
        QString icon;
        bool isDrive = false;
    };

    Node *nodeForIndex(const QModelIndex &index) const;
    QModelIndex indexForNode(Node *node) const;
    int rowForNode(const Node *node) const;
    Node *nodeForPath(const QString &path, int maxMissingLoads = -1, bool returnNearestLoaded = false);
    Node *findChild(Node *parent, const QString &path) const;
    void refreshNode(Node *node);
    void refreshNodeRecursive(Node *node);
    void watchNode(Node *node);
    void unwatchNode(Node *node);
    void unwatchSubtree(Node *node);
    void pruneInvalidWatches();
    void onWatcherEventsReady(const QList<DirectoryChangeEvent> &events);
    void onWatcherFailed(const QString &path, const QString &error);
    void scheduleRefreshForEvent(const DirectoryChangeEvent &event);
    void scheduleRefresh(const QString &path);
    QString nearestExistingDirectoryPath(const QString &path) const;
    void processPendingRefreshes();
    void clear();
    void populateRoots();
    void loadChildren(Node *node, int revealRequestId = -1);
    void cancelNodeLoad(Node *node, bool notify);
    void cancelLoads(Node *node);
    void cancelRevealLoads(Node *node);
    void applyLoadedChildren(const QString &path, bool showHidden, quint64 generation, const QVector<ChildEntry> &children);
    void continuePendingReveal();
    void refreshChildren(Node *node);
    void applyRefreshedChildren(const QString &path, bool showHidden, quint64 generation, const QVector<ChildEntry> &children);
    static QVector<ChildEntry> loadChildEntries(const QString &path,
                                                bool showHidden,
                                                const std::shared_ptr<std::atomic_bool> &cancelled = {});
    std::unique_ptr<Node> makeNode(Node *parent, const QString &name, const QString &path, const QString &icon, bool isDrive);

    Node m_root;
    std::unique_ptr<FileProvider> m_provider;
    QHash<QString, DirectoryChangeWatcher *> m_watchers;
    QSet<QString> m_watchedPaths;
    QSet<QString> m_pendingRefreshPaths;
    QTimer m_refreshTimer;
    quint64 m_loadGeneration = 0;
    QString m_pendingRevealPath;
    int m_pendingRevealRequestId = -1;
    bool m_showHidden = false;
    IsoMountManager *m_isoMountManager = nullptr;
    VolumeMonitor *m_volumeMonitor = nullptr;
};
