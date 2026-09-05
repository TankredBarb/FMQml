#include "SelectionBenchmark.h"

#include "../controllers/FilePanelController.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QThread>

#include <cstdio>
#include <functional>

namespace {
bool selectionMatches(const DirectoryModel &model, const QSet<QString> &expected)
{
    const QStringList paths = model.selectedPaths();
    if (model.selectedCount() != expected.size() || paths.size() != expected.size()
        || QSet<QString>(paths.begin(), paths.end()) != expected) return false;
    for (int row = 0; row < model.count(); ++row) {
        if (model.data(model.index(row), DirectoryModel::IsSelectedRole).toBool()
            != expected.contains(model.pathAt(row))) return false;
    }
    return true;
}

QSet<QString> rowPaths(const DirectoryModel &model, int first, int last)
{
    QSet<QString> paths;
    for (int row = first; row <= last; ++row) paths.insert(model.pathAt(row));
    return paths;
}

bool runDataset(const QString &path, int count, QJsonArray *results)
{
    if (!QDir().mkpath(path)) return false;
    for (int i = 0; i < count; ++i) {
        QFile file(QDir(path).filePath(QStringLiteral("item-%1.txt").arg(i, 5, 10, QLatin1Char('0'))));
        if (!file.open(QIODevice::WriteOnly) || file.write("fixture\n") != 8) return false;
    }
    FilePanelController controller;
    DirectoryModel &model = *controller.directoryModel();
    model.setShowHidden(false);
    model.setSortPolicy(DirectoryModel::SortByName, Qt::AscendingOrder);
    QElapsedTimer loadTimer;
    loadTimer.start();
    if (!model.openPath(path)) return false;
    while (model.loading() && loadTimer.elapsed() < 30000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    }
    if (model.loading() || model.currentPath() != path || model.count() != count) return false;

    bool success = true;
    int dataSignals = 0;
    int selectionSignals = 0;
    bool validSignals = true;
    QSet<int> notifiedRows;
    const auto dataConnection = QObject::connect(&model, &DirectoryModel::dataChanged, &model,
                     [&](const QModelIndex &first, const QModelIndex &last, const QList<int> &roles) {
        ++dataSignals;
        validSignals = validSignals && first.isValid() && last.isValid()
            && first.row() <= last.row() && last.row() < model.count()
            && roles == QList<int>{DirectoryModel::IsSelectedRole};
        for (int row = first.row(); row <= last.row(); ++row) {
            validSignals = !notifiedRows.contains(row) && validSignals;
            notifiedRows.insert(row);
        }
    });
    const auto selectionConnection = QObject::connect(
        &model, &DirectoryModel::selectionChanged, &model, [&]() { ++selectionSignals; });

    auto measure = [&](const QString &scenario, const std::function<void()> &action,
                       const QSet<QString> &expected) {
        dataSignals = 0;
        selectionSignals = 0;
        validSignals = true;
        notifiedRows.clear();
        const QStringList before = model.selectedPaths();
        QElapsedTimer timer;
        timer.start();
        action();
        const double actionMs = timer.nsecsElapsed() / 1e6;
        const bool changed = QSet<QString>(before.begin(), before.end()) != expected;
        const QSet<QString> beforePaths(before.begin(), before.end());
        QSet<int> expectedNotifications;
        for (int row = 0; row < model.count(); ++row) {
            const QString path = model.pathAt(row);
            if (beforePaths.contains(path) != expected.contains(path)) expectedNotifications.insert(row);
        }
        const bool correct = selectionMatches(model, expected) && validSignals
            && notifiedRows == expectedNotifications
            && selectionSignals == (changed ? 1 : 0);
        if (!correct) {
            std::fprintf(stderr, "[selection-regression] failed: %s/%d\n",
                         scenario.toUtf8().constData(), count);
        }
        results->append(QJsonObject{
            {QStringLiteral("scenario"), scenario},
            {QStringLiteral("dataset"), QString::number(count)},
            {QStringLiteral("actionMs"), actionMs},
            {QStringLiteral("dataChangedSignals"), dataSignals},
            {QStringLiteral("selectionChangedSignals"), selectionSignals},
            {QStringLiteral("selectedCount"), model.selectedCount()},
            {QStringLiteral("success"), correct}
        });
        success = correct && success;
    };

    const QSet<QString> all = rowPaths(model, 0, count - 1);
    measure(QStringLiteral("select-all"), [&]() { model.selectAll(); }, all);
    measure(QStringLiteral("select-all-noop"), [&]() { model.selectAll(); }, all);

    // Real controller getters, as consumed by action bindings. No QML/rendering is timed here.
    QElapsedTimer capabilityTimer;
    capabilityTimer.start();
    const bool copy = controller.canCopySelection();
    const double copyMs = capabilityTimer.nsecsElapsed() / 1e6;
    capabilityTimer.restart();
    const bool rename = controller.canRenameSelection();
    const double renameMs = capabilityTimer.nsecsElapsed() / 1e6;
    capabilityTimer.restart();
    const bool remove = controller.canDeleteSelection();
    const double deleteMs = capabilityTimer.nsecsElapsed() / 1e6;
    results->append(QJsonObject{
        {QStringLiteral("scenario"), QStringLiteral("selection-capabilities")},
        {QStringLiteral("dataset"), QString::number(count)},
        {QStringLiteral("actionMs"), copyMs + renameMs + deleteMs},
        {QStringLiteral("copyMs"), copyMs},
        {QStringLiteral("renameMs"), renameMs},
        {QStringLiteral("deleteMs"), deleteMs},
        {QStringLiteral("success"), copy && rename && remove}
    });
    success = copy && rename && remove && success;

    measure(QStringLiteral("clear-all"), [&]() { model.clearSelection(); }, {});
    measure(QStringLiteral("clear-noop"), [&]() { model.clearSelection(); }, {});
    model.selectAll();
    measure(QStringLiteral("all-to-one"), [&]() { model.selectOnly(count / 2); },
            rowPaths(model, count / 2, count / 2));
    measure(QStringLiteral("one-to-one"), [&]() { model.selectOnly(count / 2 + 1); },
            rowPaths(model, count / 2 + 1, count / 2 + 1));
    measure(QStringLiteral("select-only-noop"), [&]() { model.selectOnly(count / 2 + 1); },
            rowPaths(model, count / 2 + 1, count / 2 + 1));
    model.clearSelection();
    measure(QStringLiteral("range-reversed"), [&]() { model.selectRange(count - 2, 1); },
            rowPaths(model, 1, count - 2));
    measure(QStringLiteral("trim-range"), [&]() { model.extendOrTrimRange(2, count - 3); },
            rowPaths(model, 2, count - 3));
    const QSet<QString> middle = rowPaths(model, 2, count - 3);
    measure(QStringLiteral("invert"), [&]() { model.invertSelection(); }, all - middle);
    measure(QStringLiteral("invalid-range-noop"), [&]() { model.selectRange(-1, count); }, all - middle);
    measure(QStringLiteral("invalid-only-clears"), [&]() { model.selectOnly(-1); }, {});

    QVariantList rows;
    QSet<QString> sparse;
    for (int row = 0; row < count; row += 3) {
        rows.append(row);
        sparse.insert(model.pathAt(row));
    }
    rows.append(-1);
    rows.append(count);
    rows.append(0); // Invalid and duplicate rows must not affect the result.
    measure(QStringLiteral("sparse-rows"), [&]() { model.selectRows(rows); }, sparse);
    measure(QStringLiteral("sparse-rows-noop"), [&]() { model.selectRows(rows); }, sparse);
    model.setSortOrder(Qt::DescendingOrder);
    success = selectionMatches(model, sparse) && success;
    for (int row = 0; row < count; ++row) {
        success = model.indexOfPath(model.pathAt(row)) == row && success;
    }
    measure(QStringLiteral("clear-sorted"), [&]() { model.clearSelection(); }, {});
    model.setSearchText(QStringLiteral("item-000"));
    success = model.count() == 100 && selectionMatches(model, {}) && success;
    const QSet<QString> filtered = rowPaths(model, 0, model.count() - 1);
    measure(QStringLiteral("select-filtered"), [&]() { model.selectAll(); }, filtered);
    measure(QStringLiteral("clear-filtered"), [&]() { model.clearSelection(); }, {});
    model.setSearchText({});
    success = model.count() == count && selectionMatches(model, {}) && success;
    // Disconnect captured locals before controller/model destruction.
    QObject::disconnect(dataConnection);
    QObject::disconnect(selectionConnection);
    return success;
}

bool runSelectionEdgeCases(QJsonArray *results)
{
    DirectoryModel model;
    model.clear();
    model.setShowHidden(false);
    // Feed the real provider batch slot after clear() resets the generation to zero.
    // This exercises provider-only rows without requiring a cloud account or a test-only model API.
    FileEntry file;
    file.name = QStringLiteral("a.txt");
    file.path = QStringLiteral("/selection-fixture/a.txt");
    file.suffix = QStringLiteral("txt");
    FileEntry folder;
    folder.name = QStringLiteral("folder");
    folder.path = QStringLiteral("/selection-fixture/folder");
    folder.isDirectory = true;
    FileEntry hidden = file;
    hidden.name = QStringLiteral(".hidden.txt");
    hidden.path = QStringLiteral("/selection-fixture/.hidden.txt");
    hidden.isHidden = true;
    FileEntry more;
    more.name = QStringLiteral("Load more");
    more.path = QStringLiteral("/selection-fixture/__load_more__");
    more.specialAction = FileEntrySpecialAction::LoadMore;
    const QList<FileEntry> entries{file, hidden, more, folder};
    bool ok = QMetaObject::invokeMethod(&model, "onScannerBatchReady", Qt::DirectConnection,
                                       Q_ARG(QList<FileEntry>, entries), Q_ARG(int, 0))
        && QMetaObject::invokeMethod(&model, "processPendingInserts", Qt::DirectConnection)
        && model.count() == 3;
    QElapsedTimer timer;
    timer.start();
    auto check = [&](const char *name, const QSet<QString> &expected) {
        const bool correct = selectionMatches(model, expected);
        if (!correct) std::fprintf(stderr, "[selection-regression] failed: %s\n", name);
        ok = correct && ok;
    };
    const QSet<QString> ordinary{file.path, folder.path};
    model.selectAll();
    check("special row excluded from selectAll", ordinary);
    const int moreRow = model.indexOfPath(more.path);
    model.toggleSelected(moreRow);
    check("special row cannot be toggled", ordinary);
    model.selectOnly(moreRow);
    check("selectOnly special clears selection", {});
    model.selectRange(model.count() - 1, 0);
    check("range excludes special", ordinary);
    model.invertSelection();
    check("invert excludes special", {});
    model.selectRows({moreRow, 0, 0, -1, QStringLiteral("invalid")});
    check("selectRows excludes special and invalid rows", {model.pathAt(0)});
    model.setShowHidden(true);
    check("show hidden preserves selection", {folder.path});
    model.selectAll();
    check("hidden selectable when shown", ordinary | QSet<QString>{hidden.path});
    model.setSortOrder(Qt::DescendingOrder);
    check("sorting preserves paths", ordinary | QSet<QString>{hidden.path});
    model.setShowHidden(false);
    check("hide preserves invisible selection", ordinary | QSet<QString>{hidden.path});
    model.clearSelection();
    check("clear includes hidden rows", {});
    model.toggleSelected(model.indexOfPath(file.path));
    check("toggle selects file", {file.path});
    model.toggleSelected(model.indexOfPath(file.path));
    check("toggle deselects file", {});

    // A provider metadata update can hide an already selected row without clearing it.
    model.selectOnly(model.indexOfPath(file.path));
    FileEntry hiddenUpdate = file;
    hiddenUpdate.isHidden = true;
    const QList<FileEntry> update{hiddenUpdate};
    ok = QMetaObject::invokeMethod(&model, "onScannerBatchReady", Qt::DirectConnection,
                                  Q_ARG(QList<FileEntry>, update), Q_ARG(int, 0)) && ok;
    ok = QMetaObject::invokeMethod(&model, "processPendingInserts", Qt::DirectConnection) && ok;
    check("invisible selection retained after metadata update", {file.path});
    model.selectAll();
    check("selectAll preserves invisible selection", ordinary);
    model.clearSelection();
    check("clear removes invisible selection", {});
    model.selectOnly(model.indexOfPath(folder.path));
    model.selectRows({});
    check("empty selectRows clears", {});
    model.clear();
    model.selectAll();
    model.invertSelection();
    model.selectOnly(0);
    model.selectRange(0, 0);
    check("empty model", {});
    results->append(QJsonObject{
        {QStringLiteral("scenario"), QStringLiteral("selection-edge-cases")},
        {QStringLiteral("dataset"), QStringLiteral("synthetic-provider")},
        {QStringLiteral("actionMs"), timer.nsecsElapsed() / 1e6},
        {QStringLiteral("success"), ok}
    });
    return ok;
}
}

int SelectionBenchmark::run()
{
    QTemporaryDir fixtures(QDir(QDir::tempPath()).filePath(QStringLiteral("fm-selection-benchmark-XXXXXX")));
    QJsonArray results;
    bool success = fixtures.isValid();
    for (const int count : {100, 1000, 10000}) {
        if (!success) break;
        std::fprintf(stderr, "[selection-benchmark] files=%d\n", count);
        success = runDataset(QDir(fixtures.path()).filePath(QString::number(count)), count, &results);
    }
    if (success) success = runSelectionEdgeCases(&results);
    const QJsonObject report{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("reportType"), QStringLiteral("selection-benchmark")},
        {QStringLiteral("fixtureProfile"), QStringLiteral("selection-local-v1")},
        {QStringLiteral("results"), results},
        {QStringLiteral("success"), success}
    };
    std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
    return success ? 0 : 1;
}
