#pragma once

#include "../core/FolderCompareScanner.h"

#include <QAbstractListModel>
#include <QHash>

class FolderCompareModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int equalCount READ equalCount NOTIFY countChanged)
    Q_PROPERTY(int oneSidedCount READ oneSidedCount NOTIFY countChanged)
    Q_PROPERTY(int differentCount READ differentCount NOTIFY countChanged)
    Q_PROPERTY(int plannedCount READ plannedCount NOTIFY planChanged)
    Q_PROPERTY(int unresolvedCount READ unresolvedCount NOTIFY planChanged)
    Q_PROPERTY(qint64 plannedBytes READ plannedBytes NOTIFY planChanged)
    Q_PROPERTY(int changedAfterCompareCount READ changedAfterCompareCount NOTIFY planChanged)
    Q_PROPERTY(int filterMode READ filterMode NOTIFY viewChanged)
    Q_PROPERTY(int sortMode READ sortMode NOTIFY viewChanged)
public:
    enum Role { RelativePathRole = Qt::UserRole + 1, LeftPathRole, RightPathRole, StateRole, StateTextRole,
                LeftSizeRole, RightSizeRole, LeftModifiedRole, RightModifiedRole, LeftDirectoryRole, RightDirectoryRole,
                LeftSymlinkRole, RightSymlinkRole, PlannedActionRole, PlannedActionTextRole, ExecutionErrorRole };
    Q_ENUM(Role)
    explicit FolderCompareModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const;
    int equalCount() const;
    int oneSidedCount() const;
    int differentCount() const;
    int plannedCount() const;
    int unresolvedCount() const;
    qint64 plannedBytes() const;
    int changedAfterCompareCount() const;
    Q_INVOKABLE int countForState(int state) const;
    bool showEqual() const;
    int filterMode() const;
    int sortMode() const;
    Q_INVOKABLE void setShowEqual(bool showEqual);
    Q_INVOKABLE void setFilterMode(int mode);
    Q_INVOKABLE void setSortMode(int mode);
    Q_INVOKABLE void buildPlan(int mode);
    Q_INVOKABLE void clearPlan();
    Q_INVOKABLE void cyclePlannedAction(int row);
    Q_INVOKABLE void setPlannedAction(int row, int action);
    Q_INVOKABLE int revalidatePlan(const QString &leftRoot, const QString &rightRoot);
    void markExecutionFailures(const QStringList &failedPaths);
    QStringList plannedSources() const;
    QStringList plannedDestinations(const QString &leftRoot, const QString &rightRoot) const;
    bool plannedDestinationsWereAbsent() const;
    void setEntries(QList<FolderCompareEntry> entries);
    void clear();
signals:
    void countChanged();
    void planChanged();
    void viewChanged();
private:
    static bool isActionBlocked(const FolderCompareEntry &entry);
    static bool isPlannedActionAllowed(const FolderCompareEntry &entry, FolderComparePlanAction action);
    static FolderComparePlanAction nextPlannedAction(const FolderCompareEntry &entry);
    void recomputePlan();
    void rebuildVisibleEntries();
    QList<FolderCompareEntry> m_entries;
    QList<FolderCompareEntry> m_allEntries;
    bool m_showEqual = true;
    int m_filterMode = 0;
    int m_sortMode = 0;
    int m_planMode = 0;
    QHash<QString, FolderComparePlanAction> m_planOverrides;
};
