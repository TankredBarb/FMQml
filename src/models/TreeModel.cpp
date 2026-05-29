#include "TreeModel.h"

#include "../core/IsoMountManager.h"
#include "../core/LocalFileProvider.h"

#include <QFileSystemWatcher>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QVector>
#include <QFileInfo>
#include <algorithm>

namespace {
struct RootItem {
    QStandardPaths::StandardLocation location;
    QString name;
    QString icon;
};

bool pathEquals(const QString &lhs, const QString &rhs)
{
#if defined(Q_OS_WIN)
    return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
#else
    return lhs == rhs;
#endif
}
}

TreeModel::TreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_provider(std::make_unique<LocalFileProvider>())
{
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &TreeModel::processPendingRefreshes);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, &TreeModel::scheduleRefresh);
    populateRoots();
}

void TreeModel::setIsoMountManager(IsoMountManager *manager)
{
    if (m_isoMountManager == manager) {
        return;
    }
    if (m_isoMountManager) {
        disconnect(m_isoMountManager, nullptr, this, nullptr);
    }
    m_isoMountManager = manager;
    if (m_isoMountManager) {
        connect(m_isoMountManager, &IsoMountManager::mountsChanged, this, [this]() {
            beginResetModel();
            clear();
            populateRoots();
            endResetModel();
        });
    }
    beginResetModel();
    clear();
    populateRoots();
    endResetModel();
}

int TreeModel::rowCount(const QModelIndex &parent) const
{
    const Node *node = nodeForIndex(parent);
    return node ? node->children.size() : 0;
}

QVariant TreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return {};
    }

    Node *node = nodeForIndex(index);
    if (!node) {
        return {};
    }

    switch (role) {
    case NameRole:
        return node->name;
    case PathRole:
        return node->path;
    case IconRole:
        return node->icon;
    case IsDriveRole:
        return node->isDrive;
    default:
        return {};
    }
}

QModelIndex TreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column != 0 || row < 0) {
        return {};
    }

    Node *parentNode = nodeForIndex(parent);
    if (!parentNode || row >= parentNode->children.size()) {
        return {};
    }

    Node *child = parentNode->children.at(row).get();
    return createIndex(row, column, child);
}

QModelIndex TreeModel::parent(const QModelIndex &index) const
{
    Node *node = nodeForIndex(index);
    if (!node || !node->parent || node->parent == &m_root) {
        return {};
    }

    return indexForNode(node->parent);
}

int TreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

QHash<int, QByteArray> TreeModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PathRole, "path"},
        {IconRole, "icon"},
        {IsDriveRole, "isDrive"},
    };
}

bool TreeModel::hasChildren(const QModelIndex &parent) const
{
    const Node *node = nodeForIndex(parent);
    if (!node) {
        return !m_root.children.empty();
    }
    if (!node->loaded) {
        return node->canFetch;
    }
    return !node->children.empty();
}

bool TreeModel::canFetchMore(const QModelIndex &parent) const
{
    const Node *node = nodeForIndex(parent);
    return node && node->canFetch && !node->loaded;
}

void TreeModel::fetchMore(const QModelIndex &parent)
{
    Node *node = nodeForIndex(parent);
    if (!node || node->loaded || !node->canFetch) {
        return;
    }

    loadChildren(node);
}

void TreeModel::refresh()
{
    pruneInvalidWatches();
    refreshNodeRecursive(&m_root);
}

void TreeModel::refreshPath(const QString &path)
{
    Node *node = nodeForPath(path);
    if (node) {
        refreshNode(node);
    }
}

QModelIndex TreeModel::indexForPath(const QString &path)
{
    Node *node = nodeForPath(path);
    return node ? indexForNode(node) : QModelIndex();
}

QModelIndex TreeModel::parentIndex(const QModelIndex &index) const
{
    return parent(index);
}

QString TreeModel::pathForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return {};
    }
    const Node *node = nodeForIndex(index);
    return node ? node->path : QString();
}

bool TreeModel::isTopLevelIndex(const QModelIndex &index) const
{
    Node *node = nodeForIndex(index);
    return node && node->parent == &m_root;
}

TreeModel::Node *TreeModel::nodeForIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return const_cast<Node *>(&m_root);
    }
    return static_cast<Node *>(index.internalPointer());
}

QModelIndex TreeModel::indexForNode(Node *node) const
{
    if (!node || node == &m_root || !node->parent) {
        return {};
    }

    const int row = rowForNode(node);
    if (row < 0) {
        return {};
    }
    return createIndex(row, 0, node);
}

int TreeModel::rowForNode(const Node *node) const
{
    if (!node || !node->parent) {
        return -1;
    }

    const Node *parent = node->parent;
    for (int i = 0; i < parent->children.size(); ++i) {
        if (parent->children.at(i).get() == node) {
            return i;
        }
    }
    return -1;
}

TreeModel::Node *TreeModel::findChild(Node *parent, const QString &path) const
{
    if (!parent) {
        return nullptr;
    }

    auto stripTrailing = [](QString p) {
        if (p.endsWith(QLatin1Char('/')) || p.endsWith(QLatin1Char('\\'))) {
            if (p.length() > 3 || (p.length() == 3 && p.at(1) != QLatin1Char(':'))) {
                p.chop(1);
            }
        }
        return p;
    };
    
    QString target = stripTrailing(path);

    for (const auto &child : parent->children) {
        if (child && pathEquals(stripTrailing(child->path), target)) {
            return child.get();
        }
    }
    return nullptr;
}

void TreeModel::refreshNode(Node *node)
{
    if (!node) {
        return;
    }

    if (node == &m_root) {
        // For root, just recurse into already loaded children
        for (const auto &child : node->children) {
            if (child->loaded) {
                refreshNodeRecursive(child.get());
            }
        }
        return;
    }

    if (!m_provider->pathExists(node->path) || !m_provider->isDirectory(node->path)) {
        unwatchSubtree(node);
        Node *parent = node->parent && node->parent != &m_root ? node->parent : nullptr;
        if (parent) {
            refreshNode(parent);
        } else {
            beginResetModel();
            clear();
            populateRoots();
            endResetModel();
        }
        return;
    }

    const QStringList paths = m_provider->childPaths(node->path, m_showHidden);

    std::vector<std::unique_ptr<Node>> oldChildren = std::move(node->children);
    std::vector<std::unique_ptr<Node>> newChildren;
    newChildren.reserve(paths.size());

    auto takeChild = [&oldChildren](const QString &childPath) -> std::unique_ptr<Node> {
        for (auto it = oldChildren.begin(); it != oldChildren.end(); ++it) {
            if (*it && (*it)->path == childPath) {
                std::unique_ptr<Node> ret = std::move(*it);
                oldChildren.erase(it);
                return ret;
            }
        }
        return nullptr;
    };

    for (const QString &childPath : paths) {
        if (!m_provider->pathExists(childPath) || !m_provider->isDirectory(childPath)) {
            continue;
        }
        if (m_provider->isSymLink(childPath)) {
            continue;
        }

        const QString normalized = m_provider->normalizedPath(childPath);
        const QString name = m_provider->fileName(normalized);
        if (name.isEmpty()) {
            continue;
        }

        std::unique_ptr<Node> child = takeChild(normalized);
        if (child) {
            child->parent = node;
            child->name = name;
            child->path = normalized;
        } else {
            child = makeNode(node, name, normalized, QStringLiteral("folder"), false);
        }
        newChildren.push_back(std::move(child));
    }

    for (auto &child : oldChildren) {
        if (child) {
            unwatchSubtree(child.get());
        }
    }

    std::sort(newChildren.begin(), newChildren.end(),
        [](const std::unique_ptr<Node> &lhs, const std::unique_ptr<Node> &rhs) {
            if (lhs->isDrive != rhs->isDrive) {
                return !lhs->isDrive;
            }
            return lhs->name.compare(rhs->name, Qt::CaseInsensitive) < 0;
        });

    // Check if anything actually changed in terms of children paths
    bool changed = (newChildren.size() != oldChildren.size()); // This oldChildren is now only the "removed" ones
    if (!changed) {
        // Could check names or presence, but since we are swapping anyway...
    }

    layoutAboutToBeChanged();
    node->children = std::move(newChildren);
    node->loaded = true;
    node->canFetch = true;
    layoutChanged();

    watchNode(node);
}

void TreeModel::refreshNodeRecursive(Node *node)
{
    if (!node) return;
    
    if (node == &m_root) {
        for (const auto &child : node->children) {
            refreshNodeRecursive(child.get());
        }
    } else if (node->loaded) {
        refreshNode(node);
        for (const auto &child : node->children) {
            refreshNodeRecursive(child.get());
        }
    }
}

TreeModel::Node *TreeModel::nodeForPath(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }

    QString normalized = m_provider->normalizedPath(path);
    if (normalized.isEmpty()) {
        return nullptr;
    }

    auto stripTrailing = [](QString p) {
        if (p.endsWith(QLatin1Char('/')) || p.endsWith(QLatin1Char('\\'))) {
            if (p.length() > 3 || (p.length() == 3 && p.at(1) != QLatin1Char(':'))) {
                p.chop(1);
            }
        }
        return p;
    };

    normalized = stripTrailing(normalized);

    Node *rootMatch = nullptr;
    int bestPrefixLength = -1;
    for (const auto &child : m_root.children) {
        if (!child) {
            continue;
        }
        QString rootPath = stripTrailing(child->path);
        
        const bool rootEndsWithSeparator = rootPath.endsWith(QLatin1Char('/')) || rootPath.endsWith(QLatin1Char('\\'));
        const bool matchesRoot = rootEndsWithSeparator
            ? normalized.startsWith(rootPath, Qt::CaseInsensitive)
            : (pathEquals(normalized, rootPath)
               || normalized.startsWith(rootPath + QLatin1Char('/'), Qt::CaseInsensitive)
               || normalized.startsWith(rootPath + QLatin1Char('\\'), Qt::CaseInsensitive));

        if (matchesRoot) {
            if (rootPath.size() > bestPrefixLength) {
                bestPrefixLength = rootPath.size();
                rootMatch = child.get();
            }
        }
    }

    if (!rootMatch) {
        return nullptr;
    }

    QStringList ancestors;
    QString currentPath = normalized;
    while (true) {
        ancestors.prepend(currentPath);
        if (pathEquals(currentPath, stripTrailing(rootMatch->path))) {
            break;
        }
        QString parentPath = stripTrailing(m_provider->parentPath(currentPath));
        if (parentPath.isEmpty() || pathEquals(parentPath, currentPath)) {
            return nullptr;
        }
        currentPath = parentPath;
    }

    Node *currentNode = rootMatch;
    if (ancestors.size() == 1) {
        return currentNode;
    }

    for (int i = 1; i < ancestors.size(); ++i) {
        const QString &segmentPath = ancestors.at(i);
        if (!currentNode->loaded && currentNode->canFetch) {
            loadChildren(currentNode);
        }

        Node *match = findChild(currentNode, segmentPath);
        if (!match) {
            return nullptr;
        }
        currentNode = match;
    }

    return currentNode;
}

void TreeModel::clear()
{
    m_watcher.removePaths(m_watchedPaths.values());
    m_watchedPaths.clear();
    m_root.children.clear();
}

std::unique_ptr<TreeModel::Node> TreeModel::makeNode(Node *parent, const QString &name, const QString &path, const QString &icon, bool isDrive)
{
    auto node = std::make_unique<Node>();
    node->parent = parent;
    node->name = name;
    node->path = m_provider->normalizedPath(path);
    node->icon = icon;
    node->isDrive = isDrive;
    return node;
}

void TreeModel::populateRoots()
{
    QSet<QString> seenPaths;

    const QVector<RootItem> standard = {
        {QStandardPaths::HomeLocation, QStringLiteral("Home"), QStringLiteral("home")},
        {QStandardPaths::DesktopLocation, QStringLiteral("Desktop"), QStringLiteral("desktop")},
        {QStandardPaths::DownloadLocation, QStringLiteral("Downloads"), QStringLiteral("download")},
        {QStandardPaths::DocumentsLocation, QStringLiteral("Documents"), QStringLiteral("document")},
        {QStandardPaths::PicturesLocation, QStringLiteral("Pictures"), QStringLiteral("image")},
        {QStandardPaths::MusicLocation, QStringLiteral("Music"), QStringLiteral("music")},
        {QStandardPaths::MoviesLocation, QStringLiteral("Videos"), QStringLiteral("video")},
    };

    for (const RootItem &item : standard) {
        const QString path = QStandardPaths::writableLocation(item.location);
        if (path.isEmpty() || !m_provider->pathExists(path) || !m_provider->isDirectory(path)) {
            continue;
        }

        const QString normalized = m_provider->normalizedPath(path);
        if (seenPaths.contains(normalized)) {
            continue;
        }
        seenPaths.insert(normalized);

        m_root.children.push_back(makeNode(&m_root, item.name, normalized, item.icon, false));
        watchNode(m_root.children.back().get());
    }

    for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
        if (!storage.isValid() || !storage.isReady()) {
            continue;
        }

        const QString path = storage.rootPath();
        if (path.isEmpty() || !m_provider->pathExists(path) || !m_provider->isDirectory(path)) {
            continue;
        }

        const QString normalized = m_provider->normalizedPath(path);
        if (seenPaths.contains(normalized)) {
            continue;
        }
        seenPaths.insert(normalized);

        QString name = storage.displayName();
        if (name.isEmpty()) {
            name = normalized;
        }
        if (m_isoMountManager && m_isoMountManager->isManagedMountRoot(normalized)) {
            const IsoMountManager::Mount mount = m_isoMountManager->mountForRoot(normalized);
            const QString isoName = QFileInfo(mount.imagePath).completeBaseName();
            name = isoName.isEmpty() ? QFileInfo(mount.imagePath).fileName() : isoName;
            if (!mount.letter.isNull()) {
                name += QStringLiteral(" (%1:)").arg(mount.letter);
            }
        }
        m_root.children.push_back(makeNode(&m_root, name, normalized, QStringLiteral("drive"), true));
        watchNode(m_root.children.back().get());
    }

    if (m_isoMountManager) {
        for (const IsoMountManager::Mount &mount : m_isoMountManager->mounts()) {
            const QString path = mount.rootPath;
            if (path.isEmpty() || !m_provider->pathExists(path) || !m_provider->isDirectory(path)) {
                continue;
            }

            const QString normalized = m_provider->normalizedPath(path);
            if (seenPaths.contains(normalized)) {
                continue;
            }
            seenPaths.insert(normalized);

            QString name = QFileInfo(mount.imagePath).completeBaseName();
            if (name.isEmpty()) {
                name = QFileInfo(mount.imagePath).fileName();
            }
            if (!mount.letter.isNull()) {
                name += QStringLiteral(" (%1:)").arg(mount.letter);
            }
            m_root.children.push_back(makeNode(&m_root, name, normalized, QStringLiteral("drive"), true));
            watchNode(m_root.children.back().get());
        }
    }

    std::sort(m_root.children.begin(), m_root.children.end(),
        [](const std::unique_ptr<Node> &lhs, const std::unique_ptr<Node> &rhs) {
            if (lhs->isDrive != rhs->isDrive) {
                return !lhs->isDrive;
            }
            return lhs->name.compare(rhs->name, Qt::CaseInsensitive) < 0;
        });
}

void TreeModel::loadChildren(Node *node)
{
    if (!node || node->loaded) {
        return;
    }

    const QStringList paths = m_provider->childPaths(node->path, m_showHidden);
    std::vector<std::unique_ptr<Node>> children;
    children.reserve(paths.size());

    for (const QString &childPath : paths) {
        if (!m_provider->pathExists(childPath) || !m_provider->isDirectory(childPath)) {
            continue;
        }
        if (m_provider->isSymLink(childPath)) {
            continue;
        }

        const QString normalized = m_provider->normalizedPath(childPath);
        const QString name = m_provider->fileName(normalized);
        if (name.isEmpty()) {
            continue;
        }

        children.push_back(makeNode(node, name, normalized, QStringLiteral("folder"), false));
    }

    std::sort(children.begin(), children.end(),
        [](const std::unique_ptr<Node> &lhs, const std::unique_ptr<Node> &rhs) {
            if (lhs->isDrive != rhs->isDrive) {
                return !lhs->isDrive;
            }
            return lhs->name.compare(rhs->name, Qt::CaseInsensitive) < 0;
        });

    node->loaded = true;
    if (children.empty()) {
        node->canFetch = false;
        watchNode(node);
        return;
    }

    const QModelIndex parentIndex = indexForNode(node);
    beginInsertRows(parentIndex, 0, children.size() - 1);
    for (auto &child : children) {
        node->children.push_back(std::move(child));
    }
    endInsertRows();
    watchNode(node);
}

bool TreeModel::showHidden() const
{
    return m_showHidden;
}

void TreeModel::setShowHidden(bool show)
{
    if (m_showHidden == show) {
        return;
    }

    m_showHidden = show;
    
    // Surgical refresh of the tree without collapsing everything
    refreshNodeRecursive(&m_root);

    emit showHiddenChanged();
}

void TreeModel::watchNode(Node *node)
{
    if (!node || node == &m_root) {
        return;
    }

    const QString normalized = m_provider->normalizedPath(node->path);
    if (normalized.isEmpty() || m_watchedPaths.contains(normalized)) {
        return;
    }
    if (!m_provider->pathExists(normalized) || !m_provider->isDirectory(normalized)) {
        return;
    }

    if (m_watcher.addPath(normalized)) {
        m_watchedPaths.insert(normalized);
    }
}

void TreeModel::unwatchNode(Node *node)
{
    if (!node || node == &m_root) {
        return;
    }

    const QString normalized = m_provider->normalizedPath(node->path);
    if (normalized.isEmpty() || !m_watchedPaths.contains(normalized)) {
        return;
    }

    m_watcher.removePath(normalized);
    m_watchedPaths.remove(normalized);
}

void TreeModel::unwatchSubtree(Node *node)
{
    if (!node) {
        return;
    }

    unwatchNode(node);
    for (const auto &child : node->children) {
        if (child) {
            unwatchSubtree(child.get());
        }
    }
}

void TreeModel::pruneInvalidWatches()
{
    const auto watched = m_watchedPaths;
    for (const QString &path : watched) {
        if (!m_provider->pathExists(path) || !m_provider->isDirectory(path)) {
            m_watcher.removePath(path);
            m_watchedPaths.remove(path);
            m_pendingRefreshPaths.remove(path);
        }
    }
}

void TreeModel::scheduleRefresh(const QString &path)
{
    const QString normalized = m_provider->normalizedPath(path);
    if (normalized.isEmpty()) {
        return;
    }
    if (!m_provider->pathExists(normalized) || !m_provider->isDirectory(normalized)) {
        m_watcher.removePath(normalized);
        m_watchedPaths.remove(normalized);
        m_pendingRefreshPaths.remove(normalized);
        return;
    }

    m_pendingRefreshPaths.insert(normalized);
    if (!m_refreshTimer.isActive()) {
        m_refreshTimer.start(0);
    }
}

void TreeModel::processPendingRefreshes()
{
    pruneInvalidWatches();

    const auto pending = m_pendingRefreshPaths;
    m_pendingRefreshPaths.clear();

    for (const QString &path : pending) {
        refreshPath(path);
    }
}
