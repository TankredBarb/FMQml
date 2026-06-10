#include "TreeModel.h"

#include "../core/IsoMountManager.h"
#include "../core/LocalFileProvider.h"
#include "../core/DriveUtils.h"
#include "../core/VolumeMonitor.h"
#ifndef Q_OS_WIN
#include "../core/QtDirectoryChangeWatcher.h"
#endif

#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QPointer>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QVector>
#include <QFileInfo>
#include <QtConcurrent/QtConcurrentRun>
#include <QtGlobal>
#include <algorithm>
#include <utility>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

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

QString treeIsoMountDisplayName(const IsoMountManager::Mount &mount)
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

QString comparableTreePath(QString path)
{
    path = QDir::fromNativeSeparators(path);
    if (path.endsWith(QLatin1Char('/')) || path.endsWith(QLatin1Char('\\'))) {
        const bool driveRoot = path.length() == 3 && path.at(1) == QLatin1Char(':');
        if (path.length() > 1 && !driveRoot) {
            path.chop(1);
        }
    }
    return path;
}

bool treePathEquals(const QString &lhs, const QString &rhs)
{
    return pathEquals(comparableTreePath(lhs), comparableTreePath(rhs));
}

bool treeWatchDebugEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_WATCH_DEBUG");
    return enabled;
}

bool treeNavTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_NAV_TRACE");
    return enabled;
}

void traceTreeNav(const char *stage, const QString &path = {}, const QString &detail = {})
{
    if (!treeNavTraceEnabled()) {
        return;
    }

    qInfo().noquote() << "[FM_NAV][tree-model]" << stage
                      << "path=" << QDir::toNativeSeparators(path)
                      << detail;
}

#ifndef Q_OS_WIN
static QString normalizedLinuxTreeMountPath(const QString &path)
{
    QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path.trimmed()));
    if (normalized != QLatin1String("/") && normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized;
}

static bool treePathIsDirectChildOf(const QString &path, const QString &parent)
{
    const QString normalizedPath = normalizedLinuxTreeMountPath(path);
    QString normalizedParent = normalizedLinuxTreeMountPath(parent);
    if (normalizedPath.isEmpty() || normalizedParent.isEmpty() || normalizedPath == normalizedParent) {
        return false;
    }
    if (!normalizedParent.endsWith(QLatin1Char('/'))) {
        normalizedParent += QLatin1Char('/');
    }
    const QString tail = normalizedPath.mid(normalizedParent.size());
    return normalizedPath.startsWith(normalizedParent) && !tail.isEmpty() && !tail.contains(QLatin1Char('/'));
}

static bool treeIsLinuxNetworkFileSystem(const QString &fileSystem)
{
    static const QSet<QString> networkFileSystems = {
        QStringLiteral("nfs"),
        QStringLiteral("nfs4"),
        QStringLiteral("cifs"),
        QStringLiteral("smb3"),
        QStringLiteral("sshfs"),
        QStringLiteral("fuse.sshfs"),
        QStringLiteral("davfs"),
        QStringLiteral("fuse.davfs"),
    };
    return networkFileSystems.contains(fileSystem.toLower());
}

static bool treeIsLinuxPseudoFileSystem(const QString &fileSystem)
{
    static const QSet<QString> pseudoFileSystems = {
        QStringLiteral("autofs"),
        QStringLiteral("binfmt_misc"),
        QStringLiteral("bpf"),
        QStringLiteral("cgroup"),
        QStringLiteral("cgroup2"),
        QStringLiteral("configfs"),
        QStringLiteral("debugfs"),
        QStringLiteral("devpts"),
        QStringLiteral("devtmpfs"),
        QStringLiteral("efivarfs"),
        QStringLiteral("fusectl"),
        QStringLiteral("fuse.portal"),
        QStringLiteral("hugetlbfs"),
        QStringLiteral("mqueue"),
        QStringLiteral("proc"),
        QStringLiteral("pstore"),
        QStringLiteral("securityfs"),
        QStringLiteral("sysfs"),
        QStringLiteral("tracefs"),
    };
    return pseudoFileSystems.contains(fileSystem.toLower());
}

static bool treeIsLinuxUserFacingMount(const QStorageInfo &storage)
{
    const QString root = normalizedLinuxTreeMountPath(storage.rootPath());
    if (root.isEmpty()) {
        return false;
    }

    const QString fileSystem = QString::fromLatin1(storage.fileSystemType()).toLower();
    if (root == QLatin1String("/") || root == QLatin1String("/home")) {
        return true;
    }
    if (treeIsLinuxNetworkFileSystem(fileSystem)) {
        return true;
    }
    if (treeIsLinuxPseudoFileSystem(fileSystem)) {
        return false;
    }

    const QString userName = QString::fromLocal8Bit(qgetenv("USER")).trimmed();
    if (!userName.isEmpty()) {
        if (treePathIsDirectChildOf(root, QStringLiteral("/run/media/%1").arg(userName))
            || treePathIsDirectChildOf(root, QStringLiteral("/media/%1").arg(userName))) {
            return true;
        }
    }

    return treePathIsDirectChildOf(root, QStringLiteral("/media"))
        || treePathIsDirectChildOf(root, QStringLiteral("/mnt"));
}
#endif

#ifdef Q_OS_WIN
QString treeModelFindPattern(QString searchDir)
{
    searchDir = QDir::toNativeSeparators(searchDir);
    if (!searchDir.endsWith(QLatin1Char('\\'))) {
        searchDir += QLatin1Char('\\');
    }

    QString pattern = searchDir + QLatin1Char('*');
    if (pattern.startsWith(QStringLiteral("\\\\?\\"))) {
        return pattern;
    }
    if (pattern.startsWith(QStringLiteral("\\\\"))) {
        return QStringLiteral("\\\\?\\UNC\\") + pattern.mid(2);
    }
    return QStringLiteral("\\\\?\\") + pattern;
}
#endif
}

TreeModel::TreeModel(QObject *parent)
    : QAbstractItemModel(parent)
    , m_provider(std::make_unique<LocalFileProvider>())
{
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setInterval(120);
    connect(&m_refreshTimer, &QTimer::timeout, this, &TreeModel::processPendingRefreshes);
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

void TreeModel::setVolumeMonitor(VolumeMonitor *monitor)
{
    if (m_volumeMonitor == monitor) {
        return;
    }
    if (m_volumeMonitor) {
        disconnect(m_volumeMonitor, nullptr, this, nullptr);
    }
    m_volumeMonitor = monitor;
    if (m_volumeMonitor) {
        connect(m_volumeMonitor, &VolumeMonitor::volumesChanged, this, [this]() {
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
    case LoadingRole:
        return node->loading;
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
        {LoadingRole, "loading"},
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
    return node && node->canFetch && !node->loaded && !node->loading;
}

void TreeModel::fetchMore(const QModelIndex &parent)
{
    Node *node = nodeForIndex(parent);
    if (!node || node->loaded || node->loading || !node->canFetch) {
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
    Node *node = nodeForPath(path, 0);
    if (node) {
        refreshNode(node);
    }
}

QModelIndex TreeModel::indexForPath(const QString &path)
{
    QElapsedTimer timer;
    timer.start();
    Node *node = nodeForPath(path, 0);
    const QModelIndex result = node ? indexForNode(node) : QModelIndex();
    traceTreeNav("indexForPath", path,
                 QStringLiteral("result=%1 elapsedMs=%2")
                     .arg(result.isValid())
                     .arg(timer.elapsed()));
    return result;
}

QModelIndex TreeModel::nearestLoadedIndexForPath(const QString &path, int maxMissingLoads)
{
    QElapsedTimer timer;
    timer.start();
    Node *node = nodeForPath(path, maxMissingLoads, true);
    const QModelIndex result = node ? indexForNode(node) : QModelIndex();
    traceTreeNav("nearestLoadedIndexForPath", path,
                 QStringLiteral("result=%1 nearest=%2 maxMissingLoads=%3 elapsedMs=%4")
                     .arg(result.isValid())
                     .arg(node ? QDir::toNativeSeparators(node->path) : QStringLiteral("<none>"))
                     .arg(maxMissingLoads)
                     .arg(timer.elapsed()));
    return result;
}

void TreeModel::revealPathAsync(const QString &path, int requestId)
{
    if (!m_pendingRevealPath.isEmpty() && !treePathEquals(m_pendingRevealPath, path)) {
        cancelRevealLoads(&m_root);
    }
    m_pendingRevealPath = path;
    m_pendingRevealRequestId = requestId;
    continuePendingReveal();
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

    const QString target = comparableTreePath(path);

    for (const auto &child : parent->children) {
        if (child && treePathEquals(child->path, target)) {
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

    if (!node->loaded) {
        cancelNodeLoad(node, true);
        return;
    }

    refreshChildren(node);
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

void TreeModel::cancelNodeLoad(Node *node, bool notify)
{
    if (!node || !node->loading) {
        return;
    }

    if (node->loadCancelled) {
        node->loadCancelled->store(true);
    }
    node->loading = false;
    node->loadGeneration = 0;
    node->loadRevealRequestId = -1;
    node->loadCancelled.reset();

    if (notify) {
        const QModelIndex nodeIndex = indexForNode(node);
        if (nodeIndex.isValid()) {
            emit dataChanged(nodeIndex, nodeIndex, {LoadingRole});
        }
    }
}

void TreeModel::cancelLoads(Node *node)
{
    if (!node) {
        return;
    }

    cancelNodeLoad(node, false);
    for (const auto &child : node->children) {
        cancelLoads(child.get());
    }
}

void TreeModel::cancelRevealLoads(Node *node)
{
    if (!node) {
        return;
    }

    if (node->loading && node->loadRevealRequestId >= 0) {
        cancelNodeLoad(node, true);
    }
    for (const auto &child : node->children) {
        cancelRevealLoads(child.get());
    }
}

TreeModel::Node *TreeModel::nodeForPath(const QString &path, int maxMissingLoads, bool returnNearestLoaded)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    traceTreeNav("nodeForPath-begin", path,
                 QStringLiteral("maxMissingLoads=%1 returnNearest=%2")
                     .arg(maxMissingLoads)
                     .arg(returnNearestLoaded));
    if (path.isEmpty()) {
        traceTreeNav("nodeForPath-end", path,
                     QStringLiteral("result=null reason=empty elapsedMs=%1").arg(totalTimer.elapsed()));
        return nullptr;
    }

    QString normalized = m_provider->normalizedPath(path);
    if (normalized.isEmpty()) {
        traceTreeNav("nodeForPath-end", path,
                     QStringLiteral("result=null reason=normalize elapsedMs=%1").arg(totalTimer.elapsed()));
        return nullptr;
    }

    normalized = comparableTreePath(normalized);

    Node *rootMatch = nullptr;
    int bestPrefixLength = -1;
    for (const auto &child : m_root.children) {
        if (!child) {
            continue;
        }
        QString rootPath = comparableTreePath(child->path);
        
        const bool rootEndsWithSeparator = rootPath.endsWith(QLatin1Char('/')) || rootPath.endsWith(QLatin1Char('\\'));
        const bool matchesRoot = rootEndsWithSeparator
            ? normalized.startsWith(rootPath, Qt::CaseInsensitive)
            : (treePathEquals(normalized, rootPath)
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
        traceTreeNav("nodeForPath-end", normalized,
                     QStringLiteral("result=null reason=no-root elapsedMs=%1").arg(totalTimer.elapsed()));
        return nullptr;
    }

    QStringList ancestors;
    QString currentPath = normalized;
    while (true) {
        ancestors.prepend(currentPath);
        if (treePathEquals(currentPath, rootMatch->path)) {
            break;
        }
        QString parentPath = comparableTreePath(m_provider->parentPath(currentPath));
        if (parentPath.isEmpty() || treePathEquals(parentPath, currentPath)) {
            traceTreeNav("nodeForPath-end", normalized,
                         QStringLiteral("result=null reason=parent elapsedMs=%1").arg(totalTimer.elapsed()));
            return nullptr;
        }
        currentPath = parentPath;
    }

    Node *currentNode = rootMatch;
    if (ancestors.size() == 1) {
        return currentNode;
    }

    int missingLoads = 0;
    for (int i = 1; i < ancestors.size(); ++i) {
        const QString &segmentPath = ancestors.at(i);
        if (!currentNode->loaded && currentNode->canFetch) {
            if (maxMissingLoads >= 0 && missingLoads >= maxMissingLoads) {
                traceTreeNav("nodeForPath-load-limit", segmentPath,
                             QStringLiteral("nearest=%1 missingLoads=%2 elapsedMs=%3")
                                 .arg(QDir::toNativeSeparators(currentNode->path))
                                 .arg(missingLoads)
                                 .arg(totalTimer.elapsed()));
                return returnNearestLoaded ? currentNode : nullptr;
            }
            QElapsedTimer loadTimer;
            loadTimer.start();
            traceTreeNav("nodeForPath-loadChildren-begin", currentNode->path,
                         QStringLiteral("target=%1 missingLoads=%2")
                             .arg(QDir::toNativeSeparators(segmentPath))
                             .arg(missingLoads));
            loadChildren(currentNode);
            traceTreeNav("nodeForPath-loadChildren-end", currentNode->path,
                         QStringLiteral("target=%1 elapsedMs=%2 loaded=%3 childCount=%4")
                             .arg(QDir::toNativeSeparators(segmentPath))
                             .arg(loadTimer.elapsed())
                             .arg(currentNode->loaded)
                             .arg(currentNode->children.size()));
            ++missingLoads;
        }

        Node *match = findChild(currentNode, segmentPath);
        if (!match) {
            traceTreeNav("nodeForPath-end", segmentPath,
                         QStringLiteral("result=%1 reason=no-match nearest=%2 elapsedMs=%3")
                             .arg(returnNearestLoaded ? "nearest" : "null")
                             .arg(QDir::toNativeSeparators(currentNode->path))
                             .arg(totalTimer.elapsed()));
            return returnNearestLoaded ? currentNode : nullptr;
        }
        currentNode = match;
    }

    traceTreeNav("nodeForPath-end", normalized,
                 QStringLiteral("result=exact elapsedMs=%1 loads=%2")
                     .arg(totalTimer.elapsed())
                     .arg(missingLoads));
    return currentNode;
}

void TreeModel::clear()
{
    ++m_loadGeneration;
    cancelLoads(&m_root);
    m_pendingRevealPath.clear();
    m_pendingRevealRequestId = -1;
    for (DirectoryChangeWatcher *watcher : std::as_const(m_watchers)) {
        if (watcher) {
            watcher->stop();
            delete watcher;
        }
    }
    m_watchers.clear();
    m_watchedPaths.clear();
    m_pendingRefreshPaths.clear();
    m_root.children.clear();
    m_root.loaded = false;
    m_root.loading = false;
    m_root.canFetch = true;
    m_root.loadGeneration = 0;
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

    if (m_volumeMonitor) {
        for (const VolumeInfo &volume : m_volumeMonitor->volumes()) {
            if (!volume.isReady) {
                continue;
            }

            const QString path = volume.rootPath;
            if (path.isEmpty() || !m_provider->pathExists(path) || !m_provider->isDirectory(path)) {
                continue;
            }

            const QString normalized = m_provider->normalizedPath(path);
            if (seenPaths.contains(normalized)) {
                continue;
            }
            seenPaths.insert(normalized);

            QString name = volume.displayName.isEmpty() ? DriveUtils::rootDisplayName(path) : volume.displayName;
            if (name.isEmpty()) {
                name = normalized;
            }
            if (m_isoMountManager && m_isoMountManager->isManagedMountRoot(normalized)) {
                const IsoMountManager::Mount mount = m_isoMountManager->mountForRoot(normalized);
                const QString rootName = DriveUtils::rootDisplayName(normalized);
                if (name.isEmpty() || pathEquals(name, rootName) || pathEquals(name, normalized)) {
                    name = treeIsoMountDisplayName(mount);
                }
            }
            m_root.children.push_back(makeNode(&m_root, name, normalized, QStringLiteral("drive"), true));
            watchNode(m_root.children.back().get());
        }
    } else {
        for (const QStorageInfo &storage : QStorageInfo::mountedVolumes()) {
            if (!storage.isValid() || !storage.isReady()) {
                continue;
            }
#ifndef Q_OS_WIN
            if (!treeIsLinuxUserFacingMount(storage)) {
                continue;
            }
#endif

            const QString path = storage.rootPath();
            if (path.isEmpty() || !m_provider->pathExists(path) || !m_provider->isDirectory(path)) {
                continue;
            }

            const QString normalized = m_provider->normalizedPath(path);
            if (seenPaths.contains(normalized)) {
                continue;
            }
            seenPaths.insert(normalized);

            QString name = DriveUtils::volumeDisplayName(storage);
            if (name.isEmpty()) {
                name = normalized;
            }
            if (m_isoMountManager && m_isoMountManager->isManagedMountRoot(normalized)) {
                const IsoMountManager::Mount mount = m_isoMountManager->mountForRoot(normalized);
                const QString rootName = DriveUtils::rootDisplayName(normalized);
                if (name.isEmpty() || pathEquals(name, rootName) || pathEquals(name, normalized)) {
                    name = treeIsoMountDisplayName(mount);
                }
            }
            m_root.children.push_back(makeNode(&m_root, name, normalized, QStringLiteral("drive"), true));
            watchNode(m_root.children.back().get());
        }
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

            const QString name = treeIsoMountDisplayName(mount);
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

    m_root.loaded = true;
    m_root.loading = false;
    m_root.canFetch = false;
    m_root.loadGeneration = 0;
}

void TreeModel::loadChildren(Node *node, int revealRequestId)
{
    if (!node || node->loaded || node->loading || !node->canFetch) {
        return;
    }

    node->loading = true;
    node->loadGeneration = ++m_loadGeneration;
    node->loadRevealRequestId = revealRequestId;
    node->loadCancelled = std::make_shared<std::atomic_bool>(false);
    const QString path = node->path;
    const bool showHidden = m_showHidden;
    const quint64 generation = node->loadGeneration;
    const auto cancelled = node->loadCancelled;
    QPointer<TreeModel> self(this);
    const QModelIndex nodeIndex = indexForNode(node);
    if (nodeIndex.isValid()) {
        emit dataChanged(nodeIndex, nodeIndex, {LoadingRole});
    }

    traceTreeNav("loadChildren-async-begin", path,
                 QStringLiteral("showHidden=%1 canFetch=%2")
                     .arg(showHidden)
                     .arg(node->canFetch));

    (void)QtConcurrent::run([self, path, showHidden, generation, cancelled]() {
        QElapsedTimer timer;
        timer.start();
        const QVector<ChildEntry> children = TreeModel::loadChildEntries(path, showHidden, cancelled);
        if (cancelled && cancelled->load()) {
            traceTreeNav("loadChildren-worker-cancelled", path,
                         QStringLiteral("generation=%1 elapsedMs=%2")
                             .arg(generation)
                             .arg(timer.elapsed()));
            return;
        }
        traceTreeNav("loadChildren-worker-finished", path,
                     QStringLiteral("generation=%1 children=%2 elapsedMs=%3")
                         .arg(generation)
                         .arg(children.size())
                         .arg(timer.elapsed()));
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, path, showHidden, generation, children]() {
            if (!self) {
                return;
            }
            self->applyLoadedChildren(path, showHidden, generation, children);
        }, Qt::QueuedConnection);
    });
}

void TreeModel::applyLoadedChildren(const QString &path,
                                    bool showHidden,
                                    quint64 generation,
                                    const QVector<ChildEntry> &children)
{
    Node *node = nodeForPath(path, 0);
    if (!node || !node->loading || node->loadGeneration != generation) {
        traceTreeNav("loadChildren-apply-drop", path,
                     QStringLiteral("generation=%1 reason=stale").arg(generation));
        return;
    }

    if (showHidden != m_showHidden) {
        const int revealRequestId = node->loadRevealRequestId;
        node->loading = false;
        node->loadGeneration = 0;
        node->loadRevealRequestId = -1;
        node->loadCancelled.reset();
        traceTreeNav("loadChildren-apply-retry", path,
                     QStringLiteral("generation=%1 reason=showHiddenChanged").arg(generation));
        loadChildren(node, revealRequestId);
        return;
    }

    node->loading = false;
    node->loadGeneration = 0;
    node->loadRevealRequestId = -1;
    node->loadCancelled.reset();
    node->loaded = true;
    const QModelIndex parentIndex = indexForNode(node);
    if (parentIndex.isValid()) {
        emit dataChanged(parentIndex, parentIndex, {LoadingRole});
    }

    if (children.isEmpty()) {
        node->canFetch = false;
        watchNode(node);
        traceTreeNav("loadChildren-apply-end", path,
                     QStringLiteral("children=0 generation=%1").arg(generation));
        continuePendingReveal();
        return;
    }

    const int lastRow = children.size() - 1;
    beginInsertRows(parentIndex, 0, lastRow);
    node->children.reserve(static_cast<size_t>(children.size()));
    for (const ChildEntry &entry : children) {
        auto child = std::make_unique<Node>();
        child->parent = node;
        child->name = entry.name;
        child->path = entry.path;
        child->icon = entry.icon;
        child->isDrive = entry.isDrive;
        node->children.push_back(std::move(child));
    }
    endInsertRows();
    watchNode(node);
    traceTreeNav("loadChildren-apply-end", path,
                 QStringLiteral("children=%1 generation=%2")
                     .arg(node->children.size())
                     .arg(generation));
    continuePendingReveal();
}

void TreeModel::continuePendingReveal()
{
    if (m_pendingRevealRequestId < 0 || m_pendingRevealPath.isEmpty()) {
        return;
    }

    const QString targetPath = m_pendingRevealPath;
    const int requestId = m_pendingRevealRequestId;
    QElapsedTimer timer;
    timer.start();

    Node *node = nodeForPath(targetPath, 0, true);
    if (!node) {
        m_pendingRevealPath.clear();
        m_pendingRevealRequestId = -1;
        traceTreeNav("revealPathAsync-end", targetPath,
                     QStringLiteral("requestId=%1 result=null elapsedMs=%2")
                         .arg(requestId)
                         .arg(timer.elapsed()));
        emit pathRevealReady(requestId, QModelIndex(), false);
        return;
    }

    const QString normalized = m_provider->normalizedPath(targetPath);
    const bool exact = !normalized.isEmpty() && treePathEquals(normalized, node->path);
    if (exact) {
        const QModelIndex index = indexForNode(node);
        m_pendingRevealPath.clear();
        m_pendingRevealRequestId = -1;
        traceTreeNav("revealPathAsync-end", targetPath,
                     QStringLiteral("requestId=%1 result=exact node=%2 elapsedMs=%3")
                         .arg(requestId)
                         .arg(QDir::toNativeSeparators(node->path))
                         .arg(timer.elapsed()));
        emit pathRevealReady(requestId, index, true);
        return;
    }

    if (!node->loaded && node->canFetch) {
        traceTreeNav("revealPathAsync-wait", targetPath,
                     QStringLiteral("requestId=%1 loading=%2 nearest=%3 elapsedMs=%4")
                         .arg(requestId)
                         .arg(node->loading)
                         .arg(QDir::toNativeSeparators(node->path))
                         .arg(timer.elapsed()));
        if (!node->loading) {
            loadChildren(node, requestId);
        }
        return;
    }

    const QModelIndex index = indexForNode(node);
    m_pendingRevealPath.clear();
    m_pendingRevealRequestId = -1;
    traceTreeNav("revealPathAsync-end", targetPath,
                 QStringLiteral("requestId=%1 result=nearest node=%2 elapsedMs=%3")
                     .arg(requestId)
                     .arg(QDir::toNativeSeparators(node->path))
                     .arg(timer.elapsed()));
    emit pathRevealReady(requestId, index, false);
}

void TreeModel::refreshChildren(Node *node)
{
    if (!node || node == &m_root) {
        return;
    }

    cancelNodeLoad(node, true);
    node->loading = true;
    node->loadGeneration = ++m_loadGeneration;
    node->loadRevealRequestId = -1;
    node->loadCancelled = std::make_shared<std::atomic_bool>(false);
    const QString path = node->path;
    const bool showHidden = m_showHidden;
    const quint64 generation = node->loadGeneration;
    const auto cancelled = node->loadCancelled;
    const QModelIndex nodeIndex = indexForNode(node);
    if (nodeIndex.isValid()) {
        emit dataChanged(nodeIndex, nodeIndex, {LoadingRole});
    }

    QPointer<TreeModel> self(this);
    traceTreeNav("refreshChildren-async-begin", path,
                 QStringLiteral("showHidden=%1").arg(showHidden));

    (void)QtConcurrent::run([self, path, showHidden, generation, cancelled]() {
        QElapsedTimer timer;
        timer.start();
        const QVector<ChildEntry> children = TreeModel::loadChildEntries(path, showHidden, cancelled);
        if (cancelled && cancelled->load()) {
            traceTreeNav("refreshChildren-worker-cancelled", path,
                         QStringLiteral("generation=%1 elapsedMs=%2")
                             .arg(generation)
                             .arg(timer.elapsed()));
            return;
        }
        traceTreeNav("refreshChildren-worker-finished", path,
                     QStringLiteral("generation=%1 children=%2 elapsedMs=%3")
                         .arg(generation)
                         .arg(children.size())
                         .arg(timer.elapsed()));
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, path, showHidden, generation, children]() {
            if (!self) {
                return;
            }
            self->applyRefreshedChildren(path, showHidden, generation, children);
        }, Qt::QueuedConnection);
    });
}

void TreeModel::applyRefreshedChildren(const QString &path,
                                       bool showHidden,
                                       quint64 generation,
                                       const QVector<ChildEntry> &children)
{
    Node *node = nodeForPath(path, 0);
    if (!node || !node->loading || node->loadGeneration != generation) {
        traceTreeNav("refreshChildren-apply-drop", path,
                     QStringLiteral("generation=%1 reason=stale").arg(generation));
        return;
    }

    if (showHidden != m_showHidden) {
        node->loading = false;
        node->loadGeneration = 0;
        node->loadRevealRequestId = -1;
        node->loadCancelled.reset();
        traceTreeNav("refreshChildren-apply-retry", path,
                     QStringLiteral("generation=%1 reason=showHiddenChanged").arg(generation));
        refreshChildren(node);
        return;
    }

    node->loading = false;
    node->loadGeneration = 0;
    node->loadRevealRequestId = -1;
    node->loadCancelled.reset();
    const QModelIndex nodeIndex = indexForNode(node);
    if (nodeIndex.isValid()) {
        emit dataChanged(nodeIndex, nodeIndex, {LoadingRole});
    }

    std::vector<std::unique_ptr<Node>> oldChildren = std::move(node->children);
    std::vector<std::unique_ptr<Node>> newChildren;
    newChildren.reserve(static_cast<size_t>(children.size()));

    auto takeChild = [&oldChildren](const QString &childPath) -> std::unique_ptr<Node> {
        for (auto it = oldChildren.begin(); it != oldChildren.end(); ++it) {
            if (*it && pathEquals((*it)->path, childPath)) {
                std::unique_ptr<Node> ret = std::move(*it);
                oldChildren.erase(it);
                return ret;
            }
        }
        return nullptr;
    };

    for (const ChildEntry &entry : children) {
        std::unique_ptr<Node> child = takeChild(entry.path);
        if (child) {
            child->parent = node;
            child->name = entry.name;
            child->path = entry.path;
            child->icon = entry.icon;
            child->isDrive = entry.isDrive;
        } else {
            child = std::make_unique<Node>();
            child->parent = node;
            child->name = entry.name;
            child->path = entry.path;
            child->icon = entry.icon;
            child->isDrive = entry.isDrive;
        }
        newChildren.push_back(std::move(child));
    }

    for (auto &child : oldChildren) {
        if (child) {
            unwatchSubtree(child.get());
        }
    }

    layoutAboutToBeChanged();
    node->children = std::move(newChildren);
    node->loaded = true;
    node->canFetch = !node->children.empty();
    layoutChanged();

    watchNode(node);
    traceTreeNav("refreshChildren-apply-end", path,
                 QStringLiteral("children=%1 generation=%2")
                     .arg(node->children.size())
                     .arg(generation));
    continuePendingReveal();
}

QVector<TreeModel::ChildEntry> TreeModel::loadChildEntries(const QString &path,
                                                           bool showHidden,
                                                           const std::shared_ptr<std::atomic_bool> &cancelled)
{
    QVector<ChildEntry> children;
    if (cancelled && cancelled->load()) {
        return children;
    }

#ifdef Q_OS_WIN
    QString outputBase = QDir::fromNativeSeparators(path);
    if (!outputBase.endsWith(QLatin1Char('/'))) {
        outputBase += QLatin1Char('/');
    }

    WIN32_FIND_DATAW findData;
    const QString pattern = treeModelFindPattern(path);
    HANDLE handle = FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()),
                                     FindExInfoBasic,
                                     &findData,
                                     FindExSearchNameMatch,
                                     nullptr,
                                     FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER) {
        handle = FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()),
                                  FindExInfoBasic,
                                  &findData,
                                  FindExSearchNameMatch,
                                  nullptr,
                                  0);
    }
    if (handle != INVALID_HANDLE_VALUE) {
        do {
            if (cancelled && cancelled->load()) {
                FindClose(handle);
                return {};
            }

            const QString name = QString::fromWCharArray(findData.cFileName);
            if (name == QLatin1String(".") || name == QLatin1String("..")) {
                continue;
            }
            const DWORD attributes = findData.dwFileAttributes;
            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
                continue;
            }
            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
                continue;
            }
            if (!showHidden && ((attributes & FILE_ATTRIBUTE_HIDDEN) != 0 || name.startsWith(QLatin1Char('.')))) {
                continue;
            }

            ChildEntry entry;
            entry.name = name;
            entry.path = QDir::cleanPath(outputBase + name);
            entry.icon = QStringLiteral("folder");
            children.append(entry);
        } while (FindNextFileW(handle, &findData));

        FindClose(handle);
        std::sort(children.begin(), children.end(), [](const ChildEntry &lhs, const ChildEntry &rhs) {
            return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
        });
        return children;
    }
#endif

    LocalFileProvider provider;
    if (cancelled && cancelled->load()) {
        return {};
    }
    if (!provider.pathExists(path) || !provider.isDirectory(path)) {
        return children;
    }

    const QStringList paths = provider.childPaths(path, showHidden);
    children.reserve(paths.size());
    for (const QString &childPath : paths) {
        if (cancelled && cancelled->load()) {
            return {};
        }

        if (!provider.isDirectory(childPath) || provider.isSymLink(childPath)) {
            continue;
        }

        const QString normalized = provider.normalizedPath(childPath);
        const QString name = provider.fileName(normalized);
        if (name.isEmpty()) {
            continue;
        }

        ChildEntry entry;
        entry.name = name;
        entry.path = normalized;
        entry.icon = QStringLiteral("folder");
        children.append(entry);
    }

    std::sort(children.begin(), children.end(), [](const ChildEntry &lhs, const ChildEntry &rhs) {
        return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
    });
    return children;
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
#ifdef Q_OS_WIN
    Q_UNUSED(node)
    return;
#else
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
#ifndef Q_OS_WIN
    if (!QFileInfo(normalized).isReadable()) {
        return;
    }
#endif

    std::unique_ptr<DirectoryChangeWatcher> watcher = std::make_unique<QtDirectoryChangeWatcher>(this);
    DirectoryChangeWatcher *watcherPtr = watcher.get();
    connect(watcherPtr, &DirectoryChangeWatcher::eventsReady,
            this, &TreeModel::onWatcherEventsReady);
    connect(watcherPtr, &DirectoryChangeWatcher::watchFailed,
            this, &TreeModel::onWatcherFailed);

    if (watcherPtr->watch(normalized)) {
        m_watchers.insert(normalized, watcher.release());
        m_watchedPaths.insert(normalized);
        if (treeWatchDebugEnabled()) {
            qDebug() << "[TreeWatch] watch" << normalized;
        }
    } else if (treeWatchDebugEnabled()) {
        qDebug() << "[TreeWatch] watch-failed" << normalized;
    }
#endif
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

    DirectoryChangeWatcher *watcher = m_watchers.take(normalized);
    if (watcher) {
        watcher->stop();
        delete watcher;
    }
    m_watchedPaths.remove(normalized);
    m_pendingRefreshPaths.remove(normalized);
    if (treeWatchDebugEnabled()) {
        qDebug() << "[TreeWatch] unwatch" << normalized;
    }
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
            DirectoryChangeWatcher *watcher = m_watchers.take(path);
            if (watcher) {
                watcher->stop();
                delete watcher;
            }
            m_watchedPaths.remove(path);
            m_pendingRefreshPaths.remove(path);
        }
    }
}

void TreeModel::onWatcherEventsReady(const QList<DirectoryChangeEvent> &events)
{
    for (const DirectoryChangeEvent &event : events) {
        scheduleRefreshForEvent(event);
    }
}

void TreeModel::onWatcherFailed(const QString &path, const QString &error)
{
    const QString normalized = m_provider->normalizedPath(path);
    DirectoryChangeWatcher *watcher = m_watchers.take(normalized);
    if (watcher) {
        watcher->stop();
        delete watcher;
    }
    m_watchedPaths.remove(normalized);
    m_pendingRefreshPaths.remove(normalized);
    scheduleRefresh(m_provider->parentPath(normalized));

    if (treeWatchDebugEnabled()) {
        qDebug() << "[TreeWatch] failed" << normalized << error;
    }
}

void TreeModel::scheduleRefreshForEvent(const DirectoryChangeEvent &event)
{
    if (event.type == DirectoryChangeEvent::Type::Overflow) {
        scheduleRefresh(event.path);
        return;
    }

    if (event.type == DirectoryChangeEvent::Type::Renamed) {
        if (!event.oldPath.isEmpty()) {
            scheduleRefresh(m_provider->parentPath(event.oldPath));
        }
        if (!event.newPath.isEmpty()) {
            scheduleRefresh(m_provider->parentPath(event.newPath));
        }
        return;
    }

    if (!event.path.isEmpty()) {
        scheduleRefresh(m_provider->parentPath(event.path));
    }
}

void TreeModel::scheduleRefresh(const QString &path)
{
    const QString normalized = m_provider->normalizedPath(path);
    if (normalized.isEmpty()) {
        return;
    }
    if (!m_provider->pathExists(normalized) || !m_provider->isDirectory(normalized)) {
        DirectoryChangeWatcher *watcher = m_watchers.take(normalized);
        if (watcher) {
            watcher->stop();
            delete watcher;
        }
        m_watchedPaths.remove(normalized);
        m_pendingRefreshPaths.remove(normalized);
        return;
    }

    m_pendingRefreshPaths.insert(normalized);
    if (!m_refreshTimer.isActive()) {
        m_refreshTimer.start();
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
