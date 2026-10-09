#include "FolderMapItem.h"
#include "FileTypeIconResolver.h"
#include "IconProvider.h"
#include "MapGpuProbe.h"
#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "IsoSupport.h"
#include "DirectoryChangeWatcher.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QFontMetricsF>
#include <QFutureWatcher>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QSGSimpleTextureNode>
#include <private/qquickpixmap_p.h>
#include <QPainter>
#include <QSGGeometryNode>
#include <QSGTextNode>
#include <QSGTextureMaterial>
#include <QSGVertexColorMaterial>
#include <QSet>
#include <QTextLayout>
#include <QSvgRenderer>
#include <QtConcurrentRun>
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int MaximumEntries = 4096;
constexpr int MaximumNodes = 40000;
constexpr int MaximumVisible = 2048;
constexpr qreal HeaderFraction = 0.09;
constexpr qreal PaddingFraction = 0.018;
constexpr int IconSize = 64;
constexpr int IconStride = 72;
constexpr int IconPadding = (IconStride - IconSize) / 2;
constexpr int AtlasColumns = 4;
constexpr int EmptyFolderIcon = 16;
constexpr int MaximumThumbnails = 64;
constexpr int MaximumThumbnailRequests = 2;
constexpr int MaximumWatches = 32;
constexpr int ThumbnailSize = 256;

const QStringList &iconNames()
{
    static const QStringList names{QStringLiteral("archive"), QStringLiteral("code"),
        QStringLiteral("document"), QStringLiteral("epub"), QStringLiteral("executable"),
        QStringLiteral("fb2"), QStringLiteral("folder"), QStringLiteral("font"),
        QStringLiteral("image"), QStringLiteral("music"), QStringLiteral("pdf"),
        QStringLiteral("presentation"), QStringLiteral("shortcut"), QStringLiteral("spreadsheet"),
        QStringLiteral("text"), QStringLiteral("video")};
    return names;
}

QImage createIconAtlas(const std::shared_ptr<IconProvider> &provider)
{
    QImage image(AtlasColumns * IconStride, 5 * IconStride, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    const QStringList suffixes{QStringLiteral("zip"), QStringLiteral("cpp"),
        QStringLiteral("docx"), QStringLiteral("epub"), QStringLiteral("exe"),
        QStringLiteral("fb2"), QString(), QStringLiteral("ttf"),
        QStringLiteral("jpg"), QStringLiteral("mp3"), QStringLiteral("pdf"),
        QStringLiteral("pptx"), QStringLiteral("lnk"), QStringLiteral("xlsx"),
        QStringLiteral("txt"), QStringLiteral("mp4")};
    for (int i = 0; i < iconNames().size(); ++i) {
        const QRectF target((i % AtlasColumns) * IconStride + IconPadding,
                            (i / AtlasColumns) * IconStride + IconPadding, IconSize, IconSize);
        if (provider) {
            // Type identities only: no file I/O or per-entry uploads during zoom.
            const QString id = suffixes[i].isEmpty()
                ? QStringLiteral("folder-map-folder?directory=true&generic=true")
                : QStringLiteral("folder-map-file.%1?generic=true").arg(suffixes[i]);
            const QImage icon = provider->requestImage(id, nullptr, QSize(IconSize, IconSize));
            if (!icon.isNull()) {
                painter.drawImage(target, icon);
                continue;
            }
        }
        QSvgRenderer renderer(QStringLiteral(":/qt/qml/FM/qml/assets/filetypes-next/%1.svg").arg(iconNames()[i]));
        if (!renderer.isValid()) return {};
        renderer.render(&painter, target);
    }
    QSvgRenderer emptyFolder(QStringLiteral(":/qt/qml/FM/qml/assets/icons-classic/folder.svg"));
    if (!emptyFolder.isValid()) return {};
    painter.setOpacity(0.4);
    emptyFolder.render(&painter, QRectF(IconPadding, 4 * IconStride + IconPadding, IconSize, IconSize));
    return image;
}

std::array<QFuture<QImage>, 2> &iconAtlasFutures()
{
    static std::array<QFuture<QImage>, 2> futures;
    return futures;
}

QFuture<QImage> iconAtlasFuture(bool native)
{
    // Called on the GUI thread. Split panels share preparation rather than racing Qt's theme icon cache.
    auto &futures = iconAtlasFutures();
    static bool requested[2]{};
    if (!requested[native]) {
        requested[native] = true;
        const auto provider = native ? std::make_shared<IconProvider>() : std::shared_ptr<IconProvider>{};
        futures[native] = QtConcurrent::run([provider] { return createIconAtlas(provider); });
    }
    return futures[native];
}

struct MapLabel {
    QSGTextNode *node = nullptr;
    QString text;
    QString source;
    qreal naturalWidth = 0;
    QColor color;
};

struct MapScene : QSGNode {
    QSGGeometryNode *tiles = nullptr;
    QHash<int, MapLabel> labels;
    QHash<int, MapLabel> emptyLabels;
    QList<QSGGeometry::ColoredPoint2D> vertices;
    QSGGeometryNode *icons = nullptr;
    std::unique_ptr<QSGTexture> atlas;
    qint64 atlasKey = 0;
    struct Preview { QSGSimpleTextureNode *node = nullptr; qint64 key = 0; };
    QHash<QString, Preview> previews;
    QList<QSGGeometry::TexturedPoint2D> iconVertices;
};

void appendQuad(QList<QSGGeometry::ColoredPoint2D> &vertices, const QRectF &rect, const QColor &color)
{
    const QPointF points[]{rect.topLeft(), rect.topRight(), rect.bottomLeft(),
                           rect.bottomLeft(), rect.topRight(), rect.bottomRight()};
    for (const auto &point : points) {
        QSGGeometry::ColoredPoint2D vertex;
        vertex.set(float(point.x()), float(point.y()), color.red() * color.alpha() / 255,
                   color.green() * color.alpha() / 255, color.blue() * color.alpha() / 255, color.alpha());
        vertices.append(vertex);
    }
}

QColor tinted(const QColor &base, const QColor &tint, qreal amount)
{
    return QColor::fromRgbF(base.redF() + (tint.redF() - base.redF()) * amount,
                            base.greenF() + (tint.greenF() - base.greenF()) * amount,
                            base.blueF() + (tint.blueF() - base.blueF()) * amount, base.alphaF());
}

void appendGradientQuad(QList<QSGGeometry::ColoredPoint2D> &vertices, const QRectF &rect,
                        const QColor &top, const QColor &bottom)
{
    const QPointF points[]{rect.topLeft(), rect.topRight(), rect.bottomLeft(),
                           rect.bottomLeft(), rect.topRight(), rect.bottomRight()};
    for (int i = 0; i < 6; ++i) {
        const QColor &color = (i == 0 || i == 1 || i == 4) ? top : bottom;
        QSGGeometry::ColoredPoint2D vertex;
        vertex.set(float(points[i].x()), float(points[i].y()), color.red() * color.alpha() / 255,
                   color.green() * color.alpha() / 255, color.blue() * color.alpha() / 255, color.alpha());
        vertices.append(vertex);
    }
}
}

struct FolderMapItem::Thumbnail {
    QQuickPixmap pixmap;
    QImage image;
    bool finished = false;
};

FolderMapItem::FolderMapItem(QQuickItem *parent) : QQuickItem(parent)
{
    setFlag(ItemHasContents);
    setClip(true);
    m_modelTimer.setSingleShot(true);
    m_modelTimer.setInterval(50);
    connect(&m_modelTimer, &QTimer::timeout, this, &FolderMapItem::syncModel);
    m_watchTimer.setSingleShot(true);
    m_watchTimer.setInterval(75);
    connect(&m_watchTimer, &QTimer::timeout, this, &FolderMapItem::reloadChangedFolders);
    connect(this, &FolderMapItem::paletteChanged, this, &FolderMapItem::update);
    connect(this, &QQuickItem::visibleChanged, this, [this] {
        if (isVisible()) { queueModelSync(); polish(); }
        else { clearThumbnails(); m_watchers.clear(); }
    });
    const auto attach = [this](QQuickWindow *window) {
        disconnect(m_afterAnimating);
        if (!window) return;
        m_afterAnimating = connect(window, &QQuickWindow::afterAnimating, this, [this] {
            if (m_moving && isVisible()) polish();
        });
    };
    connect(this, &QQuickItem::windowChanged, this, attach);
    attach(window());
}

void FolderMapItem::waitForIconAtlases()
{
    // Drain the shared workers while QIcon/QPixmap still have a live GUI application.
    for (auto &future : iconAtlasFutures()) future.waitForFinished();
}

FolderMapItem::~FolderMapItem()
{
    m_cancelled->store(true);
    clearThumbnails();
}

void FolderMapItem::setDirectoryModel(QAbstractItemModel *model)
{
    if (m_directoryModel == model) return;
    for (const auto &connection : m_modelConnections) disconnect(connection);
    m_modelConnections.clear();
    m_directoryModel = model;
    if (model) {
        const auto schedule = [this] { queueModelSync(); };
        m_modelConnections.append(connect(model, &QAbstractItemModel::modelReset, this, schedule));
        m_modelConnections.append(connect(model, &QAbstractItemModel::rowsInserted, this, schedule));
        m_modelConnections.append(connect(model, &QAbstractItemModel::rowsRemoved, this, schedule));
        m_modelConnections.append(connect(model, &QAbstractItemModel::rowsMoved, this, schedule));
        m_modelConnections.append(connect(model, &QAbstractItemModel::layoutChanged, this, schedule));
        m_modelConnections.append(connect(model, &QAbstractItemModel::dataChanged, this,
            [this](const QModelIndex &first, const QModelIndex &last, const QList<int> &roles) {
                const auto names = m_directoryModel->roleNames();
                if (roles.size() == 1 && names.value(roles.first()) == "isSelected") return;
                const int pathRole = names.key("path", -1);
                for (int row = first.row(); row <= last.row(); ++row)
                    invalidateThumbnail(m_directoryModel->data(m_directoryModel->index(row, 0), pathRole).toString());
                queueModelSync();
            }));
        if (model->metaObject()->indexOfSignal("loadingChanged()") >= 0)
            m_modelConnections.append(connect(model, SIGNAL(loadingChanged()), this, SLOT(queueModelSync())));
    }
    emit directoryModelChanged();
    if (m_gpuEnabled) resetTree();
}

void FolderMapItem::queueModelSync()
{
    if (!m_gpuEnabled) return;
    if (!m_modelTimer.isActive()) m_modelTimer.start();
}

void FolderMapItem::syncModel()
{
    if (!m_directoryModel || !m_gpuEnabled || !isVisible() || !m_byPath.contains(m_currentPath)) return;
    const QVariant modelPath = m_directoryModel->property("currentPath");
    if ((modelPath.isValid() && modelPath.toString() != m_currentPath)
        || m_directoryModel->property("loading").toBool()) return;
    const auto roles = m_directoryModel->roleNames();
    const auto value = [this, &roles](int row, const char *name) {
        const int role = roles.key(name, -1);
        return role < 0 ? QVariant{} : m_directoryModel->data(m_directoryModel->index(row, 0), role);
    };
    Listing listing;
    FileTypeIconResolver resolver;
    const int count = std::min(m_directoryModel->rowCount(), MaximumEntries);
    for (int row = 0; row < count; ++row) {
        Entry entry;
        entry.path = value(row, "path").toString();
        if (entry.path.isEmpty()) continue;
        entry.name = value(row, "name").toString();
        entry.directory = value(row, "isDirectory").toBool();
        entry.symlink = value(row, "isSymLink").toBool();
        QString suffix = value(row, "suffix").toString();
        if (entry.path.endsWith(QStringLiteral(".fb2.zip"), Qt::CaseInsensitive)) suffix = QStringLiteral("fb2.zip");
        const QString source = resolver.iconForSuffix(suffix, entry.directory);
        entry.icon = iconNames().indexOf(QFileInfo(source).baseName());
        listing.entries.append(entry);
    }
    if (m_directoryModel->rowCount() > count) listing.message = QStringLiteral("Listing limited to 4096 entries; zoom or use another view");
    const int index = m_byPath.value(m_currentPath);
    m_nodes[index].listingRevision = ++m_listingSequence;
    applyListing(index, listing);
    emit treeChanged();
    emit entriesChanged();
    polish();
    update();
}

void FolderMapItem::invalidateThumbnail(const QString &path)
{
    const auto found = m_thumbnails.find(path);
    if (found == m_thumbnails.end()) return;
    found.value()->pixmap.clear(this);
    m_thumbnails.erase(found);
    emit thumbnailsChanged();
}

void FolderMapItem::remapEntry(const QString &oldPath, const QString &newPath)
{
    if (oldPath == newPath || !m_byPath.contains(oldPath)) return;
    const int parent = m_nodes[m_byPath.value(oldPath)].parent;
    if (parent >= 0) {
        m_nodes[parent].listingRevision = ++m_listingSequence;
        const QString parentPath = m_nodes[parent].entry.path;
        if (m_directoryModel && parentPath == m_currentPath) queueModelSync();
        else {
            m_dirtyFolders.insert(parentPath);
            m_watchTimer.start();
        }
    }
    QList<int> pending{m_byPath.value(oldPath)};
    while (!pending.isEmpty()) {
        const int index = pending.takeLast();
        auto &node = m_nodes[index];
        const QString path = node.entry.path;
        pending.append(node.children);
        m_byPath.remove(path);
        invalidateThumbnail(path);
        m_watchers.remove(path);
        m_watchFailures.remove(path);
        m_dirtyFolders.remove(path);
        node.entry.path = newPath + path.mid(oldPath.size());
        if (path == oldPath) node.entry.name = QFileInfo(newPath).fileName();
        node.listingRevision = ++m_listingSequence;
        if (!node.loaded) node.requested = false;
        m_byPath.insert(node.entry.path, index);
    }
    emit treeChanged();
    polish();
    update();
}

void FolderMapItem::removeBranch(int index)
{
    const QList<int> children = m_nodes[index].children;
    for (int child : children) removeBranch(child);
    const QString path = m_nodes[index].entry.path;
    invalidateThumbnail(path);
    m_watchers.remove(path);
    m_watchFailures.remove(path);
    m_byPath.remove(path);
    m_nodes[index] = Node{};
    m_freeNodes.append(index);
}

void FolderMapItem::applyListing(int index, const Listing &listing)
{
    QHash<QString, int> previous;
    for (int child : m_nodes[index].children) previous.insert(m_nodes[child].entry.path, child);
    QSet<QString> listed;
    for (const Entry &entry : listing.entries) listed.insert(entry.path);
    for (auto it = previous.begin(); it != previous.end();) {
        if (listed.contains(it.key())) { ++it; continue; }
        removeBranch(it.value());
        it = previous.erase(it);
    }
    QList<int> children;
    for (const Entry &entry : listing.entries) {
        int child = previous.take(entry.path);
        // QHash::take returns zero for a miss; the root is never a child.
        if (child && (m_nodes[child].entry.directory != entry.directory || m_nodes[child].entry.symlink != entry.symlink)) {
            removeBranch(child);
            child = 0;
        }
        if (!child) {
            if (m_byPath.size() >= MaximumNodes) continue;
            if (m_freeNodes.isEmpty()) {
                child = int(m_nodes.size());
                m_nodes.append(Node{});
            } else child = m_freeNodes.takeLast();
        }
        m_nodes[child].entry = entry;
        m_nodes[child].parent = index;
        m_byPath.insert(entry.path, child);
        children.append(child);
    }
    for (int child : previous) removeBranch(child);
    m_nodes[index].children = children;
    m_nodes[index].loaded = true;
    m_nodes[index].requested = true;
    m_nodes[index].message = children.size() < listing.entries.size()
        ? QStringLiteral("Map node limit reached; use another view for the complete folder") : listing.message;
    layoutChildren(index);
    constrainCamera(m_center, m_scale);
    constrainCamera(m_targetCenter, m_targetScale);
}

void FolderMapItem::layoutChildren(int index)
{
    const QRectF parent = m_nodes[index].rect;
    const qreal padding = parent.width() * PaddingFraction;
    const QRectF content = parent.adjusted(padding, parent.height() * HeaderFraction, -padding, -padding);
    const QList<int> children = m_nodes[index].children;
    const int count = int(children.size());
    const int columns = std::max(1, int(std::ceil(std::sqrt(count * content.width() / content.height() / 1.4))));
    const int rows = std::max(1, (count + columns - 1) / columns);
    m_nodes[index].columns = columns;
    const qreal cellWidth = content.width() / columns;
    const qreal cellHeight = content.height() / rows;
    const qreal gap = std::min(cellWidth, cellHeight) * 0.035;
    for (int i = 0; i < count; ++i) {
        const int child = children[i];
        m_nodes[child].rect = QRectF(content.x() + (i % columns) * cellWidth + gap,
            content.y() + (i / columns) * cellHeight + gap, cellWidth - 2 * gap, cellHeight - 2 * gap);
        if (m_nodes[child].loaded) layoutChildren(child);
    }
}

void FolderMapItem::syncWatchers()
{
    QSet<QString> wanted;
    for (const auto &visible : m_visible) {
        const auto &node = m_nodes[visible.index];
        if (!node.entry.directory || node.entry.symlink || !node.loaded
            || ArchiveSupport::isArchivePath(node.entry.path)
            || (m_directoryModel && node.entry.path == m_currentPath)) continue;
        if (wanted.size() < MaximumWatches) wanted.insert(node.entry.path);
    }
    for (auto it = m_watchers.begin(); it != m_watchers.end();) {
        if (wanted.contains(it.key())) { ++it; continue; }
        it = m_watchers.erase(it);
    }
    for (const QString &path : wanted) {
        if (m_watchers.contains(path) || m_watchFailures.contains(path)) continue;
        auto watcher = std::shared_ptr<DirectoryChangeWatcher>(createDirectoryChangeWatcher().release());
        connect(watcher.get(), &DirectoryChangeWatcher::eventsReady, this,
            [this, path](const QList<DirectoryChangeEvent> &events) {
                for (const auto &event : events) {
                    invalidateThumbnail(event.path);
                    invalidateThumbnail(event.oldPath);
                    invalidateThumbnail(event.newPath);
                }
                m_dirtyFolders.insert(path);
                if (!m_watchTimer.isActive()) m_watchTimer.start();
            });
        if (watcher->watch(path)) {
            m_watchers.insert(path, watcher);
            // Reconcile changes made while the branch was outside the watched region.
            m_dirtyFolders.insert(path);
            if (!m_watchTimer.isActive()) m_watchTimer.start();
        } else {
            m_watchFailures.insert(path);
            if (m_byPath.contains(path)) m_nodes[m_byPath.value(path)].message = QStringLiteral("Live updates unavailable");
        }
    }
}

void FolderMapItem::reloadChangedFolders()
{
    if (!m_gpuEnabled || !isVisible()) { m_dirtyFolders.clear(); return; }
    for (auto it = m_dirtyFolders.begin(); it != m_dirtyFolders.end();) {
        const auto found = m_byPath.constFind(*it);
        if (found == m_byPath.constEnd()) { it = m_dirtyFolders.erase(it); continue; }
        if (m_loading >= 2) break;
        m_nodes[*found].requested = false;
        requestListing(*found);
        it = m_dirtyFolders.erase(it);
    }
    if (!m_dirtyFolders.isEmpty()) m_watchTimer.start();
}

void FolderMapItem::setCurrentPath(const QString &path)
{
    if (path == m_currentPath) return;
    m_currentPath = path;
    clearThumbnails();
    queueModelSync();
    emit currentPathChanged();
    if (!m_gpuEnabled) return;
    ensureIconAtlas();
    if (m_byPath.contains(path) && !m_nodes[m_byPath.value(path)].entry.symlink) {
        constrainCamera(m_center, m_scale);
        constrainCamera(m_targetCenter, m_targetScale);
        startMotion();
    } else {
        resetTree();
    }
}

void FolderMapItem::setShowHidden(bool value)
{
    if (m_showHidden == value) return;
    m_showHidden = value;
    emit showHiddenChanged();
    if (m_gpuEnabled) resetTree();
}

void FolderMapItem::setGpuEnabled(bool value)
{
    if (m_gpuEnabled == value) return;
    m_gpuEnabled = value;
    emit gpuEnabledChanged();
    resetTree();
    if (value && MapGpuProbe::supportsPath(m_currentPath)) ensureIconAtlas();
}

void FolderMapItem::setIconsEnabled(bool enabled)
{
    if (m_iconsEnabled == enabled) return;
    m_iconsEnabled = enabled;
    if (enabled && m_gpuEnabled && MapGpuProbe::supportsPath(m_currentPath)) ensureIconAtlas();
    emit iconsChanged();
    update();
}

void FolderMapItem::setUseNativeIcons(bool enabled)
{
    if (m_useNativeIcons == enabled) return;
    m_useNativeIcons = enabled;
    if (m_gpuEnabled && MapGpuProbe::supportsPath(m_currentPath)) ensureIconAtlas();
    emit useNativeIconsChanged();
    emit iconsChanged();
    update();
}

void FolderMapItem::ensureIconAtlas()
{
    if (!m_iconsEnabled || !MapGpuProbe::supportsPath(m_currentPath)) return;
    const bool native = m_useNativeIcons;
    if (!m_iconAtlases[native].isNull()) {
        m_iconAtlas = m_iconAtlases[native];
        return;
    }
    if (m_atlasRequested[native]) return;
    m_atlasRequested[native] = true;
    auto *watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, native] {
        m_iconAtlases[native] = watcher->result();
        watcher->deleteLater();
        if (m_useNativeIcons != native) return;
        m_iconAtlas = m_iconAtlases[native];
        emit iconsChanged();
        update();
    });
    watcher->setFuture(iconAtlasFuture(native));
}

void FolderMapItem::setShowThumbnails(bool enabled)
{
    if (m_showThumbnails == enabled) return;
    m_showThumbnails = enabled;
    if (!enabled) clearThumbnails();
    emit showThumbnailsChanged();
    polish();
    update();
}

int FolderMapItem::thumbnailsReady() const
{
    int count = 0;
    for (const auto &thumbnail : m_thumbnails) if (!thumbnail->image.isNull()) ++count;
    return count;
}

void FolderMapItem::clearThumbnails()
{
    for (const auto &thumbnail : m_thumbnails) thumbnail->pixmap.clear(this);
    m_thumbnails.clear();
    emit thumbnailsChanged();
}

void FolderMapItem::thumbnailFinished()
{
    for (const auto &thumbnail : m_thumbnails) {
        if (thumbnail->finished || thumbnail->pixmap.isLoading()) continue;
        thumbnail->finished = true;
        if (thumbnail->pixmap.isReady() && thumbnail->pixmap.width() > 1 && thumbnail->pixmap.height() > 1)
            thumbnail->image = thumbnail->pixmap.image();
    }
    emit thumbnailsChanged();
    polish();
    update();
}

void FolderMapItem::scheduleThumbnails()
{
    if (!m_showThumbnails || !m_gpuEnabled || !isVisible()
        || (m_directoryModel && m_directoryModel->property("loading").toBool())) return;
    QQmlEngine *engine = qmlEngine(this);
    if (!engine || !engine->imageProvider(QStringLiteral("thumbnail"))) return;
    QList<const Visible *> candidates;
    for (const auto &visible : m_visible) {
        const auto &entry = m_nodes[visible.index].entry;
        const bool supported = entry.icon == 3 || entry.icon == 5 || entry.icon == 7
            || entry.icon == 8 || entry.icon == 9 || entry.icon == 10 || entry.icon == 15;
        if (!entry.directory && supported && visible.rect.width() >= 64 && visible.rect.height() >= 64)
            candidates.append(&visible);
    }
    std::sort(candidates.begin(), candidates.end(), [this](const Visible *a, const Visible *b) {
        const QPointF da = a->rect.center() - boundingRect().center();
        const QPointF db = b->rect.center() - boundingRect().center();
        return QPointF::dotProduct(da, da) < QPointF::dotProduct(db, db);
    });
    QSet<QString> visiblePaths;
    for (const auto *visible : candidates) visiblePaths.insert(m_nodes[visible->index].entry.path);
    if (candidates.size() > MaximumThumbnails) candidates.resize(MaximumThumbnails);
    QSet<QString> wanted;
    for (const auto *visible : candidates) wanted.insert(m_nodes[visible->index].entry.path);
    int pending = 0;
    for (auto it = m_thumbnails.begin(); it != m_thumbnails.end();) {
        if (!(m_moving ? visiblePaths : wanted).contains(it.key())) {
            it.value()->pixmap.clear(this);
            it = m_thumbnails.erase(it);
        } else {
            if (!it.value()->finished) ++pending;
            ++it;
        }
    }
    // Match the other panel views: movement pauses new decoding, not existing previews.
    if (m_moving) return;
    for (const auto *visible : candidates) {
        if (pending >= MaximumThumbnailRequests) break;
        const QString path = m_nodes[visible->index].entry.path;
        if (m_thumbnails.contains(path)) continue;
        auto thumbnail = std::make_shared<Thumbnail>();
        m_thumbnails.insert(path, thumbnail);
        // Let ThumbnailProvider validate mtime/size; a URL-only Qt image cache can go stale on refresh.
        const QByteArray id = QUrl::toPercentEncoding(path + QStringLiteral("::thumbrev=0"));
        thumbnail->pixmap.load(engine, QUrl(QStringLiteral("image://thumbnail/") + QString::fromLatin1(id)),
            QRect(), QSize(ThumbnailSize, ThumbnailSize), QQuickPixmap::Asynchronous);
        if (thumbnail->pixmap.isLoading()) {
            thumbnail->pixmap.connectFinished(this, SLOT(thumbnailFinished()));
            ++pending;
        } else {
            thumbnail->finished = true;
            if (thumbnail->pixmap.isReady() && thumbnail->pixmap.width() > 1 && thumbnail->pixmap.height() > 1)
                thumbnail->image = thumbnail->pixmap.image();
            emit thumbnailsChanged();
        }
    }
}

void FolderMapItem::setSelectedPaths(const QStringList &paths)
{
    if (m_selectedPaths == paths) return;
    m_selectedPaths = paths;
    m_selection = QSet<QString>(paths.cbegin(), paths.cend());
    emit selectedPathsChanged();
    polish();
}

void FolderMapItem::setStatus(const QString &text)
{
    if (m_status == text) return;
    m_status = text;
    emit statusChanged();
}

void FolderMapItem::refresh()
{
    clearThumbnails();
    if (m_directoryModel) queueModelSync();
    else if (m_byPath.contains(m_currentPath)) {
        m_dirtyFolders.insert(m_currentPath);
        reloadChangedFolders();
    }
}

void FolderMapItem::resetTree()
{
    clearThumbnails();
    m_watchers.clear();
    m_watchFailures.clear();
    m_dirtyFolders.clear();
    m_freeNodes.clear();
    m_watchTimer.stop();
    m_cancelled->store(true);
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    ++m_generation;
    m_nodes.clear();
    m_byPath.clear();
    m_visible.clear();
    m_loading = 0;
    if (!m_gpuEnabled) {
        m_modelTimer.stop();
        m_nodes.squeeze();
        m_visible.squeeze();
        m_freeNodes.squeeze();
    }
    m_cameraInitialized = false;
    if (m_moving) {
        m_moving = false;
        emit movingChanged();
    }
    setStatus({});
    if (m_gpuEnabled && MapGpuProbe::supportsPath(m_currentPath)) {
        Node root;
        root.entry = {m_currentPath, ArchiveSupport::isArchivePath(m_currentPath)
            ? ArchiveSupport::archiveFileName(m_currentPath) : QFileInfo(m_currentPath).fileName(), true, false};
        if (root.entry.name.isEmpty()) root.entry.name = m_currentPath;
        root.rect = QRectF(0, 0, 1024, 768);
        m_nodes.append(root);
        m_byPath.insert(m_currentPath, 0);
        m_center = m_targetCenter = root.rect.center();
        m_scale = m_targetScale = std::max(qreal(0.01), std::min(width() / 1024, height() / 768) * 0.92);
        m_cameraInitialized = width() > 0 && height() > 0;
        requestListing(0);
    }
    emit treeChanged();
    emit entriesChanged();
    polish();
    update();
}

FolderMapItem::Listing FolderMapItem::readDirectory(const QString &path, bool hidden,
                                                    const std::shared_ptr<std::atomic_bool> &cancelled)
{
    Listing result;
    if (ArchiveSupport::isArchivePath(path)) {
        // The panel opens/password-checks archives. Zoom only reads that cached catalogue.
        const auto entries = ArchiveFileProvider::cachedDirectoryEntries(path, hidden, MaximumEntries + 1, cancelled);
        if (!entries) {
            result.message = QStringLiteral("Archive listing is unavailable; reopen the folder");
            return result;
        }
        FileTypeIconResolver resolver;
        for (const auto &entry : *entries) {
            if (cancelled->load()) break;
            if (result.entries.size() == MaximumEntries) {
                result.message = QStringLiteral("Map preview limited to 4096 entries; use another view for the complete folder");
                break;
            }
            const QString source = resolver.iconForSuffix(entry.suffix, entry.isDirectory);
            result.entries.append({entry.path, entry.name, entry.isDirectory, entry.isSymLink,
                int(iconNames().indexOf(QFileInfo(source).baseName()))});
        }
        std::sort(result.entries.begin(), result.entries.end(), [](const Entry &a, const Entry &b) {
            if (a.directory != b.directory) return a.directory;
            return a.name < b.name;
        });
        return result;
    }
    const QFileInfo directory(path);
    if (!directory.isDir() || !directory.isReadable()) {
        result.message = QStringLiteral("Folder is unavailable or access is denied");
        return result;
    }
    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    if (hidden) filters |= QDir::Hidden;
    QDirIterator iterator(path, filters, QDirIterator::NoIteratorFlags);
    FileTypeIconResolver resolver;
    while (!cancelled->load() && iterator.hasNext()) {
        iterator.next();
        if (result.entries.size() == MaximumEntries) {
            result.message = QStringLiteral("Map preview limited to %1 entries; use List for the complete folder").arg(MaximumEntries);
            break;
        }
        const QFileInfo info = iterator.fileInfo();
        const QString suffix = info.fileName().endsWith(QStringLiteral(".fb2.zip"), Qt::CaseInsensitive)
            ? QStringLiteral("fb2") : info.suffix();
        const QString source = resolver.iconForSuffix(suffix, info.isDir());
        const int icon = int(iconNames().indexOf(QFileInfo(source).baseName()));
        result.entries.append({info.absoluteFilePath(), info.fileName(), info.isDir(), info.isSymLink(), icon});
    }
    std::sort(result.entries.begin(), result.entries.end(), [](const Entry &a, const Entry &b) {
        if (a.directory != b.directory) return a.directory;
        return a.name < b.name;
    });
    return result;
}

void FolderMapItem::requestListing(int index)
{
    if (m_loading >= 2 || m_nodes[index].requested || !m_nodes[index].entry.directory
        || m_nodes[index].entry.symlink) return;
    if (m_byPath.size() >= MaximumNodes) {
        m_nodes[index].requested = true;
        m_nodes[index].loaded = true;
        m_nodes[index].message = QStringLiteral("Map node limit reached; use List for the complete folder");
        return;
    }
    if (m_directoryModel && m_nodes[index].entry.path == m_currentPath) {
        queueModelSync();
        return;
    }
    m_nodes[index].requested = true;
    ++m_loading;
    const auto generation = m_generation;
    const auto revision = m_nodes[index].listingRevision = ++m_listingSequence;
    auto *watcher = new QFutureWatcher<Listing>(this);
    connect(watcher, &QFutureWatcher<Listing>::finished, this, [this, watcher, index, generation, revision] {
        const Listing listing = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation) return;
        --m_loading;
        if (index >= m_nodes.size() || m_nodes[index].listingRevision != revision) { polish(); return; }
        applyListing(index, listing);
        emit treeChanged();
        emit entriesChanged();
        polish();
        update();
    });
    watcher->setFuture(QtConcurrent::run(&FolderMapItem::readDirectory,
                                        m_nodes[index].entry.path, m_showHidden, m_cancelled));
}

QRectF FolderMapItem::screenRect(const QString &path) const
{
    const QString prefix = m_currentPath.endsWith(QLatin1Char('/')) ? m_currentPath : m_currentPath + QLatin1Char('/');
    if (path != m_currentPath && !path.startsWith(prefix)) return {};
    const auto found = m_byPath.constFind(path);
    if (found == m_byPath.constEnd()) return {};
    const QRectF rect = m_nodes[*found].rect;
    return QRectF((rect.topLeft() - m_center) * m_scale + QPointF(width() / 2, height() / 2),
                  rect.size() * m_scale);
}

QVariantMap FolderMapItem::entryAt(qreal x, qreal y) const
{
    for (auto it = m_visible.crbegin(); it != m_visible.crend(); ++it) {
        if (!it->rect.contains(QPointF(x, y))) continue;
        const auto &entry = m_nodes[it->index].entry;
        if (entry.path != it->path) continue; // A removed node's slot can be reused before the next polish.
        QVariantMap result = entryForPath(entry.path);
        result.insert(QStringLiteral("iconName"), entry.icon >= 0 ? iconNames()[entry.icon] : QString{});
        return result;
    }
    return {};
}

QVariantMap FolderMapItem::entryForPath(const QString &path) const
{
    const auto found = m_byPath.constFind(path);
    if (found == m_byPath.constEnd()) return {};
    const auto &entry = m_nodes[*found].entry;
    const QString suffix = QFileInfo(entry.name).suffix();
    return {{QStringLiteral("path"), entry.path}, {QStringLiteral("directory"), entry.directory},
            {QStringLiteral("isArchiveFile"), !entry.directory && ArchiveSupport::isArchiveExtension(suffix)},
            {QStringLiteral("isIsoImageFile"), !entry.directory && IsoSupport::isIsoImageExtension(suffix)}};
}

QVariantMap FolderMapItem::selectionContext(const QString &path) const
{
    const auto found = m_byPath.constFind(path.isEmpty() ? m_currentPath : path);
    if (found == m_byPath.constEnd()) return {};
    const int parent = path.isEmpty() || path == m_currentPath ? *found : m_nodes[*found].parent;
    if (parent < 0) return {};
    QStringList paths;
    for (int child : m_nodes[parent].children) paths.append(m_nodes[child].entry.path);
    return {{QStringLiteral("paths"), paths}, {QStringLiteral("columns"), m_nodes[parent].columns}};
}

void FolderMapItem::startMotion()
{
    if (!m_moving) {
        m_clock.start();
        m_moving = true;
        emit movingChanged();
    }
    polish();
    update();
}

void FolderMapItem::zoomAt(qreal x, qreal y, qreal steps)
{
    if (!m_gpuEnabled || m_nodes.isEmpty()) return;
    const QPointF offset(x - width() / 2, y - height() / 2);
    const QPointF anchor = m_targetCenter + offset / m_targetScale;
    m_targetScale = std::clamp(m_targetScale * std::pow(1.22, steps), qreal(0.02), qreal(1e9));
    m_targetCenter = anchor - offset / m_targetScale;
    constrainCamera(m_targetCenter, m_targetScale);
    startMotion();
}

void FolderMapItem::panBy(qreal dx, qreal dy)
{
    if (!m_gpuEnabled) return;
    m_targetCenter -= QPointF(dx, dy) / m_targetScale;
    constrainCamera(m_targetCenter, m_targetScale);
    startMotion();
}

void FolderMapItem::revealPath(const QString &path)
{
    const auto found = m_byPath.constFind(path);
    if (found == m_byPath.constEnd() || width() <= 0 || height() <= 0) return;
    const QRectF rect = m_nodes[*found].rect;
    m_targetCenter = rect.center();
    m_targetScale = std::clamp(std::min(width() / rect.width(), height() / rect.height()) * 0.92,
                               qreal(0.02), qreal(1e9));
    constrainCamera(m_targetCenter, m_targetScale);
    startMotion();
}

void FolderMapItem::fitCurrent()
{
    revealPath(m_currentPath);
}

qreal FolderMapItem::minimumScale() const
{
    const auto current = m_byPath.constFind(m_currentPath);
    if (current == m_byPath.constEnd()) return 0.02;
    const QRectF rect = m_nodes[*current].rect;
    return std::max(qreal(0.02), std::min(width() / rect.width(), height() / rect.height()) * 0.92);
}

void FolderMapItem::constrainCamera(QPointF &center, qreal &scale) const
{
    const auto current = m_byPath.constFind(m_currentPath);
    if (current == m_byPath.constEnd()) return;
    const QRectF rect = m_nodes[*current].rect;
    scale = std::max(scale, minimumScale());
    const qreal extentX = std::max(qreal(0), (rect.width() - width() * 0.92 / scale) / 2);
    const qreal extentY = std::max(qreal(0), (rect.height() - height() * 0.92 / scale) / 2);
    center.setX(std::clamp(center.x(), rect.center().x() - extentX, rect.center().x() + extentX));
    center.setY(std::clamp(center.y(), rect.center().y() - extentY, rect.center().y() + extentY));
}

void FolderMapItem::geometryChange(const QRectF &newGeometry, const QRectF &oldGeometry)
{
    QQuickItem::geometryChange(newGeometry, oldGeometry);
    if (newGeometry.size() != oldGeometry.size()) {
        if (newGeometry.width() <= 0 || newGeometry.height() <= 0) return;
        if (!m_cameraInitialized) {
            fitCurrent();
            m_cameraInitialized = true;
        } else {
            constrainCamera(m_center, m_scale);
            constrainCamera(m_targetCenter, m_targetScale);
        }
        polish();
    }
}

void FolderMapItem::collectVisible(int index, const QRectF &viewport, QList<int> &requests)
{
    if (m_visible.size() >= MaximumVisible) return;
    const auto &node = m_nodes[index];
    const QRectF rect = screenRect(node.entry.path);
    if (!rect.intersects(viewport) || rect.width() < 3 || rect.height() < 3) return;
    QString label = node.entry.name;
    if (node.entry.directory) {
        if (node.entry.symlink) label += QStringLiteral(" ↗");
        else if (!node.loaded && node.requested) label += QStringLiteral(" · loading…");
        else if (node.loaded && node.children.isEmpty() && node.message.isEmpty()) label += QStringLiteral(" · empty");
        if (!node.message.isEmpty()) label += QStringLiteral(" · ") + node.message;
    }
    m_visible.append({index, rect, label, node.entry.path, m_selection.contains(node.entry.path)});
    if (node.entry.directory && !node.entry.symlink && rect.width() >= 180 && rect.height() >= 90
        && !node.requested && requests.size() < 8) requests.append(index);
    if (rect.width() < 50 || rect.height() * HeaderFraction < 18 || node.children.isEmpty()) return;
    // Visit only the rows/columns intersecting the viewport, even for dense directories.
    const int rows = (int(node.children.size()) + node.columns - 1) / node.columns;
    const qreal padding = rect.width() * PaddingFraction;
    const QRectF content = rect.adjusted(padding, rect.height() * HeaderFraction, -padding, -padding);
    const qreal stepX = content.width() / node.columns;
    const qreal stepY = content.height() / rows;
    const int firstColumn = std::clamp(int(std::floor((viewport.left() - content.left()) / stepX)), 0, node.columns - 1);
    const int lastColumn = std::clamp(int(std::floor((viewport.right() - content.left()) / stepX)), 0, node.columns - 1);
    const int firstRow = std::clamp(int(std::floor((viewport.top() - content.top()) / stepY)), 0, rows - 1);
    const int lastRow = std::clamp(int(std::floor((viewport.bottom() - content.top()) / stepY)), 0, rows - 1);
    const int visibleCells = (lastRow - firstRow + 1) * (lastColumn - firstColumn + 1);
    if (visibleCells > MaximumVisible - m_visible.size()) {
        m_visible.last().label += QStringLiteral(" · %1 entries · zoom for detail").arg(node.children.size());
        return;
    }
    for (int row = firstRow; row <= lastRow; ++row) {
        for (int column = firstColumn; column <= lastColumn; ++column) {
            const int child = row * node.columns + column;
            if (child < node.children.size()) collectVisible(node.children[child], viewport, requests);
        }
    }
}

void FolderMapItem::updatePolish()
{
    if (!m_gpuEnabled || !isVisible()) return;
    if (m_moving) {
        const qreal seconds = std::clamp(m_clock.restart() / 1000.0, 0.001, 0.05);
        const qreal amount = 1 - std::exp(-18 * seconds);
        m_center += (m_targetCenter - m_center) * amount;
        m_scale = std::exp(std::log(m_scale) + (std::log(m_targetScale) - std::log(m_scale)) * amount);
        constrainCamera(m_center, m_scale);
        if (QLineF(m_center, m_targetCenter).length() * m_scale < 0.1
            && std::abs(std::log(m_scale / m_targetScale)) < 0.0001) {
            m_center = m_targetCenter;
            m_scale = m_targetScale;
            m_moving = false;
            emit movingChanged();
        }
    }
    const int previousCount = int(m_visible.size());
    m_visible.clear();
    QList<int> requests;
    const auto root = m_byPath.constFind(m_currentPath);
    if (root != m_byPath.constEnd()) collectVisible(*root, boundingRect(), requests);
    for (int index : requests) requestListing(index);
    const auto current = m_byPath.constFind(m_currentPath);
    setStatus(current != m_byPath.constEnd() && !m_nodes[*current].message.isEmpty()
                  ? m_nodes[*current].message
                  : m_loading > 0 ? QStringLiteral("Loading folders…") : QString{});
    scheduleThumbnails();
    syncWatchers();
    if (previousCount != m_visible.size()) emit treeChanged();
    update();
}

QSGNode *FolderMapItem::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *)
{
    if (!m_gpuEnabled) {
        delete oldNode;
        return nullptr;
    }
    auto *scene = static_cast<MapScene *>(oldNode);
    if (!scene) {
        scene = new MapScene;
        scene->tiles = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_ColoredPoint2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        scene->tiles->setGeometry(geometry);
        scene->tiles->setFlag(QSGNode::OwnsGeometry);
        scene->tiles->setMaterial(new QSGVertexColorMaterial);
        scene->tiles->setFlag(QSGNode::OwnsMaterial);
        scene->appendChildNode(scene->tiles);
    }
    auto &vertices = scene->vertices;
    vertices.clear();
    vertices.reserve(m_visible.size() * 36);
    QList<QRectF> selectedRects;
    if (scene->icons && m_iconsEnabled && !m_iconAtlas.isNull()
        && scene->atlasKey != m_iconAtlas.cacheKey()) {
        scene->atlas.reset(window()->createTextureFromImage(m_iconAtlas, QQuickWindow::TextureHasMipmaps));
        static_cast<QSGTextureMaterial *>(scene->icons->material())->setTexture(scene->atlas.get());
        scene->atlasKey = m_iconAtlas.cacheKey();
        scene->icons->markDirty(QSGNode::DirtyMaterial);
    }
    if (!scene->icons && m_iconsEnabled && !m_iconAtlas.isNull()) {
        scene->atlas.reset(window()->createTextureFromImage(m_iconAtlas, QQuickWindow::TextureHasMipmaps));
        scene->atlasKey = m_iconAtlas.cacheKey();
        scene->icons = new QSGGeometryNode;
        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawTriangles);
        geometry->setVertexDataPattern(QSGGeometry::DynamicPattern);
        scene->icons->setGeometry(geometry);
        scene->icons->setFlag(QSGNode::OwnsGeometry);
        auto *material = new QSGTextureMaterial;
        material->setTexture(scene->atlas.get());
        material->setFiltering(QSGTexture::Linear);
        material->setMipmapFiltering(QSGTexture::Linear);
        scene->icons->setMaterial(material);
        scene->icons->setFlag(QSGNode::OwnsMaterial);
        scene->insertChildNodeAfter(scene->icons, scene->tiles);
    }
    auto &iconVertices = scene->iconVertices;
    iconVertices.clear();
    iconVertices.reserve(m_visible.size() * 12);
    const auto appendIcon = [&](int icon, const QRectF &rect) {
        const QRectF uv(qreal((icon % AtlasColumns) * IconStride + IconPadding) / m_iconAtlas.width(),
                        qreal((icon / AtlasColumns) * IconStride + IconPadding) / m_iconAtlas.height(),
                        qreal(IconSize) / m_iconAtlas.width(), qreal(IconSize) / m_iconAtlas.height());
        const QPointF points[]{rect.topLeft(), rect.topRight(), rect.bottomLeft(),
                               rect.bottomLeft(), rect.topRight(), rect.bottomRight()};
        const QPointF coords[]{uv.topLeft(), uv.topRight(), uv.bottomLeft(),
                               uv.bottomLeft(), uv.topRight(), uv.bottomRight()};
        for (int i = 0; i < 6; ++i) {
            QSGGeometry::TexturedPoint2D vertex;
            vertex.set(float(points[i].x()), float(points[i].y()), float(coords[i].x()), float(coords[i].y()));
            iconVertices.append(vertex);
        }
    };
    QSet<int> retained;
    QSet<int> retainedEmpty;
    QSet<QString> retainedPreviews;
    int uploads = 0;
    bool moreUploads = false;
    QFont font;
    font.setPixelSize(13);
    const QFontMetricsF metrics(font);
    const QString emptyText = QStringLiteral("This folder is empty");
    for (const auto &visible : m_visible) {
        const Node &node = m_nodes[visible.index];
        const QRectF clipped = visible.rect.intersected(boundingRect());
        const QColor fill = node.entry.directory ? m_folderColor : m_fileColor;
        appendQuad(vertices, clipped, m_borderColor);
        const QRectF body = visible.rect.adjusted(1.5, 1.5, -1.5, -1.5).intersected(boundingRect());
        if (m_gradientStrength > 0) {
            appendGradientQuad(vertices, body,
                tinted(fill, m_gradientStartColor, m_gradientStrength),
                tinted(fill, m_gradientEndColor, m_gradientStrength * 0.45));
        } else appendQuad(vertices, body, fill);
        const qreal headerHeight = node.entry.directory
            ? std::max(qreal(20), visible.rect.height() * HeaderFraction) : visible.rect.height();
        if (node.entry.directory && visible.rect.width() >= 50 && visible.rect.height() >= 26) {
            const QRectF header(visible.rect.x() + 1.5, visible.rect.y() + 1.5,
                                visible.rect.width() - 3, headerHeight - 1.5);
            if (m_gradientStrength > 0) {
                appendGradientQuad(vertices, header.intersected(boundingRect()),
                    tinted(m_headerColor, m_gradientStartColor, m_gradientStrength), m_headerColor);
            } else appendQuad(vertices, header.intersected(boundingRect()), m_headerColor);
        }
        if (visible.selected) selectedRects.append(visible.rect);
        const bool showIcon = node.entry.icon >= 0
            && visible.rect.width() >= 32 && visible.rect.height() >= 24;
        const qreal iconSize = node.entry.directory
            ? std::clamp(headerHeight - 4, qreal(12), qreal(24))
            : std::clamp(std::min(visible.rect.width() - 12, visible.rect.height() - 24) * 0.8, qreal(12), qreal(64));
        const qreal textLeft = node.entry.directory && showIcon ? iconSize + 13 : 7;
        bool showPreview = false;
        const auto thumbnail = m_thumbnails.constFind(node.entry.path);
        if (m_showThumbnails && thumbnail != m_thumbnails.constEnd() && !(*thumbnail)->image.isNull()) {
            auto &preview = scene->previews[node.entry.path];
            const QImage &image = (*thumbnail)->image;
            if (preview.key != image.cacheKey()) {
                if (uploads < 1) {
                    auto *texture = window()->createTextureFromImage(image, QQuickWindow::TextureHasMipmaps);
                    if (preview.node) {
                        scene->removeChildNode(preview.node);
                        delete preview.node;
                    }
                    preview.node = new QSGSimpleTextureNode;
                    preview.node->setOwnsTexture(true);
                    preview.node->setFiltering(QSGTexture::Linear);
                    scene->insertChildNodeAfter(preview.node, scene->icons ? scene->icons : scene->tiles);
                    preview.node->setTexture(texture);
                    preview.key = image.cacheKey();
                    ++uploads;
                } else moreUploads = true;
            }
            if (preview.node) {
                const QSizeF room(std::max(qreal(1), visible.rect.width() - 14),
                                  std::max(qreal(1), visible.rect.height() - 28));
                const QSizeF size = QSizeF(image.size()).scaled(room, Qt::KeepAspectRatio);
                const QRectF previewRect(QPointF(visible.rect.center().x() - size.width() / 2,
                    visible.rect.top() + 4 + (room.height() - size.height()) / 2), size);
                preview.node->setRect(previewRect);
                if (node.entry.icon == 10) appendQuad(vertices, previewRect.intersected(boundingRect()), Qt::white);
                showPreview = true;
            }
            retainedPreviews.insert(node.entry.path);
        }
        if (showIcon && !showPreview && m_iconsEnabled && scene->icons) {
            const QRectF rect(node.entry.directory ? visible.rect.x() + 6 : visible.rect.center().x() - iconSize / 2,
                              node.entry.directory ? visible.rect.y() + 3
                                : visible.rect.y() + std::max(qreal(3), (visible.rect.height() - 20 - iconSize) / 2),
                              iconSize, iconSize);
            appendIcon(node.entry.icon, rect);
        }
        if (node.entry.directory && node.loaded && node.children.isEmpty() && node.message.isEmpty()
            && visible.rect.width() >= 210 && visible.rect.height() >= 170) {
            const qreal contentTop = visible.rect.top() + headerHeight;
            const qreal contentHeight = visible.rect.bottom() - contentTop;
            const qreal size = std::min(qreal(64), contentHeight * 0.32);
            const QRectF illustration(visible.rect.center().x() - size / 2,
                                      contentTop + (contentHeight - size) / 2 - 16, size, size);
            if (m_iconsEnabled && scene->icons) appendIcon(EmptyFolderIcon, illustration);
            retainedEmpty.insert(visible.index);
            auto &emptyLabel = scene->emptyLabels[visible.index];
            if (!emptyLabel.node) {
                emptyLabel.node = window()->createTextNode();
                emptyLabel.node->setRenderType(QSGTextNode::QtRendering);
                scene->appendChildNode(emptyLabel.node);
            }
            QColor emptyTextColor = m_textColor;
            emptyTextColor.setAlphaF(emptyTextColor.alphaF() * 0.5);
            if (emptyLabel.color != emptyTextColor) {
                emptyLabel.color = emptyTextColor;
                emptyLabel.node->clear();
                emptyLabel.node->setColor(emptyTextColor);
                QTextLayout layout(emptyText, font);
                layout.beginLayout();
                layout.createLine();
                layout.endLayout();
                emptyLabel.node->addTextLayout(QPointF(0, 0), &layout);
            }
            QMatrix4x4 emptyTransform;
            emptyTransform.translate(float(visible.rect.center().x() - metrics.horizontalAdvance(emptyText) / 2),
                                     float(illustration.bottom() + 8));
            emptyLabel.node->setMatrix(emptyTransform);
        }
        const qreal textScale = node.entry.directory
            ? std::clamp(headerHeight / 22, qreal(0.75), qreal(1.4)) : 1;
        if (visible.rect.width() < 65 || visible.rect.height() < 26 || headerHeight < 15
            || (!node.entry.directory && visible.rect.height() < 50)
            || visible.rect.top() < -headerHeight || visible.rect.top() > height()) continue;
        retained.insert(visible.index);
        auto &label = scene->labels[visible.index];
        if (!label.node) {
            label.node = window()->createTextNode();
            label.node->setRenderType(QSGTextNode::QtRendering);
            scene->appendChildNode(label.node);
        }
        const int room = std::max(0, int((visible.rect.width() - textLeft - 7) / textScale / 8) * 8);
        if (label.source != visible.label) {
            label.source = visible.label;
            label.naturalWidth = metrics.horizontalAdvance(visible.label);
        }
        const QString text = label.naturalWidth <= room ? visible.label
            : metrics.elidedText(visible.label, Qt::ElideRight, room);
        if (label.text != text || label.color != m_textColor) {
            label.text = text;
            label.color = m_textColor;
            label.node->clear();
            label.node->setColor(m_textColor);
            QTextLayout layout(text, font);
            layout.beginLayout();
            auto line = layout.createLine();
            if (line.isValid()) line.setLineWidth(room);
            layout.endLayout();
            label.node->addTextLayout(QPointF(0, 0), &layout);
        }
        QMatrix4x4 transform;
        const qreal textTop = node.entry.directory
            ? (showIcon ? std::max(qreal(3), (iconSize - metrics.height() * textScale) / 2 + 3) : 3)
            : visible.rect.height() - metrics.height() - 4;
        transform.translate(float(visible.rect.x() + textLeft), float(visible.rect.y() + textTop));
        transform.scale(float(textScale));
        label.node->setMatrix(transform);
    }
    for (const QRectF &rect : selectedRects) {
        const qreal left = std::round(rect.left());
        const qreal top = std::round(rect.top());
        const qreal right = std::round(rect.right());
        const qreal bottom = std::round(rect.bottom());
        if (right - left < 4 || bottom - top < 4) continue;
        appendQuad(vertices, QRectF(left, top, right - left, 2).intersected(boundingRect()), m_accentColor);
        appendQuad(vertices, QRectF(left, bottom - 2, right - left, 2).intersected(boundingRect()), m_accentColor);
        appendQuad(vertices, QRectF(left, top + 2, 2, bottom - top - 4).intersected(boundingRect()), m_accentColor);
        appendQuad(vertices, QRectF(right - 2, top + 2, 2, bottom - top - 4).intersected(boundingRect()), m_accentColor);
    }
    for (auto it = scene->labels.begin(); it != scene->labels.end();) {
        if (retained.contains(it.key())) { ++it; continue; }
        scene->removeChildNode(it->node);
        delete it->node;
        it = scene->labels.erase(it);
    }
    for (auto it = scene->emptyLabels.begin(); it != scene->emptyLabels.end();) {
        if (retainedEmpty.contains(it.key())) { ++it; continue; }
        scene->removeChildNode(it->node);
        delete it->node;
        it = scene->emptyLabels.erase(it);
    }
    for (auto it = scene->previews.begin(); it != scene->previews.end();) {
        if (retainedPreviews.contains(it.key())) { ++it; continue; }
        if (it->node) {
            scene->removeChildNode(it->node);
            delete it->node;
        }
        it = scene->previews.erase(it);
    }
    if (moreUploads) QMetaObject::invokeMethod(this, [this] { update(); }, Qt::QueuedConnection);
    auto *geometry = scene->tiles->geometry();
    geometry->allocate(int(vertices.size()));
    std::copy(vertices.cbegin(), vertices.cend(), geometry->vertexDataAsColoredPoint2D());
    geometry->markVertexDataDirty();
    scene->tiles->markDirty(QSGNode::DirtyGeometry);
    if (scene->icons) {
        auto *geometry = scene->icons->geometry();
        geometry->allocate(int(iconVertices.size()));
        std::copy(iconVertices.cbegin(), iconVertices.cend(), geometry->vertexDataAsTexturedPoint2D());
        geometry->markVertexDataDirty();
        scene->icons->markDirty(QSGNode::DirtyGeometry);
    }
    return scene;
}
