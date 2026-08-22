#include "NavigationBenchmark.h"

#include "../app/AppServices.h"
#include "../controllers/FilePanelController.h"
#include "../controllers/WorkspaceController.h"
#include "../models/DirectoryModel.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QThread>

#include <algorithm>
#include <cmath>
#include <cstdio>

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

bool parseReport(const QByteArray &bytes, bool guiReport, QJsonObject *report, QString *error)
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
        const bool timingShapeValid = guiReport
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
        QStringLiteral("thumbnailsReadyMs")
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
    Q_UNUSED(app)
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
    const QString suiteLabel = guiSuite
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
        process.setArguments({guiSuite ? QStringLiteral("--navigation-gui-benchmark")
                                       : QStringLiteral("--navigation-benchmark")});
        process.start();
        if (!process.waitForStarted(5000) || !process.waitForFinished(guiSuite ? 60000 : 30000)) {
            process.kill();
            process.waitForFinished();
            std::fprintf(stderr, "Benchmark child %d did not finish\n", index + 1);
            return 1;
        }
        QJsonObject report;
        QString error;
        if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0
            || !parseReport(process.readAllStandardOutput(), guiSuite, &report, &error)) {
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

    WorkspaceController *workspace = services.workspace();
    workspace->setSplitEnabled(false);
    workspace->setActivePanel(0);
    FilePanelController *controller = workspace->leftPanel();
    const Dataset &photos = datasets.at(1);
    QJsonArray results;
    bool success = true;
    for (int mode = 0; mode <= 2; ++mode) {
        const QJsonObject result = runGuiScenario(
            window, *controller, photos, mode, QStringLiteral("viewport-mode"));
        results.append(result);
        success = result.value(QStringLiteral("success")).toBool() && success;
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
