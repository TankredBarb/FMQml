#include "FolderMapItem.h"
#include "../../src/models/DirectoryModel.h"
#include "MapGpuProbe.h"
#include "IconProvider.h"
#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "LocalMountPointIndex.h"
#include <QPainter>
#include <QProcess>
#include <QScopeGuard>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <QSvgRenderer>

#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QImage>
#include <QQuickWindow>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QtQuick/private/qquickevents_p_p.h>
#include <QImageReader>
#include <QThread>
#include <QTemporaryDir>
#include <QTest>
#include <algorithm>
#include <cstdio>

// Unit tests supply a cached catalogue; the GUI probe exercises the real archive backend.
std::optional<QList<FileEntry>> ArchiveFileProvider::cachedDirectoryEntries(const QString &path, bool showHidden,
    int maxEntries, const std::shared_ptr<std::atomic_bool> &cancelled)
{
    const QString root = QStringLiteral("archive:///tmp/folder-map-test.zip|/");
    FileEntry entry;
    if (path == root) {
        entry.name = QStringLiteral("Branch");
        entry.isDirectory = true;
    } else if (path == root + QStringLiteral("Branch")) {
        entry.name = QStringLiteral("child.txt");
        entry.suffix = QStringLiteral("txt");
    } else return std::nullopt;
    entry.path = ArchiveSupport::archiveChildPath(path.endsWith(QLatin1Char('/')) ? path : path + QLatin1Char('/'), entry.name);
    Q_UNUSED(showHidden)
    return maxEntries > 0 && !cancelled->load() ? QList<FileEntry>{entry} : QList<FileEntry>{};
}

class MapDirectoryModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString currentPath MEMBER path)
    Q_PROPERTY(bool loading MEMBER loading NOTIFY loadingChanged)
public:
    QString path;
    bool loading = false;
    QStringList names;
    enum { Name = Qt::UserRole + 1, Path, Directory, Suffix, Selected };
    QHash<int, QByteArray> roleNames() const override {
        return {{Name, "name"}, {Path, "path"}, {Directory, "isDirectory"}, {Suffix, "suffix"}, {Selected, "isSelected"}};
    }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : int(names.size()); }
    QVariant data(const QModelIndex &index, int role) const override {
        if (!index.isValid() || index.row() >= names.size()) return {};
        const QString name = names[index.row()];
        if (role == Name) return name;
        if (role == Path) return QDir(path).filePath(name);
        if (role == Suffix) return QFileInfo(name).suffix();
        if (role == Directory || role == Selected) return false;
        return {};
    }
    void append(const QString &name) {
        const int row = int(names.size());
        beginInsertRows({}, row, row); names.append(name); endInsertRows();
    }
    void removeFirst() { beginRemoveRows({}, 0, 0); names.removeFirst(); endRemoveRows(); }
    void renameFirst(const QString &name) { names[0] = name; emit dataChanged(index(0), index(0), {Name, Path, Suffix}); }
    void reverse() { emit layoutAboutToBeChanged(); std::reverse(names.begin(), names.end()); emit layoutChanged(); }
signals:
    void loadingChanged();
};

class MapSnapshotModel : public MapDirectoryModel
{
    Q_OBJECT
public:
    Q_INVOKABLE QList<FileEntry> mapEntries(int maximumEntries) const {
        Q_UNUSED(maximumEntries)
        FileEntry folder; folder.name = "sub"; folder.path = path + "/sub"; folder.isDirectory = true;
        FileEntry image; image.name = "photo.png"; image.path = path + "/photo.png"; image.suffix = "png";
        image.primaryBadgeKind = "locked";
        return {folder, image};
    }
};

class MapThumbnailProvider : public QQuickImageProvider
{
public:
    MapThumbnailProvider() : QQuickImageProvider(Image, ForceAsynchronousImageLoading) {}
    std::atomic<int> calls{0};
    std::atomic<bool> onGuiThread{false};
    int delay = 0;
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override
    {
        ++calls;
        if (QThread::currentThread() == qApp->thread()) onGuiThread = true;
        if (delay) QThread::msleep(delay);
        QString path = QUrl::fromPercentEncoding(id.toUtf8());
        path.truncate(path.indexOf(QStringLiteral("::thumbrev=")));
        QImageReader reader(path);
        reader.setScaledSize(reader.size().scaled(requestedSize, Qt::KeepAspectRatio));
        QImage image = reader.read();
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent); // The shared provider uses this for audio without cover art.
        }
        if (size) *size = image.size();
        return image;
    }
};

class TestMap : public FolderMapItem
{
public:
    using FolderMapItem::updatePolish;
};

class FolderMapTest : public QObject
{
    Q_OBJECT
private slots:
    void badgeColorBindingsDoNotLoop()
    {
        QQmlEngine engine;
        engine.setOutputWarningsToStandardError(false);
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, &engine, [&](const QList<QQmlError> &errors) {
            for (const auto &error : errors) warnings.append(error.toString());
        });
        FolderMapItem map;
        engine.rootContext()->setContextProperty("badgeMap", &map);
        QQmlComponent component(&engine);
        component.setData(QStringLiteral(R"(
            import QtQuick
            Item {
                id: fixture
                property color accent: "#4488cc"
                property color warning: "#dd5544"
                Binding { target: badgeMap; property: "badgeColor"; value: fixture.accent }
                Binding { target: badgeMap; property: "warningBadgeColor"; value: fixture.warning }
                Binding { target: badgeMap; property: "badgeTextColor"; value: Qt.lighter(badgeMap.badgeColor, 1.5) }
                Binding { target: badgeMap; property: "warningBadgeTextColor"; value: Qt.lighter(badgeMap.warningBadgeColor, 1.5) }
            }
        )").toUtf8(), QUrl());
        std::unique_ptr<QObject> fixture(component.create());
        QVERIFY2(fixture, qPrintable(component.errorString()));
        fixture->setProperty("accent", QColor("#224466"));
        fixture->setProperty("warning", QColor("#992211"));
        QCoreApplication::processEvents();
        QCOMPARE(map.property("badgeTextColor").value<QColor>(), QColor("#224466").lighter(150));
        QCOMPARE(map.property("warningBadgeTextColor").value<QColor>(), QColor("#992211").lighter(150));
        map.m_badgeAtlasDirty = false;
        map.setProperty("folderColor", QColor("#334455"));
        QVERIFY(!map.m_badgeAtlasDirty);
        map.setProperty("badgeColor", QColor("#778899"));
        QVERIFY(map.m_badgeAtlasDirty);
        QCOMPARE(map.property("badgeTextColor").value<QColor>(), QColor("#778899").lighter(150));
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void nestedBadgesAndLivePins()
    {
        QTemporaryDir temporary;
        const QString folder = temporary.filePath("folder");
        QVERIFY(QDir().mkpath(folder));
        const QString archive = folder + "/archive.zip";
        QFile file(archive);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QString locked = folder + "/locked";
        QVERIFY(QDir().mkpath(locked));
#ifdef Q_OS_UNIX
        QVERIFY(QFile::setPermissions(locked, {}));
#endif
        const auto restorePermissions = qScopeGuard([&] {
            QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
            LocalMountPointIndex::setMountRoots({});
        });
#ifdef Q_OS_UNIX
        QVERIFY(QFile::link(archive, folder + "/link"));
        QVERIFY(QFile::link(folder + "/missing", folder + "/broken"));
#endif
        const QString mount = folder + "/mount";
        QVERIFY(QDir().mkpath(mount));
        LocalMountPointIndex::setMountRoots({mount});
        FolderMapItem map;
        map.m_currentPath = folder;
        map.m_nodes.append(FolderMapItem::Node{});
        map.m_nodes[0].entry = {folder, "folder", true};
        map.m_nodes[0].rect = QRectF(0, 0, 1024, 768);
        map.m_byPath.insert(folder, 0);
        const auto listing = FolderMapItem::readDirectory(folder, false, map.m_cancelled);
        map.applyListing(0, listing);
        QCOMPARE(map.entryForPath(archive).value("primaryBadgeKind").toString(), QStringLiteral("archive"));
#ifdef Q_OS_UNIX
        QCOMPARE(map.entryForPath(locked).value("primaryBadgeKind").toString(), QStringLiteral("locked"));
#endif
        QCOMPARE(map.entryForPath(mount).value("primaryBadgeKind").toString(), QStringLiteral("mount-point"));
#ifdef Q_OS_UNIX
        QCOMPARE(map.entryForPath(folder + "/link").value("primaryBadgeKind").toString(), QStringLiteral("link"));
        QCOMPARE(map.entryForPath(folder + "/broken").value("primaryBadgeKind").toString(), QStringLiteral("broken-link"));
#endif
        const auto revision = map.m_listingSequence;
        map.setPinnedPaths({archive, locked});
        QVERIFY(map.entryForPath(archive).value("isPinned").toBool());
        QVERIFY(map.entryForPath(locked).value("isPinned").toBool());
        map.setPinnedPaths({});
        QVERIFY(!map.entryForPath(archive).value("isPinned").toBool());
        QCOMPARE(map.m_listingSequence, revision);
    }
    void expandedFolderBodiesHaveDistinctHitRegions()
    {
        FolderMapItem map;
        map.setWidth(1024); map.setHeight(768);
        map.m_currentPath = "/root";
        map.m_nodes.append(FolderMapItem::Node{});
        map.m_nodes[0].entry = {"/root", "root", true};
        map.m_nodes[0].rect = QRectF(0, 0, 1024, 768);
        map.m_byPath.insert("/root", 0);
        FolderMapItem::Listing listing;
        listing.entries = {{"/root/folder", "folder", true}};
        map.applyListing(0, listing);
        const int folder = map.m_byPath.value("/root/folder");
        FolderMapItem::Listing files;
        files.entries = {{"/root/folder/a.png", "a.png"}, {"/root/folder/b.png", "b.png"}};
        map.applyListing(folder, files);
        QList<int> requests;
        map.collectVisible(0, map.boundingRect(), requests);
        const QRectF rect = map.screenRect("/root/folder");
        const auto header = map.entryAt(rect.center().x(), rect.top() + 2);
        QCOMPARE(header.value("path").toString(), QStringLiteral("/root/folder"));
        QVERIFY(header.value("selectable").toBool());
        const auto gap = map.entryAt(rect.left() + 1, rect.center().y());
        QCOMPARE(gap.value("path").toString(), QStringLiteral("/root/folder"));
        QVERIFY(!gap.value("selectable").toBool());
        const QRectF file = map.screenRect("/root/folder/a.png");
        const auto child = map.entryAt(file.center().x(), file.center().y());
        QCOMPARE(child.value("path").toString(), QStringLiteral("/root/folder/a.png"));
        QVERIFY(child.value("selectable").toBool());
        map.setCategoryFilter(DirectoryModel::FilterDocuments);
        requests.clear();
        map.collectVisible(0, map.boundingRect(), requests);
        QVERIFY(!map.entryAt(rect.center().x(), rect.center().y()).value("selectable").toBool());
        map.setCategoryFilter(DirectoryModel::FilterAll);
        map.applyListing(folder, {});
        map.m_visible.clear();
        requests.clear();
        map.collectVisible(0, map.boundingRect(), requests);
        const auto emptyBody = map.entryAt(rect.center().x(), rect.center().y());
        QCOMPARE(emptyBody.value("path").toString(), QStringLiteral("/root/folder"));
        QVERIFY(!emptyBody.value("selectable").toBool());
        QVERIFY(map.entryAt(rect.center().x(), rect.top() + 2).value("selectable").toBool());
    }
    void rootUsesSnapshotBeforeCategoryFiltering()
    {
        MapSnapshotModel model;
        model.path = "/root";
        FolderMapItem map;
        map.m_directoryModel = &model;
        map.m_currentPath = model.path;
        map.m_gpuEnabled = true;
        map.m_nodes.append(FolderMapItem::Node{});
        map.m_nodes[0].entry = {model.path, "root", true};
        map.m_nodes[0].rect = QRectF(0, 0, 1024, 768);
        map.m_byPath.insert(model.path, 0);
        map.setCategoryFilter(DirectoryModel::FilterImages);
        map.syncModel();
        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(map.m_nodes[0].children.size(), 2);
        QVERIFY(!map.entryForPath("/root/sub").isEmpty());
        QVERIFY(!map.entryForPath("/root/photo.png").isEmpty());
        QCOMPARE(map.entryForPath("/root/photo.png").value("primaryBadgeKind").toString(), QStringLiteral("locked"));
    }
    void denseCategorySwitchDoesNotReload()
    {
        FolderMapItem map;
        map.m_nodes.append(FolderMapItem::Node{});
        map.m_nodes[0].entry = {"/root", "root", true};
        map.m_nodes[0].rect = QRectF(0, 0, 1024, 768);
        map.m_byPath.insert("/root", 0);
        FolderMapItem::Listing listing;
        for (int i = 0; i < 4096; ++i) {
            const QString name = QString::number(i) + (i % 2 ? ".png" : ".txt");
            listing.entries.append({"/root/" + name, name});
        }
        map.applyListing(0, listing);
        const auto revision = map.m_listingSequence;
        QElapsedTimer timer; timer.start();
        for (int i = 0; i < 20; ++i) {
            map.setCategoryFilter(DirectoryModel::FilterImages);
            map.setCategoryFilter(DirectoryModel::FilterDocuments);
        }
        const double average = timer.nsecsElapsed() / 40.0 / 1e6;
        std::printf("Map category switch: 4096 cached entries, average %.3f ms\n", average);
        QVERIFY(average < 25);
        QCOMPARE(map.m_listingSequence, revision);
        QCOMPARE(map.m_loading, 0);
        QCOMPARE(map.m_nodes[0].children.size(), 2048);
        QCOMPARE(map.loadedCount(), 4097);
    }
    void categoriesKeepFoldersAndCachedChildren()
    {
        FolderMapItem map;
        map.m_nodes.append(FolderMapItem::Node{});
        map.m_nodes[0].entry = {"/root", "root", true};
        map.m_nodes[0].rect = QRectF(0, 0, 1024, 768);
        map.m_byPath.insert("/root", 0);
        FolderMapItem::Listing root;
        root.entries = {{"/root/sub", "sub", true}, {"/root/top.png", "top.png"},
                        {"/root/top.txt", "top.txt"}};
        map.applyListing(0, root);
        const int sub = map.m_byPath.value("/root/sub");
        FolderMapItem::Listing nested;
        nested.entries = {{"/root/sub/photo.JPG", "photo.JPG"}, {"/root/sub/note.txt", "note.txt"}};
        map.applyListing(sub, nested);
        const auto generation = map.m_generation;
        const auto revision = map.m_listingSequence;
        const auto center = map.m_center;
        const auto scale = map.m_scale;
        map.setCategoryFilter(DirectoryModel::FilterImages);
        QCOMPARE(map.m_nodes[0].children.size(), 2);
        QVERIFY(!map.entryForPath("/root/sub").isEmpty());
        QVERIFY(map.entryForPath("/root/top.txt").isEmpty());
        QVERIFY(map.entryForPath("/root/sub/note.txt").isEmpty());
        QCOMPARE(map.selectionContext("/root/sub/photo.JPG").value("paths").toStringList(),
                 QStringList{"/root/sub/photo.JPG"});
        map.applyListing(sub, nested); // A late listing must use the current category.
        QCOMPARE(map.m_nodes[sub].children.size(), 1);
        const auto afterListing = map.m_listingSequence;
        QElapsedTimer timer; timer.start();
        for (int i = 0; i < 100; ++i) {
            map.setCategoryFilter(DirectoryModel::FilterDocuments);
            map.setCategoryFilter(DirectoryModel::FilterImages);
        }
        QVERIFY(timer.elapsed() < 1000);
        QCOMPARE(map.m_generation, generation);
        QCOMPARE(map.m_listingSequence, afterListing);
        QCOMPARE(revision, afterListing);
        QCOMPARE(map.m_center, center);
        QCOMPARE(map.m_scale, scale);
        map.setCategoryFilter(DirectoryModel::FilterAll);
        QCOMPARE(map.m_nodes[0].children.size(), 3);
        QCOMPARE(map.m_nodes[sub].children.size(), 2);
        QVERIFY(!map.entryForPath("/root/sub/note.txt").isEmpty());
        map.setCategoryFilter(DirectoryModel::FilterImages);
        map.remapEntry("/root/sub", "/root/renamed");
        map.setCategoryFilter(DirectoryModel::FilterAll);
        QVERIFY(!map.entryForPath("/root/renamed/note.txt").isEmpty());
        QVERIFY(map.entryForPath("/root/sub/note.txt").isEmpty());
        map.setCategoryFilter(DirectoryModel::FilterImages);
        map.removeBranch(sub);
        QVERIFY(!map.m_byPath.contains("/root/renamed/note.txt"));
    }
    void wheelZoomUsesRotationForMouseAndPixelsForGestures()
    {
        QFile source(QFINDTESTDATA("../../qml/components/filepanel/FilePanelMapView.qml"));
        QVERIFY(source.open(QIODevice::ReadOnly));
        const QString qml = QString::fromUtf8(source.readAll());
        const qsizetype start = qml.indexOf(QStringLiteral("onWheel: wheel => {"));
        QVERIFY(start >= 0);
        const qsizetype end = qml.indexOf(QStringLiteral("\n        }"), start);
        QVERIFY(end > start);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData((QStringLiteral(R"(import QtQuick
            MouseArea {
                property real lastSteps: 0
                property QtObject map: QtObject {
                    function zoomAt(x, y, steps) { parentArea.lastSteps = steps }
                }
                id: parentArea
        )") + qml.mid(start, end - start) + QStringLiteral("\n}}")).toUtf8(), QUrl());
        QScopedPointer<QObject> area(component.create());
        QVERIFY2(area, qPrintable(component.errorString()));
        auto check = [&](int angle, int pixel, Qt::ScrollPhase phase, qreal expected) {
            QWheelEvent event(QPointF(40, 30), QPointF(40, 30), QPoint(0, pixel), QPoint(0, angle),
                              Qt::NoButton, Qt::NoModifier, phase, false);
            QQuickWheelEvent wheel;
            wheel.reset(&event);
            QVERIFY(QMetaObject::invokeMethod(area.data(), "wheel", Q_ARG(QQuickWheelEvent *, &wheel)));
            QCOMPARE(area->property("lastSteps").toReal(), expected);
            QVERIFY(wheel.isAccepted());
        };
        check(120, 15, Qt::NoScrollPhase, 1);
        check(-120, -15, Qt::NoScrollPhase, -1);
        check(60, 8, Qt::NoScrollPhase, 0.5);
        check(120, 0, Qt::NoScrollPhase, 1);
        check(120, 15, Qt::ScrollUpdate, 15.0 / 80);
        check(0, 16, Qt::ScrollUpdate, 0.2);
        check(0, 0, Qt::ScrollBegin, 0);
    }

    void coldIconAtlasesSurviveImmediateShutdown()
    {
        for (int i = 0; i < 3; ++i) {
            QProcess child;
            child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--icon-atlas-shutdown-probe")});
            QVERIFY(child.waitForStarted());
            QVERIFY(child.waitForFinished(10000));
            const QByteArray output = child.readAllStandardOutput() + child.readAllStandardError();
            QVERIFY2(child.exitStatus() == QProcess::NormalExit, output.constData());
            QVERIFY2(child.exitCode() == 0, output.constData());
        }
    }

    void sharedShiftClickStartsAtFirstEntry()
    {
        QQmlEngine engine;
        QQmlComponent modelComponent(&engine);
        modelComponent.setData(R"(import QtQuick
            QtObject {
                property int count: 12
                property int origin: -1
                property int target: -1
                function firstSelectedRow() { return 2 }
                function extendOrTrimRange(from, to) { origin = from; target = to }
            })", QUrl());
        QScopedPointer<QObject> model(modelComponent.create());
        QVERIFY(model);
        const QString source = QFINDTESTDATA("../../qml/components/filepanel/FilePanelSelectionPolicy.qml");
        QVERIFY(!source.isEmpty());
        QQmlComponent component(&engine, QUrl::fromLocalFile(source));
        QScopedPointer<QObject> policy(component.create());
        QVERIFY2(policy, qPrintable(component.errorString()));
        policy->setProperty("directoryModel", QVariant::fromValue(model.data()));
        for (int target : {2, 6, 4}) {
            QVERIFY(QMetaObject::invokeMethod(policy.data(), "selectClickedRow",
                Q_ARG(QVariant, target), Q_ARG(QVariant, int(Qt::ShiftModifier))));
            QCOMPARE(model->property("origin").toInt(), 0);
            QCOMPARE(model->property("target").toInt(), target);
        }
    }

    void disabledMapDoesNotScan()
    {
        QTemporaryDir directory;
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        QTest::qWait(30);
        QCOMPARE(map.loadedCount(), 0);
        QVERIFY(map.entryAt(400, 300).isEmpty());
    }

    void concurrentMapsShareNativeAtlas()
    {
        QTemporaryDir directory;
        QList<std::shared_ptr<TestMap>> maps;
        for (int i = 0; i < 8; ++i) {
            auto map = std::make_shared<TestMap>();
            map->setCurrentPath(directory.path());
            map->setGpuEnabled(true);
            maps.append(map);
        }
        for (const auto &map : maps) QTRY_VERIFY(map->iconsReady());
        for (const auto &map : maps) QCOMPARE(map->m_iconAtlas.cacheKey(), maps.first()->m_iconAtlas.cacheKey());
    }

    void lazyBranchesAndNavigation()
    {
        QTemporaryDir directory;
        QDir root(directory.path());
        for (int i = 0; i < 100; ++i) QVERIFY(root.mkpath(QString::number(i) + "/deep"));
        QVERIFY(root.mkdir(".hidden"));
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 101);
        map.updatePolish();
        QTest::qWait(30);
        QCOMPARE(map.loadedCount(), 101); // Small cells have not read their children.
        const QString child = root.filePath("0");
        const QRectF rect = map.screenRect(child);
        QCOMPARE(map.entryAt(rect.center().x(), rect.top() + 2).value("path").toString(), child);
        map.setCurrentPath(child);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.screenRect(child + "/deep").isEmpty(); })(), 3000);
        QVERIFY(map.loadedCount() >= 102 && map.loadedCount() < 201);
        QVERIFY(map.screenRect(directory.path()).isEmpty());
        const int loadedBeforeReturn = map.loadedCount();
        map.setCurrentPath(directory.path());
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.moving(); })(), 3000);
        QVERIFY(map.loadedCount() >= loadedBeforeReturn); // Returning retains the surrounding hierarchy.
        map.setShowHidden(true);
        QTRY_COMPARE(map.loadedCount(), 102);
        QTRY_VERIFY(!map.screenRect(root.filePath(".hidden")).isEmpty());
    }

    void staleResultsAndSymlink()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QDir a(first.path());
        for (int i = 0; i < 300; ++i) QVERIFY(a.mkdir(QString::number(i)));
        QVERIFY(QDir(second.path()).mkdir("only"));
        QVERIFY(QFile::link(second.path(), second.path() + "/loop"));
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setGpuEnabled(true);
        map.setCurrentPath(first.path());
        map.setCurrentPath(second.path());
        QTRY_COMPARE(map.loadedCount(), 3);
        QTest::qWait(50);
        map.updatePolish();
        QCOMPARE(map.loadedCount(), 3);
        QVERIFY(map.screenRect(first.path() + "/0").isEmpty());
        map.revealPath(second.path() + "/loop");
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.moving(); })(), 3000);
        QCOMPARE(map.loadedCount(), 3);
        map.setGpuEnabled(false);
        QCOMPARE(map.loadedCount(), 0);
    }

    void currentFolderBounds()
    {
        QTemporaryDir directory;
        QVERIFY(QDir(directory.path()).mkpath("child/deep"));
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.screenRect(directory.path() + "/child").isEmpty(); })(), 3000);
        map.setCurrentPath(directory.path() + "/child");
        map.zoomAt(0, 0, -100);
        map.panBy(1e8, -1e8);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.moving(); })(), 3000);
        const QRectF fitted = map.screenRect(map.currentPath());
        QVERIFY(fitted.left() >= 0 && fitted.right() <= 800);
        QVERIFY(fitted.top() >= 0 && fitted.bottom() <= 600);
        QVERIFY(QLineF(fitted.center(), QPointF(400, 300)).length() < 0.1);
        QVERIFY(map.screenRect(directory.path()).isEmpty());
        map.zoomAt(400, 300, 10);
        map.panBy(-1e8, 1e8);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.moving(); })(), 3000);
        const QRectF zoomed = map.screenRect(map.currentPath());
        QVERIFY(zoomed.intersects(QRectF(0, 0, 800, 600)));
        QVERIFY(zoomed.width() > fitted.width());
        const qreal zoomBeforeResize = map.m_targetScale;
        map.setSize(QSizeF(1200, 900));
        QCOMPARE(map.m_targetScale, zoomBeforeResize);
        map.setSize(QSizeF(0, 0));
        map.setSize(QSizeF(1200, 900));
        QCOMPARE(map.m_targetScale, zoomBeforeResize);
        map.zoomAt(1200, 900, -100);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.moving(); })(), 3000);
        const QRectF resized = map.screenRect(map.currentPath());
        QVERIFY(QLineF(resized.center(), QPointF(600, 450)).length() < 0.1);
    }

    void selectingLoadedFolderKeepsZoom()
    {
        QTemporaryDir directory;
        const QString child = directory.filePath(QStringLiteral("child"));
        QVERIFY(QDir().mkpath(child));
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 2);
        map.zoomAt(400, 300, 10);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.moving(); })(), 3000);
        const qreal zoom = map.m_targetScale;
        map.setCurrentPath(child);
        QCOMPARE(map.m_targetScale, zoom);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return !map.moving(); })(), 3000);
        QCOMPARE(map.m_scale, zoom);
    }

    void iconsLoadAsynchronously()
    {
        QTemporaryDir directory;
        const QString imagePath = directory.path() + "/photo.jpg";
        QFile file(imagePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        QVERIFY(!map.iconsReady());
        map.setGpuEnabled(true);
        QTRY_VERIFY(map.iconsReady());
        QTRY_COMPARE(map.loadedCount(), 2);
        map.updatePolish();
        const QRectF rect = map.screenRect(imagePath);
        QCOMPARE(map.entryAt(rect.center().x(), rect.center().y()).value("iconName").toString(), QStringLiteral("image"));
        map.setIconsEnabled(false);
        QVERIFY(map.iconsReady());
        map.setIconsEnabled(true);
        QVERIFY(map.iconsReady()); // Toggling does not launch another atlas preparation.
    }

    void nativeIconSettingSwitchesAtlas()
    {
        TestMap map;
        QTemporaryDir directory;
        map.setCurrentPath(directory.path());
        QVERIFY(map.useNativeIcons());
        map.setGpuEnabled(true);
        QTRY_VERIFY(map.iconsReady());
        const QImage nativeAtlas = map.m_iconAtlas;
        IconProvider provider;
        QImage expected(128, 128, QImage::Format_ARGB32_Premultiplied);
        expected.fill(Qt::transparent);
        {
            QPainter painter(&expected);
            painter.drawImage(QRectF(0, 0, 128, 128), provider.requestImage(
                QStringLiteral("folder-map-folder?directory=true&generic=true"), nullptr, QSize(128, 128)));
        }
        QCOMPARE(nativeAtlas.copy(276, 140, 128, 128), expected);
        map.setUseNativeIcons(false);
        QVERIFY(!map.iconsReady());
        // A late bundled result must not replace the requested native atlas.
        map.setUseNativeIcons(true);
        QTRY_VERIFY(!map.m_iconAtlases[0].isNull());
        QCOMPARE(map.m_iconAtlas, nativeAtlas);
        map.setUseNativeIcons(false);
        QVERIFY(map.iconsReady());
        expected = QImage(544, 544, QImage::Format_ARGB32_Premultiplied);
        expected.fill(Qt::transparent);
        {
            QPainter painter(&expected);
            painter.setRenderHint(QPainter::Antialiasing);
            QSvgRenderer renderer(QStringLiteral(":/qt/qml/FM/qml/assets/filetypes-next/folder.svg"));
            renderer.render(&painter, QRectF(276, 140, 128, 128));
        }
        QCOMPARE(map.m_iconAtlas.copy(276, 140, 128, 128), expected.copy(276, 140, 128, 128));
        const qint64 bundledKey = map.m_iconAtlas.cacheKey();
        map.setUseNativeIcons(true);
        QCOMPARE(map.m_iconAtlas.cacheKey(), nativeAtlas.cacheKey());
        map.setUseNativeIcons(false);
        QCOMPARE(map.m_iconAtlas.cacheKey(), bundledKey);
    }

    void thumbnailsAreAsyncBoundedAndCancelled()
    {
        QTemporaryDir directory;
        QImage photo(320, 160, QImage::Format_RGB32);
        photo.fill(Qt::red);
        for (int i = 0; i < 100; ++i)
            QVERIFY(photo.save(directory.filePath(QStringLiteral("%1.png").arg(i))));
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("Other")));
        const QString replacement = directory.filePath(QStringLiteral("Other/replacement.png"));
        QVERIFY(photo.save(replacement));
        QQmlEngine engine;
        auto *provider = new MapThumbnailProvider;
        provider->delay = 25;
        engine.addImageProvider(QStringLiteral("thumbnail"), provider);
        TestMap map;
        QQmlEngine::setContextForObject(&map, engine.rootContext());
        map.setSize(QSizeF(1400, 1000));
        map.setCurrentPath(directory.path());
        map.setShowThumbnails(false);
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 102);
        map.updatePolish();
        QTest::qWait(50);
        QCOMPARE(provider->calls.load(), 0);
        map.setShowThumbnails(true);
        map.updatePolish();
        QVERIFY(map.m_thumbnails.size() <= 2);
        const QRectF before = map.screenRect(directory.filePath(QStringLiteral("0.png")));
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return map.thumbnailsReady() == 64; })(), 10000);
        QVERIFY(!provider->onGuiThread);
        QCOMPARE(map.m_thumbnails.size(), 64);
        QCOMPARE(provider->calls.load(), 64);
        QCOMPARE(map.screenRect(directory.filePath(QStringLiteral("0.png"))), before);
        map.setShowThumbnails(false);
        map.setShowThumbnails(true);
        map.updatePolish();
        QVERIFY(map.m_thumbnails.size() <= 2);
        map.setCurrentPath(directory.filePath(QStringLiteral("Other")));
        QCOMPARE(map.thumbnailsReady(), 0);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return map.loadedCount() == 103; })(), 5000);
        QTRY_VERIFY_WITH_TIMEOUT(([&] { map.updatePolish(); return map.thumbnailsReady() == 1; })(), 5000);
        QCOMPARE(map.m_thumbnails.keys(), QStringList{replacement});
        map.setShowThumbnails(false);
        QCOMPARE(map.thumbnailsReady(), 0);
        const int calls = provider->calls;
        QTest::qWait(100);
        map.updatePolish();
        QCOMPARE(provider->calls.load(), calls);
        map.setShowThumbnails(true);
        map.updatePolish();
        QTRY_COMPARE(map.thumbnailsReady(), 1);
        map.setGpuEnabled(false);
        QCOMPARE(map.thumbnailsReady(), 0);
    }

    void thumbnailSoftMissKeepsIcon()
    {
        QTemporaryDir directory;
        const QString path = directory.filePath(QStringLiteral("music.mp3"));
        QFile music(path);
        QVERIFY(music.open(QIODevice::WriteOnly));
        music.close();
        QQmlEngine engine;
        auto *provider = new MapThumbnailProvider;
        engine.addImageProvider(QStringLiteral("thumbnail"), provider);
        TestMap map;
        QQmlEngine::setContextForObject(&map, engine.rootContext());
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 2);
        map.updatePolish();
        QTRY_COMPARE(provider->calls.load(), 1);
        QTest::qWait(50);
        QCOMPARE(map.thumbnailsReady(), 0);
        const QRectF rect = map.screenRect(path);
        QCOMPARE(map.entryAt(rect.center().x(), rect.center().y()).value("iconName").toString(), QStringLiteral("music"));
        map.updatePolish();
        QTest::qWait(50);
        QCOMPARE(provider->calls.load(), 1);
    }

    void liveModelChangesPreserveCamera()
    {
        QTemporaryDir directory;
        MapDirectoryModel model;
        model.path = directory.path();
        model.loading = true;
        model.names = {QStringLiteral("a.txt"), QStringLiteral("b.txt")};
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setDirectoryModel(&model);
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTest::qWait(80);
        QCOMPARE(map.loadedCount(), 1); // The filesystem is empty; the model owns the root listing.
        model.loading = false;
        emit model.loadingChanged();
        QTRY_COMPARE(map.loadedCount(), 3);
        const QString a = directory.filePath(QStringLiteral("a.txt"));
        const qreal originalX = map.screenRect(a).x();
        const QPointF center = map.m_center;
        const qreal scale = map.m_scale;
        model.reverse();
        QTRY_VERIFY(map.screenRect(a).x() != originalX);
        model.append(QStringLiteral("c.png"));
        QTRY_COMPARE(map.loadedCount(), 4);
        model.renameFirst(QStringLiteral("renamed.pdf"));
        QTRY_VERIFY(!map.screenRect(directory.filePath(QStringLiteral("renamed.pdf"))).isEmpty());
        QVERIFY(map.screenRect(directory.filePath(QStringLiteral("b.txt"))).isEmpty());
        model.removeFirst();
        QTRY_COMPARE(map.loadedCount(), 3);
        QCOMPARE(map.m_center, center);
        QCOMPARE(map.m_scale, scale);
        const int nodes = int(map.m_nodes.size());
        for (int i = 0; i < 5; ++i) {
            model.append(QStringLiteral("new-%1.txt").arg(i));
            QTRY_COMPARE(map.loadedCount(), 4);
            model.removeFirst();
            QTRY_COMPARE(map.loadedCount(), 3);
        }
        QVERIFY(map.m_nodes.size() <= nodes + 1); // Removed branches release their storage slots.
        emit model.dataChanged(model.index(0), model.index(0), {MapDirectoryModel::Selected});
        QVERIFY(!map.m_modelTimer.isActive());
    }

    void staleHitDoesNotSelectReusedNode()
    {
        QTemporaryDir directory;
        MapDirectoryModel model;
        model.path = directory.path();
        model.names = {QStringLiteral("old.txt")};
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setDirectoryModel(&model);
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 2);
        map.updatePolish();
        const QPointF oldPosition = map.screenRect(directory.filePath(QStringLiteral("old.txt"))).center();
        model.renameFirst(QStringLiteral("new.txt"));
        const QString replacement = directory.filePath(QStringLiteral("new.txt"));
        QTRY_VERIFY(!map.screenRect(replacement).isEmpty());
        QVERIFY(map.entryAt(oldPosition.x(), oldPosition.y()).value("path").toString() != replacement);
        map.updatePolish();
        const QPointF position = map.screenRect(replacement).center();
        QCOMPARE(map.entryAt(position.x(), position.y()).value("path").toString(), replacement);
    }

    void renamedBranchPreservesLoadedChildrenAndCamera()
    {
        QTemporaryDir directory;
        const QString branch = directory.filePath(QStringLiteral("Branch"));
        QVERIFY(QDir().mkpath(branch));
        QFile file(QDir(branch).filePath(QStringLiteral("child.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 2);
        map.updatePolish();
        QTRY_COMPARE(map.loadedCount(), 3);
        map.updatePolish();
        const QRectF childRect = map.screenRect(file.fileName());
        const QPointF center = map.m_center;
        const qreal scale = map.m_scale;
        const QString renamed = directory.filePath(QStringLiteral("Renamed"));
        QVERIFY(QDir().rename(branch, renamed));
        map.remapEntry(branch, renamed);
        const QString newChild = QDir(renamed).filePath(QStringLiteral("child.txt"));
        QCOMPARE(map.screenRect(newChild), childRect);
        QVERIFY(map.entryForPath(file.fileName()).isEmpty());
        QCOMPARE(map.loadedCount(), 3);
        map.updatePolish();
        QTRY_VERIFY(map.m_watchers.contains(renamed));
        QTest::qWait(150);
        QCOMPARE(map.screenRect(newChild), childRect);
        QCOMPARE(map.m_center, center);
        QCOMPARE(map.m_scale, scale);
    }

    void expandedFolderWatchersUpdateLive()
    {
        QTemporaryDir directory;
        const QString branch = directory.filePath(QStringLiteral("Branch"));
        QVERIFY(QDir().mkpath(branch));
        QFile initial(QDir(branch).filePath(QStringLiteral("initial.txt")));
        QVERIFY(initial.open(QIODevice::WriteOnly)); initial.close();
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 2);
        map.updatePolish();
        QTRY_COMPARE(map.loadedCount(), 3);
        map.updatePolish();
        QTRY_VERIFY(map.m_watchers.contains(branch));
        const QString initialPath = initial.fileName();
        QCOMPARE(map.entryForPath(initialPath).value("directory").toBool(), false);
        QCOMPARE(map.selectionContext(initialPath).value("paths").toStringList(), QStringList{initialPath});
        const QPointF center = map.m_center;
        const QString added = QDir(branch).filePath(QStringLiteral("added.png"));
        QFile file(added);
        QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
        QTRY_VERIFY_WITH_TIMEOUT(!map.screenRect(added).isEmpty(), 5000);
        const QString renamed = QDir(branch).filePath(QStringLiteral("renamed.png"));
        QVERIFY(QFile::rename(added, renamed));
        QTRY_VERIFY_WITH_TIMEOUT(!map.screenRect(renamed).isEmpty() && map.screenRect(added).isEmpty(), 5000);
        QVERIFY(map.entryForPath(added).isEmpty());
        QVERIFY(map.selectionContext(renamed).value("paths").toStringList().contains(initialPath));
        QVERIFY(QFile::remove(renamed));
        QTRY_VERIFY_WITH_TIMEOUT(map.screenRect(renamed).isEmpty(), 5000);
        QVERIFY(map.entryForPath(renamed).isEmpty());
        QCOMPARE(map.selectionContext(initialPath).value("paths").toStringList(), QStringList{initialPath});
        QCOMPARE(map.m_center, center);
        map.setGpuEnabled(false);
        QVERIFY(map.m_watchers.isEmpty());
    }

    void mapRejectsNonLocalPaths()
    {
        QVERIFY(MapGpuProbe::supportsPath(QDir::tempPath()));
        for (const QString &path : {QString(), QStringLiteral("relative"), QStringLiteral("file:///tmp"),
                QStringLiteral("ftp://host/folder"), QStringLiteral("gdrive://folder"),
                QStringLiteral("mega://folder"), QStringLiteral("portable://device"),
                QStringLiteral("archive://gdrive://a.zip|/"), QStringLiteral("archive://relative.zip|/"), QStringLiteral("favorites://"),
                QStringLiteral("devices://"), QStringLiteral("new-provider://folder")}) {
            QVERIFY2(!MapGpuProbe::supportsPath(path), qPrintable(path));
            TestMap map;
            map.setCurrentPath(path);
            map.setGpuEnabled(true);
            QCOMPARE(map.loadedCount(), 0);
            QVERIFY(!map.m_atlasRequested[0] && !map.m_atlasRequested[1]);
        }
    }

    void archiveFoldersExpandWithoutFilesystemWatchers()
    {
        if (!ArchiveSupport::archiveBackendAvailable()) QSKIP("Archive backend unavailable");
        const QString root = QStringLiteral("archive:///tmp/folder-map-test.zip|/");
        QVERIFY(MapGpuProbe::supportsPath(root));
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(root);
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 2);
        map.updatePolish();
        QTRY_COMPARE(map.loadedCount(), 3);
        map.updatePolish();
        QVERIFY(!map.screenRect(root + QStringLiteral("Branch/child.txt")).isEmpty());
        QVERIFY(map.m_watchers.isEmpty());
        QVERIFY(map.m_watchFailures.isEmpty());
        map.setCurrentPath(root + QStringLiteral("Branch"));
        map.updatePolish();
        QVERIFY(!map.screenRect(root + QStringLiteral("Branch/child.txt")).isEmpty());
        map.setCurrentPath(QDir::tempPath());
        QVERIFY(map.screenRect(root + QStringLiteral("Branch/child.txt")).isEmpty());
    }

    void denseDirectoryIsBounded()
    {
        QTemporaryDir directory;
        for (int i = 0; i < 4200; ++i) {
            QFile file(directory.path() + "/" + QString::number(i));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 4097);
        map.updatePolish();
        QVERIFY(map.visibleCount() <= 2048);
        QVERIFY(map.status().contains("limited"));
    }

    void rapidRootsAndDisableDiscardPendingResults()
    {
        QTemporaryDir directory;
        QStringList roots;
        for (int i = 0; i < 4; ++i) {
            const QString path = directory.filePath(QString::number(i));
            QVERIFY(QDir().mkpath(path));
            for (int j = 0; j < 64; ++j) {
                QFile file(path + QStringLiteral("/root-%1-file-%2.txt").arg(i).arg(j));
                QVERIFY(file.open(QIODevice::WriteOnly));
            }
            roots.append(path);
        }
        TestMap left, right;
        for (auto *map : {&left, &right}) map->setSize(QSizeF(800, 600));
        for (int i = 0; i < 48; ++i) {
            for (auto *map : {&left, &right}) {
                map->setGpuEnabled(i % 4 == 3);
                map->setCurrentPath(roots.at(i % roots.size()));
                map->updatePolish();
            }
            QCoreApplication::processEvents();
        }
        for (auto *map : {&left, &right}) {
            map->setCurrentPath(roots.first());
            map->setGpuEnabled(true);
            QTRY_COMPARE(map->loadedCount(), 65);
            for (const auto &path : map->m_byPath.keys()) QVERIFY(path == roots.first() || path.startsWith(roots.first() + "/"));
            map->setGpuEnabled(false);
            QTRY_VERIFY(map->findChildren<QFutureWatcherBase *>().isEmpty());
            QTest::qWait(50);
            QCOMPARE(map->loadedCount(), 0);
            QVERIFY(map->m_watchers.isEmpty());
            QVERIFY(map->m_thumbnails.isEmpty());
            QCOMPARE(map->m_nodes.capacity(), 0);
            QCOMPARE(map->m_visible.capacity(), 0);
        }
    }

    void nestedWatcherCoalescesBulkChanges()
    {
        QTemporaryDir directory;
        const QString branch = directory.filePath(QStringLiteral("Branch"));
        QVERIFY(QDir().mkpath(branch));
        TestMap map;
        map.setSize(QSizeF(800, 600));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 2);
        map.updatePolish();
        QTRY_VERIFY(map.m_nodes[map.m_byPath.value(branch)].loaded);
        map.updatePolish();
        QTRY_VERIFY(map.m_watchers.contains(branch));
        QStringList expected;
        for (int i = 0; i < 120; ++i) {
            const QString path = branch + QStringLiteral("/item-%1.txt").arg(i);
            QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); file.close();
            if (i < 60) {
                const QString renamed = branch + QStringLiteral("/renamed-%1.txt").arg(i);
                QVERIFY(QFile::rename(path, renamed)); expected.append(renamed);
            } else if (i < 90) QVERIFY(QFile::remove(path));
            else expected.append(path);
        }
        const QString anchor = expected.first();
        QTRY_COMPARE(map.selectionContext(anchor).value("paths").toStringList().size(), expected.size());
        const QStringList paths = map.selectionContext(anchor).value("paths").toStringList();
        for (const QString &path : expected) QVERIFY(paths.contains(path));
        QVERIFY(map.entryForPath(branch + QStringLiteral("/item-0.txt")).isEmpty());
        QVERIFY(map.entryForPath(branch + QStringLiteral("/item-60.txt")).isEmpty());
        QVERIFY(map.m_loading <= 2);
        QVERIFY(map.m_watchers.size() <= 32);
    }

    void unexpandableEntriesDoNotStarveFolderListings()
    {
        for (bool symlinks : {false, true}) {
            QTemporaryDir directory, target;
            for (int i = 0; i < 8; ++i) {
                const QString path = directory.filePath(QStringLiteral("A-%1").arg(i));
                if (symlinks) QVERIFY(QFile::link(target.path(), path));
                else { QFile file(path); QVERIFY(file.open(QIODevice::WriteOnly)); }
            }
            const QString branch = directory.filePath(QStringLiteral("Z-normal"));
            QVERIFY(QDir().mkpath(branch));
            QFile child(branch + QStringLiteral("/child.txt"));
            QVERIFY(child.open(QIODevice::WriteOnly));
            child.close();
            TestMap map;
            map.setIconsEnabled(false);
            map.setSize(QSizeF(1600, 1200));
            map.setCurrentPath(directory.path());
            map.setGpuEnabled(true);
            QTRY_VERIFY(map.loadedCount() >= 10);
            QTRY_VERIFY_WITH_TIMEOUT(([&] {
                map.updatePolish();
                const int index = map.m_byPath.value(branch, -1);
                return index >= 0 && map.m_nodes[index].loaded;
            })(), 2000);
            QVERIFY(!map.entryForPath(child.fileName()).isEmpty());
        }
    }

    void contextEntriesExposeArchiveAndIsoTypes()
    {
        QTemporaryDir directory;
        for (const QString &name : {QStringLiteral("file.ZIP"), QStringLiteral("image.ISO"),
                                   QStringLiteral("image.udf"), QStringLiteral("plain.txt")}) {
            QFile file(directory.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QVERIFY(QDir(directory.path()).mkdir(QStringLiteral("folder.zip")));
        TestMap map;
        map.setIconsEnabled(false);
        map.setSize(QSizeF(1600, 1200));
        map.setCurrentPath(directory.path());
        map.setGpuEnabled(true);
        QTRY_COMPARE(map.loadedCount(), 6);
        map.updatePolish();
        for (const QString &name : {QStringLiteral("file.ZIP"), QStringLiteral("image.ISO"),
                                   QStringLiteral("image.udf"), QStringLiteral("plain.txt"), QStringLiteral("folder.zip")}) {
            const QString path = directory.filePath(name);
            const QVariantMap entry = map.entryForPath(path);
            QVERIFY(entry.contains("isArchiveFile"));
            QVERIFY(entry.contains("isIsoImageFile"));
            QCOMPARE(entry.value("isArchiveFile").toBool(), name == QStringLiteral("file.ZIP"));
            QCOMPARE(entry.value("isIsoImageFile").toBool(), name.startsWith(QStringLiteral("image.")));
            const QRectF rect = map.screenRect(path);
            const QVariantMap hit = map.entryAt(rect.center().x(), rect.top() + 2);
            QCOMPARE(hit.value("isArchiveFile"), entry.value("isArchiveFile"));
            QCOMPARE(hit.value("isIsoImageFile"), entry.value("isIsoImageFile"));
        }
        QTRY_VERIFY(map.m_nodes[map.m_byPath.value(directory.filePath(QStringLiteral("folder.zip")))].loaded);
    }

    void softwareRendererIsRejected()
    {
        QQuickWindow window;
        MapGpuProbe probe(window.contentItem());
        window.resize(100, 100);
        window.show();
        QTRY_VERIFY(probe.ready());
        QVERIFY(!probe.available());
    }
};

int gpuSmoke(QGuiApplication &app)
{
    QTemporaryDir directory;
    QDir root(directory.path());
    for (int i = 0; i < 36; ++i) {
        for (int j = 0; j < 36; ++j) root.mkpath(QStringLiteral("Folder %1/Child %2").arg(i).arg(j));
    }
    const bool thumbnailSmoke = app.arguments().contains(QStringLiteral("--thumbnail-smoke"));
    const bool coldThumbnails = app.arguments().contains(QStringLiteral("--cold-thumbnails"));
    const QStringList suffixes = thumbnailSmoke ? QStringList{"jpg"}
        : QStringList{"jpg", "pdf", "zip", "mp3", "txt", "cpp"};
    for (int i = 0; i < 72; ++i) {
        QFile file(root.filePath(QStringLiteral("File %1.%2").arg(i).arg(suffixes[i % suffixes.size()])));
        if (suffixes[i % suffixes.size()] == QLatin1String("jpg")) {
            QImage image(640, 360, QImage::Format_RGB32);
            image.fill(QColor::fromHsv((i * 19) % 360, 180, 220));
            if (!image.save(file.fileName())) return 1;
        } else if (!file.open(QIODevice::WriteOnly)) return 1;
    }
    QQmlEngine engine;
    auto *thumbnailProvider = new MapThumbnailProvider;
    thumbnailProvider->delay = coldThumbnails ? 40 : 0;
    engine.addImageProvider(QStringLiteral("thumbnail"), thumbnailProvider);
    QQuickWindow window;
    window.setTitle(QStringLiteral("FMQml GPU folder map test"));
    window.setColor(QColor("#111820"));
    const QSize viewport = thumbnailSmoke ? QSize(1500, 1050) : QSize(1000, 700);
    window.resize(viewport);
    FolderMapItem map(window.contentItem());
    QQmlEngine::setContextForObject(&map, engine.rootContext());
    map.setShowThumbnails(!app.arguments().contains(QStringLiteral("--without-thumbnails")));
    map.setSize(viewport);
    map.setCurrentPath(directory.path());
    map.setUseNativeIcons(!app.arguments().contains(QStringLiteral("--bundled-icons")));
    const bool coldIcons = app.arguments().contains(QStringLiteral("--cold-icons"));
    map.setIconsEnabled(!coldIcons && !app.arguments().contains(QStringLiteral("--without-icons")));
    MapGpuProbe probe(window.contentItem());
    QObject::connect(&probe, &MapGpuProbe::capabilityChanged, &map, [&] {
        map.setGpuEnabled(probe.available());
    });
    window.show();
    QElapsedTimer deadline;
    deadline.start();
    while (!probe.ready() && deadline.elapsed() < 5000) {
        app.processEvents();
        QTest::qWait(5);
    }
    if (!probe.available()) {
        std::fprintf(stderr, "Hardware GPU unavailable: %s\n", qPrintable(probe.deviceName()));
        return 1;
    }
    while ((map.loadedCount() < 109 || (map.iconsEnabled() && !map.iconsReady())
            || (thumbnailSmoke && !coldThumbnails && map.showThumbnails() && map.thumbnailsReady() < 64))
            && deadline.elapsed() < 10000) {
        app.processEvents();
        QTest::qWait(5);
    }
    if (thumbnailSmoke && !coldThumbnails) {
        // Ready images precede GPU uploads. Drain the one-texture-per-frame budget before steady timing.
        int warmFrames = 0;
        const auto warming = QObject::connect(&window, &QQuickWindow::frameSwapped, &window, [&] {
            ++warmFrames;
            map.update();
        }, Qt::QueuedConnection);
        map.update();
        deadline.restart();
        while (warmFrames < 70 && deadline.elapsed() < 5000) {
            app.processEvents();
            QTest::qWait(1);
        }
        QObject::disconnect(warming);
        if (warmFrames < 70) return 1;
    }
    QList<double> intervals;
    QList<QPair<int, double>> longFrames;
    QElapsedTimer frameClock;
    frameClock.start();
    int frames = 0;
    QObject::connect(&window, &QQuickWindow::frameSwapped, &window, [&] {
        if (frames >= 210) return;
        const double ms = frameClock.nsecsElapsed() / 1e6;
        frameClock.restart();
        if (++frames > (coldThumbnails ? 0 : 30)) {
            intervals.append(ms);
            if (ms >= 12) longFrames.append({frames, ms});
        }
        if (coldIcons && frames == 60) map.setIconsEnabled(true);
        if (frames < 210) {
            map.zoomAt(500, 350, frames < 105 ? 0.04 : -0.04);
        }
    }, Qt::QueuedConnection);
    window.update();
    deadline.restart();
    while (frames < 210 && deadline.elapsed() < 15000) {
        app.processEvents();
        QTest::qWait(1);
    }
    const int readyDuringZoom = map.thumbnailsReady();
    if (thumbnailSmoke && map.showThumbnails()) {
        deadline.restart();
        while ((map.moving() || map.thumbnailsReady() < 64) && deadline.elapsed() < 10000) {
            app.processEvents();
            QTest::qWait(5);
        }
    }
    const QImage capture = window.grabWindow();
    if (frames < 210 || capture.isNull() || !capture.save("/tmp/folder-map-gpu.png")) return 1;
    if (map.iconsEnabled() && !map.iconsReady()) return 1;
    if (thumbnailSmoke && !coldThumbnails && map.showThumbnails() && map.thumbnailsReady() < 64) return 1;
    std::fprintf(stdout, "Thumbnails: %s; ready during zoom/settled: %d/%d; provider calls: %d\n",
        coldThumbnails ? "cold arrival" : map.showThumbnails() ? "on" : "off",
        readyDuringZoom, map.thumbnailsReady(), thumbnailProvider->calls.load());
    for (const auto &sample : longFrames)
        std::fprintf(stdout, "Long frame: %d; interval: %.2f ms\n", sample.first, sample.second);
    std::sort(intervals.begin(), intervals.end());
    std::fprintf(stdout, "GPU: %s; icons: %s; frames: %d; frame interval p50/p95/max: %.2f/%.2f/%.2f ms; visible: %d\n",
                 qPrintable(probe.deviceName()), coldIcons ? "cold arrival" : map.iconsEnabled() ? "on" : "off",
                 frames, intervals[intervals.size() / 2], intervals[int(intervals.size() * 0.95)],
                 intervals.last(), map.visibleCount());
    return 0;
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    const auto iconAtlasCleanup = qScopeGuard([] { FolderMapItem::waitForIconAtlases(); });
    if (app.arguments().contains(QStringLiteral("--icon-atlas-shutdown-probe"))) {
        // Hold the only worker so both shared atlas tasks remain pending at exit.
        QThreadPool::globalInstance()->setMaxThreadCount(1);
        const auto blocker = QtConcurrent::run([] { QThread::msleep(250); });
        MapDirectoryModel model;
        model.path = QDir::tempPath();
        TestMap bundled, native;
        for (auto *map : {&bundled, &native}) {
            map->setDirectoryModel(&model);
            map->setCurrentPath(model.path);
        }
        bundled.setUseNativeIcons(false);
        bundled.setGpuEnabled(true);
        native.setUseNativeIcons(true);
        native.setGpuEnabled(true);
        for (auto *map : {&bundled, &native}) {
            const auto *watcher = map->findChild<QFutureWatcherBase *>();
            if (!watcher || watcher->isFinished()) return 1;
        }
        return 0;
    }
    if (app.arguments().contains(QStringLiteral("--gpu-smoke"))) return gpuSmoke(app);
    FolderMapTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "FolderMapTest.moc"
