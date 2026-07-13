#include "FolderCompareController.h"

#include "../core/ArchiveSupport.h"
#include "../core/OperationQueue.h"

#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrentRun>

FolderCompareController::FolderCompareController(QObject *parent) : QObject(parent), m_resultsModel(this)
{}
bool FolderCompareController::busy() const { return m_busy; }
QString FolderCompareController::error() const { return m_error; }
bool FolderCompareController::planReady() const { return m_planReady; }
bool FolderCompareController::executing() const { return m_executing; }
QString FolderCompareController::executionSummary() const { return m_executionSummary; }
bool FolderCompareController::executionSucceeded() const { return m_executionSucceeded; }
void FolderCompareController::setOperationQueue(OperationQueue *queue)
{
    if (m_operationQueue == queue) return;
    m_operationQueue = queue;
    if (!m_operationQueue) return;
    connect(m_operationQueue, &OperationQueue::operationFinishedDetailed, this,
            [this](OperationQueue::Type type, const QStringList &, const QString &,
                   int succeededCount, int failedCount, const QStringList &failedPaths, bool aborted) {
        if (!m_executing || type != OperationQueue::Type::Copy) return;
        m_executing = false;
        emit executingChanged();
        m_executionSucceeded = !aborted && failedCount == 0 && m_operationQueue->error().isEmpty();
        m_executionSummary = m_executionSucceeded
            ? QStringLiteral("Synchronized %1 item(s).").arg(succeededCount)
            : (aborted
                ? QStringLiteral("Cancelled • %1 completed • %2 failed").arg(succeededCount).arg(failedCount)
                : QStringLiteral("Completed with errors • %1 completed • %2 failed").arg(succeededCount).arg(failedCount));
        emit executionSummaryChanged();
        if (!m_executionSucceeded) {
            m_resultsModel.markExecutionFailures(failedPaths);
            const QString operationError = aborted
                ? QStringLiteral("Synchronization cancelled. Revalidate the remaining plan before continuing.")
                : m_operationQueue->error();
            if (m_error != operationError) { m_error = operationError; emit errorChanged(); }
            revalidatePlan();
            emit synchronizationFinished();
            return;
        }
        m_resultsModel.clear();
        if (m_planReady) { m_planReady = false; emit planChanged(); }
        emit synchronizationFinished();
    });
}
FolderCompareModel *FolderCompareController::resultsModel() { return &m_resultsModel; }
bool FolderCompareController::canCompare(const QString &leftPath, const QString &rightPath) const
{
    const auto valid = [](const QString &path) { return !path.isEmpty() && !ArchiveSupport::isArchivePath(path) && QFileInfo(QDir::fromNativeSeparators(path)).isDir(); };
    return valid(leftPath) && valid(rightPath);
}
void FolderCompareController::compare(const QString &leftPath, const QString &rightPath, bool recursive, bool includeHidden, bool compareContents)
{
    cancel();
    const quint64 generation = ++m_compareGeneration;
    if (m_cancelToken) m_cancelToken->store(true);
    if (m_busy) { m_busy = false; emit stateChanged(); }
    m_leftRoot = QFileInfo(QDir::fromNativeSeparators(leftPath)).absoluteFilePath(); m_rightRoot = QFileInfo(QDir::fromNativeSeparators(rightPath)).absoluteFilePath();
    m_resultsModel.clear(); if (m_planReady) { m_planReady = false; emit planChanged(); } if (m_error.size()) { m_error.clear(); emit errorChanged(); }
    if (!canCompare(m_leftRoot, m_rightRoot)) { m_error = QStringLiteral("Choose two local folders to compare."); emit errorChanged(); return; }
    m_cancelToken = std::make_shared<std::atomic_bool>(false);
    const auto cancelToken = m_cancelToken;
    m_busy = true; emit stateChanged();
    FolderCompareOptions options; options.recursive = recursive; options.includeHidden = includeHidden; options.compareContents = compareContents;
    auto *watcher = new QFutureWatcher<FolderCompareResult>(this);
    connect(watcher, &QFutureWatcher<FolderCompareResult>::finished, this, [this, watcher, generation] {
        const FolderCompareResult result = watcher->result();
        watcher->deleteLater();
        if (generation != m_compareGeneration) return;
        m_busy = false; emit stateChanged();
        if (!result.cancelled) m_resultsModel.setEntries(result.entries);
        if (m_error != result.error) { m_error = result.error; emit errorChanged(); }
        if (!result.cancelled) emit comparisonFinished();
    });
    watcher->setFuture(QtConcurrent::run([left = m_leftRoot, right = m_rightRoot, options, cancelToken] {
        return FolderCompareScanner::compare(left, right, options, cancelToken.get());
    }));
}
void FolderCompareController::cancel() { if (m_busy && m_cancelToken) m_cancelToken->store(true); }
void FolderCompareController::setShowEqual(bool showEqual) { m_resultsModel.setShowEqual(showEqual); }
void FolderCompareController::buildPlan(int mode) { if (m_busy || mode < 1 || mode > 5) return; m_resultsModel.buildPlan(mode); if (!m_planReady) { m_planReady = true; emit planChanged(); } }
void FolderCompareController::clearPlan() { m_resultsModel.clearPlan(); if (m_planReady) { m_planReady = false; emit planChanged(); } }
int FolderCompareController::revalidatePlan() { return m_planReady ? m_resultsModel.revalidatePlan(m_leftRoot, m_rightRoot) : 0; }
bool FolderCompareController::executePlan()
{
    if (!m_planReady || m_executing || !m_operationQueue || m_operationQueue->busy()
        || m_resultsModel.unresolvedCount() > 0 || revalidatePlan() > 0) return false;
    const QStringList sources = m_resultsModel.plannedSources();
    const QStringList destinations = m_resultsModel.plannedDestinations(m_leftRoot, m_rightRoot);
    if (sources.isEmpty() || sources.size() != destinations.size()) return false;
    if (!m_error.isEmpty()) { m_error.clear(); emit errorChanged(); }
    if (!m_executionSummary.isEmpty()) { m_executionSummary.clear(); m_executionSucceeded = false; emit executionSummaryChanged(); }
    m_executing = true;
    emit executingChanged();
    m_operationQueue->copyToExactDestinations(sources, destinations);
    return true;
}
void FolderCompareController::cancelExecution() { if (m_executing && m_operationQueue) m_operationQueue->cancel(); }
void FolderCompareController::clear() { cancel(); ++m_compareGeneration; if (m_busy) { m_busy = false; emit stateChanged(); } m_resultsModel.clear(); if (m_planReady) { m_planReady = false; emit planChanged(); } m_leftRoot.clear(); m_rightRoot.clear(); if (!m_error.isEmpty()) { m_error.clear(); emit errorChanged(); } if (!m_executionSummary.isEmpty()) { m_executionSummary.clear(); m_executionSucceeded = false; emit executionSummaryChanged(); } }
