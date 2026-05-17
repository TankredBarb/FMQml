#include "TreeModel.h"

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
    for (const auto &child : m_root.children) {
        if (child && child->loaded) {
            refreshNode(child.get());
        }
    }
}

void TreeModel::refreshPath(const QString &path)
{
    if (path.isEmpty()) {
        refresh();
        return;
    }

    const QString normalized = m_provider->normalizedPath(path);
    if (normalized.isEmpty()) {
        return;
    }

    QString targetPath = normalized;
    if (!m_provider->pathExists(targetPath) || !m_provider->isDirectory(targetPath)) {
        targetPath = m_provider->parentPath(targetPath);
    }

    if (targetPath.isEmpty()) {
        return;
    }

    Node *node = nodeForPath(targetPath);
    if (!node) {
        return;
    }

    refreshNode(node);
}

QModelIndex TreeModel::indexForPath(const QString &path)
{
    Node *node = nodeForPath(path);
    return node ? indexForNode(node) : QModelIndex();
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

    for (const auto &child : parent->children) {
        if (child && pathEquals(child->path, path)) {
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

    if (node != &m_root && (!m_provider->pathExists(node->path) || !m_provider->isDirectory(node->path))) {
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

    const QStringList paths = node == &m_root ? QStringList() : m_provider->childPaths(node->path, false);

    std::vector<std::unique_ptr<Node>> oldChildren = std::move(node->children);
    std::vector<std::unique_ptr<Node>> newChildren;
    newChildren.reserve(paths.size());

    auto takeChild = [&oldChildren](const QString &childPath) -> std::unique_ptr<Node> {
        for (auto &child : oldChildren) {
            if (child && pathEquals(child->path, childPath)) {
                return std::move(child);
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
            child->icon = QStringLiteral("folder");
            child->isDrive = false;
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

    layoutAboutToBeChanged();
    node->children = std::move(newChildren);
    node->loaded = true;
    node->canFetch = true;
    layoutChanged();

    watchNode(node);
}

TreeModel::Node *TreeModel::nodeForPath(const QString &path)
{
    if (path.isEmpty()) {
        return nullptr;
    }

    const QString normalized = m_provider->normalizedPath(path);
    if (normalized.isEmpty()) {
        return nullptr;
    }

    Node *rootMatch = nullptr;
    int bestPrefixLength = -1;
    for (const auto &child : m_root.children) {
        if (!child) {
            continue;
        }
        const QString &rootPath = child->path;
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
        if (pathEquals(currentPath, rootMatch->path)) {
            break;
        }
        const QString parentPath = m_provider->parentPath(currentPath);
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
        m_root.children.push_back(makeNode(&m_root, name, normalized, QStringLiteral("drive"), true));
        watchNode(m_root.children.back().get());
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

    const QStringList paths = m_provider->childPaths(node->path, false);
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

void TreeModel::watchNode(Node *node)
{
    if (!node || node == &m_root) {
        return;
    }

    const QString normalized = m_provider->normalizedPath(node->path);
    if (normalized.isEmpty() || m_watchedPaths.contains(normalized)) {
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

void TreeModel::scheduleRefresh(const QString &path)
{
    const QString normalized = m_provider->normalizedPath(path);
    if (normalized.isEmpty()) {
        return;
    }

    m_pendingRefreshPaths.insert(normalized);
    if (!m_refreshTimer.isActive()) {
        m_refreshTimer.start(0);
    }
}

void TreeModel::processPendingRefreshes()
{
    const auto pending = m_pendingRefreshPaths;
    m_pendingRefreshPaths.clear();

    for (const QString &path : pending) {
        refreshPath(path);
    }
}
