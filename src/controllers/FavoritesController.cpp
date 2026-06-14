#include "FavoritesController.h"

#include "../core/ArchiveSupport.h"
#include "../core/IsoMountManager.h"
#include "../core/TerminalLauncher.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QUrl>

namespace {
constexpr qsizetype MaxVisibleFrequentEntries = 5;

bool isArchivePath(const QString &path)
{
    return ArchiveSupport::isArchivePath(path);
}

bool canPinPath(const QString &path)
{
    const QString normalized = path.trimmed();
    return !normalized.isEmpty() && !ArchiveSupport::isArchivePath(normalized);
}
}

FavoritesController::FavoritesController(QObject *parent)
    : QObject(parent)
{
    refreshModel();
}

FavoritesModel *FavoritesController::model()
{
    return &m_model;
}

FavoritesModel *FavoritesController::pinnedModel()
{
    return &m_pinnedModel;
}

FavoritesModel *FavoritesController::frequentModel()
{
    return &m_frequentModel;
}

int FavoritesController::pinnedCount() const
{
    return int(m_store.pinnedEntries().size());
}

int FavoritesController::frequentCount() const
{
    int count = 0;
    for (const FavoriteUsageEntry &entry : m_store.usageEntries()) {
        if (!m_store.isPinned(entry.targetPath)
            && !isArchivePath(entry.targetPath)
            && !(m_isoMountManager && m_isoMountManager->isInsideManagedMount(entry.targetPath))) {
            ++count;
            if (count >= MaxVisibleFrequentEntries) {
                break;
            }
        }
    }
    return count;
}

int FavoritesController::tagCount() const
{
    QSet<QString> tags;
    for (const FavoritePinnedEntry &entry : m_store.pinnedEntries()) {
        for (const QString &tag : entry.tags) {
            tags.insert(tag.toCaseFolded());
        }
    }
    return tags.size();
}

bool FavoritesController::pinPath(const QString &path)
{
    if (!canPinPath(path)) {
        return false;
    }
    const bool changed = m_store.pinPath(path);
    if (changed) {
        refreshModel();
    }
    return changed;
}

bool FavoritesController::unpinPath(const QString &path)
{
    const bool changed = m_store.unpinPath(path);
    if (changed) {
        refreshModel();
    }
    return changed;
}

bool FavoritesController::movePinnedUp(const QString &path)
{
    const bool changed = m_store.movePinnedPath(path, -1);
    if (changed) {
        refreshModel();
    }
    return changed;
}

bool FavoritesController::movePinnedDown(const QString &path)
{
    const bool changed = m_store.movePinnedPath(path, 1);
    if (changed) {
        refreshModel();
    }
    return changed;
}

bool FavoritesController::setPinnedLabel(const QString &path, const QString &label)
{
    const bool changed = m_store.setPinnedLabel(path, label);
    if (changed) {
        refreshModel();
    }
    return changed;
}

bool FavoritesController::setPinnedTags(const QString &path, const QStringList &tags)
{
    const bool changed = m_store.setPinnedTags(path, tags);
    if (changed) {
        refreshModel();
    }
    return changed;
}

bool FavoritesController::togglePinned(const QString &path)
{
    if (!isPinned(path) && !canPinPath(path)) {
        return false;
    }
    return isPinned(path) ? unpinPath(path) : pinPath(path);
}

bool FavoritesController::isPinned(const QString &path) const
{
    return m_store.isPinned(path);
}

int FavoritesController::pinPaths(const QStringList &paths)
{
    int changed = 0;
    for (const QString &path : paths) {
        if (!canPinPath(path)) {
            continue;
        }
        if (m_store.pinPath(path)) {
            ++changed;
        }
    }
    if (changed > 0) {
        refreshModel();
    }
    return changed;
}

int FavoritesController::unpinPaths(const QStringList &paths)
{
    int changed = 0;
    for (const QString &path : paths) {
        if (m_store.unpinPath(path)) {
            ++changed;
        }
    }
    if (changed > 0) {
        refreshModel();
    }
    return changed;
}

bool FavoritesController::forgetUsagePath(const QString &path)
{
    const bool changed = m_store.forgetUsagePath(path);
    if (changed) {
        refreshModel();
    }
    return changed;
}

bool FavoritesController::clearFrequent()
{
    const bool changed = m_store.clearUsage();
    if (changed) {
        refreshModel();
    }
    return changed;
}

QStringList FavoritesController::tagsForPath(const QString &path) const
{
    return m_store.tagsForPath(path);
}

void FavoritesController::recordVisit(const QString &path)
{
    const QString normalized = path.trimmed();
    if (normalized.isEmpty()
        || normalized.startsWith(QStringLiteral("devices://"), Qt::CaseInsensitive)
        || normalized.startsWith(QStringLiteral("favorites://"), Qt::CaseInsensitive)
        || ArchiveSupport::isArchivePath(normalized)
        || (m_isoMountManager && m_isoMountManager->isInsideManagedMount(normalized))) {
        return;
    }

    if (m_store.recordVisit(normalized)) {
        refreshModel();
    }
}

QString FavoritesController::targetPathForItem(const QString &id) const
{
    for (const FavoritePinnedEntry &entry : m_store.pinnedEntries()) {
        if (entry.id == id) {
            return entry.targetPath;
        }
    }
    return {};
}

bool FavoritesController::openItem(const QString &id)
{
    const QString target = targetPathForItem(id);
    if (target.isEmpty()) {
        return false;
    }
    emit openPathRequested(target);
    return true;
}

bool FavoritesController::openPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    emit openPathRequested(path);
    return true;
}

bool FavoritesController::revealPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    if (!info.exists()) {
        return false;
    }

#if defined(Q_OS_WIN)
    const QString nativePath = QDir::toNativeSeparators(info.absoluteFilePath());
    const QString arg = info.isDir()
        ? nativePath
        : QStringLiteral("/select,\"%1\"").arg(nativePath);
    return QProcess::startDetached(QStringLiteral("explorer.exe"), {arg});
#elif defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), info.absoluteFilePath()});
#else
    const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
}

bool FavoritesController::openTerminalAtPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        return false;
    }

    return TerminalLauncher::openTerminalAt(folder);
}

void FavoritesController::setIsoMountManager(IsoMountManager *manager)
{
    m_isoMountManager = manager;
}

void FavoritesController::refreshModel()
{
    QList<FavoriteUsageEntry> frequentEntries;
    for (const FavoriteUsageEntry &entry : m_store.usageEntries()) {
        if (frequentEntries.size() >= MaxVisibleFrequentEntries) {
            break;
        }
        if (m_store.isPinned(entry.targetPath)
            || isArchivePath(entry.targetPath)
            || (m_isoMountManager && m_isoMountManager->isInsideManagedMount(entry.targetPath))) {
            continue;
        }
        frequentEntries.append(entry);
    }

    const QList<FavoritePinnedEntry> pinnedEntries = m_store.pinnedEntries();
    m_model.setEntries(pinnedEntries, frequentEntries);
    m_pinnedModel.setEntries(pinnedEntries, {});
    m_frequentModel.setEntries({}, frequentEntries);
    emit countsChanged();
}
