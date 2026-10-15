#include "NavigationBenchmark.h"
#include "SelectionBenchmark.h"
#include "FolderMapStress.h"
#ifdef FM_ENABLE_RESIZE_BENCHMARK
#include "PanelResizeBenchmark.h"
#endif

#include "../app/AppServices.h"
#include "../controllers/FilePanelController.h"
#include "../controllers/QuickLookNavigationController.h"
#include "../controllers/AppSettingsController.h"
#include "../controllers/WorkspaceController.h"
#include "../models/DirectoryModel.h"
#include "../ui/foldermap/FolderMapItem.h"
#include "../ui/foldermap/MapGpuProbe.h"
#include "../core/ArchiveSupport.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQuickWindow>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlProperty>
#include <QPdfWriter>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QClipboard>
#include <QCryptographicHash>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

namespace {
constexpr int kTimeoutMs = 10000;

struct Dataset {
    QString name;
    QString path;
    int expectedVisibleCount = 0;
};

bool writeFile(const QString &path, const QByteArray &contents = {})
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly)
        && file.write(contents) == contents.size();
}

bool createFixtures(const QString &rootPath, QList<Dataset> *datasets, QString *error)
{
    QDir root(rootPath);
    const QString documentsPath = root.filePath(QStringLiteral("documents"));
    const QString photosPath = root.filePath(QStringLiteral("photos"));
    const QString mixedPath = root.filePath(QStringLiteral("mixed"));
    if (!root.mkpath(QStringLiteral("documents"))
        || !root.mkpath(QStringLiteral("photos"))
        || !root.mkpath(QStringLiteral("mixed"))) {
        *error = QStringLiteral("Could not create benchmark dataset directories");
        return false;
    }

    for (int i = 0; i < 250; ++i) {
        const QString name = QStringLiteral("document-%1.%2")
                                 .arg(i, 4, 10, QLatin1Char('0'))
                                 .arg(i % 3 == 0 ? QStringLiteral("txt")
                                                : (i % 3 == 1 ? QStringLiteral("md")
                                                              : QStringLiteral("json")));
        if (!writeFile(QDir(documentsPath).filePath(name), QByteArray("benchmark\n"))) {
            *error = QStringLiteral("Could not create Documents fixture");
            return false;
        }
    }

    // A valid deterministic 1x1 PNG. Dataset generation is outside the measured interval.
    const QByteArray png = QByteArray::fromBase64(
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=");
    for (int i = 0; i < 300; ++i) {
        const QString name = QStringLiteral("photo-%1.png").arg(i, 4, 10, QLatin1Char('0'));
        if (!writeFile(QDir(photosPath).filePath(name), png)) {
            *error = QStringLiteral("Could not create Photos fixture");
            return false;
        }
    }

    static const QStringList suffixes = {
        QStringLiteral("txt"), QStringLiteral("jpg"), QStringLiteral("mp4"),
        QStringLiteral("pdf"), QStringLiteral("zip"), QStringLiteral("md")
    };
    for (int i = 0; i < 570; ++i) {
        const QString name = QStringLiteral("mixed-%1.%2")
                                 .arg(i, 4, 10, QLatin1Char('0'))
                                 .arg(suffixes.at(i % suffixes.size()));
        if (!writeFile(QDir(mixedPath).filePath(name), QByteArray("mixed\n"))) {
            *error = QStringLiteral("Could not create Mixed fixture");
            return false;
        }
    }
    for (int i = 0; i < 20; ++i) {
        if (!QDir(mixedPath).mkdir(QStringLiteral("folder-%1").arg(i, 2, 10, QLatin1Char('0')))) {
            *error = QStringLiteral("Could not create Mixed folders");
            return false;
        }
    }
    for (int i = 0; i < 10; ++i) {
        if (!writeFile(QDir(mixedPath).filePath(
                QStringLiteral(".hidden-%1.txt").arg(i, 2, 10, QLatin1Char('0'))))) {
            *error = QStringLiteral("Could not create Mixed hidden fixtures");
            return false;
        }
    }

    *datasets = {
        {QStringLiteral("documents"), documentsPath, 250},
        {QStringLiteral("photos"), photosPath, 300},
        {QStringLiteral("mixed"), mixedPath, 590}
    };
    return true;
}

QJsonObject runLoad(DirectoryModel &model, const Dataset &dataset, const QString &scenario)
{
    std::fprintf(stderr, "[navigation-benchmark] start %s/%s\n",
                 scenario.toUtf8().constData(), dataset.name.toUtf8().constData());
    QElapsedTimer timer;
    timer.start();
    qint64 firstRowsMs = -1;
    const QMetaObject::Connection countConnection = QObject::connect(
        &model, &DirectoryModel::countChanged, &model, [&]() {
            if (firstRowsMs < 0 && model.count() > 0
                && QDir::cleanPath(model.currentPath()) == QDir::cleanPath(dataset.path)) {
                firstRowsMs = timer.elapsed();
            }
        });

    const bool accepted = model.openPath(dataset.path);
    std::fprintf(stderr, "[navigation-benchmark] accepted=%d %s/%s\n", accepted,
                 scenario.toUtf8().constData(), dataset.name.toUtf8().constData());
    while (accepted && timer.elapsed() < kTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (!model.loading()
            && QDir::cleanPath(model.currentPath()) == QDir::cleanPath(dataset.path)) {
            break;
        }
        QThread::msleep(1);
    }
    QObject::disconnect(countConnection);

    const bool timedOut = accepted && model.loading();
    const bool pathMatches = QDir::cleanPath(model.currentPath()) == QDir::cleanPath(dataset.path);
    const bool countMatches = model.count() == dataset.expectedVisibleCount;
    QJsonObject result{
        {QStringLiteral("scenario"), scenario},
        {QStringLiteral("dataset"), dataset.name},
        {QStringLiteral("accepted"), accepted},
        {QStringLiteral("firstRowsMs"), firstRowsMs},
        {QStringLiteral("settledMs"), timer.elapsed()},
        {QStringLiteral("expectedCount"), dataset.expectedVisibleCount},
        {QStringLiteral("actualCount"), model.count()},
        {QStringLiteral("pathMatches"), pathMatches},
        {QStringLiteral("timedOut"), timedOut}
    };
    result.insert(QStringLiteral("success"),
                  accepted && !timedOut && pathMatches && countMatches && firstRowsMs >= 0);
    std::fprintf(stderr, "[navigation-benchmark] settled %s/%s elapsed=%lld count=%d\n",
                 scenario.toUtf8().constData(), dataset.name.toUtf8().constData(),
                 static_cast<long long>(timer.elapsed()), model.count());
    return result;
}

bool resultSucceeded(const QJsonObject &result)
{
    return result.value(QStringLiteral("success")).toBool();
}

QString scenarioKey(const QJsonObject &result)
{
    QString key = result.value(QStringLiteral("scenario")).toString()
        + QLatin1Char('/') + result.value(QStringLiteral("dataset")).toString();
    if (result.contains(QStringLiteral("viewMode"))) {
        key += QStringLiteral("/viewMode=%1").arg(result.value(QStringLiteral("viewMode")).toInt());
    }
    return key;
}

double median(QList<double> values)
{
    if (values.isEmpty()) return 0.0;
    std::sort(values.begin(), values.end());
    const qsizetype middle = values.size() / 2;
    return values.size() % 2 == 0
        ? (values.at(middle - 1) + values.at(middle)) / 2.0
        : values.at(middle);
}

double percentile95(QList<double> values)
{
    if (values.isEmpty()) return 0.0;
    std::sort(values.begin(), values.end());
    const qsizetype index = qBound<qsizetype>(
        0, static_cast<qsizetype>(std::ceil(values.size() * 0.95)) - 1, values.size() - 1);
    return values.at(index);
}

QString optionValue(const QStringList &arguments, const QString &name)
{
    const qsizetype index = arguments.indexOf(name);
    return index >= 0 && index + 1 < arguments.size() ? arguments.at(index + 1) : QString{};
}

bool parseReport(const QByteArray &bytes, bool guiReport, bool selectionReport, QJsonObject *report, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(bytes.trimmed(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("Invalid benchmark JSON: %1").arg(parseError.errorString());
        return false;
    }
    *report = document.object();
    if (report->value(QStringLiteral("schemaVersion")).toInt() != 1
        || !report->value(QStringLiteral("success")).toBool()
        || !report->value(QStringLiteral("results")).isArray()
        || report->value(QStringLiteral("results")).toArray().isEmpty()) {
        *error = QStringLiteral("Benchmark report is incomplete or unsuccessful");
        return false;
    }
    for (const QJsonValue &value : report->value(QStringLiteral("results")).toArray()) {
        const QJsonObject result = value.toObject();
        const bool timingShapeValid = (selectionReport
            || result.value(QStringLiteral("scenario")).toString() == QLatin1String("selection-notifications"))
            ? result.value(QStringLiteral("actionMs")).isDouble()
                && result.value(QStringLiteral("actionMs")).toDouble() >= 0
            : guiReport
            ? result.value(QStringLiteral("modelSettledMs")).isDouble()
                && result.value(QStringLiteral("viewportReadyMs")).isDouble()
                && result.value(QStringLiteral("thumbnailsReadyMs")).isDouble()
            : result.value(QStringLiteral("firstRowsMs")).isDouble()
                && result.value(QStringLiteral("settledMs")).isDouble();
        if (!result.value(QStringLiteral("success")).toBool()
            || result.value(QStringLiteral("scenario")).toString().isEmpty()
            || result.value(QStringLiteral("dataset")).toString().isEmpty()
            || !timingShapeValid) {
            *error = QStringLiteral("Benchmark report contains an invalid scenario result");
            return false;
        }
    }
    return true;
}

QJsonArray aggregateReports(const QJsonArray &runs)
{
    QHash<QString, QHash<QString, QList<double>>> samples;
    QHash<QString, QJsonObject> labels;
    const QStringList timingNames = {
        QStringLiteral("firstRowsMs"), QStringLiteral("settledMs"),
        QStringLiteral("sequenceElapsedMs"), QStringLiteral("modelSettledMs"),
        QStringLiteral("viewportReadyMs"), QStringLiteral("firstThumbnailScheduledMs"),
        QStringLiteral("allThumbnailsScheduledMs"), QStringLiteral("firstThumbnailReadyMs"),
        QStringLiteral("thumbnailsReadyMs"), QStringLiteral("actionMs"),
        QStringLiteral("copyMs"), QStringLiteral("renameMs"), QStringLiteral("deleteMs")
    };
    for (const QJsonValue &runValue : runs) {
        for (const QJsonValue &resultValue : runValue.toObject().value(QStringLiteral("results")).toArray()) {
            const QJsonObject result = resultValue.toObject();
            const QString key = scenarioKey(result);
            QJsonObject label{
                {QStringLiteral("scenario"), result.value(QStringLiteral("scenario"))},
                {QStringLiteral("dataset"), result.value(QStringLiteral("dataset"))}
            };
            if (result.contains(QStringLiteral("viewMode"))) {
                label.insert(QStringLiteral("viewMode"), result.value(QStringLiteral("viewMode")));
            }
            labels.insert(key, label);
            for (const QString &timingName : timingNames) {
                if (result.value(timingName).isDouble() && result.value(timingName).toDouble() >= 0.0) {
                    samples[key][timingName].append(result.value(timingName).toDouble());
                }
            }
        }
    }

    QStringList keys = samples.keys();
    std::sort(keys.begin(), keys.end());
    QJsonArray aggregates;
    for (const QString &key : keys) {
        QJsonObject metrics;
        QStringList metricNames = samples.value(key).keys();
        std::sort(metricNames.begin(), metricNames.end());
        for (const QString &metricName : metricNames) {
            const QList<double> values = samples.value(key).value(metricName);
            metrics.insert(metricName, QJsonObject{
                {QStringLiteral("median"), median(values)},
                {QStringLiteral("p95"), percentile95(values)}
            });
        }
        QJsonObject aggregate = labels.value(key);
        aggregate.insert(QStringLiteral("metrics"), metrics);
        aggregates.append(aggregate);
    }
    return aggregates;
}

QJsonObject baselineChanges(const QJsonArray &current, const QJsonArray &baseline)
{
    QHash<QString, QJsonObject> baselineByKey;
    for (const QJsonValue &value : baseline) {
        const QJsonObject entry = value.toObject();
        baselineByKey.insert(scenarioKey(entry), entry);
    }

    QJsonObject changes;
    for (const QJsonValue &value : current) {
        const QJsonObject entry = value.toObject();
        const QString key = scenarioKey(entry);
        const QJsonObject baselineEntry = baselineByKey.value(key);
        if (baselineEntry.isEmpty()) continue;
        QJsonObject metricChanges;
        const QJsonObject metrics = entry.value(QStringLiteral("metrics")).toObject();
        const QJsonObject baselineMetrics = baselineEntry.value(QStringLiteral("metrics")).toObject();
        for (auto it = metrics.begin(); it != metrics.end(); ++it) {
            const double currentMedian = it.value().toObject().value(QStringLiteral("median")).toDouble();
            const double baselineMedian = baselineMetrics.value(it.key()).toObject()
                                              .value(QStringLiteral("median")).toDouble();
            if (baselineMedian > 0.0) {
                metricChanges.insert(it.key(), (currentMedian - baselineMedian) * 100.0 / baselineMedian);
            }
        }
        if (!metricChanges.isEmpty()) changes.insert(key, metricChanges);
    }
    return changes;
}

QVariant invokeBenchmarkMethod(QQuickWindow &window, const char *method,
                               const QVariantList &arguments = {})
{
    QVariant result;
    bool invoked = false;
    if (arguments.isEmpty()) {
        invoked = QMetaObject::invokeMethod(&window, method, Q_RETURN_ARG(QVariant, result));
    } else {
        invoked = QMetaObject::invokeMethod(&window, method, Q_RETURN_ARG(QVariant, result),
                                            Q_ARG(QVariant, arguments.constFirst()));
    }
    return invoked ? result : QVariant{};
}

QJsonObject runGuiScenario(QQuickWindow &window, FilePanelController &controller,
                           const Dataset &dataset, int viewMode, const QString &scenario,
                           const QList<Dataset> &replacementSequence = {})
{
    invokeBenchmarkMethod(window, "benchmarkSetViewMode", {viewMode});
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);

    QElapsedTimer timer;
    timer.start();
    if (!replacementSequence.isEmpty()) {
        for (const Dataset &replacement : replacementSequence) {
            invokeBenchmarkMethod(window, "benchmarkOpenPath", {replacement.path});
        }
    }
    const bool accepted = invokeBenchmarkMethod(window, "benchmarkOpenPath", {dataset.path}).toBool();
    qint64 modelSettledMs = -1;
    qint64 viewportReadyMs = -1;
    qint64 firstThumbnailScheduledMs = -1;
    qint64 allThumbnailsScheduledMs = -1;
    qint64 firstThumbnailReadyMs = -1;
    qint64 thumbnailsReadyMs = -1;
    QVariantMap snapshot;
    while (accepted && timer.elapsed() < kTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        window.requestUpdate();
        snapshot = invokeBenchmarkMethod(window, "benchmarkPanelSnapshot").toMap();
        const bool targetPath = QDir::cleanPath(controller.currentPath())
            == QDir::cleanPath(dataset.path);
        if (modelSettledMs < 0 && targetPath && !controller.directoryModel()->loading()
            && controller.directoryModel()->count() == dataset.expectedVisibleCount) {
            modelSettledMs = timer.elapsed();
        }
        if (modelSettledMs >= 0 && viewportReadyMs < 0
            && snapshot.value(QStringLiteral("visibleDelegateCount")).toInt() > 0
            && snapshot.value(QStringLiteral("visiblePathsMatch")).toBool()) {
            viewportReadyMs = timer.elapsed();
        }
        const int eligible = snapshot.value(QStringLiteral("eligibleThumbnailCount")).toInt();
        const int scheduled = snapshot.value(QStringLiteral("scheduledThumbnailCount")).toInt();
        const int ready = snapshot.value(QStringLiteral("readyThumbnailCount")).toInt();
        if (viewportReadyMs >= 0 && eligible > 0 && scheduled > 0
            && firstThumbnailScheduledMs < 0) {
            firstThumbnailScheduledMs = timer.elapsed();
        }
        if (viewportReadyMs >= 0 && eligible > 0 && scheduled == eligible
            && allThumbnailsScheduledMs < 0) {
            allThumbnailsScheduledMs = timer.elapsed();
        }
        if (viewportReadyMs >= 0 && ready > 0 && firstThumbnailReadyMs < 0) {
            firstThumbnailReadyMs = timer.elapsed();
        }
        if (viewportReadyMs >= 0 && eligible > 0
            && ready == eligible) {
            thumbnailsReadyMs = timer.elapsed();
        }
        if (viewportReadyMs >= 0 && (viewMode == 0 || thumbnailsReadyMs >= 0)) break;
        QThread::msleep(1);
    }

    const bool pathMatches = QDir::cleanPath(controller.currentPath()) == QDir::cleanPath(dataset.path);
    const int eligible = snapshot.value(QStringLiteral("eligibleThumbnailCount")).toInt();
    const int ready = snapshot.value(QStringLiteral("readyThumbnailCount")).toInt();
    const bool success = accepted && pathMatches && modelSettledMs >= 0 && viewportReadyMs >= 0
        && snapshot.value(QStringLiteral("visiblePathsMatch")).toBool()
        && (viewMode == 0 || (eligible > 0 && ready == eligible && thumbnailsReadyMs >= 0));
    return QJsonObject{
        {QStringLiteral("scenario"), scenario},
        {QStringLiteral("dataset"), dataset.name},
        {QStringLiteral("viewMode"), viewMode},
        {QStringLiteral("accepted"), accepted},
        {QStringLiteral("modelSettledMs"), modelSettledMs},
        {QStringLiteral("viewportReadyMs"), viewportReadyMs},
        {QStringLiteral("firstThumbnailScheduledMs"), firstThumbnailScheduledMs},
        {QStringLiteral("allThumbnailsScheduledMs"), allThumbnailsScheduledMs},
        {QStringLiteral("firstThumbnailReadyMs"), firstThumbnailReadyMs},
        {QStringLiteral("thumbnailsReadyMs"), thumbnailsReadyMs},
        {QStringLiteral("visibleDelegateCount"), snapshot.value(QStringLiteral("visibleDelegateCount")).toInt()},
        {QStringLiteral("instantiatedDelegateCount"), snapshot.value(QStringLiteral("instantiatedDelegateCount")).toInt()},
        {QStringLiteral("eligibleThumbnailCount"), eligible},
        {QStringLiteral("scheduledThumbnailCount"), snapshot.value(QStringLiteral("scheduledThumbnailCount")).toInt()},
        {QStringLiteral("readyThumbnailCount"), ready},
        {QStringLiteral("visiblePathsMatch"), snapshot.value(QStringLiteral("visiblePathsMatch")).toBool()},
        {QStringLiteral("pathMatches"), pathMatches},
        {QStringLiteral("success"), success}
    };
}

QJsonObject runGuiSelectionScenario(QQuickWindow &window, FilePanelController &controller,
                                    const Dataset &dataset, int viewMode)
{
    DirectoryModel &model = *controller.directoryModel();
    QSet<QString> all;
    for (int row = 0; row < model.count(); ++row) all.insert(model.pathAt(row));
    bool success = model.count() == dataset.expectedVisibleCount;
    double actionMs = 0;
    double settledMs = 0;
    auto verify = [&](const std::function<void()> &action, const QSet<QString> &expected) {
        QElapsedTimer timer;
        timer.start();
        action();
        actionMs += timer.nsecsElapsed() / 1e6;
        bool matches = false;
        while (timer.elapsed() < 2000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
            window.requestUpdate();
            const QVariantMap snapshot = invokeBenchmarkMethod(window, "benchmarkPanelSnapshot").toMap();
            const QStringList visiblePaths = snapshot.value(QStringLiteral("visiblePaths")).toStringList();
            const QStringList selectedPaths = snapshot.value(QStringLiteral("visibleSelectedPaths")).toStringList();
            const QStringList modelPaths = model.selectedPaths();
            const QSet<QString> visible(visiblePaths.begin(), visiblePaths.end());
            matches = !visible.isEmpty()
                && QSet<QString>(selectedPaths.begin(), selectedPaths.end()) == (expected & visible)
                && QSet<QString>(modelPaths.begin(), modelPaths.end()) == expected
                && model.selectedCount() == expected.size();
            if (matches) break;
            QThread::msleep(1);
        }
        settledMs += timer.nsecsElapsed() / 1e6;
        success = matches && success;
    };
    verify([&]() { model.selectAll(); }, all);
    verify([&]() { model.clearSelection(); }, {});
    const QSet<QString> single{model.pathAt(0)};
    verify([&]() { model.selectOnly(0); }, single);
    verify([&]() { model.invertSelection(); }, all - single);
    verify([&]() { model.clearSelection(); }, {});
    return QJsonObject{
        {QStringLiteral("scenario"), QStringLiteral("selection-notifications")},
        {QStringLiteral("dataset"), dataset.name},
        {QStringLiteral("viewMode"), viewMode},
        {QStringLiteral("actionMs"), actionMs},
        {QStringLiteral("settledMs"), settledMs},
        {QStringLiteral("success"), success}
    };
}

QJsonObject runFolderPeekScenario(QQuickWindow &window, FilePanelController &controller,
                                  const Dataset &dataset, int viewMode, const QString &scenario,
                                  const QList<Dataset> &replacementSequence = {})
{
    const QString initialPath = replacementSequence.isEmpty()
        ? dataset.path : replacementSequence.constFirst().path;
    const QVariantMap request{
        {QStringLiteral("path"), initialPath},
        {QStringLiteral("viewMode"), viewMode}
    };
    QElapsedTimer timer;
    timer.start();
    const bool accepted = invokeBenchmarkMethod(window, "benchmarkOpenFolderPeek", {request}).toBool();
    if (accepted && !replacementSequence.isEmpty()) {
        for (qsizetype index = 1; index < replacementSequence.size(); ++index) {
            controller.folderPeekController()->navigate(replacementSequence.at(index).path);
        }
        controller.folderPeekController()->navigate(dataset.path);
    }
    qint64 entriesReadyMs = -1;
    qint64 viewportReadyMs = -1;
    qint64 firstThumbnailScheduledMs = -1;
    qint64 allThumbnailsScheduledMs = -1;
    qint64 firstThumbnailReadyMs = -1;
    qint64 thumbnailsReadyMs = -1;
    QVariantMap snapshot;
    while (accepted && timer.elapsed() < kTimeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        window.requestUpdate();
        snapshot = invokeBenchmarkMethod(window, "benchmarkFolderPeekSnapshot").toMap();
        const bool targetPath = QDir::cleanPath(snapshot.value(QStringLiteral("currentPath")).toString())
            == QDir::cleanPath(dataset.path);
        if (entriesReadyMs < 0 && targetPath
            && snapshot.value(QStringLiteral("state")).toString() == QLatin1String("ready")
            && snapshot.value(QStringLiteral("entryCount")).toInt() == dataset.expectedVisibleCount) {
            entriesReadyMs = timer.elapsed();
        }
        if (entriesReadyMs >= 0 && viewportReadyMs < 0
            && snapshot.value(QStringLiteral("visibleDelegateCount")).toInt() > 0
            && snapshot.value(QStringLiteral("visiblePathsMatch")).toBool()) {
            viewportReadyMs = timer.elapsed();
        }
        const int eligible = snapshot.value(QStringLiteral("eligibleThumbnailCount")).toInt();
        const int scheduled = snapshot.value(QStringLiteral("scheduledThumbnailCount")).toInt();
        const int ready = snapshot.value(QStringLiteral("readyThumbnailCount")).toInt();
        if (viewportReadyMs >= 0 && eligible > 0 && scheduled > 0
            && firstThumbnailScheduledMs < 0) {
            firstThumbnailScheduledMs = timer.elapsed();
        }
        if (viewportReadyMs >= 0 && eligible > 0 && scheduled == eligible
            && allThumbnailsScheduledMs < 0) {
            allThumbnailsScheduledMs = timer.elapsed();
        }
        if (viewportReadyMs >= 0 && ready > 0 && firstThumbnailReadyMs < 0) {
            firstThumbnailReadyMs = timer.elapsed();
        }
        if (viewportReadyMs >= 0 && eligible > 0 && ready == eligible) {
            thumbnailsReadyMs = timer.elapsed();
            break;
        }
        QThread::msleep(1);
    }

    const bool pathMatches = QDir::cleanPath(snapshot.value(QStringLiteral("currentPath")).toString())
        == QDir::cleanPath(dataset.path);
    const int eligible = snapshot.value(QStringLiteral("eligibleThumbnailCount")).toInt();
    const int ready = snapshot.value(QStringLiteral("readyThumbnailCount")).toInt();
    const bool success = accepted && pathMatches && entriesReadyMs >= 0 && viewportReadyMs >= 0
        && snapshot.value(QStringLiteral("visiblePathsMatch")).toBool()
        && eligible > 0 && ready == eligible && thumbnailsReadyMs >= 0;
    controller.folderPeekController()->close();
    QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    return QJsonObject{
        {QStringLiteral("scenario"), scenario},
        {QStringLiteral("dataset"), dataset.name},
        {QStringLiteral("viewMode"), viewMode},
        {QStringLiteral("accepted"), accepted},
        {QStringLiteral("modelSettledMs"), entriesReadyMs},
        {QStringLiteral("viewportReadyMs"), viewportReadyMs},
        {QStringLiteral("firstThumbnailScheduledMs"), firstThumbnailScheduledMs},
        {QStringLiteral("allThumbnailsScheduledMs"), allThumbnailsScheduledMs},
        {QStringLiteral("firstThumbnailReadyMs"), firstThumbnailReadyMs},
        {QStringLiteral("thumbnailsReadyMs"), thumbnailsReadyMs},
        {QStringLiteral("visibleDelegateCount"), snapshot.value(QStringLiteral("visibleDelegateCount")).toInt()},
        {QStringLiteral("instantiatedDelegateCount"), snapshot.value(QStringLiteral("instantiatedDelegateCount")).toInt()},
        {QStringLiteral("eligibleThumbnailCount"), eligible},
        {QStringLiteral("scheduledThumbnailCount"), snapshot.value(QStringLiteral("scheduledThumbnailCount")).toInt()},
        {QStringLiteral("readyThumbnailCount"), ready},
        {QStringLiteral("visiblePathsMatch"), snapshot.value(QStringLiteral("visiblePathsMatch")).toBool()},
        {QStringLiteral("pathMatches"), pathMatches},
        {QStringLiteral("success"), success}
    };
}

QJsonObject runFolderPeekCloseScenario(QQuickWindow &window, FilePanelController &controller,
                                       const Dataset &dataset)
{
    const QVariantMap request{
        {QStringLiteral("path"), dataset.path},
        {QStringLiteral("viewMode"), 0}
    };
    QElapsedTimer timer;
    timer.start();
    const bool accepted = invokeBenchmarkMethod(window, "benchmarkOpenFolderPeek", {request}).toBool();
    controller.folderPeekController()->close();
    while (timer.elapsed() < 250) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        QThread::msleep(1);
    }
    const bool closedCleanly = !controller.folderPeekController()->isOpen()
        && controller.folderPeekController()->currentPath().isEmpty()
        && controller.folderPeekController()->entries().isEmpty()
        && controller.folderPeekController()->state() == QLatin1String("idle");
    return QJsonObject{
        {QStringLiteral("scenario"), QStringLiteral("folder-peek-close-during-load")},
        {QStringLiteral("dataset"), dataset.name},
        {QStringLiteral("viewMode"), 0},
        {QStringLiteral("accepted"), accepted},
        {QStringLiteral("modelSettledMs"), timer.elapsed()},
        {QStringLiteral("viewportReadyMs"), timer.elapsed()},
        {QStringLiteral("firstThumbnailScheduledMs"), -1},
        {QStringLiteral("allThumbnailsScheduledMs"), -1},
        {QStringLiteral("firstThumbnailReadyMs"), -1},
        {QStringLiteral("thumbnailsReadyMs"), 0},
        {QStringLiteral("closedCleanly"), closedCleanly},
        {QStringLiteral("success"), accepted && closedCleanly}
    };
}
}

int NavigationBenchmark::run(QApplication &app)
{
    if (app.arguments().contains(QStringLiteral("--selection"))) return SelectionBenchmark::run();
    std::fputs("[navigation-benchmark] creating fixtures\n", stderr);
    QTemporaryDir fixtureRoot(
        QDir(QDir::tempPath()).filePath(QStringLiteral("fm-navigation-benchmark-XXXXXX")));
    QJsonObject report{{QStringLiteral("schemaVersion"), 1}};
    if (!fixtureRoot.isValid()) {
        report.insert(QStringLiteral("success"), false);
        report.insert(QStringLiteral("error"), QStringLiteral("Could not create temporary fixture root"));
        std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return 2;
    }

    QList<Dataset> datasets;
    QString fixtureError;
    if (!createFixtures(fixtureRoot.path(), &datasets, &fixtureError)) {
        report.insert(QStringLiteral("success"), false);
        report.insert(QStringLiteral("error"), fixtureError);
        std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return 2;
    }

    std::fputs("[navigation-benchmark] running model scenarios\n", stderr);
    DirectoryModel model;
    std::fputs("[navigation-benchmark] model ready\n", stderr);
    QJsonArray results;
    bool success = true;
    for (const Dataset &dataset : datasets) {
        const QJsonObject result = runLoad(model, dataset, QStringLiteral("initial"));
        results.append(result);
        success = resultSucceeded(result) && success;
    }

    const QJsonObject warmResult = runLoad(model, datasets.constFirst(), QStringLiteral("warm-revisit"));
    results.append(warmResult);
    success = resultSucceeded(warmResult) && success;

    QElapsedTimer replacementTimer;
    replacementTimer.start();
    model.openPath(datasets.at(0).path);
    model.openPath(datasets.at(1).path);
    const QJsonObject replacement = runLoad(model, datasets.at(2), QStringLiteral("replacement-a-b-c"));
    QJsonObject replacementWithTotal = replacement;
    replacementWithTotal.insert(QStringLiteral("sequenceElapsedMs"), replacementTimer.elapsed());
    results.append(replacementWithTotal);
    success = resultSucceeded(replacementWithTotal) && success;

    report.insert(QStringLiteral("fixtureProfile"), QStringLiteral("everyday-local-v1"));
    report.insert(QStringLiteral("results"), results);
    report.insert(QStringLiteral("success"), success);
    std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
    std::fputs("[navigation-benchmark] complete\n", stderr);
    return success ? 0 : 1;
}

int NavigationBenchmark::runSuite(QApplication &app)
{
    const QStringList arguments = app.arguments();
    const bool guiSuite = arguments.contains(QStringLiteral("--gui"));
    const bool selectionSuite = arguments.contains(QStringLiteral("--selection"));
    if (guiSuite && selectionSuite) {
        std::fputs("--selection measures model/controller work; use it without --gui\n", stderr);
        return 2;
    }
    const QString suiteLabel = selectionSuite ? QStringLiteral("selection-benchmark-suite") : guiSuite
        ? QStringLiteral("navigation-gui-benchmark-suite")
        : QStringLiteral("navigation-benchmark-suite");
    bool runsOk = false;
    const QString runsValue = optionValue(arguments, QStringLiteral("--runs"));
    const int requestedRuns = runsValue.isEmpty() ? 7 : runsValue.toInt(&runsOk);
    if ((!runsValue.isEmpty() && !runsOk) || requestedRuns < 1 || requestedRuns > 50) {
        std::fputs("--runs must be an integer between 1 and 50\n", stderr);
        return 2;
    }

    QJsonArray runs;
    for (int index = 0; index < requestedRuns; ++index) {
        std::fprintf(stderr, "[%s] run %d/%d\n", suiteLabel.toUtf8().constData(),
                     index + 1, requestedRuns);
        QProcess process;
        QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        if (guiSuite) environment.insert(QStringLiteral("QSG_RHI_BACKEND"), QStringLiteral("software"));
        process.setProcessEnvironment(environment);
        process.setProgram(QCoreApplication::applicationFilePath());
        QStringList childArguments{guiSuite ? QStringLiteral("--navigation-gui-benchmark")
                                           : QStringLiteral("--navigation-benchmark")};
        if (selectionSuite) childArguments.append(QStringLiteral("--selection"));
        process.setArguments(childArguments);
        process.start();
        if (!process.waitForStarted(5000) || !process.waitForFinished(guiSuite || selectionSuite ? 60000 : 30000)) {
            process.kill();
            process.waitForFinished();
            std::fprintf(stderr, "Benchmark child %d did not finish\n", index + 1);
            return 1;
        }
        QJsonObject report;
        QString error;
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
            || !parseReport(process.readAllStandardOutput(), guiSuite, selectionSuite, &report, &error)) {
            std::fprintf(stderr, "Benchmark child %d failed: %s\n", index + 1,
                         error.toUtf8().constData());
            const QByteArray childError = process.readAllStandardError();
            if (!childError.isEmpty()) std::fwrite(childError.constData(), 1, childError.size(), stderr);
            return 1;
        }
        runs.append(report);
    }

    const QJsonArray aggregates = aggregateReports(runs);
    QJsonObject suite{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("reportType"), suiteLabel},
        {QStringLiteral("runCount"), requestedRuns},
        {QStringLiteral("timingsAreInformational"), true},
        {QStringLiteral("runs"), runs},
        {QStringLiteral("aggregates"), aggregates},
        {QStringLiteral("success"), true}
    };

    const QString baselinePath = optionValue(arguments, QStringLiteral("--baseline"));
    if (!baselinePath.isEmpty()) {
        QFile baselineFile(baselinePath);
        QJsonParseError parseError;
        if (!baselineFile.open(QIODevice::ReadOnly)) {
            std::fprintf(stderr, "Could not open baseline: %s\n", baselinePath.toUtf8().constData());
            return 2;
        }
        const QJsonDocument baselineDocument = QJsonDocument::fromJson(baselineFile.readAll(), &parseError);
        const QJsonObject baseline = baselineDocument.object();
        if (parseError.error != QJsonParseError::NoError
            || baseline.value(QStringLiteral("reportType")).toString()
                != suiteLabel
            || !baseline.value(QStringLiteral("aggregates")).isArray()) {
            std::fprintf(stderr, "Baseline is not a %s report\n", suiteLabel.toUtf8().constData());
            return 2;
        }
        suite.insert(QStringLiteral("baselinePath"), baselinePath);
        suite.insert(QStringLiteral("medianChangePercent"),
                     baselineChanges(aggregates, baseline.value(QStringLiteral("aggregates")).toArray()));
    }

    const QByteArray output = QJsonDocument(suite).toJson(QJsonDocument::Indented);
    const QString outputPath = optionValue(arguments, QStringLiteral("--output"));
    if (!outputPath.isEmpty()) {
        QFile outputFile(outputPath);
        if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Truncate)
            || outputFile.write(output) != output.size()) {
            std::fprintf(stderr, "Could not write report: %s\n", outputPath.toUtf8().constData());
            return 2;
        }
        std::fprintf(stderr, "[%s] report=%s\n", suiteLabel.toUtf8().constData(),
                     outputPath.toUtf8().constData());
    } else {
        std::fwrite(output.constData(), 1, output.size(), stdout);
    }
    return 0;
}

int NavigationBenchmark::runGui(QApplication &app, AppServices &services, QQuickWindow &window)
{
    Q_UNUSED(app)
    const QVariantMap startupState = services.settings()->workspaceState();
    const bool startupViewModesPassed = services.workspace()->leftPanel()->viewMode()
            == startupState.value(QStringLiteral("leftViewMode")).toInt()
        && services.workspace()->rightPanel()->viewMode()
            == startupState.value(QStringLiteral("rightViewMode")).toInt();
    if (app.arguments().contains(QStringLiteral("--map-startup"))) {
        auto *left = services.workspace()->leftPanel();
        auto *right = services.workspace()->rightPanel();
        QObject startupObserver;
        QList<int> leftModes{left->viewMode()}, rightModes{right->viewMode()};
        const auto leftConnection = QObject::connect(left, &FilePanelController::viewModeChanged,
            &startupObserver, [&] { leftModes.append(left->viewMode()); });
        const auto rightConnection = QObject::connect(right, &FilePanelController::viewModeChanged,
            &startupObserver, [&] { rightModes.append(right->viewMode()); });
        int frames = 0;
        const auto frameConnection = QObject::connect(&window, &QQuickWindow::frameSwapped,
            &startupObserver, [&] {
                if (++frames == 3)
                    QMetaObject::invokeMethod(&window, "startupShellReady", Qt::QueuedConnection);
            }, Qt::QueuedConnection);
        QElapsedTimer wait;
        wait.start();
        while (wait.elapsed() < kTimeoutMs) {
            window.update();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            if (frames >= 10 && window.property("workspaceStateRestored").toBool()) break;
            QThread::msleep(1);
        }
        QObject::disconnect(frameConnection);
        QObject::disconnect(leftConnection);
        QObject::disconnect(rightConnection);
        const auto *probe = window.findChild<MapGpuProbe *>();
        const bool gpu = probe && probe->ready() && probe->available();
        const auto validModes = [](const QList<int> &modes) {
            return std::all_of(modes.cbegin(), modes.cend(), [](int mode) { return mode == 3; });
        };
        const bool success = startupViewModesPassed && probe && probe->ready() && frames >= 10
            && window.property("workspaceStateRestored").toBool()
            && validModes(leftModes) && validModes(rightModes);
        QJsonObject report{
            {QStringLiteral("success"), success},
            {QStringLiteral("gpuAvailable"), gpu},
            {QStringLiteral("startupViewModesPassed"), startupViewModesPassed},
            {QStringLiteral("leftModes"), QJsonArray::fromVariantList(QVariant::fromValue(leftModes).toList())},
            {QStringLiteral("rightModes"), QJsonArray::fromVariantList(QVariant::fromValue(rightModes).toList())}
        };
        std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return success ? 0 : 1;
    }
    QTemporaryDir fixtureRoot(
        QDir(QDir::tempPath()).filePath(QStringLiteral("fm-navigation-gui-benchmark-XXXXXX")));
    QJsonObject report{
        {QStringLiteral("schemaVersion"), 1},
        {QStringLiteral("reportType"), QStringLiteral("navigation-gui-benchmark")}
    };
    QList<Dataset> datasets;
    QString error;
    if (!fixtureRoot.isValid() || !createFixtures(fixtureRoot.path(), &datasets, &error)) {
        report.insert(QStringLiteral("success"), false);
        report.insert(QStringLiteral("error"), error.isEmpty()
                          ? QStringLiteral("Could not create GUI benchmark fixtures") : error);
        std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return 2;
    }

    QElapsedTimer startupWait;
    startupWait.start();
    while (!window.property("workspaceStateRestored").toBool() && startupWait.elapsed() < 3000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }

    if (app.arguments().contains(QStringLiteral("--resize"))) {
#ifdef FM_ENABLE_RESIZE_BENCHMARK
        return PanelResizeBenchmark::run(services, window, datasets.at(1).path);
#else
        std::fputs("Resize benchmark requires BUILD_TESTING on Linux\n", stderr);
        return 2;
#endif
    }

    WorkspaceController *workspace = services.workspace();
    if (app.arguments().contains(QStringLiteral("--map-stress")))
        return FolderMapStress::run(services, window, fixtureRoot.path());
    workspace->setSplitEnabled(false);
    workspace->setActivePanel(0);
    FilePanelController *controller = workspace->leftPanel();
    if (app.arguments().contains(QStringLiteral("--sftp-create"))) {
        auto waitFor = [](const std::function<bool()> &condition, int timeout = 4000) {
            QElapsedTimer timer;
            timer.start();
            do {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                if (condition()) return true;
                QThread::msleep(1);
            } while (timer.elapsed() < timeout);
            return false;
        };
        window.requestActivate();
        if (!waitFor([&] { return window.isActive(); })) {
            std::fputs("SFTP keyboard GUI test requires an active window\n", stderr); return 2;
        }
        const auto fixture = services.pluginActions()->triggerAction(QStringLiteral("fm.sftp-provider::creationFixture"),
            {{QStringLiteral("parameters"), QVariantMap{{QStringLiteral("enabled"), true}, {QStringLiteral("mutationDelayMs"), 250}}}});
        const auto cleanup = qScopeGuard([&] {
            controller->openPath(fixtureRoot.path());
            waitFor([&] { return !controller->directoryModel()->loading(); });
            services.pluginActions()->triggerAction(QStringLiteral("fm.sftp-provider::creationFixture"), {});
        });
        if (!fixture.value(QStringLiteral("ok")).toBool()) return 2;
        const QString url = fixture.value(QStringLiteral("url")).toString();
        const QString directory = fixture.value(QStringLiteral("directory")).toString();
        if (!controller->openPath(url) || !waitFor([&] {
            return controller->currentPath() == url && !controller->navigationPending()
                && !controller->directoryModel()->loading();
        })) return 2;
        QQuickItem *panel = nullptr;
        for (auto *item : window.findChildren<QQuickItem *>()) {
            if (item->metaObject()->indexOfProperty("hoverSuppressed") >= 0
                && item->property("controller").value<QObject *>() == controller) panel = item;
        }
        auto *cancelButton = panel ? panel->findChild<QQuickItem *>(QStringLiteral("filePanelCancelLoading")) : nullptr;
        if (!cancelButton) return 2;
        int createdDuringCancel = 0;
        const auto creationConnection = QObject::connect(controller, &FilePanelController::entryCreated, &window,
                                                         [&] { ++createdDuringCancel; });
        const auto disconnectCreation = qScopeGuard([&] { QObject::disconnect(creationConnection); });
        controller->createFolder(QStringLiteral("Cancelled UI folder"));
        if (!waitFor([&] { return controller->remoteMutationPending() && cancelButton->isVisible() && cancelButton->width() > 0; })) return 1;
        const QPointF cancelPosition = cancelButton->mapToScene(QPointF(cancelButton->width() / 2, cancelButton->height() / 2));
        const QPointF cancelGlobal = window.mapToGlobal(cancelPosition.toPoint());
        QMouseEvent cancelPress(QEvent::MouseButtonPress, cancelPosition, cancelGlobal, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QMouseEvent cancelRelease(QEvent::MouseButtonRelease, cancelPosition, cancelGlobal, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &cancelPress);
        QCoreApplication::sendEvent(&window, &cancelRelease);
        if (!waitFor([&] { return !controller->remoteMutationPending() && !controller->directoryModel()->loading(); })
            || createdDuringCancel != 0 || panel->property("isRenaming").toBool() || !controller->lastError().isEmpty()) {
            std::fputs("SFTP Cancel button did not cancel the pending UI operation\n", stderr); return 1;
        }
        QObject::disconnect(creationConnection);
        QVariant menuResult;
        if (!panel || !QMetaObject::invokeMethod(panel, "ensureFilePanelEmptyMenu", Q_RETURN_ARG(QVariant, menuResult))) return 2;
        auto *menu = menuResult.value<QObject *>();
        if (!menu || !QMetaObject::invokeMethod(menu, "popupEmptyMenu")) return 2;
        auto *popup = menu->findChild<QObject *>(QStringLiteral("filePanelEmptyPopup"));
        if (!popup || !waitFor([&] { return popup->property("opened").toBool(); })) return 2;
        const QStringList labels{QStringLiteral("New Folder"), QStringLiteral("New File"), QStringLiteral("New Text File")};
        QList<QObject *> actions;
        for (const auto &label : labels) {
            QObject *action = nullptr;
            for (auto *object : menu->findChildren<QObject *>()) {
                if (object->metaObject()->indexOfSignal("triggered()") >= 0
                    && object->property("text").toString() == label) action = object;
            }
            if (!action || !action->property("visible").toBool() || !action->property("enabled").toBool()) {
                std::fprintf(stderr, "SFTP creation action unavailable: %s action=%p visible=%d enabled=%d path=%s create=%d manual=%d\n",
                    qPrintable(label), static_cast<void *>(action), action && action->property("visible").toBool(),
                    action && action->property("enabled").toBool(), qPrintable(controller->currentPath()),
                    controller->canCreateInCurrentPath(), controller->canCreateManualItem()); return 1;
            }
            actions.append(action);
        }
        QMetaObject::invokeMethod(popup, "close");
        waitFor([&] { return !popup->property("visible").toBool(); });
        for (const QString &location : {url, fixtureRoot.path()}) {
            if (!controller->openPath(location) || !waitFor([&] {
                return controller->currentPath() == location && !controller->navigationPending()
                    && !controller->directoryModel()->loading();
            })) return 2;
            const auto itemPath = [&](const QString &name) {
                if (location != url) return QDir(location).filePath(name);
                QUrl createdUrl(url);
                createdUrl.setPath(createdUrl.path() + QLatin1Char('/') + name);
                return createdUrl.toString(QUrl::FullyEncoded);
            };
            const QString localDirectory = location == url ? directory : location;
            for (int mode = 0; mode < 3; ++mode) {
                controller->setViewMode(mode);
                for (int type = 0; type < labels.size(); ++type) {
                    QString name = type == 2 ? QStringLiteral("New Text File") : labels.at(type);
                    if (mode > 0) name += QStringLiteral(" (%1)").arg(mode);
                    if (type == 2) name += QStringLiteral(".txt");
                    if (type == 0) QMetaObject::invokeMethod(&window, "createFolderInActivePanel");
                    else QMetaObject::invokeMethod(actions.at(type), "triggered");
                    const QString path = itemPath(name);
                    if (!waitFor([&] {
                        return controller->directoryModel()->indexOfPath(path) >= 0 && panel->property("isRenaming").toBool();
                    })) {
                        std::fprintf(stderr, "SFTP creation/inline rename failed: mode=%d name=%s error=%s\n",
                                     mode, qPrintable(name), qPrintable(controller->statusMessage())); return 1;
                    }
                    const QFileInfo info(QDir(localDirectory).filePath(name));
                    if (!info.exists() || (type == 0 ? !info.isDir() : !info.isFile() || info.size() != 0)) return 1;
                    if (type < 2 || (mode == 2 && type == 2)) {
                        if (!waitFor([&] {
                            return window.activeFocusItem() && window.activeFocusItem()->property("text").toString() == name;
                        })) { std::fputs("Created item's rename editor did not receive focus\n", stderr); return 1; }
                        const QString renamed = type < 2 ? name : QStringLiteral("Renamed file.txt");
                        window.activeFocusItem()->setProperty("text", renamed);
                        QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                        QKeyEvent release(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
                        QCoreApplication::sendEvent(&window, &press);
                        QCoreApplication::sendEvent(&window, &release);
                        if (!waitFor([&] {
                            return controller->directoryModel()->indexOfPath(itemPath(renamed)) >= 0
                                && !panel->property("isRenaming").toBool() && !controller->remoteMutationPending()
                                && !controller->directoryModel()->loading();
                        }) || !controller->lastError().isEmpty()
                            || (renamed != name && QFileInfo::exists(info.filePath()))
                            || !QFileInfo::exists(QDir(localDirectory).filePath(renamed))) {
                            std::fprintf(stderr, "Inline rename/default-name confirmation failed: name=%s error=%s\n",
                                qPrintable(name), qPrintable(controller->lastError().value(QStringLiteral("message")).toString())); return 1;
                        }
                    } else QMetaObject::invokeMethod(panel, "cancelInlineRename");
                    if (!waitFor([&] { return !panel->property("isRenaming").toBool(); })) return 1;
                }
                const QString original = mode == 0 ? QStringLiteral("New File") : QStringLiteral("New File (%1)").arg(mode);
                const int row = controller->directoryModel()->indexOfPath(itemPath(original));
                if (row < 0) return 1;
                controller->directoryModel()->selectOnly(row);
                QVariant viewResult;
                QMetaObject::invokeMethod(panel, "activeView", Q_RETURN_ARG(QVariant, viewResult));
                auto *view = viewResult.value<QObject *>();
                if (!view) return 2;
                view->setProperty("currentIndex", row);
                QMetaObject::invokeMethod(panel, "focusContent");
                waitFor([&] { return controller->canRenameSelection(); });
                QVariant contextResult;
                QMetaObject::invokeMethod(panel, "ensureFilePanelContextMenu", Q_RETURN_ARG(QVariant, contextResult));
                auto *contextMenu = contextResult.value<QObject *>();
                if (!contextMenu || !QMetaObject::invokeMethod(contextMenu, "popupContextMenu",
                    Q_ARG(QVariant, row), Q_ARG(QVariant, itemPath(original)),
                    Q_ARG(QVariant, false), Q_ARG(QVariant, false), Q_ARG(QVariant, false))) return 2;
                auto *contextPopup = contextMenu->findChild<QObject *>(QStringLiteral("filePanelItemPopup"));
                if (!contextPopup || !waitFor([&] { return contextPopup->property("opened").toBool(); })) return 2;
                for (const auto &label : {QStringLiteral("Rename"), QStringLiteral("Cut to Clipboard")}) {
                    QObject *action = nullptr;
                    if (contextMenu) for (auto *object : contextMenu->findChildren<QObject *>()) {
                        if (object->property("text").toString() == label
                            && object->metaObject()->indexOfSignal("triggered()") >= 0) action = object;
                    }
                    if (!action || !action->property("visible").toBool() || !action->property("enabled").toBool()) {
                        std::fprintf(stderr, "SFTP item menu action unavailable: %s visible=%d enabled=%d nativeRename=%d nativeCut=%d selected=%d pending=%d\n",
                            qPrintable(label), action && action->property("visible").toBool(), action && action->property("enabled").toBool(),
                            controller->canRenameSelection(), controller->canCutSelection(), controller->directoryModel()->selectedCount(), controller->remoteMutationPending()); return 1;
                    }
                }
                QMetaObject::invokeMethod(contextPopup, "close");
                if (!waitFor([&] { return !contextPopup->property("visible").toBool(); })) return 1;
                view->setProperty("currentIndex", controller->directoryModel()->indexOfPath(itemPath(original)));
                QMetaObject::invokeMethod(view, "forceLayout");
                if (!waitFor([&] {
                    auto *item = view->property("currentItem").value<QObject *>();
                    return item && item->property("path").toString() == itemPath(original);
                })) { std::fputs("Selected rename delegate did not settle\n", stderr); return 1; }
                QMetaObject::invokeMethod(panel, "focusContent");
                QKeyEvent renamePress(QEvent::KeyPress, Qt::Key_F2, Qt::NoModifier);
                QKeyEvent renameRelease(QEvent::KeyRelease, Qt::Key_F2, Qt::NoModifier);
                QCoreApplication::sendEvent(&window, &renamePress);
                QCoreApplication::sendEvent(&window, &renameRelease);
                if (!waitFor([&] { return panel->property("isRenaming").toBool()
                    && window.activeFocusItem() && window.activeFocusItem()->property("text").toString() == original; })) {
                    std::fprintf(stderr, "F2 rename unavailable: mode=%d path=%s renaming=%d currentIndex=%d focusText=%s pending=%d\n",
                        mode, qPrintable(location), panel->property("isRenaming").toBool(), view->property("currentIndex").toInt(),
                        qPrintable(window.activeFocusItem() ? window.activeFocusItem()->property("text").toString() : QString()),
                        controller->remoteMutationPending()); return 1;
                }
                const QString renamed = QStringLiteral("F2 renamed %1.txt").arg(mode);
                window.activeFocusItem()->setProperty("text", renamed);
                QKeyEvent enterPress(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QKeyEvent enterRelease(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
                QCoreApplication::sendEvent(&window, &enterPress);
                QCoreApplication::sendEvent(&window, &enterRelease);
                if (!waitFor([&] { return !panel->property("isRenaming").toBool()
                    && !controller->remoteMutationPending() && !controller->directoryModel()->loading()
                    && controller->directoryModel()->indexOfPath(itemPath(renamed)) >= 0; })
                    || !controller->lastError().isEmpty()) return 1;
                if (!controller->renamePath(itemPath(renamed), original) || !waitFor([&] {
                    return !controller->remoteMutationPending() && !controller->directoryModel()->loading()
                        && controller->directoryModel()->indexOfPath(itemPath(original)) >= 0;
                })) return 1;
                controller->directoryModel()->selectOnly(controller->directoryModel()->indexOfPath(itemPath(original)));
                QMetaObject::invokeMethod(panel, "focusContent");
                QKeyEvent cutPress(QEvent::KeyPress, Qt::Key_X, Qt::ControlModifier);
                QKeyEvent cutRelease(QEvent::KeyRelease, Qt::Key_X, Qt::ControlModifier);
                QCoreApplication::sendEvent(&window, &cutPress);
                QCoreApplication::sendEvent(&window, &cutRelease);
                if (!waitFor([&] { return workspace->clipboardCut()
                    && workspace->clipboardPaths() == QStringList{itemPath(original)}; })) {
                    std::fprintf(stderr, "Ctrl+X unavailable: mode=%d path=%s\n", mode, qPrintable(location)); return 1;
                }
                workspace->copyTextToClipboard(QStringLiteral("sftp-ui-fixture"));
            }
        }
        std::puts("SFTP/local creation: menu actions, unique names, default Enter, F2 and Ctrl+X passed in Details/Grid/Brief");
        return 0;
    }
    if (app.arguments().contains(QStringLiteral("--places-ui"))) {
        auto waitFor = [](const std::function<bool()> &condition, int timeout = 3000) {
            QElapsedTimer timer;
            timer.start();
            do {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                if (condition()) return true;
                QThread::msleep(1);
            } while (timer.elapsed() < timeout);
            return false;
        };
        auto pause = [&](int ms) { waitFor([] { return false; }, ms); };
        QQuickItem *sidebar = nullptr;
        for (auto *item : window.findChildren<QQuickItem *>()) {
            if (item->metaObject()->indexOfProperty("selectedPlacePath") >= 0
                && item->metaObject()->indexOfProperty("placesList") >= 0) sidebar = item;
        }
        auto *list = sidebar ? qobject_cast<QQuickItem *>(sidebar->property("placesList").value<QObject *>()) : nullptr;
        if (!list) { std::fputs("Places list unavailable\n", stderr); return 2; }
        sidebar->setProperty("hiddenPanelIds", QStringList{QStringLiteral("recent"), QStringLiteral("folders")});

        QSettings settings;
        const QString group = QStringLiteral("sftp/connections/places-") + QFileInfo(fixtureRoot.path()).fileName();
        const auto cleanup = qScopeGuard([&] {
            services.pluginActions()->triggerAction(QStringLiteral("fm.sftp-provider::placesFixture"), {});
            for (int i = 0; i < 12; ++i) settings.remove(group + QString::number(i));
            settings.sync();
        });
        for (int i = 0; i < 12; ++i) {
            const QString entry = group + QString::number(i) + QLatin1Char('/');
            settings.setValue(entry + QStringLiteral("url"), QStringLiteral("sftp://sidebar-test-%1@127.0.0.1/").arg(i));
            settings.setValue(entry + QStringLiteral("name"), QStringLiteral("Sidebar server %1").arg(i));
            settings.setValue(entry + QStringLiteral("rememberPassword"), false);
        }
        settings.sync();
        auto *model = workspace->placesModel();
        auto findPath = [&](const QString &path) {
            for (int row = 0; row < model->rowCount(); ++row)
                if (model->data(model->index(row), PlacesModel::PathRole).toString() == path) return row;
            return -1;
        };
        services.pluginActions()->triggerAction(QStringLiteral("fm.sftp-provider::refreshPlaces"), {});
        pause(300);
        const bool disconnectedHidden = findPath(QStringLiteral("sftp://")) < 0
            && findPath(QStringLiteral("sftp://sidebar-test-11@127.0.0.1/")) < 0;
        services.pluginActions()->triggerAction(QStringLiteral("fm.sftp-provider::placesFixture"),
            {{QStringLiteral("parameters"), QVariantMap{{QStringLiteral("enabled"), true}}}});
        if (!waitFor([&] { return findPath(QStringLiteral("sftp://sidebar-test-11@127.0.0.1/")) >= 0; })) {
            std::fputs("SFTP Places fixtures unavailable\n", stderr); return 2;
        }
        pause(300);
        int cloudHeaders = 0;
        for (int row = 0; row < model->rowCount(); ++row) {
            const auto index = model->index(row);
            if (model->data(index, PlacesModel::VisualSectionRole).toString() == QLatin1String("cloud")
                && model->data(index, PlacesModel::ShowSectionHeaderRole).toBool()) ++cloudHeaders;
        }
        int periodicResets = 0;
        const auto connection = QObject::connect(model, &QAbstractItemModel::modelReset, [&] { ++periodicResets; });
        const auto disconnect = qScopeGuard([&] { QObject::disconnect(connection); });
        QMetaObject::invokeMethod(list, "positionViewAtEnd");
        pause(250);
        const qreal before = list->property("contentY").toReal() - list->property("originY").toReal();
        pause(5500);
        const qreal afterPeriodic = list->property("contentY").toReal() - list->property("originY").toReal();
        const bool idleStable = periodicResets == 0 && std::abs(before - afterPeriodic) < 1.0;
        const int resetsWhileIdle = periodicResets;

        QMetaObject::invokeMethod(list, "positionViewAtEnd");
        pause(250);
        const qreal beforeRefresh = list->property("contentY").toReal() - list->property("originY").toReal();
        model->refresh();
        pause(300);
        const qreal afterRefresh = list->property("contentY").toReal() - list->property("originY").toReal();
        const bool refreshStable = std::abs(beforeRefresh - afterRefresh) < 1.0;

        QMetaObject::invokeMethod(list, "positionViewAtBeginning");
        pause(100);
        const qreal atBeginning = list->property("contentY").toReal() - list->property("originY").toReal();
        model->refresh();
        pause(300);
        const qreal afterBeginningRefresh = list->property("contentY").toReal() - list->property("originY").toReal();
        const bool beginningStable = std::abs(atBeginning - afterBeginningRefresh) < 1.0;

        const qreal beforeDisconnect = beforeRefresh / 2;
        list->setProperty("contentY", list->property("originY").toReal() + beforeDisconnect);
        pause(100);
        const QString disconnectedUrl = QStringLiteral("sftp://sidebar-test-0@127.0.0.1/");
        services.pluginActions()->triggerAction(QStringLiteral("fm.sftp-provider::disconnect"),
            {{QStringLiteral("currentPath"), disconnectedUrl}});
        const bool disconnectedRemoved = waitFor([&] { return findPath(disconnectedUrl) < 0; });
        pause(300);
        const qreal afterDisconnect = list->property("contentY").toReal() - list->property("originY").toReal();
        const bool disconnectStable = disconnectedRemoved && std::abs(beforeDisconnect - afterDisconnect) < 1.0;
        const bool success = cloudHeaders == 1 && before > 50 && idleStable && refreshStable
            && beginningStable && disconnectedHidden && disconnectStable;
        std::printf("Places sidebar: cloudHeaders=%d periodicResets=%d idleScroll=%.1f/%.1f refreshScroll=%.1f/%.1f topScroll=%.1f/%.1f disconnectedHidden=%d disconnectScroll=%.1f/%.1f success=%d\n",
                    cloudHeaders, resetsWhileIdle, before, afterPeriodic, beforeRefresh, afterRefresh,
                    atBeginning, afterBeginningRefresh, disconnectedHidden, beforeDisconnect, afterDisconnect, success);
        return success ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--sftp-ui"))) {
        QQuickItem *panel = nullptr;
        for (auto *item : window.findChildren<QQuickItem *>()) {
            if (item->metaObject()->indexOfProperty("hoverSuppressed") >= 0
                && item->property("controller").value<QObject *>() == controller) panel = item;
        }
        if (!panel) return 2;
        QVariant opened;
        const QString url = QStringLiteral("sftp://faye@192.168.0.105/home/faye");
        if (!QMetaObject::invokeMethod(panel, "openPath", Q_RETURN_ARG(QVariant, opened),
                Q_ARG(QVariant, url), Q_ARG(QVariant, false)) || !opened.toBool()) {
            std::fputs("SFTP URL did not open its connection form\n", stderr); return 1;
        }
        QQuickItem *card = nullptr;
        QElapsedTimer wait;
        wait.start();
        while (wait.elapsed() < 3000 && !card) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            for (auto *item : window.findChildren<QQuickItem *>()) {
                if (item->isVisible() && QString::fromLatin1(item->metaObject()->className()).startsWith(QStringLiteral("SftpSettingsCard"))) card = item;
            }
            QThread::msleep(1);
        }
        if (!card || card->width() <= 0 || card->height() <= 0) {
            std::fprintf(stderr, "SFTP card unavailable: item=%p width=%.0f height=%.0f\n",
                static_cast<void *>(card), card ? card->width() : 0, card ? card->height() : 0);
            QQmlComponent diagnostic(qmlEngine(&window), QUrl(QStringLiteral("qrc:/sftp/settings/SftpSettingsCard.qml")));
            std::fprintf(stderr, "%s\n", qPrintable(diagnostic.errorString()));
            const int capture = app.arguments().indexOf(QStringLiteral("--sftp-ui-capture"));
            if (capture >= 0) window.grabWindow().save(app.arguments().value(capture + 1));
            return 1;
        }
        QElapsedTimer settle;
        settle.start();
        while (settle.elapsed() < 250) { QCoreApplication::processEvents(QEventLoop::AllEvents, 10); QThread::msleep(1); }
        QObject *auth = nullptr;
        for (auto *object : card->findChildren<QObject *>()) {
            if (object->inherits("SftpConnectionController")) auth = object;
        }
        if (!auth || !QMetaObject::invokeMethod(auth, "signIn", Q_ARG(QString, QString()), Q_ARG(int, 22),
                Q_ARG(QString, QStringLiteral("tester")), Q_ARG(QString, QStringLiteral("test")), Q_ARG(QString, QStringLiteral("/")))) return 1;
        auto *errorLabel = card->findChild<QQuickItem *>(QStringLiteral("sftpStatusMessage"));
        if (!auth->property("hasError").toBool() || !errorLabel || !errorLabel->isVisible()
            || errorLabel->property("text").toString() != auth->property("status").toString()) return 1;
        bool footerReportsError = false;
        QObject *sftpDialog = nullptr;
        for (auto *object : window.findChildren<QObject *>()) {
            if (QString::fromLatin1(object->metaObject()->className()).startsWith(QStringLiteral("PluginUiDialog"))) {
                sftpDialog = object;
                footerReportsError = object->property("statusIsError").toBool()
                    && object->property("statusText").toString() == auth->property("status").toString();
            }
        }
        if (!footerReportsError) { std::fputs("SFTP error was not shown in the dialog footer\n", stderr); return 1; }
        const auto fixture = services.pluginActions()->triggerAction(QStringLiteral("fm.sftp-provider::creationFixture"),
            {{QStringLiteral("parameters"), QVariantMap{{QStringLiteral("enabled"), true}}}});
        if (!fixture.value(QStringLiteral("ok")).toBool()) return 2;
        const QString fixtureUrl = fixture.value(QStringLiteral("url")).toString();
        QUrl identity(fixtureUrl);
        identity.setPath(QStringLiteral("/"));
        const QString profileGroup = QStringLiteral("sftp/connections/")
            + QString::fromLatin1(QCryptographicHash::hash(identity.toString(QUrl::FullyEncoded).toUtf8(), QCryptographicHash::Sha256).toHex());
        QSettings profileSettings;
        profileSettings.setValue(profileGroup + QStringLiteral("/url"), fixtureUrl);
        profileSettings.setValue(profileGroup + QStringLiteral("/name"), QStringLiteral("Test laptop"));
        profileSettings.setValue(profileGroup + QStringLiteral("/rememberPassword"), false);
        profileSettings.sync();
        const auto cleanupUiFixture = qScopeGuard([&] {
            profileSettings.remove(profileGroup);
            profileSettings.sync();
            services.pluginActions()->triggerAction(QStringLiteral("fm.sftp-provider::creationFixture"), {});
        });
        auto settleUi = [&] {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 120) { QCoreApplication::processEvents(QEventLoop::AllEvents, 10); QThread::msleep(1); }
        };
        auto clickItem = [&](QQuickItem *item) {
            if (!item || !item->isVisible() || !item->isEnabled()) return false;
            const QPointF position = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
            const QPointF global = window.mapToGlobal(position.toPoint());
            QMouseEvent press(QEvent::MouseButtonPress, position, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, position, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &press);
            QCoreApplication::sendEvent(&window, &release);
            settleUi();
            return true;
        };
        const std::function<QQuickItem *(QQuickItem *, const QString &)> findVisualItem
            = [&](QQuickItem *parent, const QString &name) -> QQuickItem * {
                if (parent->objectName() == name) return parent;
                for (auto *child : parent->childItems()) {
                    if (auto *found = findVisualItem(child, name)) return found;
                }
                return nullptr;
            };
        QMetaObject::invokeMethod(auth, "refresh");
        QMetaObject::invokeMethod(card, "editConnection", Q_ARG(QVariant, fixtureUrl));
        settleUi();
        auto *host = card->findChild<QQuickItem *>(QStringLiteral("sftpHostField"));
        auto *login = card->findChild<QQuickItem *>(QStringLiteral("sftpLoginField"));
        auto *name = card->findChild<QQuickItem *>(QStringLiteral("sftpNameField"));
        auto *connectionsSection = card->findChild<QQuickItem *>(QStringLiteral("sftpConnectionsSection"));
        auto *editorSection = card->findChild<QQuickItem *>(QStringLiteral("sftpEditorSection"));
        if (!host || !login || !name || !connectionsSection || !editorSection
            || host->property("text").toString() != QStringLiteral("127.0.0.1")
            || login->property("text").toString() != QStringLiteral("tester")
            || name->property("text").toString() != QStringLiteral("Test laptop")) return 1;
        const qreal normalWidth = card->width();
        for (const qreal width : {normalWidth, qreal(440)}) {
            card->setWidth(width);
            settleUi();
            const bool separate = width >= 680
                ? connectionsSection->x() + connectionsSection->width() <= editorSection->x()
                : connectionsSection->y() + connectionsSection->height() <= editorSection->y();
            if (!separate || editorSection->x() + editorSection->width() > width + 1) {
                std::fputs("SFTP connections and editor overlap or overflow\n", stderr); return 1;
            }
            for (const auto &field : {QStringLiteral("sftpHostField"), QStringLiteral("sftpPortField"),
                    QStringLiteral("sftpLoginField"), QStringLiteral("sftpPasswordField"),
                    QStringLiteral("sftpFolderField"), QStringLiteral("sftpNameField")}) {
                auto *item = card->findChild<QQuickItem *>(field);
                const QPointF position = item ? item->mapToItem(editorSection, QPointF()) : QPointF();
                if (!item || item->width() < 40 || position.x() < 0
                    || position.x() + item->width() > editorSection->width() + 1
                    || item->property("font").value<QFont>().family() != host->property("font").value<QFont>().family()) return 1;
            }
        }
        card->setWidth(normalWidth);
        settleUi();
        auto *pluginItem = sftpDialog->property("pluginItem").value<QObject *>();
        if (!pluginItem) return 1;
        const qreal unusedHeight = pluginItem->property("height").toReal() - card->height();
        if (unusedHeight > 33 || sftpDialog->property("height").toReal() > sftpDialog->property("maximumHeight").toReal() + 1) {
            std::fprintf(stderr, "SFTP dialog leaves unused height or exceeds the viewport: gap=%.0f\n", unusedHeight); return 1;
        }
        auto *storage = panel->findChild<QQuickItem *>(QStringLiteral("panelStorageView"));
        QVariant iconSource;
        if (!storage || !QMetaObject::invokeMethod(storage, "folderIconSource", Q_RETURN_ARG(QVariant, iconSource),
                Q_ARG(QVariant, QStringLiteral("sftp"))) || iconSource.toString() != QStringLiteral("qrc:/sftp/settings/sftp.svg")
                || !QFile::exists(QStringLiteral(":/sftp/settings/sftp.svg"))) {
            std::fputs("Cloud SFTP icon does not resolve to the plugin resource\n", stderr); return 1;
        }
        QQmlComponent tileComponent(qmlEngine(&window), QUrl(QStringLiteral("qrc:/qt/qml/FM/qml/components/common/IconTile.qml")));
        std::unique_ptr<QObject> tile(tileComponent.create(qmlContext(&window)));
        if (!tile) return 1;
        tile->setProperty("asynchronous", false);
        tile->setProperty("source", iconSource);
        settleUi();
        QQuickItem *svgImage = nullptr;
        for (auto *item : tile->findChildren<QQuickItem *>()) {
            if (item->metaObject()->indexOfProperty("status") >= 0 && item->metaObject()->indexOfProperty("sourcePath") >= 0) svgImage = item;
        }
        if (!svgImage || svgImage->property("status").toInt() != 1) {
            std::fputs("SFTP IconTile failed to render through the SVG recolor provider\n", stderr); return 1;
        }
        tile->setProperty("useOriginalColor", true);
        settleUi();
        if (svgImage->property("status").toInt() != 1) return 1;
        const int capture = app.arguments().indexOf(QStringLiteral("--sftp-ui-capture"));
        if (capture >= 0 && !window.grabWindow().save(app.arguments().value(capture + 1))) return 1;
        if (!clickItem(card->findChild<QQuickItem *>(QStringLiteral("sftpNewConnection")))
            || !host->property("text").toString().isEmpty() || !login->property("text").toString().isEmpty()) {
            std::fputs("SFTP New connection did not clear the editor\n", stderr); return 1;
        }
        if (!clickItem(findVisualItem(card, QStringLiteral("sftpConnectionRow")))
            || host->property("text").toString() != QStringLiteral("127.0.0.1")) {
            auto *row = findVisualItem(card, QStringLiteral("sftpConnectionRow"));
            const QPointF position = row ? row->mapToScene(QPointF()) : QPointF();
            std::fprintf(stderr, "SFTP row did not populate editor: row=%p pos=%.0f,%.0f size=%.0fx%.0f host=%s selected=%s busy=%d\n",
                static_cast<void *>(row), position.x(), position.y(), row ? row->width() : 0, row ? row->height() : 0,
                qPrintable(host->property("text").toString()), qPrintable(card->property("selectedUrl").toString()), auth->property("busy").toBool());
            if (capture >= 0) window.grabWindow().save(app.arguments().value(capture + 1) + QStringLiteral("-failed.png"));
            return 1;
        }
        auto *forget = card->findChild<QQuickItem *>(QStringLiteral("sftpForgetConnection"));
        if (!forget || !forget->isVisible() || !forget->isEnabled()
            || !clickItem(card->findChild<QQuickItem *>(QStringLiteral("sftpDisconnectConnection")))) {
            std::fputs("SFTP saved connection actions unavailable\n", stderr); return 1;
        }
        auto *open = card->findChild<QQuickItem *>(QStringLiteral("sftpOpenConnection"));
        bool savedDisconnected = false;
        for (const auto &item : auth->property("connections").toList()) {
            const auto profile = item.toMap();
            if (profile.value(QStringLiteral("url")).toString() == fixtureUrl)
                savedDisconnected = profile.value(QStringLiteral("saved")).toBool() && !profile.value(QStringLiteral("connected")).toBool();
        }
        if (!open || open->property("text").toString() != QStringLiteral("Connect") || !savedDisconnected) {
            std::fputs("SFTP Disconnect did not preserve the saved profile\n", stderr); return 1;
        }
        QQuickItem *password = nullptr;
        for (auto *item : card->findChildren<QQuickItem *>()) {
            if (item->metaObject()->indexOfProperty("echoMode") >= 0
                && item->property("echoMode").toInt() == 2) password = item;
        }
        if (!password) { std::fputs("SFTP password input unavailable\n", stderr); return 1; }
        password->setProperty("text", QStringLiteral("ui-test-secret"));
        for (auto *object : window.findChildren<QObject *>()) {
            if (QString::fromLatin1(object->metaObject()->className()).startsWith(QStringLiteral("PluginUiDialog")))
                QMetaObject::invokeMethod(object, "close");
        }
        settle.restart();
        while (settle.elapsed() < 250) { QCoreApplication::processEvents(QEventLoop::AllEvents, 10); QThread::msleep(1); }
        if (!password->property("text").toString().isEmpty()) { std::fputs("SFTP password input retained its value after close\n", stderr); return 1; }
        if (!QMetaObject::invokeMethod(&window, "openSettingsDialog", Q_ARG(QVariant, QStringLiteral("files")))) return 1;
        QQuickItem *settingsCard = nullptr;
        settle.restart();
        while (settle.elapsed() < 4000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            for (auto *object : window.findChildren<QObject *>()) {
                if (!QString::fromLatin1(object->metaObject()->className()).startsWith(QStringLiteral("SettingsDialog"))) continue;
                auto *settingsContent = object->property("contentItem").value<QQuickItem *>();
                if (settingsContent) settingsCard = findVisualItem(settingsContent, QStringLiteral("sftpSettingsCard"));
            }
            if (settingsCard && settingsCard->width() >= 300 && settingsCard->height() > 0) break;
            QThread::msleep(1);
        }
        if (!settingsCard || settingsCard->width() < 300 || settingsCard->height() <= 0
            || !settingsCard->findChild<QQuickItem *>(QStringLiteral("sftpConnectionsSection"))
            || !settingsCard->findChild<QQuickItem *>(QStringLiteral("sftpEditorSection"))) {
            std::fputs("Redesigned SFTP form unavailable in Files settings\n", stderr); return 1;
        }
        const auto originalMode = services.theme()->mode();
        const auto originalScheme = services.theme()->scheme();
        const auto restoreTheme = qScopeGuard([&] {
            services.theme()->setScheme(originalScheme);
            services.theme()->setMode(originalMode);
        });
        auto *settingsRoot = settingsCard->property("dialogRoot").value<QObject *>();
        for (const auto scheme : {ThemeController::CatppuccinLatte, ThemeController::GraphiteSage}) {
            services.theme()->setScheme(scheme);
            for (const auto mode : {ThemeController::Light, ThemeController::Dark}) {
                services.theme()->setMode(mode);
                settleUi();
                for (auto *testedCard : {card, settingsCard}) {
                    for (const auto &name : {QStringLiteral("sftpConnectionsSection"), QStringLiteral("sftpEditorSection")}) {
                        auto *section = testedCard->findChild<QQuickItem *>(name);
                        if (!section || section->property("color").value<QColor>() != settingsRoot->property("sectionFill").value<QColor>()
                            || QQmlProperty(section, QStringLiteral("border.color")).read().value<QColor>()
                                != settingsRoot->property("sectionBorder").value<QColor>()) {
                            std::fputs("SFTP section palette differs from Settings theme surfaces\n", stderr); return 1;
                        }
                    }
                }
            }
        }
        auto *settingsPassword = settingsCard->findChild<QQuickItem *>(QStringLiteral("sftpPasswordField"));
        if (!settingsPassword) return 1;
        settingsPassword->setProperty("text", QStringLiteral("settings-test-secret"));
        QMetaObject::invokeMethod(settingsCard->property("dialogRoot").value<QObject *>(), "close");
        settle.restart();
        while (settle.elapsed() < 1000 && !settingsPassword->property("text").toString().isEmpty()) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10); QThread::msleep(1);
        }
        if (!settingsPassword->property("text").toString().isEmpty()) {
            std::fputs("SFTP password was retained after Settings closed\n", stderr); return 1;
        }
        std::printf("SFTP plugin UI loaded with real application controls: %.0fx%.0f\n", card->width(), card->height());
        return 0;
    }
    if (app.arguments().contains(QStringLiteral("--hover-previews"))) {
        workspace->setSplitEnabled(true);
        auto waitFor = [](const std::function<bool()> &condition, int timeout = 3000) {
            QElapsedTimer timer;
            timer.start();
            do {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                if (condition()) return true;
                QThread::msleep(1);
            } while (timer.elapsed() < timeout);
            return false;
        };
        auto pause = [&](int ms) { waitFor([] { return false; }, ms); };
        auto find = [](auto &&self, QQuickItem *node, const std::function<bool(QQuickItem *)> &match) -> QQuickItem * {
            if (match(node)) return node;
            for (auto *child : node->childItems()) if (auto *found = self(self, child, match)) return found;
            return nullptr;
        };
        auto *panel = find(find, window.contentItem(), [controller](QQuickItem *item) {
            return item->metaObject()->indexOfProperty("showMediaHoverPreviews") >= 0
                && item->property("controller").value<QObject *>() == controller;
        });
        if (!panel) return 2;
        panel->setProperty("showMediaHoverPreviews", true);
        panel->setProperty("showFolderHoverPreviews", true);
        const QString path = QDir(fixtureRoot.path()).filePath(QStringLiteral("hover"));
        QDir().mkpath(path + QStringLiteral("/Folder"));
        writeFile(path + QStringLiteral("/Folder/entry.txt"), "preview\n");
        const QString photo = path + QStringLiteral("/elias-maurer-JW58XJDQYrE-unsplash.png");
        QImage image(320, 240, QImage::Format_RGB32);
        image.fill(Qt::blue);
        if (!image.save(photo) || !controller->openPath(path)) return 2;
        bool success = true;
        for (const QString &target : {photo, path + QStringLiteral("/Folder")}) {
            controller->setViewMode(0);
            const bool settled = waitFor([&] {
                return !controller->directoryModel()->loading() && !panel->property("hoverSuppressed").toBool();
            });
            pause(250);
            auto *row = find(find, panel, [&](QQuickItem *item) {
                return item->isVisible() && item->property("path").toString() == target
                    && QString::fromLatin1(item->metaObject()->className()).startsWith(QStringLiteral("FileTableDelegate"));
            });
            auto *hit = row ? find(find, row, [](QQuickItem *item) {
                return item->metaObject()->indexOfProperty("nameRight") >= 0;
            }) : nullptr;
            if (!settled || !hit) {
                std::fprintf(stderr, "Hover fixture incomplete: settled=%d row=%p hit=%p\n", settled, static_cast<void *>(row), static_cast<void *>(hit));
                return 2;
            }
            auto *icon = find(find, row, [](QQuickItem *item) {
                return QString::fromLatin1(item->metaObject()->className()).startsWith(QStringLiteral("FileIconCell"));
            });
            auto *label = find(find, row, [&](QQuickItem *item) {
                return item->property("text").toString() == QFileInfo(target).completeBaseName();
            });
            if (!icon || !label) return 2;
            bool geometry = true;
            for (bool badges : {false, true}) {
                panel->setProperty("showSelectionBadges", badges);
                pause(80);
                // Compare with rendered content, never use the hit area as the input oracle.
                const auto iconPoint = hit->mapFromItem(icon, icon->width() / 2, icon->height() / 2);
                const auto textPoint = hit->mapFromItem(label, std::min(15.0, label->width() / 2), label->height() / 2);
                auto *lastLabel = qobject_cast<QQuickItem *>(hit->property("lastNameLabel").value<QObject *>());
                const double left = icon->mapToItem(hit->parentItem(), 0, 0).x();
                const double right = lastLabel ? lastLabel->mapToItem(hit->parentItem(), lastLabel->width(), 0).x() : -1;
                const bool bounds = std::abs(hit->x() - left) < .5 && std::abs(hit->x() + hit->width() - right) < .5;
                std::printf("Details badges=%d hit=[%.1f,%.1f] content=[%.1f,%.1f]\n", badges, hit->x(), hit->x() + hit->width(), left, right);
                geometry = bounds && hit->contains(iconPoint) && hit->contains(textPoint) && geometry;
            }
            std::printf("Details icon/name geometry tracks layout=%d\n", geometry);
            success = geometry && success;
            auto move = [&](const QPointF &point) {
                QMouseEvent event(QEvent::MouseMove, point, window.mapToGlobal(point.toPoint()),
                                  Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(&window, &event);
            };
            move(QPointF(5, 5));
            pause(200);
            move(row->mapToScene(QPointF(row->width() - 20, row->height() / 2)));
            pause(50);
            move(label->mapToScene(QPointF(std::min(15.0, label->width() / 2), label->height() / 2)));
            const bool hovered = waitFor([&] { return controller->hoveredPath() == target; }, 800);
            const bool visible = waitFor([&] {
                QVariant result;
                QMetaObject::invokeMethod(panel, "hoverPreviewVisibleForMenuTransition", Q_RETURN_ARG(QVariant, result));
                return result.toBool();
            });
            std::printf("Details hover: %s hit=%.1fx%.1f hovered=%d visible=%d\n", qPrintable(target), hit->width(), hit->height(), hovered, visible);
            success = hovered && visible && success;
            // Leave the pointer inside the card, where delayed hover clearing is retained.
            auto *card = find(find, panel, [](QQuickItem *item) {
                return item->isVisible() && item->metaObject()->indexOfProperty("pointerInside") >= 0
                    && item->metaObject()->indexOfProperty("anchorRect") >= 0;
            });
            if (card) move(card->mapToScene(QPointF(card->width() / 2, card->height() / 2)));
            pause(100);
            for (int mode : {1, 2, 3, 0}) {
                // A retained old target must be cleared even without a delegate exit.
                controller->setHoveredPath(target);
                controller->setViewMode(mode);
                const bool cleared = controller->hoveredPath().isEmpty() && (!card || !card->isVisible());
                std::printf("view change to %d cleared and hidden=%d\n", mode, cleared);
                success = cleared && success;
                pause(100);
            }
        }
        report.insert(QStringLiteral("success"), success);
        std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return success ? 0 : 1;
    }
    if (app.arguments().contains(QStringLiteral("--map"))) {
        const QString path = QDir(fixtureRoot.path()).filePath(QStringLiteral("map"));
        QDir fixture(path);
        for (int i = 0; i < 24; ++i) {
            for (int j = 0; j < 24; ++j) fixture.mkpath(QStringLiteral("Folder %1/Child %2").arg(i).arg(j));
        }
        for (const QString &name : {QStringLiteral("Photo.jpg"), QStringLiteral("Document.pdf"),
                QStringLiteral("Archive.zip"), QStringLiteral("Music.mp3"), QStringLiteral("Notes.txt"),
                QStringLiteral("Source.cpp")}) {
            if (!writeFile(fixture.filePath(QStringLiteral("Folder 0/") + name), QByteArray("fixture\n"))) return 2;
        }
        QImage mapPhoto(640, 360, QImage::Format_RGB32);
        mapPhoto.fill(QColor(QStringLiteral("#358ac1")));
        if (!mapPhoto.save(fixture.filePath(QStringLiteral("Folder 0/Photo.jpg")))) return 2;
        {
            QPdfWriter pdf(fixture.filePath(QStringLiteral("Folder 0/Document.pdf")));
            QPainter painter(&pdf);
            painter.drawText(QPointF(100, 100), QStringLiteral("Map PDF preview"));
        }
        MapGpuProbe *probe = window.findChild<MapGpuProbe *>();
        QElapsedTimer wait;
        wait.start();
        while (probe && !probe->ready() && wait.elapsed() < kTimeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            QThread::msleep(1);
        }
        controller->openPath(path);
        controller->setViewMode(3);
        FolderMapItem *map = nullptr;
        wait.restart();
        while (wait.elapsed() < kTimeoutMs) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
            for (auto *candidate : window.findChildren<FolderMapItem *>()) {
                if (candidate->parentItem()->isVisible() && candidate->currentPath() == path) map = candidate;
            }
            if (!controller->directoryModel()->loading()
                && ((!probe || !probe->available()) || (map && map->loadedCount() >= 25))) break;
            QThread::msleep(1);
        }
        const bool gpu = probe && probe->ready() && probe->available();
        bool success = startupViewModesPassed && probe && probe->ready() && (gpu ? controller->viewMode() == 3 && map
                                                        && map->loadedCount() >= 25
                                                    : controller->viewMode() == 3 && map && !map->gpuEnabled()
                                                        && map->parentItem()->findChild<QQuickItem *>(QStringLiteral("mapUnavailablePlaceholder"))->isVisible());
        QObject *viewControls = window.findChild<QObject *>(QStringLiteral("viewControls"));
        QObject *cycleButton = window.findChild<QObject *>(QStringLiteral("viewCycleButton"));
        bool availabilityPassed = viewControls && cycleButton;
        if (availabilityPassed) {
            QQmlComponent component(qmlEngine(viewControls));
            component.setData("import QtQml\nQtObject { property string currentPath; property int viewMode: 2 }", QUrl());
            std::unique_ptr<QObject> mock(component.create());
            availabilityPassed = bool(mock);
            if (mock) {
                const QVariant original = viewControls->property("controller");
                viewControls->setProperty("controller", QVariant::fromValue(mock.get()));
                for (const QString &location : {path, QStringLiteral("gdrive://folder"), QStringLiteral("ftp://host"),
                        QStringLiteral("portable://device"), QStringLiteral("archive:///tmp/file.zip|/"),
                        QStringLiteral("favorites://"), QStringLiteral("devices://"), QStringLiteral("file:///tmp")}) {
                    mock->setProperty("currentPath", location);
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                    const bool available = gpu && (location == path
                        || (ArchiveSupport::isArchivePath(location) && ArchiveSupport::archiveBackendAvailable()));
                    availabilityPassed = availabilityPassed && viewControls->property("mapAvailable").toBool() == available
                        && cycleButton->property("nextViewMode").toInt() == 3
                        && cycleButton->property("nextViewLabel").toString() == QStringLiteral("Switch to Map");
                }
                viewControls->setProperty("controller", original);
            }
        }
        success = success && availabilityPassed;
        bool placeholderPassed = map;
        if (map) {
            controller->openPath(QStringLiteral("devices://"));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            auto *placeholder = map->parentItem()->findChild<QQuickItem *>(QStringLiteral("mapUnavailablePlaceholder"));
            placeholderPassed = controller->viewMode() == 3 && placeholder && placeholder->isVisible() && !map->gpuEnabled();
            report.insert(QStringLiteral("placeholderVisible"), placeholderPassed);
            if (gpu) window.grabWindow().save(QStringLiteral("/tmp/folder-map-unavailable-panel.png"));
            QQuickItem *detailsButton = nullptr;
            QList<QQuickItem *> pendingItems;
            if (placeholder) pendingItems.append(placeholder);
            while (!pendingItems.isEmpty()) {
                auto *candidate = pendingItems.takeLast();
                if (candidate->objectName() == QLatin1String("mapPlaceholderViewButton0")) detailsButton = candidate;
                pendingItems.append(candidate->childItems());
            }
            if (detailsButton) {
                const QPointF position = detailsButton->mapToScene(QPointF(detailsButton->width() / 2, detailsButton->height() / 2));
                const QPointF global(window.mapToGlobal(position.toPoint()));
                QMouseEvent press(QEvent::MouseButtonPress, position, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, position, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QCoreApplication::sendEvent(&window, &press);
                QCoreApplication::sendEvent(&window, &release);
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            placeholderPassed = placeholderPassed && detailsButton && controller->viewMode() == 0;
            report.insert(QStringLiteral("placeholderButtonFound"), detailsButton != nullptr);
            report.insert(QStringLiteral("placeholderButtonPassed"), controller->viewMode() == 0);
            if (gpu) {
                window.requestActivate();
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                QKeyEvent press(QEvent::KeyPress, Qt::Key_4, Qt::ControlModifier);
                QKeyEvent release(QEvent::KeyRelease, Qt::Key_4, Qt::ControlModifier);
                QCoreApplication::sendEvent(&window, &press);
                QCoreApplication::sendEvent(&window, &release);
            } else {
                QMetaObject::invokeMethod(&window, "setActiveViewMode", Q_ARG(QVariant, QVariant(3)));
            }
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            report.insert(QStringLiteral("placeholderShortcutPassed"), controller->viewMode() == 3);
            placeholderPassed = placeholderPassed && controller->viewMode() == 3 && placeholder->isVisible();
            controller->openPath(path);
            wait.restart();
            while (wait.elapsed() < kTimeoutMs && (controller->directoryModel()->loading() || (gpu && map->loadedCount() < 25))) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            placeholderPassed = placeholderPassed && controller->viewMode() == 3
                && (gpu ? map->gpuEnabled() && !placeholder->isVisible() : placeholder->isVisible());
        }
        report.insert(QStringLiteral("placeholderPassed"), placeholderPassed);
        success = success && placeholderPassed;
        if (gpu && map) {
            const QString child = fixture.filePath(QStringLiteral("Folder 0"));
            controller->openPath(child);
            wait.restart();
            while (wait.elapsed() < kTimeoutMs && (map->moving() || controller->directoryModel()->loading()
                    || map->screenRect(child + QStringLiteral("/Child 0")).isEmpty())) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            success = success && map->currentPath() == child
                && map->screenRect(path).isEmpty()
                && !map->screenRect(child + QStringLiteral("/Child 0")).isEmpty();
            map->zoomAt(0, 0, -100);
            map->panBy(1e6, -1e6);
            wait.restart();
            while ((map->moving() || !map->iconsReady()) && wait.elapsed() < kTimeoutMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            const QRectF bounds = map->screenRect(child);
            success = success && bounds.left() >= 0 && bounds.top() >= 0
                && bounds.right() <= map->width() && bounds.bottom() <= map->height() && map->iconsReady();
#ifdef HAS_QT_PDF
            constexpr int expectedThumbnails = 2;
#else
            constexpr int expectedThumbnails = 1;
#endif
            const bool originalShowThumbnails = services.settings()->showThumbnails();
            const bool originalGradients = services.settings()->useGradientColors();
            services.settings()->setUseGradientColors(false);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            success = success && map->property("gradientStrength").toDouble() == 0;
            services.settings()->setUseGradientColors(true);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            success = success && map->property("gradientStrength").toDouble() > 0;
            services.settings()->setUseGradientColors(originalGradients);
            const QColor originalFileColor = map->property("fileColor").value<QColor>();
            map->setProperty("fileColor", QColor(QStringLiteral("#203040")));
            services.settings()->setShowThumbnails(true);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            wait.restart();
            while (map->thumbnailsReady() < expectedThumbnails && wait.elapsed() < kTimeoutMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            success = success && map->thumbnailsReady() >= expectedThumbnails;
            services.settings()->setShowThumbnails(false);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            success = success && !map->showThumbnails() && map->thumbnailsReady() == 0;
            window.grabWindow();
            services.settings()->setShowThumbnails(true);
            wait.restart();
            while (map->thumbnailsReady() < expectedThumbnails && wait.elapsed() < kTimeoutMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            success = success && map->showThumbnails() && map->thumbnailsReady() >= expectedThumbnails;
            const bool originalNativeIcons = services.settings()->useNativeIcons();
            for (const bool native : {false, true}) {
                services.settings()->setUseNativeIcons(native);
                wait.restart();
                while ((map->useNativeIcons() != native || !map->iconsReady()) && wait.elapsed() < kTimeoutMs) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                    QThread::msleep(1);
                }
                success = success && map->useNativeIcons() == native && map->iconsReady();
                window.grabWindow(); // Exercise the scene graph texture replacement for both modes.
            }
            services.settings()->setUseNativeIcons(originalNativeIcons);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            const QImage capture = window.grabWindow();
            const QPoint photoCenter = map->mapToScene(map->screenRect(child + QStringLiteral("/Photo.jpg")).center()).toPoint();
            success = success && capture.rect().contains(photoCenter)
                && capture.pixelColor(photoCenter).rgb() == mapPhoto.pixelColor(0, 0).rgb();
#ifdef HAS_QT_PDF
            const QRectF pdfRect = map->screenRect(child + QStringLiteral("/Document.pdf"));
            const QPoint pdfPaper = map->mapToScene(pdfRect.center() + QPointF(0, -pdfRect.height() * 0.2)).toPoint();
            success = success && capture.rect().contains(pdfPaper) && capture.pixelColor(pdfPaper) == QColor(Qt::white);
#endif
            map->setProperty("fileColor", originalFileColor);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            success = window.grabWindow().save(QStringLiteral("/tmp/folder-map-panel.png")) && success;
            services.settings()->setShowThumbnails(originalShowThumbnails);
            controller->setViewMode(0);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            success = success && controller->viewMode() == 0 && !map->gpuEnabled()
                && controller->directoryModel()->count() == 30;
            controller->setViewMode(3);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            success = success && map->gpuEnabled() && controller->viewMode() == 3;
            wait.restart();
            while (map->loadedCount() < 31 && wait.elapsed() < kTimeoutMs) {
                QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                QThread::msleep(1);
            }
            success = success && map->loadedCount() >= 31;
            const auto until = [&wait](const std::function<bool()> &ready) {
                wait.restart();
                while (!ready() && wait.elapsed() < kTimeoutMs) {
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
                    QThread::msleep(1);
                }
                return ready();
            };
            const QString source = child + QStringLiteral("/Photo.jpg");
            until([&] { const QPointF point = map->screenRect(source).center();
                return map->entryAt(point.x(), point.y()).value(QStringLiteral("path")).toString() == source; });
            window.grabWindow();
            const QPointF photoPosition = map->mapToScene(map->screenRect(source).center());
            const qreal zoomBeforeSelection = map->screenRect(child).width();
            const QPointF globalPosition(window.mapToGlobal(photoPosition.toPoint()));
            QMouseEvent press(QEvent::MouseButtonPress, photoPosition, globalPosition,
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent release(QEvent::MouseButtonRelease, photoPosition, globalPosition,
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &press);
            QCoreApplication::sendEvent(&window, &release);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            bool transfers = controller->selectedPaths() == QStringList{source};
            report.insert(QStringLiteral("clickSelectionPassed"), transfers);
            const bool selectionZoomPreserved = qAbs(map->screenRect(child).width() - zoomBeforeSelection) < 0.1;
            report.insert(QStringLiteral("selectionZoomPreserved"), selectionZoomPreserved);
            success = selectionZoomPreserved && success;
            success = window.grabWindow().save(QStringLiteral("/tmp/folder-map-selection.png")) && success;
            auto previousClipboard = std::make_unique<QMimeData>();
            if (const QMimeData *mime = QGuiApplication::clipboard()->mimeData()) {
                for (const QString &format : mime->formats()) previousClipboard->setData(format, mime->data(format));
            }
            const auto restoreClipboard = qScopeGuard([&] { QGuiApplication::clipboard()->setMimeData(previousClipboard.release()); });
            QJsonArray clipboardTrace;
            const auto traceClipboard = [&](const QString &stage) {
                const auto *clipboard = QGuiApplication::clipboard();
                const auto *mime = clipboard->mimeData();
                clipboardTrace.append(QJsonObject{
                    {QStringLiteral("stage"), stage},
                    {QStringLiteral("windowActive"), window.isActive()},
                    {QStringLiteral("ownsClipboard"), clipboard->ownsClipboard()},
                    {QStringLiteral("paths"), QJsonArray::fromStringList(workspace->clipboardPaths())},
                    {QStringLiteral("formats"), QJsonArray::fromStringList(mime ? mime->formats() : QStringList{})}});
            };
            QObject clipboardObserver;
            QObject::connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, &clipboardObserver,
                [&] { traceClipboard(QStringLiteral("dataChanged")); });
            workspace->copyToClipboard();
            traceClipboard(QStringLiteral("local-copy-published"));
            const QString copiedFolder = child + QStringLiteral("/Child 0");
            const QString copied = copiedFolder + QStringLiteral("/Photo.jpg");
            controller->openPath(copiedFolder);
            transfers = until([&] { return controller->currentPath() == copiedFolder && !controller->directoryModel()->loading(); }) && transfers;
            traceClipboard(QStringLiteral("local-before-paste"));
            workspace->pasteFromClipboard();
            const bool copiedReady = until([&] { return QFileInfo::exists(copied)
                && controller->directoryModel()->indexOfPath(copied) >= 0 && !map->screenRect(copied).isEmpty(); });
            report.insert(QStringLiteral("copyPassed"), copiedReady);
            transfers = copiedReady && transfers;
            if (controller->directoryModel()->indexOfPath(copied) >= 0) {
                controller->directoryModel()->selectOnly(controller->directoryModel()->indexOfPath(copied));
                workspace->cutToClipboard();
                const QString movedFolder = child + QStringLiteral("/Child 1");
                const QString moved = movedFolder + QStringLiteral("/Photo.jpg");
                controller->openPath(movedFolder);
                transfers = until([&] { return controller->currentPath() == movedFolder && !controller->directoryModel()->loading(); }) && transfers;
                workspace->pasteFromClipboard();
                const bool movedReady = until([&] { return !QFileInfo::exists(copied) && QFileInfo::exists(moved)
                    && controller->directoryModel()->indexOfPath(moved) >= 0 && !map->screenRect(moved).isEmpty(); });
                report.insert(QStringLiteral("movePassed"), movedReady);
                transfers = movedReady && transfers;
                transfers = transfers && QImage(moved).pixelColor(0, 0).rgb() == mapPhoto.pixelColor(0, 0).rgb();
            } else transfers = false;
            controller->openPath(child);
            transfers = until([&] { return controller->currentPath() == child && !controller->directoryModel()->loading(); }) && transfers;
            const QString incomingSource = fixtureRoot.filePath(QStringLiteral("Incoming.txt"));
            transfers = writeFile(incomingSource, QByteArray("incoming drop fixture")) && transfers;
            const QVariantList urls{QUrl::fromLocalFile(incomingSource)};
            transfers = workspace->externalDropCapabilities(urls, 0, child).value(QStringLiteral("canCopy")).toBool() && transfers;
            QMimeData incomingMime;
            incomingMime.setUrls({QUrl::fromLocalFile(incomingSource)});
            const QPoint dropPosition = map->mapToScene(QPointF(map->width() / 2, map->height() / 2)).toPoint();
#if QT_VERSION >= QT_VERSION_CHECK(6, 12, 0)
            QDragEnterEvent dragEnter(QPointF(dropPosition), Qt::CopyAction, &incomingMime, Qt::LeftButton, Qt::NoModifier);
#else
            QDragEnterEvent dragEnter(dropPosition, Qt::CopyAction, &incomingMime, Qt::LeftButton, Qt::NoModifier);
#endif
            QCoreApplication::sendEvent(&window, &dragEnter);
            QDropEvent drop(dropPosition, Qt::CopyAction, &incomingMime, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &drop);
            report.insert(QStringLiteral("incomingDropPassed"), dragEnter.isAccepted() && drop.isAccepted());
            transfers = dragEnter.isAccepted() && drop.isAccepted() && transfers;
            const QString incoming = child + QStringLiteral("/Incoming.txt");
            transfers = until([&] { return controller->directoryModel()->indexOfPath(incoming) >= 0
                && !controller->directoryModel()->loading() && !map->screenRect(incoming).isEmpty() && !map->moving(); }) && transfers;
            window.grabWindow();
            const QRectF cameraBefore = map->screenRect(child);
            const QString renamed = child + QStringLiteral("/Renamed.txt");
            transfers = QFile::rename(incoming, renamed) && transfers;
            transfers = until([&] { return controller->directoryModel()->indexOfPath(renamed) >= 0
                && !controller->directoryModel()->loading() && !map->moving()
                && !map->screenRect(renamed).isEmpty() && map->screenRect(incoming).isEmpty(); }) && transfers;
            window.grabWindow();
            const QRectF cameraAfter = map->screenRect(child);
            const bool cameraPreserved = QLineF(cameraBefore.topLeft(), cameraAfter.topLeft()).length() < 0.1
                && qAbs(cameraBefore.width() - cameraAfter.width()) < 0.1
                && qAbs(cameraBefore.height() - cameraAfter.height()) < 0.1;
            report.insert(QStringLiteral("cameraPreserved"), cameraPreserved);
            transfers = transfers && cameraPreserved;
            transfers = QFile::remove(renamed) && transfers;
            transfers = until([&] { return controller->directoryModel()->indexOfPath(renamed) < 0
                && map->screenRect(renamed).isEmpty(); }) && transfers;
            report.insert(QStringLiteral("transfersPassed"), transfers);
            success = success && transfers;

            auto clickMapEntry = [&](const QString &entryPath, Qt::KeyboardModifiers modifiers, bool header = false) {
                window.grabWindow(); // Settle layout and visible hit regions before synthesizing a click.
                const QRectF rect = map->screenRect(entryPath);
                const QPointF position = map->mapToScene(header ? rect.topLeft() + QPointF(15, 10) : rect.center());
                const QPointF global(window.mapToGlobal(position.toPoint()));
                QMouseEvent press(QEvent::MouseButtonPress, position, global,
                                  Qt::LeftButton, Qt::LeftButton, modifiers);
                QMouseEvent release(QEvent::MouseButtonRelease, position, global,
                                    Qt::LeftButton, Qt::NoButton, modifiers);
                QCoreApplication::sendEvent(&window, &press);
                QCoreApplication::sendEvent(&window, &release);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            };

            QObject *mapView = map->parentItem();
            QObject *mapPanelHost = mapView->property("panel").value<QObject *>();
            const QString focusedFolder = child + QStringLiteral("/Child 2");
            controller->directoryModel()->setSortPolicy(DirectoryModel::SortByName, Qt::AscendingOrder);
            map->revealPath(focusedFolder);
            success = until([&] { return !map->moving() && !map->screenRect(focusedFolder).isEmpty(); }) && success;
            clickMapEntry(focusedFolder, Qt::NoModifier);
            const int oldFocusRow = controller->directoryModel()->indexOfPath(focusedFolder);
            const QString insertedFolder = child + QStringLiteral("/A-before");
            QObject insertionObserver;
            int focusInsertSignals = 0, focusInsertResets = 0;
            QObject::connect(controller->directoryModel(), &QAbstractItemModel::rowsInserted,
                &insertionObserver, [&] { ++focusInsertSignals; });
            QObject::connect(controller->directoryModel(), &QAbstractItemModel::modelReset,
                &insertionObserver, [&] { ++focusInsertResets; });
            bool sortedInsertFocus = QDir().mkpath(insertedFolder) && until([&] {
                return controller->directoryModel()->indexOfPath(insertedFolder) >= 0;
            });
            sortedInsertFocus = sortedInsertFocus && focusInsertSignals > 0 && focusInsertResets == 0
                && controller->currentItemPath() == focusedFolder
                && controller->directoryModel()->indexOfPath(focusedFolder) == oldFocusRow + 1
                && mapView->property("currentIndex").toInt() == oldFocusRow + 1;
            QKeyEvent enterPress(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QKeyEvent enterRelease(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &enterPress);
            QCoreApplication::sendEvent(&window, &enterRelease);
            sortedInsertFocus = until([&] { return controller->currentPath() == focusedFolder
                && !controller->directoryModel()->loading(); }) && sortedInsertFocus;
            report.insert(QStringLiteral("sortedInsertFocus"), sortedInsertFocus);
            success = sortedInsertFocus && success;
            QDir().rmdir(insertedFolder);
            controller->openPath(child);
            success = until([&] { return controller->currentPath() == child
                && !controller->directoryModel()->loading() && map->currentPath() == child; }) && success;

            const QString nestedMenuFolder = child + QStringLiteral("/Child 2");
            const QString rootIso = child + QStringLiteral("/Menu-image.ISO");
            const QString nestedZip = nestedMenuFolder + QStringLiteral("/Menu-archive.ZIP");
            const QString nestedIso = nestedMenuFolder + QStringLiteral("/Menu-image.udf");
            bool mapContextTypes = writeFile(rootIso) && writeFile(nestedZip) && writeFile(nestedIso);
            mapContextTypes = until([&] { return controller->directoryModel()->indexOfPath(rootIso) >= 0; }) && mapContextTypes;
            map->revealPath(nestedMenuFolder);
            mapContextTypes = until([&] { return !map->entryForPath(nestedIso).isEmpty(); }) && mapContextTypes;
            QJsonArray contextTypeResults;
            for (const QString &menuPath : {child + QStringLiteral("/Archive.zip"), rootIso, nestedZip, nestedIso}) {
                window.grabWindow(); // Apply watcher updates before choosing the camera target.
                map->revealPath(menuPath);
                mapContextTypes = until([&] { return !map->moving() && !map->screenRect(menuPath).isEmpty(); }) && mapContextTypes;
                mapContextTypes = QMetaObject::invokeMethod(mapView, "showContextMenu",
                    Q_ARG(QVariant, map->entryForPath(menuPath))) && mapContextTypes;
                QObject *itemMenu = nullptr, *itemPopup = nullptr;
                const bool opened = until([&] {
                    itemMenu = mapPanelHost->property("filePanelContextMenuItem").value<QObject *>();
                    itemPopup = itemMenu ? itemMenu->findChild<QObject *>(QStringLiteral("filePanelItemPopup")) : nullptr;
                    return itemPopup && itemPopup->property("opened").toBool();
                });
                const bool iso = menuPath == rootIso || menuPath == nestedIso;
                mapContextTypes = opened && itemMenu && itemMenu->property("contextPathValue").toString() == menuPath
                    && itemMenu->property("contextCanExtractArchive").toBool() == !iso
                    && itemMenu->property("contextCanMountIso").toBool() == iso && mapContextTypes;
                if (opened && !iso) {
                    QObject *policy = itemMenu->findChild<QObject *>(QStringLiteral("filePanelItemMenuPolicy"));
                    QVariant canExtract;
                    mapContextTypes = policy && QMetaObject::invokeMethod(policy, "canExtractContextArchive",
                        Q_RETURN_ARG(QVariant, canExtract)) && canExtract.toBool() && mapContextTypes;
                }
                contextTypeResults.append(QJsonObject{
                    {QStringLiteral("path"), menuPath}, {QStringLiteral("opened"), opened},
                    {QStringLiteral("entry"), QJsonObject::fromVariantMap(map->entryForPath(menuPath))},
                    {QStringLiteral("actualPath"), itemMenu ? itemMenu->property("contextPathValue").toString() : QString{}},
                    {QStringLiteral("extract"), itemMenu && itemMenu->property("contextCanExtractArchive").toBool()},
                    {QStringLiteral("mount"), itemMenu && itemMenu->property("contextCanMountIso").toBool()}});
                if (itemPopup) QMetaObject::invokeMethod(itemPopup, "close");
                mapContextTypes = until([&] { return itemPopup && !itemPopup->property("visible").toBool(); }) && mapContextTypes;
            }
            report.insert(QStringLiteral("contextTypeResults"), contextTypeResults);
            report.insert(QStringLiteral("mapContextTypes"), mapContextTypes);
            success = mapContextTypes && success;
            QFile::remove(rootIso);
            QFile::remove(nestedZip);
            QFile::remove(nestedIso);

            if (ArchiveSupport::archiveBackendAvailable()) {
                const QString archiveSource = fixtureRoot.filePath(QStringLiteral("archive-source"));
                QDir().mkpath(QDir(archiveSource).filePath(QStringLiteral("Branch/Deep")));
                bool archives = writeFile(QDir(archiveSource).filePath(QStringLiteral("Branch/Deep/inside.txt")), "inside archive")
                    && writeFile(QDir(archiveSource).filePath(QStringLiteral("archive-note.txt")), "archive note")
                    && writeFile(QDir(archiveSource).filePath(QStringLiteral(".hidden.txt")), "hidden");
                const QString archive = QDir(child).filePath(QStringLiteral("Archive.zip"));
                QFile::remove(archive); // Replace the empty mixed-file fixture with a real archive.
                QProcess compressor;
                compressor.setWorkingDirectory(archiveSource);
                compressor.start(ArchiveSupport::sevenZipExecutablePath(),
                    {QStringLiteral("a"), QStringLiteral("-tzip"), QStringLiteral("-y"), archive,
                     QStringLiteral("Branch"), QStringLiteral("archive-note.txt"), QStringLiteral(".hidden.txt")});
                archives = compressor.waitForFinished(kTimeoutMs) && compressor.exitCode() == 0 && archives;
                archives = until([&] { return controller->directoryModel()->indexOfPath(archive) >= 0; }) && archives;
                QObject observer;
                QList<int> modes{controller->viewMode()};
                QObject::connect(controller, &FilePanelController::viewModeChanged, &observer,
                    [&] { modes.append(controller->viewMode()); });
                controller->openItem(controller->directoryModel()->indexOfPath(archive));
                const QString archiveRoot = ArchiveSupport::archiveRootPath(archive);
                const QString branch = ArchiveSupport::archiveChildPath(archiveRoot, QStringLiteral("Branch"));
                const QString inside = ArchiveSupport::archiveChildPath(branch + QLatin1Char('/'), QStringLiteral("Deep/inside.txt"));
                archives = until([&] { return controller->currentPath() == archiveRoot
                    && !controller->directoryModel()->loading() && map->currentPath() == archiveRoot
                    && !map->screenRect(branch).isEmpty(); }) && archives;
                map->revealPath(branch);
                archives = until([&] { return !map->screenRect(inside).isEmpty(); }) && archives;
                map->revealPath(inside);
                archives = until([&] { return !map->moving() && !map->screenRect(inside).isEmpty(); }) && archives;
                clickMapEntry(inside, Qt::NoModifier);
                workspace->copyToClipboard();
                const bool archiveNestedCopied = workspace->clipboardPaths() == QStringList{inside};
                const QString archiveNote = ArchiveSupport::archiveChildPath(archiveRoot, QStringLiteral("archive-note.txt"));
                map->revealPath(archiveNote);
                archives = until([&] { return !map->moving() && !map->screenRect(archiveNote).isEmpty(); }) && archives;
                clickMapEntry(archiveNote, Qt::NoModifier);
                workspace->copyToClipboard();
                const bool archiveRootCopied = workspace->clipboardPaths() == QStringList{archiveNote};
                traceClipboard(QStringLiteral("archive-copy-published"));
                const QString archiveDestination = fixtureRoot.filePath(QStringLiteral("archive-paste"));
                QDir().mkpath(archiveDestination);
                controller->openPath(archiveDestination);
                archives = until([&] { return controller->currentPath() == archiveDestination
                    && !controller->directoryModel()->loading(); }) && archives;
                report.insert(QStringLiteral("archiveClipboardBeforePaste"), QJsonArray::fromStringList(workspace->clipboardPaths()));
                const QMimeData *archiveMime = QApplication::clipboard()->mimeData();
                report.insert(QStringLiteral("archiveClipboardFormats"), QJsonArray::fromStringList(archiveMime ? archiveMime->formats() : QStringList{}));
                traceClipboard(QStringLiteral("archive-before-paste"));
                workspace->pasteFromClipboard();
                const bool archivePaste = until([&] {
                    QFile extracted(QDir(archiveDestination).filePath(QStringLiteral("archive-note.txt")));
                    return extracted.open(QIODevice::ReadOnly) && extracted.readAll() == QByteArrayLiteral("archive note");
                });
                report.insert(QStringLiteral("archiveNestedCopied"), archiveNestedCopied);
                report.insert(QStringLiteral("archiveRootCopied"), archiveRootCopied);
                report.insert(QStringLiteral("archivePaste"), archivePaste);
                archives = archiveNestedCopied && archiveRootCopied && archivePaste && archives;
                controller->openPath(archiveRoot);
                archives = until([&] { return controller->currentPath() == archiveRoot
                    && !controller->directoryModel()->loading() && !map->screenRect(branch).isEmpty(); }) && archives;
                controller->openItem(controller->directoryModel()->indexOfPath(branch));
                archives = until([&] { return controller->currentPath() == branch
                    && !controller->directoryModel()->loading() && !map->screenRect(inside).isEmpty(); }) && archives;
                controller->goUp();
                archives = until([&] { return controller->currentPath() == archiveRoot
                    && !controller->directoryModel()->loading(); }) && archives;
                controller->goUp();
                archives = until([&] { return controller->currentPath() == child
                    && !controller->directoryModel()->loading() && map->currentPath() == child
                    && !map->screenRect(source).isEmpty(); }) && archives;
                archives = archives && std::all_of(modes.cbegin(), modes.cend(), [](int mode) { return mode == 3; });
                report.insert(QStringLiteral("archivesPassed"), archives);
                success = success && archives;
            }
            const qreal zoomBeforePanelSwitch = map->screenRect(child).width();
            workspace->setSplitEnabled(true);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            workspace->setActivePanel(1);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            workspace->setActivePanel(0);
            workspace->setSplitEnabled(false);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            const bool panelZoomPreserved = qAbs(map->screenRect(child).width() - zoomBeforePanelSwitch) < 0.1;
            report.insert(QStringLiteral("panelZoomPreserved"), panelZoomPreserved);
            success = panelZoomPreserved && success;

            controller->goUp();
            const QString nestedFolder = child + QStringLiteral("/Child 2");
            success = until([&] { return controller->currentPath() == path && map->currentPath() == path
                && !map->screenRect(child).isEmpty(); }) && success;
            map->revealPath(child);
            success = until([&] { return !map->screenRect(nestedFolder).isEmpty(); }) && success;
            success = until([&] { return !map->moving(); }) && success;
            const QString nestedSibling = child + QStringLiteral("/Child 3");
            auto hasMapSelection = [&](const QStringList &expected) {
                const QStringList paths = controller->selectedPaths();
                return controller->currentPath() == path && paths.size() == expected.size()
                    && controller->directoryModel()->selectedCount() == expected.size()
                    && map->selectedPaths() == paths
                    && std::all_of(expected.cbegin(), expected.cend(), [&](const QString &p) { return paths.contains(p); });
            };
            QMetaObject::invokeMethod(&window, "setPreviewPaneVisible", Q_ARG(QVariant, true));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            map->revealPath(child);
            success = until([&] { return !map->moving() && !map->screenRect(source).isEmpty(); }) && success;
            clickMapEntry(source, Qt::NoModifier);
            const bool nestedPreview = until([&] { return services.quickLook()->path() == source
                && services.quickLook()->type() == QStringLiteral("image") && !services.quickLook()->loading(); })
                && controller->currentPath() == path && controller->currentItemPath() == source;
            report.insert(QStringLiteral("nestedPreview"), nestedPreview);
            success = nestedPreview && success;
            QObject *mapPanel = map->parentItem()->property("panel").value<QObject *>();
            if (mapPanel) QMetaObject::invokeMethod(mapPanel, "focusContentAndQueueCurrentIndexEnsure");
            QElapsedTimer focusWait;
            focusWait.start();
            until([&] { return focusWait.elapsed() >= 250; });
            const bool nestedFocusPreserved = controller->currentItemPath() == source
                && controller->selectedPaths() == QStringList{source};
            report.insert(QStringLiteral("nestedFocusPreserved"), nestedFocusPreserved);
            success = nestedFocusPreserved && success;
            QMetaObject::invokeMethod(&window, "quickLookActiveTarget");
            QObject *quickLookPopup = nullptr;
            const bool nestedQuickLook = until([&] {
                quickLookPopup = window.property("quickLookPopupItem").value<QObject *>();
                return quickLookPopup && quickLookPopup->property("opened").toBool();
            }) && quickLookPopup->property("previewPath").toString() == source
                && services.quickLook()->path() == source && controller->currentPath() == path;
            report.insert(QStringLiteral("nestedQuickLook"), nestedQuickLook);
            success = nestedQuickLook && success;
            auto *navigation = services.quickLookNavigation();
            const auto previewPaths = map->selectionContext(source).value(QStringLiteral("paths")).toStringList();
            const int previewIndex = previewPaths.indexOf(source);
            auto *previousButton = quickLookPopup ? quickLookPopup->findChild<QQuickItem *>(QStringLiteral("quickLookPreviousButton")) : nullptr;
            auto *nextButton = quickLookPopup ? quickLookPopup->findChild<QQuickItem *>(QStringLiteral("quickLookNextButton")) : nullptr;
            bool nestedQuickLookNavigation = nestedQuickLook && navigation->active()
                && previousButton && previousButton->isVisible() && nextButton && nextButton->isVisible()
                && previousButton->isEnabled() == (previewIndex > 0)
                && nextButton->isEnabled() == (previewIndex + 1 < previewPaths.size())
                && navigation->canGoPrevious() == (previewIndex > 0)
                && navigation->canGoNext() == (previewIndex + 1 < previewPaths.size());
            for (int direction : {-1, 1}) {
                const int adjacent = previewIndex + direction;
                if (adjacent < 0 || adjacent >= previewPaths.size()) continue;
                const QString target = previewPaths.at(adjacent);
                nestedQuickLookNavigation = navigation->navigate(direction)
                    && until([&] { return services.quickLook()->path() == target
                        && controller->currentItemPath() == target && controller->selectedPaths() == QStringList{target}; })
                    && controller->currentPath() == path && nestedQuickLookNavigation;
                nestedQuickLookNavigation = navigation->navigate(-direction)
                    && until([&] { return services.quickLook()->path() == source
                        && controller->currentItemPath() == source && controller->selectedPaths() == QStringList{source}; })
                    && controller->currentPath() == path && nestedQuickLookNavigation;
            }
            report.insert(QStringLiteral("nestedQuickLookNavigation"), nestedQuickLookNavigation);
            success = nestedQuickLookNavigation && success;
            if (quickLookPopup) QMetaObject::invokeMethod(quickLookPopup, "close");
            success = until([&] { return quickLookPopup && !quickLookPopup->property("visible").toBool(); }) && success;
            QMetaObject::invokeMethod(&window, "setPreviewPaneVisible", Q_ARG(QVariant, false));
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            map->revealPath(child);
            success = until([&] { return !map->moving(); }) && success;
            clickMapEntry(nestedFolder, Qt::NoModifier);
            clickMapEntry(nestedSibling, Qt::ShiftModifier);
            const QStringList siblings = map->selectionContext(nestedFolder).value(QStringLiteral("paths")).toStringList();
            const int firstSibling = siblings.indexOf(nestedFolder), lastSibling = siblings.indexOf(nestedSibling);
            bool nestedKeyboardSelection = hasMapSelection(siblings.mid(0, lastSibling + 1));
            clickMapEntry(nestedFolder, Qt::NoModifier);
            const auto sendMapKey = [&](int key, Qt::KeyboardModifiers modifiers) {
                window.requestActivate();
                success = until([&] { return window.isActive(); }) && success;
                QKeyEvent press(QEvent::KeyPress, key, modifiers);
                QKeyEvent release(QEvent::KeyRelease, key, modifiers);
                QCoreApplication::sendEvent(&window, &press);
                QCoreApplication::sendEvent(&window, &release);
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            };
            sendMapKey(Qt::Key_Right, Qt::ControlModifier);
            const QString nextSibling = siblings.value(firstSibling + 1);
            nestedKeyboardSelection = controller->currentItemPath() == nextSibling
                && hasMapSelection({nestedFolder}) && nestedKeyboardSelection;
            sendMapKey(Qt::Key_Space, Qt::ControlModifier);
            nestedKeyboardSelection = hasMapSelection({nestedFolder, nextSibling}) && nestedKeyboardSelection;
            sendMapKey(Qt::Key_Right, Qt::NoModifier);
            nestedKeyboardSelection = hasMapSelection({siblings.value(firstSibling + 2)}) && nestedKeyboardSelection;
            sendMapKey(Qt::Key_Right, Qt::ControlModifier | Qt::ShiftModifier);
            nestedKeyboardSelection = hasMapSelection(siblings.mid(0, firstSibling + 4))
                && nestedKeyboardSelection;
            report.insert(QStringLiteral("nestedKeyboardSelection"), nestedKeyboardSelection);
            success = nestedKeyboardSelection && success;
            clickMapEntry(source, Qt::NoModifier);
            const bool nestedRemovalCleared = QFile::remove(source) && until([&] {
                return map->entryForPath(source).isEmpty() && controller->selectedPaths().isEmpty()
                    && controller->currentItemPath() != source;
            });
            report.insert(QStringLiteral("nestedRemovalCleared"), nestedRemovalCleared);
            success = nestedRemovalCleared && success;
            clickMapEntry(nestedFolder, Qt::NoModifier);
            clickMapEntry(nestedSibling, Qt::ControlModifier);
            bool nestedCtrlSelection = hasMapSelection({nestedFolder, nestedSibling});
            clickMapEntry(nestedFolder, Qt::ControlModifier);
            nestedCtrlSelection = hasMapSelection({nestedSibling}) && nestedCtrlSelection;
            clickMapEntry(child, Qt::ControlModifier, true);
            nestedCtrlSelection = hasMapSelection({nestedSibling, child}) && nestedCtrlSelection;
            clickMapEntry(nestedFolder, Qt::ControlModifier);
            nestedCtrlSelection = hasMapSelection({nestedSibling, child, nestedFolder}) && nestedCtrlSelection;
            clickMapEntry(child, Qt::NoModifier, true);
            nestedCtrlSelection = hasMapSelection({child}) && nestedCtrlSelection;
            clickMapEntry(nestedFolder, Qt::NoModifier);
            nestedCtrlSelection = hasMapSelection({nestedFolder}) && nestedCtrlSelection;
            report.insert(QStringLiteral("nestedCtrlSelection"), nestedCtrlSelection);
            success = nestedCtrlSelection && success;
            const auto checkShiftRanges = [&](const QStringList &scope) {
                if (scope.size() < 11) {
                    report.insert(QStringLiteral("incompleteShiftScope"), QJsonObject{
                        {QStringLiteral("path"), controller->currentPath()},
                        {QStringLiteral("entries"), QJsonArray::fromStringList(scope)}});
                    return false;
                }
                clickMapEntry(scope.at(2), Qt::NoModifier);
                clickMapEntry(scope.at(2), Qt::ShiftModifier);
                bool passed = hasMapSelection(scope.mid(0, 3));
                clickMapEntry(scope.at(10), Qt::ControlModifier);
                clickMapEntry(scope.at(6), Qt::ShiftModifier);
                QStringList expected = scope.mid(0, 7);
                expected.append(scope.at(10));
                passed = hasMapSelection(expected) && passed;
                clickMapEntry(scope.at(4), Qt::ShiftModifier);
                expected = scope.mid(0, 5);
                expected.append(scope.at(10));
                passed = hasMapSelection(expected) && passed;
                clickMapEntry(scope.at(8), Qt::ShiftModifier);
                expected = scope.mid(0, 9);
                expected.append(scope.at(10));
                passed = hasMapSelection(expected) && passed;
                controller->directoryModel()->clearSelection();
                clickMapEntry(scope.at(8), Qt::ShiftModifier);
                return hasMapSelection(scope.mid(0, 9)) && passed;
            };
            const bool nestedShiftRanges = checkShiftRanges(map->selectionContext(nestedFolder).value(QStringLiteral("paths")).toStringList());
            report.insert(QStringLiteral("nestedShiftRanges"), nestedShiftRanges);
            success = nestedShiftRanges && success;
            map->fitCurrent();
            success = until([&] { return !map->moving(); }) && success;
            const bool rootShiftRanges = checkShiftRanges(map->selectionContext(QString{}).value(QStringLiteral("paths")).toStringList());
            report.insert(QStringLiteral("rootShiftRanges"), rootShiftRanges);
            success = rootShiftRanges && success;
            controller->directoryModel()->clearSelection();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            window.grabWindow();
            map->revealPath(nestedFolder);
            success = until([&] { return !map->moving(); }) && success;
            const QPointF nestedPosition = map->mapToScene(map->screenRect(nestedFolder).center());
            const QPointF nestedGlobal(window.mapToGlobal(nestedPosition.toPoint()));
            QMouseEvent nestedPress(QEvent::MouseButtonPress, nestedPosition, nestedGlobal,
                                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QMouseEvent nestedRelease(QEvent::MouseButtonRelease, nestedPosition, nestedGlobal,
                                      Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &nestedPress);
            QCoreApplication::sendEvent(&window, &nestedRelease);
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            const bool nestedClickSelected = controller->currentPath() == path
                && controller->selectedPaths() == QStringList{nestedFolder}
                && controller->directoryModel()->selectedCount() == 1
                && controller->currentItemPath() == nestedFolder
                && map->selectedPaths() == QStringList{nestedFolder};
            report.insert(QStringLiteral("nestedClickSelected"), nestedClickSelected);
            success = nestedClickSelected && success;
            QObject *panel = map->parentItem()->property("panel").value<QObject *>();
            QMouseEvent nestedRightPress(QEvent::MouseButtonPress, nestedPosition, nestedGlobal,
                                         Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            QMouseEvent nestedRightRelease(QEvent::MouseButtonRelease, nestedPosition, nestedGlobal,
                                           Qt::RightButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &nestedRightPress);
            QCoreApplication::sendEvent(&window, &nestedRightRelease);
            const bool nestedMenuOpened = until([&] { return panel && panel->property("contextMenuOpen").toBool()
                && panel->property("filePanelContextMenuItem").value<QObject *>(); });
            QObject *menu = panel ? panel->property("filePanelContextMenuItem").value<QObject *>() : nullptr;
            QObject *popup = menu ? menu->findChild<QObject *>(QStringLiteral("filePanelItemPopup")) : nullptr;
            const bool popupOpened = until([&] { return popup && popup->property("opened").toBool(); });
            bool nestedRightClickMenu = nestedMenuOpened && popupOpened && menu
                && controller->currentPath() == path && controller->selectedPaths() == QStringList{nestedFolder}
                && controller->currentItemPath() == nestedFolder
                && menu->property("contextPathValue").toString() == nestedFolder
                && menu->property("contextRowValue").toInt() == -1
                && menu->property("contextDirectoryValue").toBool();
            QObject *menuPolicy = menu ? menu->findChild<QObject *>(QStringLiteral("filePanelItemMenuPolicy")) : nullptr;
            for (const char *action : {"canOpenContextItem", "canShowContextProperties", "canAnalyzeContextFolder", "canRenameSelection"}) {
                QVariant enabled;
                nestedRightClickMenu = menuPolicy && QMetaObject::invokeMethod(menuPolicy, action,
                    Q_RETURN_ARG(QVariant, enabled)) && enabled.toBool() && nestedRightClickMenu;
            }
            report.insert(QStringLiteral("nestedRightClickMenu"), nestedRightClickMenu);
            success = nestedRightClickMenu && success;
            if (popup) QMetaObject::invokeMethod(popup, "close");
            success = until([&] { return popup && !popup->property("visible").toBool(); }) && success;
            clickMapEntry(nestedFolder, Qt::NoModifier);
            const QRectF renameCamera = map->screenRect(path);
            int renameRequests = 0;
            QObject renameObserver;
            QObject::connect(workspace, &WorkspaceController::renameRequested, &renameObserver,
                [&] { ++renameRequests; });
            report.insert(QStringLiteral("renameSelectionBeforeF2"), QJsonArray::fromStringList(controller->selectedPaths()));
            report.insert(QStringLiteral("renameAllowedBeforeF2"), controller->canRenameSelection());
            report.insert(QStringLiteral("overlayBeforeF2"), window.property("anyOverlayOpen").toBool());
            sendMapKey(Qt::Key_F2, Qt::NoModifier);
            QObject *renameDialog = nullptr;
            bool mapRenamePassed = until([&] {
                renameDialog = window.findChild<QObject *>(QStringLiteral("mapRenameDialog"));
                return renameDialog && renameDialog->property("opened").toBool();
            });
            QObject *nameField = window.findChild<QObject *>(QStringLiteral("mapRenameNameField"));
            report.insert(QStringLiteral("renameRequestsAfterF2"), renameRequests);
            report.insert(QStringLiteral("renameWindowActive"), window.isActive());
            report.insert(QStringLiteral("renameDialogFound"), renameDialog != nullptr);
            mapRenamePassed = mapRenamePassed && nameField && nameField->property("selectedText").toString() == QFileInfo(nestedFolder).fileName()
                && window.property("workspaceOverlayOpen").toBool();
            report.insert(QStringLiteral("mapRenameSource"), renameDialog ? renameDialog->property("sourcePath").toString() : QString{});
            if (renameDialog && renameDialog->property("sourcePath").toString() == nestedFolder) {
                window.grabWindow().save(QStringLiteral("/tmp/folder-map-rename-dialog.png"));
                sendMapKey(Qt::Key_1, Qt::ControlModifier);
                mapRenamePassed = controller->viewMode() == 3 && renameDialog->property("opened").toBool() && mapRenamePassed;
                sendMapKey(Qt::Key_Escape, Qt::NoModifier);
                mapRenamePassed = until([&] { return !renameDialog->property("visible").toBool(); }) && mapRenamePassed;
                mapRenamePassed = QFileInfo::exists(nestedFolder) && map->screenRect(path) == renameCamera && mapRenamePassed;
                sendMapKey(Qt::Key_F2, Qt::NoModifier);
                mapRenamePassed = until([&] { return renameDialog->property("opened").toBool(); }) && mapRenamePassed;
                if (nameField) {
                    nameField->setProperty("text", QString{});
                    QMetaObject::invokeMethod(renameDialog, "submit");
                    mapRenamePassed = renameDialog->property("opened").toBool() && QFileInfo::exists(nestedFolder) && mapRenamePassed;
                    nameField->setProperty("text", QStringLiteral("Child 1"));
                    QMetaObject::invokeMethod(renameDialog, "submit");
                    mapRenamePassed = renameDialog->property("opened").toBool()
                        && !renameDialog->property("errorMessage").toString().isEmpty()
                        && QFileInfo::exists(nestedFolder) && mapRenamePassed;
                    const QString renamedFolder = child + QStringLiteral("/Renamed Child");
                    nameField->setProperty("text", QStringLiteral("Renamed Child"));
                    sendMapKey(Qt::Key_Return, Qt::NoModifier);
                    mapRenamePassed = until([&] { return !renameDialog->property("visible").toBool()
                        && QFileInfo::exists(renamedFolder) && !map->screenRect(renamedFolder).isEmpty(); }) && mapRenamePassed;
                    mapRenamePassed = hasMapSelection({renamedFolder}) && controller->currentItemPath() == renamedFolder
                        && map->screenRect(path) == renameCamera && mapRenamePassed;
                    sendMapKey(Qt::Key_F2, Qt::NoModifier);
                    mapRenamePassed = until([&] { return renameDialog->property("opened").toBool(); }) && mapRenamePassed;
                    nameField->setProperty("text", QFileInfo(nestedFolder).fileName());
                    sendMapKey(Qt::Key_Return, Qt::NoModifier);
                    mapRenamePassed = until([&] { return !renameDialog->property("visible").toBool()
                        && QFileInfo::exists(nestedFolder) && !map->screenRect(nestedFolder).isEmpty(); }) && mapRenamePassed;
                }
            }
            if (renameDialog && renameDialog->property("visible").toBool()) QMetaObject::invokeMethod(renameDialog, "reject");
            if (renameDialog && nameField) {
                map->revealPath(child);
                mapRenamePassed = until([&] { return !map->moving(); }) && mapRenamePassed;
                window.grabWindow();
                clickMapEntry(child + QStringLiteral("/Notes.txt"), Qt::NoModifier);
                sendMapKey(Qt::Key_F2, Qt::NoModifier);
                mapRenamePassed = until([&] { return renameDialog->property("opened").toBool(); }) && mapRenamePassed;
                mapRenamePassed = nameField->property("selectedText").toString() == QStringLiteral("Notes") && mapRenamePassed;
                sendMapKey(Qt::Key_Escape, Qt::NoModifier);
                mapRenamePassed = until([&] { return !renameDialog->property("visible").toBool(); }) && mapRenamePassed;
                clickMapEntry(child, Qt::NoModifier, true);
                sendMapKey(Qt::Key_F2, Qt::NoModifier);
                mapRenamePassed = until([&] { return renameDialog->property("opened").toBool(); }) && mapRenamePassed;
                mapRenamePassed = renameDialog->property("sourcePath").toString() == child && mapRenamePassed;
                sendMapKey(Qt::Key_Escape, Qt::NoModifier);
                mapRenamePassed = until([&] { return !renameDialog->property("visible").toBool(); }) && mapRenamePassed;
            }
            report.insert(QStringLiteral("mapRenamePassed"), mapRenamePassed);
            success = mapRenamePassed && success;
            clickMapEntry(nestedFolder, Qt::NoModifier);
            clickMapEntry(nestedSibling, Qt::ControlModifier);
            sendMapKey(Qt::Key_F2, Qt::NoModifier);
            QObject *batchDialog = nullptr;
            const bool nestedBatchRename = until([&] {
                for (auto *candidate : window.findChildren<QObject *>()) {
                    if (candidate->property("title").toString() == QStringLiteral("Batch Rename")
                        && candidate->property("opened").toBool()) batchDialog = candidate;
                }
                return batchDialog != nullptr;
            });
            report.insert(QStringLiteral("nestedBatchRename"), nestedBatchRename);
            report.insert(QStringLiteral("renameRequestsTotal"), renameRequests);
            report.insert(QStringLiteral("clipboardTrace"), clipboardTrace);
            success = nestedBatchRename && success;
            if (batchDialog) QMetaObject::invokeMethod(batchDialog, "reject");
            success = until([&] { return batchDialog && !batchDialog->property("visible").toBool(); }) && success;
            controller->directoryModel()->clearSelection();
            QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            window.grabWindow();
            map->revealPath(nestedFolder);
            success = until([&] { return !map->moving(); }) && success;
            window.grabWindow();
            clickMapEntry(nestedFolder, Qt::NoModifier);
            window.grabWindow();
            const QRectF cameraBeforeEntering = map->screenRect(path);
            const QPointF cameraOffsetBeforeEntering = cameraBeforeEntering.center() - QPointF(map->width() / 2, map->height() / 2);
            QMouseEvent nestedDoubleClick(QEvent::MouseButtonDblClick, nestedPosition, nestedGlobal,
                                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &nestedPress);
            QCoreApplication::sendEvent(&window, &nestedDoubleClick);
            QCoreApplication::sendEvent(&window, &nestedRelease);
            const bool nestedDoubleClickOpened = until([&] { return controller->currentPath() == nestedFolder; });
            report.insert(QStringLiteral("nestedDoubleClickOpened"), nestedDoubleClickOpened);
            success = nestedDoubleClickOpened && success;
            success = until([&] { return map->currentPath() == nestedFolder && !map->moving(); }) && success;
            success = window.grabWindow().save(QStringLiteral("/tmp/folder-map-empty.png")) && success;
            const QPointF emptyPosition = map->mapToScene(QPointF(map->width() / 2, map->height() / 2));
            const QPointF emptyGlobal(window.mapToGlobal(emptyPosition.toPoint()));
            QMouseEvent emptyPress(QEvent::MouseButtonPress, emptyPosition, emptyGlobal,
                                   Qt::RightButton, Qt::RightButton, Qt::NoModifier);
            QMouseEvent emptyRelease(QEvent::MouseButtonRelease, emptyPosition, emptyGlobal,
                                     Qt::RightButton, Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&window, &emptyPress);
            QCoreApplication::sendEvent(&window, &emptyRelease);
            QObject *emptyPopup = nullptr;
            const bool emptySpaceMenu = until([&] {
                QObject *emptyMenu = panel->property("filePanelEmptyMenuItem").value<QObject *>();
                emptyPopup = emptyMenu ? emptyMenu->findChild<QObject *>(QStringLiteral("filePanelEmptyPopup")) : nullptr;
                return emptyPopup && emptyPopup->property("opened").toBool();
            }) && controller->currentPath() == nestedFolder;
            report.insert(QStringLiteral("emptySpaceMenu"), emptySpaceMenu);
            success = emptySpaceMenu && success;
            if (emptyPopup) QMetaObject::invokeMethod(emptyPopup, "close");
            success = until([&] { return emptyPopup && !emptyPopup->property("visible").toBool(); }) && success;
            controller->goBack();
            const bool backArrived = until([&] { return map->currentPath() == path && !map->moving()
                && !controller->directoryModel()->loading(); });
            window.grabWindow();
            const QRectF cameraAfterBack = map->screenRect(path);
            const bool backCameraPreserved = backArrived
                && QLineF(cameraOffsetBeforeEntering,
                    cameraAfterBack.center() - QPointF(map->width() / 2, map->height() / 2)).length() < 0.1
                && qAbs(cameraBeforeEntering.width() - cameraAfterBack.width()) < 0.1
                && qAbs(cameraBeforeEntering.height() - cameraAfterBack.height()) < 0.1;
            report.insert(QStringLiteral("backCameraPreserved"), backCameraPreserved);
            success = backCameraPreserved && success;
            bool breadcrumbFits = true;
            QJsonArray breadcrumbResults;
            for (const auto &crumb : {std::pair{"toolbarPathBar", child}, std::pair{"panelPathBar", path}}) {
                controller->openPath(nestedFolder);
                const bool nestedArrived = until([&] { return map->currentPath() == nestedFolder && !map->moving(); });
                QObject *pathBar = window.findChild<QObject *>(QLatin1String(crumb.first));
                QVariant opened;
                const bool breadcrumbOpened = pathBar && QMetaObject::invokeMethod(pathBar, "openPath", Q_RETURN_ARG(QVariant, opened),
                    Q_ARG(QVariant, crumb.second), Q_ARG(QVariant, true)) && opened.toBool();
                const bool overviewFits = until([&] {
                    const QRectF overview = map->screenRect(crumb.second);
                    return map->currentPath() == crumb.second && !map->moving() && !overview.isEmpty()
                        && overview.left() >= -0.1 && overview.top() >= -0.1
                        && overview.right() <= map->width() + 0.1 && overview.bottom() <= map->height() + 0.1
                        && QLineF(overview.center(), QPointF(map->width() / 2, map->height() / 2)).length() < 0.1;
                });
                breadcrumbFits = nestedArrived && breadcrumbOpened && overviewFits && breadcrumbFits;
                const QRectF overview = map->screenRect(crumb.second);
                breadcrumbResults.append(QJsonObject{
                    {QStringLiteral("bar"), QLatin1String(crumb.first)},
                    {QStringLiteral("found"), pathBar != nullptr},
                    {QStringLiteral("nestedArrived"), nestedArrived},
                    {QStringLiteral("opened"), opened.toBool()},
                    {QStringLiteral("overviewFits"), overviewFits},
                    {QStringLiteral("currentPath"), map->currentPath()},
                    {QStringLiteral("target"), crumb.second},
                    {QStringLiteral("moving"), map->moving()},
                    {QStringLiteral("left"), overview.left()},
                    {QStringLiteral("top"), overview.top()},
                    {QStringLiteral("width"), overview.width()},
                    {QStringLiteral("height"), overview.height()},
                    {QStringLiteral("viewportWidth"), map->width()},
                    {QStringLiteral("viewportHeight"), map->height()}
                });
            }
            report.insert(QStringLiteral("breadcrumbResults"), breadcrumbResults);
            report.insert(QStringLiteral("breadcrumbFits"), breadcrumbFits);
            success = breadcrumbFits && success;
        }
        report.insert(QStringLiteral("success"), success);
        report.insert(QStringLiteral("gpuAvailable"), gpu);
        report.insert(QStringLiteral("availabilityPassed"), availabilityPassed);
        report.insert(QStringLiteral("startupViewModesPassed"), startupViewModesPassed);
        report.insert(QStringLiteral("deviceName"), probe ? probe->deviceName() : QString{});
        report.insert(QStringLiteral("viewMode"), controller->viewMode());
        report.insert(QStringLiteral("loadedCount"), map ? map->loadedCount() : 0);
        std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
        return success ? 0 : 1;
    }
    const Dataset &photos = datasets.at(1);
    QJsonArray results;
    bool success = true;
    for (int mode = 0; mode <= 2; ++mode) {
        const QJsonObject result = runGuiScenario(
            window, *controller, photos, mode, QStringLiteral("viewport-mode"));
        results.append(result);
        success = result.value(QStringLiteral("success")).toBool() && success;
        if (result.value(QStringLiteral("success")).toBool()) {
            const QJsonObject selection = runGuiSelectionScenario(window, *controller, photos, mode);
            results.append(selection);
            success = selection.value(QStringLiteral("success")).toBool() && success;
        }
    }
    const QJsonObject replacement = runGuiScenario(
        window, *controller, photos, 1, QStringLiteral("replacement-a-b-c"),
        {datasets.at(0), datasets.at(2)});
    results.append(replacement);
    success = replacement.value(QStringLiteral("success")).toBool() && success;

    for (int mode = 0; mode <= 1; ++mode) {
        const QJsonObject result = runFolderPeekScenario(
            window, *controller, photos, mode, QStringLiteral("folder-peek-viewport"));
        results.append(result);
        success = result.value(QStringLiteral("success")).toBool() && success;
    }
    const QJsonObject warmReopen = runFolderPeekScenario(
        window, *controller, photos, 0, QStringLiteral("folder-peek-warm-reopen"));
    results.append(warmReopen);
    success = warmReopen.value(QStringLiteral("success")).toBool() && success;

    const QJsonObject peekReplacement = runFolderPeekScenario(
        window, *controller, photos, 0, QStringLiteral("folder-peek-replacement-a-b-c"),
        {datasets.at(0), datasets.at(2)});
    results.append(peekReplacement);
    success = peekReplacement.value(QStringLiteral("success")).toBool() && success;

    const QJsonObject closeDuringLoad = runFolderPeekCloseScenario(window, *controller, photos);
    results.append(closeDuringLoad);
    success = closeDuringLoad.value(QStringLiteral("success")).toBool() && success;

    report.insert(QStringLiteral("fixtureProfile"), QStringLiteral("everyday-local-v1"));
    report.insert(QStringLiteral("results"), results);
    report.insert(QStringLiteral("success"), success);
    std::puts(QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
    return success ? 0 : 1;
}
