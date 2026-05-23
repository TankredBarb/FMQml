#pragma once

#include <QAbstractItemModel>
#include <QFileSystemWatcher>
#include <QSet>
#include <QTimer>
#include <memory>
#include <vector>

#include "../core/FileProvider.h"

class TreeModel final : public QAbstractItemModel {
    Q_OBJECT
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        IconRole,
        IsDriveRole
    };

    explicit TreeModel(QObject *parent = nullptr);

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
    Q_INVOKABLE QModelIndex parentIndex(const QModelIndex &index) const;
    Q_INVOKABLE QString pathForIndex(const QModelIndex &index) const;
    Q_INVOKABLE bool isTopLevelIndex(const QModelIndex &index) const;

    bool showHidden() const;
    void setShowHidden(bool show);

signals:
    void showHiddenChanged();

private:
    struct Node {
        Node *parent = nullptr;
        std::vector<std::unique_ptr<Node>> children;
        QString name;
        QString path;
        QString icon;
        bool isDrive = false;
        bool loaded = false;
        bool canFetch = true;
    };

    Node *nodeForIndex(const QModelIndex &index) const;
    QModelIndex indexForNode(Node *node) const;
    int rowForNode(const Node *node) const;
    Node *nodeForPath(const QString &path);
    Node *findChild(Node *parent, const QString &path) const;
    void refreshNode(Node *node);
    void refreshNodeRecursive(Node *node);
    void watchNode(Node *node);
    void unwatchNode(Node *node);
    void unwatchSubtree(Node *node);
    void scheduleRefresh(const QString &path);
    void processPendingRefreshes();
    void clear();
    void populateRoots();
    void loadChildren(Node *node);
    std::unique_ptr<Node> makeNode(Node *parent, const QString &name, const QString &path, const QString &icon, bool isDrive);

    Node m_root;
    std::unique_ptr<FileProvider> m_provider;
    QFileSystemWatcher m_watcher;
    QSet<QString> m_watchedPaths;
    QSet<QString> m_pendingRefreshPaths;
    QTimer m_refreshTimer;
    bool m_showHidden = false;
};
