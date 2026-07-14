#include "FolderCompareModel.h"

#include "../core/DriveUtils.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>

#include <algorithm>

namespace {
QString stateText(FolderCompareState state)
{
    switch (state) {
    case FolderCompareState::EqualMetadata: return QStringLiteral("Equal");
    case FolderCompareState::EqualContent: return QStringLiteral("Equal content");
    case FolderCompareState::LeftOnly: return QStringLiteral("Left only");
    case FolderCompareState::RightOnly: return QStringLiteral("Right only");
    case FolderCompareState::LeftNewer: return QStringLiteral("Left newer");
    case FolderCompareState::RightNewer: return QStringLiteral("Right newer");
    case FolderCompareState::DifferentSize: return QStringLiteral("Different size");
    case FolderCompareState::DifferentContent: return QStringLiteral("Different content");
    case FolderCompareState::TypeConflict: return QStringLiteral("Type conflict");
    case FolderCompareState::LinkConflict: return QStringLiteral("Link conflict");
    case FolderCompareState::InaccessibleLeft: return QStringLiteral("Left inaccessible");
    case FolderCompareState::InaccessibleRight: return QStringLiteral("Right inaccessible");
    case FolderCompareState::ChangedAfterCompare: return QStringLiteral("Changed after compare");
    }
    return {};
}

QString plannedActionText(FolderComparePlanAction action)
{
    switch (action) {
    case FolderComparePlanAction::CopyLeftToRight: return QStringLiteral("Left → Right");
    case FolderComparePlanAction::CopyRightToLeft: return QStringLiteral("Right → Left");
    case FolderComparePlanAction::Unresolved: return QStringLiteral("Unresolved");
    case FolderComparePlanAction::None: return {};
    }
    return {};
}
}
FolderCompareModel::FolderCompareModel(QObject *parent) : QAbstractListModel(parent) {}
int FolderCompareModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_entries.size(); }
QVariant FolderCompareModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) return {};
    const auto &entry = m_entries.at(index.row());
    switch (role) {
    case RelativePathRole: return entry.relativePath; case LeftPathRole: return entry.leftPath; case RightPathRole: return entry.rightPath;
    case StateRole: return static_cast<int>(entry.state); case StateTextRole: return stateText(entry.state);
    case LeftSizeRole: return entry.leftSymlink ? QStringLiteral("Link") : (entry.leftDirectory ? QStringLiteral("Folder") : (entry.leftSize < 0 ? QString() : DriveUtils::formatSize(entry.leftSize)));
    case RightSizeRole: return entry.rightSymlink ? QStringLiteral("Link") : (entry.rightDirectory ? QStringLiteral("Folder") : (entry.rightSize < 0 ? QString() : DriveUtils::formatSize(entry.rightSize)));
    case LeftModifiedRole: return entry.leftModified.isValid() ? entry.leftModified.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
    case RightModifiedRole: return entry.rightModified.isValid() ? entry.rightModified.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString();
    case LeftDirectoryRole: return entry.leftDirectory; case RightDirectoryRole: return entry.rightDirectory;
    case LeftSymlinkRole: return entry.leftSymlink; case RightSymlinkRole: return entry.rightSymlink;
    case PlannedActionRole: return static_cast<int>(entry.plannedAction);
    case PlannedActionTextRole: return plannedActionText(entry.plannedAction);
    case ExecutionErrorRole: return entry.executionError;
    default: return {};
    }
}
QHash<int, QByteArray> FolderCompareModel::roleNames() const { return {{RelativePathRole,"relativePath"},{LeftPathRole,"leftPath"},{RightPathRole,"rightPath"},{StateRole,"state"},{StateTextRole,"stateText"},{LeftSizeRole,"leftSize"},{RightSizeRole,"rightSize"},{LeftModifiedRole,"leftModified"},{RightModifiedRole,"rightModified"},{LeftDirectoryRole,"leftDirectory"},{RightDirectoryRole,"rightDirectory"},{LeftSymlinkRole,"leftSymlink"},{RightSymlinkRole,"rightSymlink"},{PlannedActionRole,"plannedAction"},{PlannedActionTextRole,"plannedActionText"},{ExecutionErrorRole,"executionError"}}; }
int FolderCompareModel::count() const { return m_entries.size(); }
int FolderCompareModel::equalCount() const
{
    return countForState(static_cast<int>(FolderCompareState::EqualMetadata))
        + countForState(static_cast<int>(FolderCompareState::EqualContent));
}
int FolderCompareModel::oneSidedCount() const
{
    return countForState(static_cast<int>(FolderCompareState::LeftOnly))
        + countForState(static_cast<int>(FolderCompareState::RightOnly));
}
int FolderCompareModel::differentCount() const
{
    return countForState(static_cast<int>(FolderCompareState::LeftNewer))
        + countForState(static_cast<int>(FolderCompareState::RightNewer))
        + countForState(static_cast<int>(FolderCompareState::DifferentSize))
        + countForState(static_cast<int>(FolderCompareState::DifferentContent))
        + countForState(static_cast<int>(FolderCompareState::TypeConflict))
        + countForState(static_cast<int>(FolderCompareState::LinkConflict))
        + countForState(static_cast<int>(FolderCompareState::InaccessibleLeft))
        + countForState(static_cast<int>(FolderCompareState::InaccessibleRight))
        + countForState(static_cast<int>(FolderCompareState::ChangedAfterCompare));
}
int FolderCompareModel::plannedCount() const
{
    int count = 0;
    for (const auto &entry : m_allEntries) {
        if (entry.plannedAction == FolderComparePlanAction::CopyLeftToRight
            || entry.plannedAction == FolderComparePlanAction::CopyRightToLeft) ++count;
    }
    return count;
}
int FolderCompareModel::unresolvedCount() const
{
    int count = 0;
    for (const auto &entry : m_allEntries) if (entry.plannedAction == FolderComparePlanAction::Unresolved) ++count;
    return count;
}
qint64 FolderCompareModel::plannedBytes() const
{
    qint64 bytes = 0;
    for (const auto &entry : m_allEntries) {
        if (entry.plannedAction == FolderComparePlanAction::CopyLeftToRight && !entry.leftDirectory) bytes += qMax<qint64>(0, entry.leftSize);
        else if (entry.plannedAction == FolderComparePlanAction::CopyRightToLeft && !entry.rightDirectory) bytes += qMax<qint64>(0, entry.rightSize);
    }
    return bytes;
}
int FolderCompareModel::changedAfterCompareCount() const
{
    return countForState(static_cast<int>(FolderCompareState::ChangedAfterCompare));
}
int FolderCompareModel::countForState(int state) const
{
    int count = 0;
    for (const auto &entry : m_allEntries) if (static_cast<int>(entry.state) == state) ++count;
    return count;
}
bool FolderCompareModel::showEqual() const { return m_showEqual; }
int FolderCompareModel::filterMode() const { return m_filterMode; }
int FolderCompareModel::sortMode() const { return m_sortMode; }
void FolderCompareModel::setShowEqual(bool showEqual) { if (m_showEqual == showEqual) return; m_showEqual = showEqual; rebuildVisibleEntries(); }
void FolderCompareModel::setFilterMode(int mode) { if (mode < 0 || mode > 5 || m_filterMode == mode) return; m_filterMode = mode; rebuildVisibleEntries(); emit viewChanged(); }
void FolderCompareModel::setSortMode(int mode) { if (mode < 0 || mode > 3 || m_sortMode == mode) return; m_sortMode = mode; rebuildVisibleEntries(); emit viewChanged(); }
void FolderCompareModel::buildPlan(int mode)
{
    m_planMode = mode;
    m_planOverrides.clear();
    recomputePlan();
}
void FolderCompareModel::recomputePlan()
{
    for (auto &entry : m_allEntries) {
        entry.plannedAction = FolderComparePlanAction::None;
        if (m_planMode == 4) {
            if (entry.state == FolderCompareState::RightOnly) entry.plannedAction = entry.rightSymlink
                ? FolderComparePlanAction::Unresolved : FolderComparePlanAction::CopyRightToLeft;
            continue;
        }
        if (m_planMode == 5) {
            if (entry.state == FolderCompareState::LeftOnly) entry.plannedAction = entry.leftSymlink
                ? FolderComparePlanAction::Unresolved : FolderComparePlanAction::CopyLeftToRight;
            continue;
        }
        if ((entry.leftSymlink || entry.rightSymlink)
            && entry.state != FolderCompareState::EqualMetadata
            && entry.state != FolderCompareState::EqualContent) {
            entry.plannedAction = FolderComparePlanAction::Unresolved;
            continue;
        }
        const bool blocked = entry.state == FolderCompareState::TypeConflict
            || entry.state == FolderCompareState::LinkConflict
            || entry.state == FolderCompareState::InaccessibleLeft
            || entry.state == FolderCompareState::InaccessibleRight;
        if (blocked) { entry.plannedAction = FolderComparePlanAction::Unresolved; continue; }
        if (entry.state == FolderCompareState::EqualMetadata || entry.state == FolderCompareState::EqualContent) continue;
        if (m_planMode == 1) {
            if (!entry.rightPath.isEmpty()) entry.plannedAction = FolderComparePlanAction::CopyRightToLeft;
        } else if (m_planMode == 2) {
            if (!entry.leftPath.isEmpty()) entry.plannedAction = FolderComparePlanAction::CopyLeftToRight;
        } else if (m_planMode == 3) {
            if (entry.state == FolderCompareState::LeftOnly || entry.state == FolderCompareState::LeftNewer) entry.plannedAction = FolderComparePlanAction::CopyLeftToRight;
            else if (entry.state == FolderCompareState::RightOnly || entry.state == FolderCompareState::RightNewer) entry.plannedAction = FolderComparePlanAction::CopyRightToLeft;
            else entry.plannedAction = FolderComparePlanAction::Unresolved;
        }
    }
    for (auto &entry : m_allEntries) {
        const auto overrideIt = m_planOverrides.constFind(entry.relativePath);
        if (overrideIt != m_planOverrides.cend()) entry.plannedAction = overrideIt.value();
    }
    for (int i = 0; i < m_allEntries.size(); ++i) {
        const auto &parent = m_allEntries.at(i);
        const bool plannedDirectory = (parent.leftDirectory || parent.rightDirectory)
            && (parent.plannedAction == FolderComparePlanAction::CopyLeftToRight
                || parent.plannedAction == FolderComparePlanAction::CopyRightToLeft);
        if (!plannedDirectory) continue;
        const QString prefix = parent.relativePath + QLatin1Char('/');
        for (int childIndex = i + 1; childIndex < m_allEntries.size(); ++childIndex) {
            auto &child = m_allEntries[childIndex];
            if (!child.relativePath.startsWith(prefix)) continue;
            if (child.plannedAction == parent.plannedAction) child.plannedAction = FolderComparePlanAction::None;
        }
    }
    rebuildVisibleEntries();
    emit planChanged();
}
void FolderCompareModel::clearPlan()
{
    m_planMode = 0;
    m_planOverrides.clear();
    for (auto &entry : m_allEntries) entry.plannedAction = FolderComparePlanAction::None;
    rebuildVisibleEntries();
    emit planChanged();
}
bool FolderCompareModel::isActionBlocked(const FolderCompareEntry &entry)
{
    return entry.state == FolderCompareState::TypeConflict
        || entry.state == FolderCompareState::LinkConflict
        || entry.state == FolderCompareState::InaccessibleLeft
        || entry.state == FolderCompareState::InaccessibleRight
        || entry.state == FolderCompareState::ChangedAfterCompare
        || ((entry.leftSymlink || entry.rightSymlink)
            && entry.state != FolderCompareState::EqualMetadata
            && entry.state != FolderCompareState::EqualContent);
}
bool FolderCompareModel::isPlannedActionAllowed(const FolderCompareEntry &entry, FolderComparePlanAction action)
{
    if (action == FolderComparePlanAction::None || action == FolderComparePlanAction::Unresolved) return true;
    if (isActionBlocked(entry)) return false;
    if (action == FolderComparePlanAction::CopyLeftToRight) return !entry.leftPath.isEmpty();
    if (action == FolderComparePlanAction::CopyRightToLeft) return !entry.rightPath.isEmpty();
    return false;
}
FolderComparePlanAction FolderCompareModel::nextPlannedAction(const FolderCompareEntry &entry)
{
    if (isActionBlocked(entry)) {
        return entry.plannedAction == FolderComparePlanAction::Unresolved
            ? FolderComparePlanAction::None
            : FolderComparePlanAction::Unresolved;
    }
    if (entry.plannedAction == FolderComparePlanAction::CopyLeftToRight) {
        return entry.rightPath.isEmpty()
            ? FolderComparePlanAction::None
            : FolderComparePlanAction::CopyRightToLeft;
    }
    if (entry.plannedAction == FolderComparePlanAction::CopyRightToLeft) return FolderComparePlanAction::None;
    if (!entry.leftPath.isEmpty()) return FolderComparePlanAction::CopyLeftToRight;
    if (!entry.rightPath.isEmpty()) return FolderComparePlanAction::CopyRightToLeft;
    return FolderComparePlanAction::None;
}
void FolderCompareModel::cyclePlannedAction(int row)
{
    if (row < 0 || row >= m_entries.size()) return;
    const auto &visibleEntry = m_entries.at(row);
    const auto nextAction = nextPlannedAction(visibleEntry);
    if (!isPlannedActionAllowed(visibleEntry, nextAction)) return;
    m_planOverrides.insert(visibleEntry.relativePath, nextAction);
    recomputePlan();
}
void FolderCompareModel::setPlannedAction(int row, int action)
{
    if (row < 0 || row >= m_entries.size() || action < 0 || action > static_cast<int>(FolderComparePlanAction::Unresolved)) return;
    const auto &visibleEntry = m_entries.at(row);
    const auto requestedAction = static_cast<FolderComparePlanAction>(action);
    if (!isPlannedActionAllowed(visibleEntry, requestedAction)) return;
    m_planOverrides.insert(visibleEntry.relativePath, requestedAction);
    recomputePlan();
}
int FolderCompareModel::revalidatePlan(const QString &leftRoot, const QString &rightRoot)
{
    const auto unchanged = [](const QString &path, qint64 expectedSize, const QDateTime &expectedModified, bool expectedDirectory) {
        const QFileInfo info(path);
        if (!info.exists() || info.isDir() != expectedDirectory) return false;
        if (!expectedDirectory && info.size() != expectedSize) return false;
        return !expectedModified.isValid()
            || qAbs(info.lastModified().toUTC().msecsTo(expectedModified)) <= 1000;
    };
    int changed = 0;
    for (auto &entry : m_allEntries) {
        if (entry.plannedAction != FolderComparePlanAction::CopyLeftToRight
            && entry.plannedAction != FolderComparePlanAction::CopyRightToLeft) continue;
        const bool leftToRight = entry.plannedAction == FolderComparePlanAction::CopyLeftToRight;
        const QString sourcePath = leftToRight ? entry.leftPath : entry.rightPath;
        const QString destinationSnapshotPath = leftToRight ? entry.rightPath : entry.leftPath;
        const QString destinationRoot = leftToRight ? rightRoot : leftRoot;
        const qint64 sourceSize = leftToRight ? entry.leftSize : entry.rightSize;
        const qint64 destinationSize = leftToRight ? entry.rightSize : entry.leftSize;
        const QDateTime sourceModified = leftToRight ? entry.leftModified : entry.rightModified;
        const QDateTime destinationModified = leftToRight ? entry.rightModified : entry.leftModified;
        const bool sourceDirectory = leftToRight ? entry.leftDirectory : entry.rightDirectory;
        const bool destinationDirectory = leftToRight ? entry.rightDirectory : entry.leftDirectory;
        const QString expectedDestinationPath = destinationSnapshotPath.isEmpty()
            ? QDir(destinationRoot).filePath(entry.relativePath)
            : destinationSnapshotPath;
        const bool sourceChanged = !unchanged(sourcePath, sourceSize, sourceModified, sourceDirectory);
        const bool destinationChanged = destinationSnapshotPath.isEmpty()
            ? QFileInfo::exists(expectedDestinationPath)
            : !unchanged(expectedDestinationPath, destinationSize, destinationModified, destinationDirectory);
        if (sourceChanged || destinationChanged) {
            entry.state = FolderCompareState::ChangedAfterCompare;
            entry.plannedAction = FolderComparePlanAction::Unresolved;
            ++changed;
        }
    }
    rebuildVisibleEntries();
    emit planChanged();
    return changed;
}
void FolderCompareModel::markExecutionFailures(const QStringList &failedPaths)
{
    const QSet<QString> failures(failedPaths.cbegin(), failedPaths.cend());
    for (auto &entry : m_allEntries) {
        entry.executionError = failures.contains(entry.leftPath) || failures.contains(entry.rightPath)
            ? QStringLiteral("Copy failed") : QString();
    }
    rebuildVisibleEntries();
}
QStringList FolderCompareModel::plannedSources() const
{
    QStringList sources;
    for (const auto &entry : m_allEntries) {
        if (entry.plannedAction == FolderComparePlanAction::CopyLeftToRight) sources.append(entry.leftPath);
        else if (entry.plannedAction == FolderComparePlanAction::CopyRightToLeft) sources.append(entry.rightPath);
    }
    return sources;
}
QStringList FolderCompareModel::plannedDestinations(const QString &leftRoot, const QString &rightRoot) const
{
    QStringList destinations;
    for (const auto &entry : m_allEntries) {
        if (entry.plannedAction == FolderComparePlanAction::CopyLeftToRight) destinations.append(QDir(rightRoot).filePath(entry.relativePath));
        else if (entry.plannedAction == FolderComparePlanAction::CopyRightToLeft) destinations.append(QDir(leftRoot).filePath(entry.relativePath));
    }
    return destinations;
}
bool FolderCompareModel::plannedDestinationsWereAbsent() const
{
    for (const auto &entry : m_allEntries) {
        if (entry.plannedAction == FolderComparePlanAction::CopyLeftToRight && !entry.rightPath.isEmpty()) return false;
        if (entry.plannedAction == FolderComparePlanAction::CopyRightToLeft && !entry.leftPath.isEmpty()) return false;
    }
    return true;
}
void FolderCompareModel::setEntries(QList<FolderCompareEntry> entries) { m_planMode = 0; m_planOverrides.clear(); m_allEntries = std::move(entries); rebuildVisibleEntries(); }
void FolderCompareModel::clear() { m_planMode = 0; m_planOverrides.clear(); if (m_allEntries.isEmpty() && m_entries.isEmpty()) return; m_allEntries.clear(); rebuildVisibleEntries(); }
void FolderCompareModel::rebuildVisibleEntries()
{
    beginResetModel(); m_entries.clear();
    for (const auto &entry : m_allEntries) {
        const bool equal = entry.state == FolderCompareState::EqualMetadata || entry.state == FolderCompareState::EqualContent;
        const bool oneSided = entry.state == FolderCompareState::LeftOnly || entry.state == FolderCompareState::RightOnly;
        const bool conflict = entry.state == FolderCompareState::TypeConflict
            || entry.state == FolderCompareState::LinkConflict
            || entry.state == FolderCompareState::InaccessibleLeft
            || entry.state == FolderCompareState::InaccessibleRight
            || entry.state == FolderCompareState::ChangedAfterCompare;
        const bool matchesFilter = m_filterMode == 0
            || (m_filterMode == 1 && equal)
            || (m_filterMode == 2 && oneSided)
            || (m_filterMode == 3 && !equal && !oneSided && !conflict)
            || (m_filterMode == 4 && conflict)
            || (m_filterMode == 5 && (oneSided || (!equal && !conflict)));
        if (matchesFilter && (m_showEqual || !equal)) m_entries.append(entry);
    }
    std::sort(m_entries.begin(), m_entries.end(), [this](const FolderCompareEntry &a, const FolderCompareEntry &b) {
        if (m_sortMode == 1 && a.state != b.state) return a.state < b.state;
        if (m_sortMode == 2 && a.leftModified != b.leftModified) return a.leftModified > b.leftModified;
        if (m_sortMode == 3 && a.rightModified != b.rightModified) return a.rightModified > b.rightModified;
        return QString::compare(a.relativePath, b.relativePath, Qt::CaseSensitive) < 0;
    });
    endResetModel(); emit countChanged();
}
