#pragma once

#include "../core/FolderCompareScanner.h"
#include "../models/FolderCompareModel.h"

#include <QObject>

#include <memory>

class OperationQueue;

class FolderCompareController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(bool planReady READ planReady NOTIFY planChanged)
    Q_PROPERTY(bool executing READ executing NOTIFY executingChanged)
    Q_PROPERTY(QString executionSummary READ executionSummary NOTIFY executionSummaryChanged)
    Q_PROPERTY(bool executionSucceeded READ executionSucceeded NOTIFY executionSummaryChanged)
    Q_PROPERTY(FolderCompareModel *resultsModel READ resultsModel CONSTANT)
public:
    explicit FolderCompareController(QObject *parent = nullptr);
    bool busy() const; QString error() const; bool planReady() const; bool executing() const; QString executionSummary() const; bool executionSucceeded() const;
    void setOperationQueue(OperationQueue *queue);
    FolderCompareModel *resultsModel();
    Q_INVOKABLE bool canCompare(const QString &leftPath, const QString &rightPath) const;
    Q_INVOKABLE void compare(const QString &leftPath, const QString &rightPath, bool recursive = false, bool includeHidden = false, bool compareContents = false);
    Q_INVOKABLE void setShowEqual(bool showEqual);
    Q_INVOKABLE void buildPlan(int mode);
    Q_INVOKABLE void clearPlan();
    Q_INVOKABLE int revalidatePlan();
    Q_INVOKABLE bool executePlan();
    Q_INVOKABLE void cancelExecution();
    Q_INVOKABLE void cancel(); Q_INVOKABLE void clear();
signals:
    void stateChanged(); void errorChanged(); void planChanged(); void executingChanged(); void executionSummaryChanged(); void comparisonFinished(); void synchronizationFinished();
private:
    bool m_busy = false; bool m_planReady = false; bool m_executing = false; QString m_leftRoot; QString m_rightRoot; QString m_error;
    std::shared_ptr<std::atomic_bool> m_cancelToken;
    quint64 m_compareGeneration = 0;
    OperationQueue *m_operationQueue = nullptr;
    QString m_executionSummary; bool m_executionSucceeded = false;
    FolderCompareModel m_resultsModel;
};
