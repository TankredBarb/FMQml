#include "PanelResizeBenchmark.h"
#include "../app/AppServices.h"
#include "../controllers/FilePanelController.h"
#include "../models/DirectoryModel.h"

#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QScreen>
#include <QSGRendererInterface>
#include <QSet>
#include <QTest>
#include <QTimer>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <memory>
#include <mutex>

namespace {
void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

QString option(const QString &name, const QString &fallback)
{
    const auto args = QCoreApplication::arguments();
    const int index = args.indexOf(name);
    return index >= 0 ? args.value(index + 1) : fallback;
}

QQuickItem *named(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name) return root;
    for (auto *child : root->childItems()) {
        if (auto *found = named(child, name)) return found;
    }
    return nullptr;
}

QQuickItem *panel(QQuickItem *root, int side)
{
    if (root->metaObject()->indexOfProperty("panelSide") >= 0
        && root->property("panelSide").toInt() == side) return root;
    for (auto *child : root->childItems()) {
        if (auto *found = panel(child, side)) return found;
    }
    return nullptr;
}

QQuickItem *view(QQuickItem *root)
{
    if (root->isVisible() && (root->inherits("QQuickGridView") || root->inherits("QQuickListView"))
        && root->property("count").toInt() > 0) return root;
    for (auto *child : root->childItems()) {
        if (auto *found = view(child)) return found;
    }
    return nullptr;
}

QJsonObject statistics(QVector<double> values)
{
    std::sort(values.begin(), values.end());
    if (values.isEmpty()) return {{"count", 0}};
    return {{"count", values.size()}, {"p50", values[(values.size() - 1) / 2]},
            {"p95", values[int((values.size() - 1) * .95)]},
            {"p99", values[int((values.size() - 1) * .99)]}, {"max", values.last()}};
}

QJsonObject snapshot(QQuickItem *left, QQuickItem *right)
{
    QJsonObject result;
    for (auto *item : {left, right}) {
        auto *v = view(item);
        const auto content = qvariant_cast<QQuickItem *>(v->property("contentItem"));
        int thumbnailEligible = 0, thumbnailReady = 0;
        if (content) {
            for (auto *child : content->childItems()) {
                thumbnailEligible += child->property("benchmarkThumbnailEligible").toBool();
                thumbnailReady += child->property("benchmarkThumbnailReady").toBool();
            }
        }
        result.insert(item == left ? "left" : "right", QJsonObject{
            {"viewMode", item->property("viewMode").toInt()},
            {"width", item->width()}, {"viewWidth", v->width()},
            {"cellWidth", v->property("cellWidth").toDouble()},
            {"nameWidth", item->property("effectiveColWidthName").toDouble()},
            {"resizing", item->property("resizeOptimized").toBool()},
            {"lightweight", item->property("lightweightDelegates").toBool()},
            {"thumbnailsPaused", item->property("thumbnailLoadingPaused").toBool()},
            {"cacheBuffer", v->property("cacheBuffer").toInt()},
            {"reuseItems", v->property("reuseItems").toBool()},
            {"thumbnailEligible", thumbnailEligible}, {"thumbnailReady", thumbnailReady},
            {"storageWidth", named(item, "panelStorageView")->width()},
            {"favoritesWidth", named(item, "panelFavoritesView")->width()},
            {"children", content ? content->childItems().size() : 0}});
    }
    return result;
}

struct Samples {
    QElapsedTimer clock;
    std::mutex mutex;
    bool dragging = false, recovering = false;
    double lastFrame = 0, syncStart = 0, renderStart = 0, pressStart = 0, firstFrameDelay = 0;
    double lastRecoveryFrame = 0;
    QVector<double> frames, startupFrames, steadyFrames, recoveryFrames, sync, render;
    bool traceEnabled = false;
    int traceId = 0;
    int columns[2] = {}, added[2] = {}, destroyed[2] = {};
    double widths[2] = {};
    QJsonObject guiTrace, renderTrace;
    QJsonArray trace;
    double now() const { return clock.nsecsElapsed() / 1e6; }
};
}

int PanelResizeBenchmark::run(AppServices &services, QQuickWindow &window, const QString &photosPath)
{
    bool modeOk = false, repetitionsOk = false;
    const int mode = option("--view-mode", "1").toInt(&modeOk);
    const int repetitions = option("--resize-runs", "3").toInt(&repetitionsOk);
    bool rightModeOk = false;
    const int rightMode = option("--right-view-mode", QString::number(mode)).toInt(&rightModeOk);
    const QString kind = option("--divider", "panels");
    const bool middle = kind == "middle-left" || kind == "middle-right";
    if (!modeOk || mode < 0 || mode > 2 || !rightModeOk || rightMode < 0 || rightMode > 2 || !repetitionsOk || repetitions < 1 || repetitions > 10
        || (kind != "panels" && kind != "sidebar" && kind != "preview" && !middle)) return 2;

    for (int i = 0; i < 300; ++i) {
        QImage image(1024, 768, QImage::Format_RGB32);
        image.fill(QColor::fromHsv(i % 360, 140, 180));
        QPainter painter(&image);
        for (int j = 0; j < 12; ++j)
            painter.fillRect(j * 85, 0, 40, 768, QColor::fromHsv((i * 7 + j * 23) % 360, 180, 220));
        painter.end();
        if (!image.save(QDir(photosPath).filePath(QStringLiteral("photo-%1.png").arg(i, 4, 10, QLatin1Char('0'))))) return 2;
    }
    auto *workspace = services.workspace();
    workspace->setSplitEnabled(true);
    workspace->setActivePanel(0);
    for (auto *controller : {workspace->leftPanel(), workspace->rightPanel()}) {
        controller->setViewMode(controller == workspace->leftPanel() ? mode : rightMode);
        if (!controller->openPath(photosPath)) return 2;
    }
    window.setProperty("sidebarPreferredWidth", 220.0);
    window.setProperty("previewPanePlacement", middle ? QStringLiteral("between-panels") : QStringLiteral("right"));
    window.setProperty("previewPaneStoredWidth", 420.0);
    QMetaObject::invokeMethod(&window, "setPreviewPaneVisible", Q_ARG(QVariant, QVariant(kind == "preview" || middle)));
    window.showNormal();
    window.resize(1800, 900);
    wait(3000);
    for (int attempt = 0; attempt < 8 && window.size() != QSize(1800, 900); ++attempt) {
        window.showNormal();
        window.resize(1800, 900);
        wait(300);
    }
    wait(500);
    if (window.size() != QSize(1800, 900)) {
        std::fprintf(stderr, "Resize benchmark requires an 1800x900 window; got %dx%d\n", window.width(), window.height());
        return 2;
    }
    auto *left = panel(window.contentItem(), 0), *right = panel(window.contentItem(), 1);
    auto *ws = named(window.contentItem(), "fileWorkspace");
    auto *split = named(window.contentItem(), kind == "panels" || middle ? "panelSplitView" : "mainSplitView");
    auto *sidebar = named(window.contentItem(), "sidebar");
    auto *preview = named(window.contentItem(), middle ? "middlePreviewHost" : "trailingPreviewHost");
    if (!left || !right || !ws || !split || !sidebar || !preview || !view(left) || !view(right)) return 2;

    auto samples = std::make_shared<Samples>();
    samples->clock.start();
    QObject observer;
    samples->traceEnabled = QCoreApplication::arguments().contains(QStringLiteral("--resize-trace"));
    if (samples->traceEnabled) {
        int side = 0;
        for (auto *item : {left, right}) {
            auto *v = view(item);
            const int currentSide = side++;
            auto updateWidth = [samples, v, currentSide] {
                const double width = v->width();
                const double cell = v->property("cellWidth").toDouble();
                std::lock_guard lock(samples->mutex);
                samples->widths[currentSide] = width;
                samples->columns[currentSide] = cell > 0 ? std::max(1, int(width / cell)) : 1;
            };
            updateWidth();
            QObject::connect(v, &QQuickItem::widthChanged, &observer, updateWidth);
            auto *content = qvariant_cast<QQuickItem *>(v->property("contentItem"));
            auto known = std::make_shared<QSet<QObject *>>();
            auto trackChildren = [content, known, samples, currentSide, &observer] {
                for (auto *child : content->childItems()) {
                    if (known->contains(child)) continue;
                    known->insert(child);
                    {
                        std::lock_guard lock(samples->mutex);
                        if (samples->dragging) ++samples->added[currentSide];
                    }
                    QObject::connect(child, &QObject::destroyed, &observer,
                        [known, samples, currentSide, child] {
                            known->remove(child);
                            std::lock_guard lock(samples->mutex);
                            if (samples->dragging) ++samples->destroyed[currentSide];
                        });
                }
            };
            trackChildren();
            QObject::connect(content, &QQuickItem::childrenChanged, &observer, trackChildren);
        }
        QObject::connect(&window, &QQuickWindow::afterAnimating, &observer, [samples] {
            std::lock_guard lock(samples->mutex);
            samples->guiTrace = {};
            if (!samples->dragging) return;
            samples->guiTrace = {{"id", ++samples->traceId},
                {"afterAnimatingMs", samples->now() - samples->pressStart},
                {"leftColumnsBeforePolish", samples->columns[0]},
                {"rightColumnsBeforePolish", samples->columns[1]}};
        });
    }
    QObject::connect(&window, &QQuickWindow::beforeSynchronizing, &observer, [samples] {
        std::lock_guard lock(samples->mutex); samples->syncStart = samples->now();
        if (samples->traceEnabled) {
            samples->renderTrace = samples->guiTrace;
            if (!samples->renderTrace.isEmpty()) {
                samples->renderTrace.insert("preSyncElapsedMs", samples->syncStart - samples->pressStart
                    - samples->renderTrace["afterAnimatingMs"].toDouble());
                samples->renderTrace.insert("leftWidth", samples->widths[0]);
                samples->renderTrace.insert("rightWidth", samples->widths[1]);
                samples->renderTrace.insert("leftColumns", samples->columns[0]);
                samples->renderTrace.insert("rightColumns", samples->columns[1]);
                samples->renderTrace.insert("leftAddedTotal", samples->added[0]);
                samples->renderTrace.insert("rightAddedTotal", samples->added[1]);
                samples->renderTrace.insert("leftDestroyedTotal", samples->destroyed[0]);
                samples->renderTrace.insert("rightDestroyedTotal", samples->destroyed[1]);
            }
        }
    }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterSynchronizing, &observer, [samples] {
        std::lock_guard lock(samples->mutex);
        if (samples->dragging) samples->sync.append(samples->now() - samples->syncStart);
        if (!samples->renderTrace.isEmpty())
            samples->renderTrace.insert("syncMs", samples->now() - samples->syncStart);
    }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::beforeRendering, &observer, [samples] {
        std::lock_guard lock(samples->mutex); samples->renderStart = samples->now();
    }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::afterRendering, &observer, [samples] {
        std::lock_guard lock(samples->mutex);
        if (samples->dragging) samples->render.append(samples->now() - samples->renderStart);
        if (!samples->renderTrace.isEmpty())
            samples->renderTrace.insert("renderMs", samples->now() - samples->renderStart);
    }, Qt::DirectConnection);
    QObject::connect(&window, &QQuickWindow::frameSwapped, &observer, [samples] {
        std::lock_guard lock(samples->mutex);
        const double now = samples->now();
        if (samples->dragging) {
            if (!samples->renderTrace.isEmpty()) {
                samples->renderTrace.insert("swappedMs", now - samples->pressStart);
                samples->renderTrace.insert("intervalMs", samples->lastFrame > 0 ? now - samples->lastFrame : 0);
                samples->trace.append(samples->renderTrace);
                samples->renderTrace = {};
            }
            if (samples->lastFrame > 0) {
                samples->frames.append(now - samples->lastFrame);
                if (now - samples->pressStart < 200)
                    samples->startupFrames.append(now - samples->lastFrame);
                if (now - samples->pressStart > 500)
                    samples->steadyFrames.append(now - samples->lastFrame);
            }
            else samples->firstFrameDelay = now - samples->pressStart;
        }
        if (samples->recovering) {
            samples->recoveryFrames.append(now - samples->lastRecoveryFrame);
            samples->lastRecoveryFrame = now;
        }
        samples->lastFrame = now;
    }, Qt::DirectConnection);

    if (QCoreApplication::arguments().contains(QStringLiteral("--check-scrollbar-arrows"))) {
        auto *v = view(left);
        QQuickItem *bar = nullptr;
        auto findBar = [&](auto &&self, QQuickItem *node) -> void {
            if (node->metaObject()->indexOfProperty("wheelHandler") >= 0
                && qvariant_cast<QQuickItem *>(node->property("wheelTarget")) == v) bar = node;
            for (auto *child : node->childItems()) self(self, child);
        };
        findBar(findBar, left);
        if (!bar) return 2;
        auto *handler = qvariant_cast<QQuickItem *>(bar->property("wheelHandler"));
        if (!handler) return 2;
        for (bool increase : {false, true}) {
            for (bool repeat : {false, true}) {
                QMetaObject::invokeMethod(handler, "cancel");
                v->setProperty("contentY", 600.0);
                wait(100);
                const QPointF at = v->mapToScene({150, 150});
                QWheelEvent event(at, window.mapToGlobal(at.toPoint()), {}, QPoint(0, -120),
                                  Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                event.setTimestamp(10000);
                QCoreApplication::sendEvent(&window, &event);
                wait(30);
                if (!handler->property("active").toBool()) return 1;
                const double before = v->property("contentY").toDouble();
                const QPoint arrow = bar->mapToScene({bar->width() / 2, increase ? bar->height() - 7 : 7}).toPoint();
                QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, arrow);
                const bool cancelled = !handler->property("active").toBool();
                if (repeat) wait(450);
                QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, arrow);
                const double clicked = v->property("contentY").toDouble();
                wait(400);
                if (!cancelled || std::abs(clicked - v->property("contentY").toDouble()) > .5
                    || (increase ? clicked <= before : clicked >= before)) {
                    std::fputs("Scrollbar arrow did not take over pending wheel motion\n", stderr);
                    return 1;
                }
            }
        }
        std::puts("Both scrollbar arrows and repeats cancel pending wheel motion");
        return 0;
    }
    const bool checkFreeze = QCoreApplication::arguments().contains(QStringLiteral("--check-resize-freeze"));
    const bool checkWideBrief = QCoreApplication::arguments().contains(QStringLiteral("--check-brief-wide-resize"));
    if (checkWideBrief && (mode != 2 || rightMode != 2 || kind != "panels")) return 2;
    const QString capturePath = option("--resize-capture", "");
    QJsonArray results;
    bool success = true;
    for (int rep = 0; rep < repetitions; ++rep) {
        if (kind == "panels" || middle) QMetaObject::invokeMethod(ws, "splitEvenly");
        if (checkWideBrief) {
            ws->setProperty("splitRatio", .18);
            QMetaObject::invokeMethod(ws, "applySplitRatio");
        }
        if (middle) {
            QQmlProperty(preview, "SplitView.preferredWidth", qmlContext(preview)).write(420.0);
            QMetaObject::invokeMethod(ws, "applySplitRatio");
        }
        if (kind == "sidebar") QQmlProperty(sidebar, "SplitView.preferredWidth", qmlContext(sidebar)).write(220.0);
        if (kind == "preview") QQmlProperty(preview, "SplitView.preferredWidth", qmlContext(preview)).write(420.0);
        wait(1000);
        QQuickItem *handle = nullptr;
        const QString handleName = kind == "panels" || middle ? "panelSplitHandle" : "mainSplitHandle";
        const double targetX = kind == "panels" || kind == "middle-right" ? right->mapToScene({}).x()
                              : kind == "sidebar" ? sidebar->mapToScene({}).x() + sidebar->width()
                                                   : preview->mapToScene({}).x();
        auto findHandle = [&](auto &&self, QQuickItem *item) -> void {
            if (item->objectName() == handleName && item->isVisible()
                && std::abs(item->mapToScene({}).x() - targetX) < 8) handle = item;
            for (auto *child : item->childItems()) self(self, child);
        };
        findHandle(findHandle, split);
        if (!handle) return 2;
        const QPoint start = handle->mapToScene({handle->width() / 2, handle->height() * .52}).toPoint();
        QPoint pointer = start;
        const auto before = snapshot(left, right);
        const auto selectionLeft = workspace->leftPanel()->directoryModel()->selectedPaths();
        const auto selectionRight = workspace->rightPanel()->directoryModel()->selectedPaths();
        {
            std::lock_guard lock(samples->mutex);
            samples->frames.clear(); samples->sync.clear(); samples->render.clear();
            samples->startupFrames.clear(); samples->steadyFrames.clear(); samples->recoveryFrames.clear();
            samples->lastFrame = 0; samples->firstFrameDelay = 0;
            samples->trace = {}; samples->traceId = 0;
            for (int side = 0; side < 2; ++side) {
                samples->added[side] = 0; samples->destroyed[side] = 0;
            }
            samples->pressStart = samples->now(); samples->dragging = true;
        }
        QElapsedTimer cost;
        cost.start(); QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
        const double pressMs = cost.nsecsElapsed() / 1e6;
        const auto afterPress = snapshot(left, right);
        const bool pressed = split->property("resizing").toBool();
        QVector<double> moves, heartbeat, errors;
        bool delivering = false;
        bool freezeStable = true;
        bool hiddenGeometryStable = true;
        bool briefSingleColumn = true;
        QQuickItem *storageViews[] = {named(left, "panelStorageView"), named(right, "panelStorageView")};
        QQuickItem *favoritesViews[] = {named(left, "panelFavoritesView"), named(right, "panelFavoritesView")};
        double previous = samples->now();
        QTimer pulse;
        pulse.setTimerType(Qt::PreciseTimer); pulse.setInterval(2);
        QObject::connect(&pulse, &QTimer::timeout, &observer, [&] {
            const double now = samples->now(); heartbeat.append(now - previous); previous = now;
            errors.append(std::abs(handle->mapToScene({handle->width() / 2, 0}).x() - pointer.x()));
            if (checkFreeze && now - samples->pressStart > 500) {
                for (auto *item : {left, right}) {
                    if (item->property("viewMode").toInt() != 2) continue;
                    auto *v = view(item);
                    briefSingleColumn = briefSingleColumn && int(v->width() / v->property("cellWidth").toDouble()) == 1;
                    auto *content = qvariant_cast<QQuickItem *>(v->property("contentItem"));
                    for (auto *child : content->childItems()) {
                        if (child->metaObject()->indexOfProperty("index") >= 0 && child->width() > 0
                            && child->y() + child->height() > v->property("contentY").toDouble()
                            && child->y() < v->property("contentY").toDouble() + v->height())
                            briefSingleColumn = briefSingleColumn && std::abs(child->x()) < .01;
                    }
                }
            }
        });
        pulse.start();
        const double amplitude = checkWideBrief ? 620 : kind == "panels" ? 180 : kind == "sidebar" ? 65 : 100;
        QElapsedTimer drag; drag.start();
        QTimer input;
        input.setTimerType(Qt::PreciseTimer); input.setInterval(4);
        QObject::connect(&input, &QTimer::timeout, &observer, [&] {
            if (delivering) return;
            delivering = true;
            const double phase = drag.nsecsElapsed() / 1e6 * 2 * M_PI / 1500;
            pointer = start + QPoint(qRound(amplitude * (checkWideBrief ? .5 * (1 - std::cos(phase)) : std::sin(phase))), 0);
            cost.start(); QTest::mouseMove(&window, pointer); moves.append(cost.nsecsElapsed() / 1e6);
            if (checkFreeze) {
                for (auto *item : {left, right}) {
                    const auto initial = before[item == left ? "left" : "right"].toObject();
                    const int side = item == left ? 0 : 1;
                    hiddenGeometryStable = hiddenGeometryStable
                        && std::abs(storageViews[side]->width() - initial["storageWidth"].toDouble()) < .01
                        && std::abs(favoritesViews[side]->width() - initial["favoritesWidth"].toDouble()) < .01;
                    if (item->property("viewMode").toInt() == 0)
                        freezeStable = freezeStable && std::abs(item->property("effectiveColWidthName").toDouble()
                                                              - initial["nameWidth"].toDouble()) < .01;
                    if (item->property("viewMode").toInt() == 2) {
                        auto *content = qvariant_cast<QQuickItem *>(view(item)->property("contentItem"));
                        for (auto *child : content->childItems()) {
                            auto *row = named(child, "briefResizeContent");
                            if (row && child->width() > 0)
                                freezeStable = freezeStable && std::abs(row->width()
                                    - (std::max(160.0, initial["viewWidth"].toDouble())
                                       - QQmlProperty(row, "anchors.leftMargin").read().toDouble()
                                       - QQmlProperty(row, "anchors.rightMargin").read().toDouble())) < .01;
                        }
                    }
                }
            }
            delivering = false;
        });
        input.start(); wait(100);
        const auto during = snapshot(left, right);
        wait(2900); input.stop(); pulse.stop();
        const QPoint end = start + QPoint(checkFreeze || !capturePath.isEmpty() ? 60 : 0, 0);
        QTest::mouseMove(&window, end);
        wait(20);
        { std::lock_guard lock(samples->mutex); samples->dragging = false; }
        if (rep == 0 && !capturePath.isEmpty()) {
            wait(200);
            if (!window.grabWindow().save(capturePath)) return 2;
        }
        QVector<double> recoveryHeartbeat;
        QTimer recoveryPulse;
        recoveryPulse.setTimerType(Qt::PreciseTimer); recoveryPulse.setInterval(2);
        previous = samples->now();
        QObject::connect(&recoveryPulse, &QTimer::timeout, &observer, [&] {
            const double now = samples->now(); recoveryHeartbeat.append(now - previous); previous = now;
        });
        {
            std::lock_guard lock(samples->mutex);
            samples->recovering = true; samples->lastRecoveryFrame = samples->now();
        }
        recoveryPulse.start();
        cost.start(); QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, end);
        const double releaseMs = cost.nsecsElapsed() / 1e6;
        wait(600); recoveryPulse.stop();
        { std::lock_guard lock(samples->mutex); samples->recovering = false; }
        const auto after = snapshot(left, right);
        bool lifecycle = pressed;
        for (const auto &side : {QStringLiteral("left"), QStringLiteral("right")}) {
            lifecycle = lifecycle && !before[side].toObject()["resizing"].toBool()
                        && during[side].toObject()["resizing"].toBool()
                        && during[side].toObject()["thumbnailsPaused"].toBool()
                        && !after[side].toObject()["resizing"].toBool();
        }
        const bool selectionStable = selectionLeft == workspace->leftPanel()->directoryModel()->selectedPaths()
                                     && selectionRight == workspace->rightPanel()->directoryModel()->selectedPaths();
        const bool tracking = !errors.isEmpty() && statistics(errors)["p95"].toDouble() < 20;
        const bool finalGeometry = std::abs(handle->mapToScene({handle->width() / 2, 0}).x() - end.x()) < 2;
        bool recovered = true;
        for (const auto &side : {QStringLiteral("left"), QStringLiteral("right")}) {
            const auto restored = after[side].toObject();
            recovered = recovered && !restored["lightweight"].toBool()
                        && !restored["thumbnailsPaused"].toBool()
                        && restored["cacheBuffer"].toInt() == 1600;
            if (restored["thumbnailEligible"].toInt() > 0)
                recovered = recovered && restored["thumbnailReady"].toInt() > 0;
        }
        if (checkFreeze) {
            for (auto *item : {left, right}) {
                const auto initial = before[item == left ? "left" : "right"].toObject();
                if (item->property("viewMode").toInt() == 0)
                    recovered = recovered && std::abs(item->property("effectiveColWidthName").toDouble()
                                                      - initial["nameWidth"].toDouble()) > 1;
                if (item->property("viewMode").toInt() == 2)
                    recovered = recovered && std::abs(view(item)->property("cellWidth").toDouble()
                                                      - std::max(160.0, std::floor(view(item)->width() / 2))) < .01;
            }
        }
        bool renameRestored = true;
        bool shortResizeRestored = true;
        bool virtualViewsRestored = true;
        bool gridContentRestored = true;
        bool gridMissingContentRejected = true;
        const bool gridContentChecked = checkFreeze && (left->property("viewMode").toInt() == 1
                                                        || right->property("viewMode").toInt() == 1);
        if (gridContentChecked) {
            auto contentReady = [&] {
                bool ready = true;
                for (auto *item : {left, right}) {
                    if (item->property("viewMode").toInt() != 1) continue;
                    auto *v = view(item);
                    const double cellWidth = v->property("cellWidth").toDouble();
                    const double cellHeight = v->property("cellHeight").toDouble();
                    const double originY = v->property("originY").toDouble();
                    const double top = v->property("contentY").toDouble();
                    const double firstRow = originY + std::floor((top - originY) / cellHeight) * cellHeight;
                    const int columns = std::max(1, int(v->width() / cellWidth));
                    int checked = 0;
                    for (double y = firstRow; y < top + v->height(); y += cellHeight) {
                        for (int column = 0; column < columns; ++column) {
                            const int index = qRound((y - originY) / cellHeight) * columns + column;
                            if (index < 0 || index >= v->property("count").toInt()) continue;
                            ++checked;
                            QQuickItem *delegate = nullptr;
                            QMetaObject::invokeMethod(v, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, delegate), Q_ARG(int, index));
                            QQuickItem *layout = nullptr;
                            if (delegate) {
                                for (auto *child : delegate->childItems()) {
                                    auto *loaded = qvariant_cast<QQuickItem *>(child->property("item"));
                                    if (loaded && loaded->metaObject()->indexOfProperty("iconCell") >= 0) layout = loaded;
                                }
                            }
                            if (!layout || !delegate->isVisible() || delegate->width() <= 0 || delegate->height() <= 0) {
                                ready = false;
                                continue;
                            }
                            const double margin = delegate->property("contentMargin").toDouble();
                            ready = ready && std::abs(layout->x() - margin) < .01
                                && std::abs(layout->y() - margin) < .01
                                && std::abs(layout->width() - (delegate->width() - 2 * margin)) < .01
                                && std::abs(layout->height() - (delegate->height() - 2 * margin)) < .01;
                            auto *icon = qvariant_cast<QQuickItem *>(layout->property("iconCell"));
                            QVariant hit;
                            if (icon) {
                                const QPointF center = icon->mapToItem(delegate, {icon->width() / 2, icon->height() / 2});
                                QMetaObject::invokeMethod(delegate, "isPointOnDragSurface", Q_RETURN_ARG(QVariant, hit),
                                                          Q_ARG(QVariant, center.x()), Q_ARG(QVariant, center.y()));
                            }
                            ready = ready && hit.toBool();
                        }
                    }
                    ready = ready && checked > 0;
                }
                return ready;
            };
            // Cache incubation may still be underway; every expected viewport item must be ready.
            QElapsedTimer readiness; readiness.start();
            do {
                gridContentRestored = contentReady();
                if (!gridContentRestored) wait(20);
            } while (!gridContentRestored && readiness.elapsed() < 1000);
            // A missing visible subtree must still fail, unlike an unfinished cache stub.
            if (gridContentRestored) {
                auto *v = view(left->property("viewMode").toInt() == 1 ? left : right);
                const int index = std::max(0, int(std::floor((v->property("contentY").toDouble()
                    - v->property("originY").toDouble()) / v->property("cellHeight").toDouble())))
                    * std::max(1, int(v->width() / v->property("cellWidth").toDouble()));
                QQuickItem *delegate = nullptr;
                QMetaObject::invokeMethod(v, "itemAtIndex", Q_RETURN_ARG(QQuickItem *, delegate), Q_ARG(int, index));
                QQuickItem *loader = nullptr;
                for (auto *child : delegate->childItems()) {
                    auto *loaded = qvariant_cast<QQuickItem *>(child->property("item"));
                    if (loaded && loaded->metaObject()->indexOfProperty("iconCell") >= 0) loader = child;
                }
                loader->setProperty("active", false);
                gridMissingContentRejected = !contentReady();
                loader->setProperty("active", true);
                gridContentRestored = contentReady();
            }
        }
        if (checkFreeze) {
            workspace->leftPanel()->directoryModel()->selectOnly(0);
            QMetaObject::invokeMethod(left, "startRename");
            wait(100);
            renameRestored = left->property("isRenaming").toBool();
            QMetaObject::invokeMethod(left, "cancelActiveInlineRename");
            wait(50);
            renameRestored = renameRestored && !left->property("isRenaming").toBool();
            // Release before cache trimming completes, then grab again during recovery.
            for (int attempt = 0; attempt < 2; ++attempt) {
                const QPoint quickStart = handle->mapToScene({handle->width() / 2, handle->height() * .52}).toPoint();
                QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, quickStart);
                shortResizeRestored = shortResizeRestored && split->property("resizing").toBool();
                QTest::mouseMove(&window, quickStart + QPoint(5, 0));
                wait(20);
                QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, quickStart + QPoint(5, 0));
                shortResizeRestored = shortResizeRestored && !split->property("resizing").toBool();
            }
            wait(600);
            for (auto *item : {left, right})
                shortResizeRestored = shortResizeRestored && !item->property("resizeOptimized").toBool()
                                     && item->property("activeViewCacheBuffer").toInt() == 1600;
            for (const auto &path : {QStringLiteral("devices://"), QStringLiteral("favorites://")}) {
                virtualViewsRestored = workspace->leftPanel()->openPath(path) && virtualViewsRestored;
                wait(100);
                auto *shown = path == QStringLiteral("devices://") ? storageViews[0] : favoritesViews[0];
                virtualViewsRestored = virtualViewsRestored && shown->isVisible()
                    && std::abs(shown->width() - shown->parentItem()->width()) < .01
                    && std::abs(shown->height() - (shown->parentItem()->height()
                                                 - left->property("bottomChromeHeight").toDouble())) < .01;
                // A visible virtual view must continue to follow the panel width.
                const QPoint virtualStart = handle->mapToScene({handle->width() / 2, handle->height() * .52}).toPoint();
                QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, virtualStart);
                QTest::mouseMove(&window, virtualStart + QPoint(15, 0));
                wait(30);
                QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, virtualStart + QPoint(15, 0));
                wait(100);
                virtualViewsRestored = virtualViewsRestored
                    && std::abs(shown->width() - shown->parentItem()->width()) < .01;
            }
            virtualViewsRestored = workspace->leftPanel()->openPath(photosPath) && virtualViewsRestored;
            wait(100);
        }
        const bool accepted = lifecycle && selectionStable && tracking && finalGeometry
                              && freezeStable && recovered && renameRestored && shortResizeRestored
                              && hiddenGeometryStable && virtualViewsRestored && gridContentRestored
                              && gridMissingContentRejected && briefSingleColumn;
        QJsonObject result{{"rep", rep}, {"accepted", accepted}, {"lifecycle", lifecycle},
                           {"selectionStable", selectionStable}, {"finalGeometry", finalGeometry},
                           {"freezeStable", freezeStable}, {"recovered", recovered},
                           {"renameChecked", checkFreeze}, {"renameRestored", renameRestored},
                           {"shortResizeChecked", checkFreeze}, {"shortResizeRestored", shortResizeRestored},
                           {"hiddenGeometryChecked", checkFreeze}, {"hiddenGeometryStable", hiddenGeometryStable},
                           {"virtualViewsRestored", virtualViewsRestored},
                           {"gridContentChecked", gridContentChecked}, {"gridContentRestored", gridContentRestored},
                           {"gridMissingContentRejected", gridMissingContentRejected},
                           {"briefSingleColumn", briefSingleColumn}, {"wideBriefChecked", checkWideBrief},
                           {"before", before}, {"afterPress", afterPress}, {"during", during}, {"after", after},
                           {"pressMs", pressMs}, {"releaseMs", releaseMs},
                           {"inputCallMs", statistics(moves)}, {"heartbeatMs", statistics(heartbeat)},
                           {"recoveryHeartbeatMs", statistics(recoveryHeartbeat)},
                           {"pointerErrorPx", statistics(errors)}};
        {
            std::lock_guard lock(samples->mutex);
            result.insert("firstFrameDelayMs", samples->firstFrameDelay);
            result.insert("frameIntervalMs", statistics(samples->frames));
            result.insert("startupFrameIntervalMs", statistics(samples->startupFrames));
            result.insert("steadyFrameIntervalMs", statistics(samples->steadyFrames));
            result.insert("recoveryFrameIntervalMs", statistics(samples->recoveryFrames));
            result.insert("synchronizationMs", statistics(samples->sync));
            result.insert("renderingMs", statistics(samples->render));
            if (samples->traceEnabled) result.insert("frameTrace", samples->trace);
        }
        results.append(result); success = accepted && success;
    }
    const QJsonObject report{{"schemaVersion", 1}, {"reportType", "panel-resize-benchmark"},
        {"qt", qVersion()}, {"platform", QGuiApplication::platformName()},
        {"windowWidth", window.width()}, {"windowHeight", window.height()},
        {"graphicsApi", int(window.rendererInterface()->graphicsApi())},
        {"swapInterval", window.format().swapInterval()},
        {"refreshRate", window.screen()->refreshRate()}, {"viewMode", mode}, {"rightViewMode", rightMode}, {"divider", kind},
        {"inputRoute", "QTest QWindow public API"}, {"results", results}, {"success", success}};
    const QByteArray output = QJsonDocument(report).toJson(QJsonDocument::Indented);
    const QString outputPath = option("--resize-output", "");
    if (!outputPath.isEmpty()) {
        QFile file(outputPath);
        if (!file.open(QIODevice::WriteOnly) || file.write(output) != output.size()) return 2;
    } else std::fwrite(output.constData(), 1, output.size(), stdout);
    return success ? 0 : 1;
}
