#include "FolderMapStress.h"
#include "../app/AppServices.h"
#include "../controllers/AppSettingsController.h"
#include "../controllers/WorkspaceController.h"
#include "../controllers/FilePanelController.h"
#include "../controllers/FavoritesController.h"
#include "../models/DirectoryModel.h"
#include "../ui/foldermap/FolderMapItem.h"
#include "../ui/foldermap/MapGpuProbe.h"
#include <QtQuick/private/qquickitem_p.h>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFutureWatcher>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QScopeGuard>
#include <QSGSimpleTextureNode>
#include <QSGTextureMaterial>
#include <QThread>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <functional>
#include <mutex>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif
#ifdef __GLIBC__
#include <malloc.h>
#endif

// Test instrumentation stays out of the QML API and the renderer's hot path.
class FolderMapStressProbe {
public:
    static QImage badgeAtlas(FolderMapItem *map) { return map->m_badgeAtlas; }
    static void setBadgeAtlas(FolderMapItem *map, const QImage &image) { map->m_badgeAtlas = image; map->update(); }
    static quint64 listingSequence(FolderMapItem *map) { return map->m_listingSequence; }
    static QRectF fileIconRect(FolderMapItem *map, const QString &path) {
        const QRectF tile = map->screenRect(path);
        QList<QSGNode *> pending;
        if (auto *node = QQuickItemPrivate::get(map)->paintNode) pending.append(node);
        while (!pending.isEmpty()) {
            auto *node = pending.takeLast();
            if (auto *geometry = dynamic_cast<QSGGeometryNode *>(node)) {
                if (auto *material = dynamic_cast<QSGTextureMaterial *>(geometry->material());
                    material && material->texture() && material->texture()->textureSize() == map->m_iconAtlas.size()) {
                    const auto *vertices = geometry->geometry()->vertexDataAsTexturedPoint2D();
                    for (int i = 0; i + 5 < geometry->geometry()->vertexCount(); i += 6) {
                        const QRectF rect(QPointF(vertices[i].x, vertices[i].y), QPointF(vertices[i + 5].x, vertices[i + 5].y));
                        if (tile.contains(rect.center())) return rect;
                    }
                }
            }
            for (auto *child = node->firstChild(); child; child = child->nextSibling()) pending.append(child);
        }
        return {};
    }
    static QJsonObject listingCost(const QString &path) {
        QElapsedTimer timer; timer.start();
        const auto listing = FolderMapItem::readDirectory(path, false, std::make_shared<std::atomic_bool>(false));
        return {{"entries", listing.entries.size()}, {"elapsedMs", timer.nsecsElapsed() / 1e6}};
    }
    static int badgeQuads(FolderMapItem *map, const QSize &atlasSize) {
        QList<QSGNode *> pending;
        if (auto *node = QQuickItemPrivate::get(map)->paintNode) pending.append(node);
        while (!pending.isEmpty()) {
            auto *node = pending.takeLast();
            if (auto *geometry = dynamic_cast<QSGGeometryNode *>(node)) {
                if (auto *material = dynamic_cast<QSGTextureMaterial *>(geometry->material());
                    material && material->texture() && material->texture()->textureSize() == atlasSize)
                    return geometry->geometry()->vertexCount() / 6;
            }
            for (auto *child = node->firstChild(); child; child = child->nextSibling()) pending.append(child);
        }
        return 0;
    }
    static QRectF folderBadgeRect(FolderMapItem *map, const QString &path) {
        const QRectF tile = map->screenRect(path);
        const QRectF header(tile.topLeft(), QSizeF(tile.width(), std::max(qreal(20), tile.height() * 0.09)));
        QList<QSGNode *> pending;
        if (auto *node = QQuickItemPrivate::get(map)->paintNode) pending.append(node);
        while (!pending.isEmpty()) {
            auto *node = pending.takeLast();
            if (auto *geometry = dynamic_cast<QSGGeometryNode *>(node)) {
                if (auto *material = dynamic_cast<QSGTextureMaterial *>(geometry->material());
                    material && material->texture() && material->texture()->textureSize() == map->m_badgeAtlas.size()) {
                    const auto *vertices = geometry->geometry()->vertexDataAsTexturedPoint2D();
                    for (int i = 0; i + 5 < geometry->geometry()->vertexCount(); i += 6) {
                        const QRectF rect(QPointF(vertices[i].x, vertices[i].y), QPointF(vertices[i + 5].x, vertices[i + 5].y));
                        if (header.contains(rect.center())) return rect;
                    }
                }
            }
            for (auto *child = node->firstChild(); child; child = child->nextSibling()) pending.append(child);
        }
        return {};
    }
    static QJsonObject snapshot(FolderMapItem *map) {
        return {{"nodes", map->m_byPath.size()}, {"visible", map->m_visible.size()},
                {"watchers", map->m_watchers.size()}, {"listings", map->m_loading},
                {"requests", map->m_thumbnails.size()}, {"ready", map->thumbnailsReady()},
                {"retainedBufferBytes", qint64(map->m_nodes.capacity() * sizeof(FolderMapItem::Node)
                    + map->m_visible.capacity() * sizeof(FolderMapItem::Visible)
                    + map->m_freeNodes.capacity() * sizeof(int))},
                {"futures", map->findChildren<QFutureWatcherBase *>().size()}};
    }
    // Called on the render thread after rendering. Shared glyph caches are excluded.
    static int textures(FolderMapItem *map) {
        QSet<QSGTexture *> textures;
        QList<QSGNode *> pending;
        if (auto *node = QQuickItemPrivate::get(map)->paintNode) pending.append(node);
        while (!pending.isEmpty()) {
            auto *node = pending.takeLast();
            if (auto *image = dynamic_cast<QSGSimpleTextureNode *>(node)) textures.insert(image->texture());
            else if (auto *geometry = dynamic_cast<QSGGeometryNode *>(node)) {
                if (auto *material = dynamic_cast<QSGTextureMaterial *>(geometry->material())) textures.insert(material->texture());
            }
            for (auto *child = node->firstChild(); child; child = child->nextSibling()) pending.append(child);
        }
        textures.remove(nullptr);
        return int(textures.size());
    }
};

namespace {
qint64 mapStressResidentBytes()
{
#ifdef Q_OS_LINUX
    QFile stat(QStringLiteral("/proc/self/statm"));
    if (stat.open(QIODevice::ReadOnly)) return stat.readAll().simplified().split(' ').value(1).toLongLong() * sysconf(_SC_PAGESIZE);
#endif
    return 0;
}
qint64 mapStressLiveHeapBytes()
{
#ifdef __GLIBC__
    const auto info = mallinfo2();
    return qint64(info.uordblks + info.hblkhd);
#else
    return 0;
#endif
}
bool writeMapStressFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("map stress\n") == 11;
}
}

int FolderMapStress::run(AppServices &services, QQuickWindow &window, const QString &fixturePath)
{
    QJsonObject report{{"reportType", "folder-map-stress"}, {"qtVersion", qVersion()},
                       {"directoryViewSwitches", 48}, {"reentryCyclesRequested", 8}};
    QJsonArray failures;
    const auto check = [&](const QString &name, bool passed) {
        report.insert(name, passed);
        if (!passed) failures.append(name);
        std::fprintf(stderr, "Map stress: %s %s\n", qPrintable(name), passed ? "passed" : "FAILED");
    };
    const auto until = [&](const std::function<bool()> &ready) {
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < 10000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            // Match a normal event loop: processEvents alone starves deferred deletion.
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            window.update();
            if (ready()) return true;
            QThread::msleep(1);
        }
        return false;
    };
    auto *probe = window.findChild<MapGpuProbe *>();
    if (!until([&] { return probe && probe->ready(); }) || !probe->available()) {
        report.insert("error", "Hardware GPU required for this stress test");
        std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return 2;
    }
    report.insert("deviceName", probe->deviceName());
    auto *workspace = services.workspace();
    std::array<FilePanelController *, 2> panels{workspace->leftPanel(), workspace->rightPanel()};
    std::array<QString, 2> roots;
    std::array<QString, 2> days;
    bool fixtures = true;
    for (int side = 0; side < 2; ++side) {
        roots[side] = QDir(fixturePath).filePath(QStringLiteral("map-stress-%1").arg(side));
        for (const QString &branch : {QStringLiteral("Day"), QStringLiteral("Night")}) {
            const QString folder = QDir(roots[side]).filePath(branch);
            fixtures = QDir().mkpath(folder + QStringLiteral("/Work")) && fixtures;
            for (int i = 0; i < 64; ++i) {
                QImage image(384, 256, QImage::Format_RGB32);
                image.fill(QColor::fromHsv((i * 17 + side * 31) % 360, 190, 220));
                fixtures = image.save(QDir(folder).filePath(QStringLiteral("image-%1.jpg").arg(i))) && fixtures;
            }
            for (int i = 0; i < 8; ++i) fixtures = writeMapStressFile(folder + QStringLiteral("/Work/initial-%1.txt").arg(i)) && fixtures;
        }
        days[side] = roots[side] + QStringLiteral("/Day");
    }
    check("fixtures", fixtures);
    const bool originalThumbnails = services.settings()->showThumbnails();
    const auto restore = qScopeGuard([&] { services.settings()->setShowThumbnails(originalThumbnails); });
    services.settings()->setShowThumbnails(false);
    workspace->setSplitEnabled(true);
    window.resize(1800, 1000);
    for (int side = 0; side < 2; ++side) { panels[side]->openPath(days[side]); panels[side]->setViewMode(3); }
    std::array<FolderMapItem *, 2> maps{};
    const bool loaded = until([&] {
        for (auto *map : window.findChildren<FolderMapItem *>())
            for (int side = 0; side < 2; ++side) if (map->directoryModel() == panels[side]->directoryModel()) maps[side] = map;
        return maps[0] && maps[1] && maps[0]->gpuEnabled() && maps[1]->gpuEnabled()
            && maps[0]->loadedCount() >= 66 && maps[1]->loadedCount() >= 66
            && maps[0]->iconsReady() && maps[1]->iconsReady();
    });
    check("twoMapsLoaded", loaded);
    if (!loaded) {
        report.insert("failures", failures);
        std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return 1;
    }
    std::array<std::atomic<int>, 2> textures{};
    const auto renderObserver = QObject::connect(&window, &QQuickWindow::afterRendering, &window, [&] {
        for (int side = 0; side < 2; ++side) textures[side] = FolderMapStressProbe::textures(maps[side]);
    }, Qt::DirectConnection);
    const auto disconnectRender = qScopeGuard([&] { QObject::disconnect(renderObserver); });
    const auto resources = [&] {
        window.grabWindow();
        QJsonArray result;
        for (int side = 0; side < 2; ++side) {
            auto state = FolderMapStressProbe::snapshot(maps[side]);
            state.insert("gpuTextures", textures[side].load());
            result.append(state);
        }
        return result;
    };
    const auto mutate = [&](const QString &folder, int count) {
        bool ok = true;
        for (int i = 0; i < count; ++i) ok = writeMapStressFile(folder + QStringLiteral("/burst-%1.txt").arg(i)) && ok;
        for (int i = 0; i < count / 2; ++i) ok = QFile::rename(folder + QStringLiteral("/burst-%1.txt").arg(i), folder + QStringLiteral("/renamed-%1.txt").arg(i)) && ok;
        for (int i = count / 2; i < count * 3 / 4; ++i) ok = QFile::remove(folder + QStringLiteral("/burst-%1.txt").arg(i)) && ok;
        return ok;
    };
    // Explicitly expand a nested directory so the burst also tests native nested watchers.
    for (int side = 0; side < 2; ++side) maps[side]->revealPath(days[side] + QStringLiteral("/Work"));
    check("nestedLoaded", until([&] { return maps[0]->loadedCount() >= 74 && maps[1]->loadedCount() >= 74
        && !maps[0]->moving() && !maps[1]->moving(); }));
    check("nestedWatchersActive", until([&] { return FolderMapStressProbe::snapshot(maps[0])["watchers"].toInt() > 0
        && FolderMapStressProbe::snapshot(maps[1])["watchers"].toInt() > 0; }));
    bool nestedBurstWritten = true;
    for (int side = 0; side < 2; ++side) nestedBurstWritten = mutate(days[side] + QStringLiteral("/Work"), 80) && nestedBurstWritten;
    check("nestedWatcherUpdates", nestedBurstWritten && until([&] {
        for (int side = 0; side < 2; ++side) {
            const QString nested = days[side] + QStringLiteral("/Work/renamed-0.txt");
            if (!maps[side]->entryForPath(nested).contains("path")
                || maps[side]->selectionContext(nested).value("paths").toStringList().size() != 68) return false;
        }
        return true;
    }));
    for (auto *map : maps) map->fitCurrent();
    until([&] { return !maps[0]->moving() && !maps[1]->moving(); });
    for (auto *map : maps) map->zoomAt(map->width() / 2, map->height() / 2, 3);
    until([&] { return !maps[0]->moving() && !maps[1]->moving(); });
    bool burstWritten = true;
    bool coldArrivals = false;
    const auto cadence = [&](const QString &name, bool burst) {
        QList<double> intervals;
        std::mutex mutex;
        QElapsedTimer clock; clock.start();
        int steps = 0;
        const auto timing = QObject::connect(&window, &QQuickWindow::frameSwapped, &window, [&] {
            std::lock_guard lock(mutex);
            intervals.append(clock.nsecsElapsed() / 1e6);
            clock.restart();
        }, Qt::DirectConnection);
        const auto motion = QObject::connect(&window, &QQuickWindow::frameSwapped, &window, [&] {
            if (++steps > 300) return;
            if (burst && steps == 90) coldArrivals = maps[0]->thumbnailsReady() > 0 && maps[1]->thumbnailsReady() > 0;
            if (steps >= 90) for (auto *map : maps) {
                map->zoomAt(map->width() / 2, map->height() / 2, steps < 195 ? 0.025 : -0.025);
                map->panBy(steps < 195 ? 0.7 : -0.7, 0);
            }
            if (burst && steps == 140) for (int side = 0; side < 2; ++side) {
                burstWritten = mutate(days[side], 96) && burstWritten;
            }
            window.update();
        }, Qt::QueuedConnection);
        window.update();
        const bool complete = until([&] { return steps >= 300; });
        QObject::disconnect(motion);
        QObject::disconnect(timing);
        QList<double> samples;
        { std::lock_guard lock(mutex); samples = intervals; }
        std::sort(samples.begin(), samples.end());
        QJsonObject stats{{"frames", samples.size()}};
        if (!samples.isEmpty()) {
            stats.insert("p50Ms", samples.at(samples.size() / 2));
            stats.insert("p95Ms", samples.at(samples.size() * 95 / 100));
            stats.insert("maxMs", samples.last());
            stats.insert("over16Ms", std::count_if(samples.cbegin(), samples.cend(), [](double ms) { return ms > 16.67; }));
        }
        report.insert(name, stats);
        check(name + QStringLiteral("Responsive"), complete && !samples.isEmpty() && samples.last() < 250
            && samples.at(samples.size() * 95 / 100) < 33.4);
    };
    services.settings()->setShowThumbnails(true);
    cadence(QStringLiteral("coldThumbnailsAndBurst"), true);
    check("coldArrivals", coldArrivals);
    check("burstWritten", burstWritten);
    for (auto *map : maps) map->fitCurrent();
    check("bulkUpdates", until([&] {
        for (int side = 0; side < 2; ++side) {
            if (panels[side]->directoryModel()->loading() || panels[side]->directoryModel()->count() != 137
                || maps[side]->entryForPath(days[side] + QStringLiteral("/burst-48.txt")).contains("path")
                || !maps[side]->entryForPath(days[side] + QStringLiteral("/renamed-0.txt")).contains("path")) return false;
            const QString nested = days[side] + QStringLiteral("/Work/renamed-0.txt");
            if (!maps[side]->entryForPath(nested).contains("path") || maps[side]->selectionContext(nested).value("paths").toStringList().size() != 68) return false;
        }
        return true;
    }));
    for (auto *map : maps) map->zoomAt(map->width() / 2, map->height() / 2, 3);
    check("thumbnailsReady", until([&] { return !maps[0]->moving() && !maps[1]->moving()
        && maps[0]->thumbnailsReady() >= 24 && maps[1]->thumbnailsReady() >= 24; }));
    check("thumbnailsUploaded", until([&] { return textures[0] >= 25 && textures[1] >= 25; }));
    report.insert("loadedResources", resources());
    cadence(QStringLiteral("steadyThumbnails"), false);
    services.settings()->setShowThumbnails(false);
    cadence(QStringLiteral("withoutThumbnails"), false);
    // Rapid navigation/view changes deliberately leave older asynchronous work unfinished.
    for (int i = 0; i < 48; ++i) {
        for (int side = 0; side < 2; ++side) {
            panels[side]->setViewMode(i % 4);
            panels[side]->openPath(roots[side] + (i % 2 ? QStringLiteral("/Day/Work") : QStringLiteral("/Night")));
        }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 2);
    }
    for (int side = 0; side < 2; ++side) { panels[side]->openPath(days[side]); panels[side]->setViewMode(3); }
    check("rapidSwitches", until([&] {
        for (int side = 0; side < 2; ++side) if (panels[side]->directoryModel()->loading()
            || panels[side]->currentPath() != days[side] || maps[side]->currentPath() != days[side]
            || maps[side]->loadedCount() < 138 || !maps[side]->entryForPath(days[side] + QStringLiteral("/renamed-0.txt")).contains("path")) return false;
        return true;
    }));
    QJsonArray exitSamples;
    bool releasePassed = true, cyclesPassed = true;
    qint64 warmedRss = 0, finalRss = 0, warmedHeap = 0, finalHeap = 0;
    services.settings()->setShowThumbnails(true);
    for (int cycle = 0; cycle < 8; ++cycle) {
        for (auto *panel : panels) panel->setViewMode(3);
        cyclesPassed = until([&] { return maps[0]->loadedCount() >= 138 && maps[1]->loadedCount() >= 138; }) && cyclesPassed;
        for (auto *map : maps) map->zoomAt(map->width() / 2, map->height() / 2, 3);
        cyclesPassed = until([&] { return !maps[0]->moving() && !maps[1]->moving()
            && maps[0]->thumbnailsReady() >= 24 && maps[1]->thumbnailsReady() >= 24; }) && cyclesPassed;
        window.grabWindow();
        for (auto *panel : panels) panel->setViewMode(0);
        const bool drained = until([&] { return !maps[0]->gpuEnabled() && !maps[1]->gpuEnabled()
            && maps[0]->findChildren<QFutureWatcherBase *>().isEmpty() && maps[1]->findChildren<QFutureWatcherBase *>().isEmpty(); });
        int retirementFrames = 0;
        const auto retired = QObject::connect(&window, &QQuickWindow::frameSwapped, &window,
            [&] { ++retirementFrames; window.update(); }, Qt::QueuedConnection);
        const bool retiredGpuWork = until([&] { return retirementFrames >= 16; });
        QObject::disconnect(retired);
        const QJsonArray states = resources();
        for (const auto &state : states) {
            const auto s = state.toObject();
            releasePassed = releasePassed && drained && retiredGpuWork && s["nodes"].toInt() == 0 && s["watchers"].toInt() == 0
                && s["visible"].toInt() == 0 && s["listings"].toInt() == 0 && s["ready"].toInt() == 0
                && s["requests"].toInt() == 0 && s["gpuTextures"].toInt() == 0 && s["retainedBufferBytes"].toInteger() == 0;
        }
        finalRss = mapStressResidentBytes();
        finalHeap = mapStressLiveHeapBytes();
        exitSamples.append(QJsonObject{{"cycle", cycle}, {"rssBytes", finalRss}, {"liveHeapBytes", finalHeap}, {"maps", states}});
        if (cycle == 2) { warmedRss = finalRss; warmedHeap = finalHeap; }
        if (!cyclesPassed) break;
    }
    check("reentryCycles", cyclesPassed);
    check("mapResourcesReleased", releasePassed);
    check("residentMemoryBounded", warmedRss == 0 || finalRss - warmedRss < 64 * 1024 * 1024);
    check("liveHeapBounded", warmedHeap == 0 || finalHeap - warmedHeap < 32 * 1024 * 1024);
    report.insert("exitSamples", exitSamples);
#ifdef __GLIBC__
    malloc_trim(0); // Diagnostic only: separate allocator retention from live resources.
    report.insert("rssAfterAllocatorTrim", mapStressResidentBytes());
#endif
    report.insert("failures", failures);
    report.insert("success", failures.isEmpty());
    const QByteArray json = QJsonDocument(report).toJson(QJsonDocument::Indented);
    QFile output(QStringLiteral("/tmp/folder-map-stress.json"));
    if (output.open(QIODevice::WriteOnly)) output.write(json);
    std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
    return failures.isEmpty() ? 0 : 1;
}

int FolderMapStress::runBadges(AppServices &services, QQuickWindow &window, const QString &fixturePath)
{
    QStringList bindingLoops;
    const auto warningConnection = QObject::connect(qmlEngine(&window), &QQmlEngine::warnings, &window,
        [&](const QList<QQmlError> &errors) {
            for (const auto &error : errors)
                if (error.url().toString().contains(QStringLiteral("FilePanelMapView.qml"))
                    && error.description().contains(QStringLiteral("Binding loop"))) bindingLoops.append(error.toString());
        });
    const auto disconnectWarnings = qScopeGuard([&] { QObject::disconnect(warningConnection); });
    const auto originalMode = services.theme()->mode();
    const bool originalThumbnails = services.settings()->showThumbnails();
    QStringList temporaryPins;
    const auto restore = qScopeGuard([&] {
        services.favorites()->unpinPaths(temporaryPins);
        services.settings()->setShowThumbnails(originalThumbnails);
        services.theme()->setMode(originalMode);
    });
    const auto until = [&](const std::function<bool()> &ready) {
        QElapsedTimer timer; timer.start();
        while (timer.elapsed() < 15000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            if (ready()) return true;
            QThread::msleep(1);
        }
        return false;
    };
    auto *probe = window.findChild<MapGpuProbe *>();
    if (!until([&] { return probe && probe->ready(); }) || !probe->available()) return 2;
    auto *workspace = services.workspace();
    workspace->setSplitEnabled(false);
    workspace->setActivePanel(0);
    auto *panel = workspace->leftPanel();
    const QString root = fixturePath + QStringLiteral("/badges");
    if (!QDir().mkpath(root)) return 2;
    QStringList paths;
    for (int i = 0; i < 512; ++i) {
        const QString path = root + QStringLiteral("/archive-%1.zip").arg(i);
        if (!writeMapStressFile(path)) return 2;
        paths.append(path);
    }
    services.settings()->setShowThumbnails(false);
    panel->openPath(root);
    panel->setViewMode(3);
    FolderMapItem *map = nullptr;
    if (!until([&] {
        for (auto *candidate : window.findChildren<FolderMapItem *>())
            if (candidate->directoryModel() == panel->directoryModel()) map = candidate;
        return map && map->gpuEnabled() && map->loadedCount() == 513 && map->iconsReady()
            && !FolderMapStressProbe::badgeAtlas(map).isNull();
    })) return 2;
    services.favorites()->pinPaths(paths);
    temporaryPins = paths;
    if (!until([&] { return map->entryForPath(paths.first()).value("isPinned").toBool(); })) return 2;
    map->zoomAt(map->width() / 2, map->height() / 2, 2);
    if (!until([&] { return !map->moving(); })) return 2;
    const QImage atlas = FolderMapStressProbe::badgeAtlas(map);
    const auto listingCost = FolderMapStressProbe::listingCost(root);
    const auto revision = FolderMapStressProbe::listingSequence(map);
    const auto stats = [](QList<double> samples) {
        std::sort(samples.begin(), samples.end());
        if (samples.isEmpty()) return QJsonObject{};
        return QJsonObject{{"frames", samples.size()}, {"p50Ms", samples[samples.size() / 2]},
            {"p95Ms", samples[samples.size() * 95 / 100]}, {"maxMs", samples.last()}};
    };
    QJsonArray rounds;
    bool success = true;
    for (bool enabled : {false, true, true, false}) {
        FolderMapStressProbe::setBadgeAtlas(map, enabled ? atlas : QImage{});
        std::atomic<int> frames{0};
        int quads = 0;
        QList<double> intervals, syncTimes;
        QElapsedTimer frameClock, syncClock;
        frameClock.start();
        std::mutex mutex;
        const auto before = QObject::connect(&window, &QQuickWindow::beforeSynchronizing, &window,
            [&] { syncClock.start(); }, Qt::DirectConnection);
        const auto after = QObject::connect(&window, &QQuickWindow::afterSynchronizing, &window, [&] {
            std::lock_guard lock(mutex);
            if (frames > 40) syncTimes.append(syncClock.nsecsElapsed() / 1e6);
            quads = FolderMapStressProbe::badgeQuads(map, atlas.size());
        }, Qt::DirectConnection);
        const auto timing = QObject::connect(&window, &QQuickWindow::frameSwapped, &window, [&] {
            std::lock_guard lock(mutex);
            if (frames > 40) intervals.append(frameClock.nsecsElapsed() / 1e6);
            frameClock.restart();
        }, Qt::DirectConnection);
        const auto motion = QObject::connect(&window, &QQuickWindow::frameSwapped, &window, [&] {
            ++frames;
            map->panBy(frames % 120 < 60 ? 0.6 : -0.6, 0);
            window.update();
        }, Qt::QueuedConnection);
        window.update();
        const bool complete = until([&] { return frames >= 280; });
        QObject::disconnect(motion); QObject::disconnect(timing);
        QObject::disconnect(before); QObject::disconnect(after);
        {
            std::lock_guard lock(mutex);
            rounds.append(QJsonObject{{"enabled", enabled}, {"frameIntervals", stats(intervals)},
                {"sync", stats(syncTimes)}, {"badgeQuads", quads}});
            success = complete && (enabled ? quads > 100 : quads == 0) && success;
        }
        if (enabled) window.grabWindow().save(QStringLiteral("/tmp/fmqml-map-badges-dense.png"));
    }
    FolderMapStressProbe::setBadgeAtlas(map, atlas);
    const bool noReloads = FolderMapStressProbe::listingSequence(map) == revision;
    success = noReloads && success;
    map->revealPath(paths.first());
    bool iconFillsTile = until([&] { return !map->moving(); });
    QJsonArray iconSizes;
    qreal previousIconSize = 0;
    for (qreal steps : {-4.0, 2.0}) {
        map->zoomAt(map->width() / 2, map->height() / 2, steps);
        iconFillsTile = until([&] { return !map->moving(); }) && iconFillsTile;
        QRectF icon;
        std::mutex iconMutex;
        const auto capture = QObject::connect(&window, &QQuickWindow::afterSynchronizing, &window, [&] {
            std::lock_guard lock(iconMutex);
            icon = FolderMapStressProbe::fileIconRect(map, paths.first());
        }, Qt::DirectConnection);
        const auto image = window.grabWindow();
        QObject::disconnect(capture);
        std::lock_guard lock(iconMutex);
        const QRectF tile = map->screenRect(paths.first());
        const qreal available = std::min(tile.width() - 12, tile.height() - 24);
        iconFillsTile = std::abs(icon.width() - available) < 1 && icon.width() > 64
            && icon.width() > previousIconSize && iconFillsTile;
        previousIconSize = icon.width();
        iconSizes.append(QJsonObject{{"tileWidth", tile.width()}, {"tileHeight", tile.height()}, {"iconSize", icon.width()}});
        image.save(QStringLiteral("/tmp/fmqml-map-icon-zoom-%1.png").arg(iconSizes.size()));
    }
    success = iconFillsTile && success;
    const QString gallery = fixturePath + QStringLiteral("/badge-examples");
    const QString pinnedFolder = gallery + QStringLiteral("/Pinned folder");
    const QString lockedFolder = gallery + QStringLiteral("/Locked folder");
    const QString lockedFile = gallery + QStringLiteral("/Locked file.txt");
    bool examples = QDir().mkpath(pinnedFolder) && QDir().mkpath(lockedFolder)
        && writeMapStressFile(gallery + QStringLiteral("/Archive.zip")) && writeMapStressFile(lockedFile);
    QImage preview(320, 240, QImage::Format_RGB32); preview.fill(QColor("#638eaa"));
    const QString previewPath = gallery + QStringLiteral("/Pinned preview.png");
    examples = preview.save(previewPath) && examples;
#ifdef Q_OS_UNIX
    examples = QFile::setPermissions(lockedFolder, {}) && QFile::setPermissions(lockedFile, {})
        && QFile::link(pinnedFolder, gallery + QStringLiteral("/Folder link"))
        && QFile::link(gallery + QStringLiteral("/missing"), gallery + QStringLiteral("/Broken link")) && examples;
#endif
    const auto restorePermissions = qScopeGuard([&] {
        QFile::setPermissions(lockedFolder, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QFile::setPermissions(lockedFile, QFile::ReadOwner | QFile::WriteOwner);
    });
    services.favorites()->pinPaths({pinnedFolder, previewPath});
    temporaryPins.append({pinnedFolder, previewPath});
    services.settings()->setShowThumbnails(true);
    panel->openPath(gallery);
    examples = until([&] { return map->currentPath() == gallery && map->thumbnailsReady() > 0
        && !map->entryForPath(lockedFile).isEmpty(); }) && examples;
    if (examples) {
        examples = map->entryForPath(pinnedFolder).value("isPinned").toBool()
            && map->entryForPath(previewPath).value("isPinned").toBool();
#ifdef Q_OS_UNIX
        examples = map->entryForPath(lockedFile).value("primaryBadgeKind").toString() == QStringLiteral("locked")
            && map->entryForPath(lockedFolder).value("primaryBadgeKind").toString() == QStringLiteral("locked") && examples;
#endif
        for (auto mode : {ThemeController::Light, ThemeController::Dark}) {
            const auto oldAtlas = FolderMapStressProbe::badgeAtlas(map).cacheKey();
            const bool changed = services.theme()->mode() != mode;
            services.theme()->setMode(mode);
            if (changed) examples = until([&] { return FolderMapStressProbe::badgeAtlas(map).cacheKey() != oldAtlas; }) && examples;
            QElapsedTimer settle; settle.start();
            until([&] { return settle.elapsed() >= 200; });
            examples = window.grabWindow().save(QStringLiteral("/tmp/fmqml-map-badges-%1.png").arg(mode == ThemeController::Light ? "light" : "dark")) && examples;
        }
        services.favorites()->unpinPath(pinnedFolder);
        examples = until([&] { return !map->entryForPath(pinnedFolder).value("isPinned").toBool(); }) && examples;
    }
    services.favorites()->pinPath(gallery);
    temporaryPins.append(gallery);
    bool headerBadgeScales = until([&] { return map->entryForPath(gallery).value("isPinned").toBool(); });
    map->fitCurrent();
    headerBadgeScales = until([&] { return !map->moving(); }) && headerBadgeScales;
    QJsonArray headerBadgeSizes;
    qreal previousBadgeSize = 0;
    for (int step = 0; step < 3; ++step) {
        if (step > 0) {
            const QRectF tile = map->screenRect(gallery);
            map->zoomAt(tile.right() - 20, tile.top() + tile.height() * 0.045, 2);
            headerBadgeScales = until([&] { return !map->moving(); }) && headerBadgeScales;
        }
        QRectF badge;
        std::mutex badgeMutex;
        const auto capture = QObject::connect(&window, &QQuickWindow::afterSynchronizing, &window, [&] {
            std::lock_guard lock(badgeMutex);
            badge = FolderMapStressProbe::folderBadgeRect(map, gallery);
        }, Qt::DirectConnection);
        const auto image = window.grabWindow();
        QObject::disconnect(capture);
        std::lock_guard lock(badgeMutex);
        const qreal headerHeight = std::max(qreal(20), map->screenRect(gallery).height() * 0.09);
        const qreal expected = std::clamp(headerHeight * 0.5, qreal(12), qreal(64));
        headerBadgeScales = std::abs(badge.width() - expected) < 1 && badge.width() > previousBadgeSize && headerBadgeScales;
        previousBadgeSize = badge.width();
        headerBadgeSizes.append(QJsonObject{{"headerHeight", headerHeight}, {"badgeSize", badge.width()}});
        image.save(QStringLiteral("/tmp/fmqml-map-header-badge-%1.png").arg(step));
    }
    success = examples && headerBadgeScales && bindingLoops.isEmpty() && success;
    QJsonObject report{{"reportType", "map-badges"}, {"device", probe->deviceName()},
        {"rounds", rounds}, {"noListingReloads", noReloads}, {"nestedListing", listingCost}, {"examples", examples},
        {"badgeBindingLoops", QJsonArray::fromStringList(bindingLoops)},
        {"iconFillsTile", iconFillsTile}, {"iconZoom", iconSizes},
        {"headerBadgeScales", headerBadgeScales}, {"headerBadgeZoom", headerBadgeSizes},
        {"atlasBytes", atlas.sizeInBytes()}, {"success", success}};
    std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
    return success ? 0 : 1;
}
