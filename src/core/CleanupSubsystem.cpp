#include "CleanupSubsystem.h"

#include <QtConcurrent>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QTimer>
#include <QUuid>
#include <QDebug>
#include <QVariantMap>

#include <algorithm>

namespace {
constexpr int kCleanupSchemaVersion = 1;
constexpr int kStartupCleanupDelayMs = 3000;
constexpr qsizetype kMaxRetainedDeletedLeases = 16;

bool cleanupTraceEnabled()
{
    return qEnvironmentVariableIsSet("FM_CLEANUP_TRACE");
}

QString normalizedAbsolutePath(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return {};
    }
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QJsonObject leaseToJson(const CleanupLease &lease)
{
    QJsonObject object;
    object.insert(QStringLiteral("leaseId"), lease.leaseId);
    object.insert(QStringLiteral("kind"), cleanupArtifactKindToString(lease.kind));
    object.insert(QStringLiteral("state"), cleanupLeaseStateToString(lease.state));
    object.insert(QStringLiteral("createdAt"), lease.createdAt.toUTC().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("touchedAt"), lease.touchedAt.toUTC().toString(Qt::ISODateWithMs));
    object.insert(QStringLiteral("creatingPid"), QString::number(lease.creatingPid));
    object.insert(QStringLiteral("stagingRoot"), lease.stagingRoot);
    QJsonArray paths;
    for (const QString &path : lease.artifactPaths) {
        paths.append(path);
    }
    object.insert(QStringLiteral("artifactPaths"), paths);
    object.insert(QStringLiteral("deletionSafetyRoot"), lease.deletionSafetyRoot);
    object.insert(QStringLiteral("allowRecursive"), lease.allowRecursive);
    object.insert(QStringLiteral("ownerMarkerPath"), lease.ownerMarkerPath);
    return object;
}

CleanupArtifactKind kindFromString(const QString &kind)
{
    if (kind == QLatin1String("ProviderTransfer")) return CleanupArtifactKind::ProviderTransfer;
    if (kind == QLatin1String("ArchiveExtract")) return CleanupArtifactKind::ArchiveExtract;
    if (kind == QLatin1String("ArchiveBrowse")) return CleanupArtifactKind::ArchiveBrowse;
    if (kind == QLatin1String("RemotePreview")) return CleanupArtifactKind::RemotePreview;
    if (kind == QLatin1String("ThumbnailAdapter")) return CleanupArtifactKind::ThumbnailAdapter;
    if (kind == QLatin1String("YakuakeSession")) return CleanupArtifactKind::YakuakeSession;
    return CleanupArtifactKind::PartFile;
}

CleanupLeaseState stateFromString(const QString &state)
{
    if (state == QLatin1String("FinalizePending")) return CleanupLeaseState::FinalizePending;
    if (state == QLatin1String("DeletePending")) return CleanupLeaseState::DeletePending;
    if (state == QLatin1String("Deleted")) return CleanupLeaseState::Deleted;
    if (state == QLatin1String("Failed")) return CleanupLeaseState::Failed;
    return CleanupLeaseState::Active;
}

CleanupLease leaseFromJson(const QJsonObject &object)
{
    CleanupLease lease;
    lease.leaseId = object.value(QStringLiteral("leaseId")).toString();
    lease.kind = kindFromString(object.value(QStringLiteral("kind")).toString());
    lease.state = stateFromString(object.value(QStringLiteral("state")).toString());
    lease.createdAt = QDateTime::fromString(object.value(QStringLiteral("createdAt")).toString(), Qt::ISODateWithMs);
    lease.touchedAt = QDateTime::fromString(object.value(QStringLiteral("touchedAt")).toString(), Qt::ISODateWithMs);
    lease.creatingPid = object.value(QStringLiteral("creatingPid")).toString().toLongLong();
    lease.stagingRoot = object.value(QStringLiteral("stagingRoot")).toString();
    const QJsonArray paths = object.value(QStringLiteral("artifactPaths")).toArray();
    for (const QJsonValue &path : paths) {
        lease.artifactPaths.append(path.toString());
    }
    lease.deletionSafetyRoot = object.value(QStringLiteral("deletionSafetyRoot")).toString();
    lease.allowRecursive = object.value(QStringLiteral("allowRecursive")).toBool(false);
    lease.ownerMarkerPath = object.value(QStringLiteral("ownerMarkerPath")).toString();
    return lease;
}

bool isLocalCandidate(const QString &path)
{
    return !path.trimmed().isEmpty()
        && !path.contains(QStringLiteral("://"))
        && QFileInfo(path).isAbsolute();
}

QString candidateParentFromPath(const QString &path)
{
    if (!isLocalCandidate(path)) {
        return {};
    }
    const QFileInfo info(path);
    if (info.exists() && info.isDir()) {
        return normalizedAbsolutePath(path);
    }
    return normalizedAbsolutePath(info.absolutePath());
}
}

QString cleanupArtifactKindToString(CleanupArtifactKind kind)
{
    switch (kind) {
    case CleanupArtifactKind::PartFile: return QStringLiteral("PartFile");
    case CleanupArtifactKind::ProviderTransfer: return QStringLiteral("ProviderTransfer");
    case CleanupArtifactKind::ArchiveExtract: return QStringLiteral("ArchiveExtract");
    case CleanupArtifactKind::ArchiveBrowse: return QStringLiteral("ArchiveBrowse");
    case CleanupArtifactKind::RemotePreview: return QStringLiteral("RemotePreview");
    case CleanupArtifactKind::ThumbnailAdapter: return QStringLiteral("ThumbnailAdapter");
    case CleanupArtifactKind::YakuakeSession: return QStringLiteral("YakuakeSession");
    }
    return QStringLiteral("PartFile");
}

QString cleanupLeaseStateToString(CleanupLeaseState state)
{
    switch (state) {
    case CleanupLeaseState::Active: return QStringLiteral("Active");
    case CleanupLeaseState::FinalizePending: return QStringLiteral("FinalizePending");
    case CleanupLeaseState::DeletePending: return QStringLiteral("DeletePending");
    case CleanupLeaseState::Deleted: return QStringLiteral("Deleted");
    case CleanupLeaseState::Failed: return QStringLiteral("Failed");
    }
    return QStringLiteral("Active");
}

namespace StagingLocationPolicy {
QString defaultCleanupRoot()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if (!base.isEmpty()) {
            base = QDir(base).filePath(QStringLiteral("FMQml"));
        }
    }
    if (base.isEmpty()) {
        return {};
    }
    return QDir::fromNativeSeparators(QDir(base).filePath(QStringLiteral("fm-cleanup")));
}

bool isStagingParentUsable(const QString &parentPath)
{
    const QString normalized = normalizedAbsolutePath(parentPath);
    if (normalized.isEmpty()) {
        return false;
    }
    if (!QDir().mkpath(normalized)) {
        return false;
    }
    QTemporaryFile probe(QDir(normalized).filePath(QStringLiteral(".fm-cleanup-probe-XXXXXX")));
    probe.setAutoRemove(true);
    return probe.open();
}

QString resolveStagingParentDirectory(const QString &candidateParent,
                                      const QString &sourcePath,
                                      const QString &callerProvided,
                                      bool allowDefault)
{
    const QString normalizedCandidate = normalizedAbsolutePath(candidateParent);
    if (!normalizedCandidate.isEmpty() && isStagingParentUsable(normalizedCandidate)) {
        return normalizedCandidate;
    }

    const QString sourceParent = candidateParentFromPath(sourcePath);
    if (!sourceParent.isEmpty() && isStagingParentUsable(sourceParent)) {
        return sourceParent;
    }

    const QString callerParent = normalizedAbsolutePath(callerProvided);
    if (!callerParent.isEmpty() && isStagingParentUsable(callerParent)) {
        return callerParent;
    }

    if (allowDefault) {
        const QString fallback = defaultCleanupRoot();
        if (!fallback.isEmpty() && isStagingParentUsable(fallback)) {
            return normalizedAbsolutePath(fallback);
        }
    }

    return {};
}

QString resolveStagingParent(const QString &destinationPath,
                             const QString &sourcePath,
                             const QString &callerProvided,
                             bool allowDefault)
{
    return resolveStagingParentDirectory(candidateParentFromPath(destinationPath),
                                         sourcePath,
                                         callerProvided,
                                         allowDefault);
}
}

CleanupSubsystem &CleanupSubsystem::instance()
{
    static CleanupSubsystem subsystem;
    return subsystem;
}

CleanupSubsystem::CleanupSubsystem(QObject *parent)
    : QObject(parent)
    , m_pid(QCoreApplication::applicationPid())
{
    QString cacheRoot = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (cacheRoot.isEmpty()) {
        cacheRoot = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if (!cacheRoot.isEmpty()) {
            cacheRoot = QDir(cacheRoot).filePath(QStringLiteral("FMQml"));
        }
    }
    if (cacheRoot.isEmpty()) {
        cacheRoot = StagingLocationPolicy::defaultCleanupRoot();
    }
    if (!cacheRoot.isEmpty()) {
        QDir().mkpath(cacheRoot);
        m_registryPath = QDir(cacheRoot).filePath(QStringLiteral("fm-cleanup-registry.json"));
    }
    loadRegistry();
}

QString CleanupSubsystem::allocateStagingDirectory(CleanupArtifactKind kind,
                                                   const QString &preferredParent,
                                                   const QString &operationId,
                                                   QString *leaseId)
{
    if (leaseId) {
        leaseId->clear();
    }
    const QString parent = StagingLocationPolicy::resolveStagingParentDirectory(preferredParent, {}, {}, true);
    if (parent.isEmpty()) {
        trace(QStringLiteral("allocate failed: no staging parent"));
        return {};
    }

    const QString safeOperationId = operationId.trimmed().isEmpty()
        ? QUuid::createUuid().toString(QUuid::WithoutBraces)
        : operationId;
    const QString stagingPath = QDir(parent).filePath(QStringLiteral(".fm-tmp/%1").arg(safeOperationId));
    if (!QDir().mkpath(stagingPath)) {
        trace(QStringLiteral("allocate failed: cannot create %1").arg(stagingPath));
        return {};
    }

    const QString lid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (!writeOwnerMarker(stagingPath, lid, kind)) {
        QDir(stagingPath).removeRecursively();
        return {};
    }

    CleanupLease lease;
    lease.leaseId = lid;
    lease.kind = kind;
    lease.state = CleanupLeaseState::Active;
    lease.createdAt = QDateTime::currentDateTimeUtc();
    lease.touchedAt = lease.createdAt;
    lease.creatingPid = m_pid;
    lease.stagingRoot = normalizedAbsolutePath(stagingPath);
    lease.artifactPaths = {lease.stagingRoot};
    lease.deletionSafetyRoot = parent;
    lease.allowRecursive = true;
    lease.ownerMarkerPath = QDir(lease.stagingRoot).filePath(QStringLiteral(".fm-cleanup-owner.json"));

    {
        QMutexLocker locker(&m_mutex);
        m_leases.append(lease);
        if (leaseId) {
            *leaseId = lid;
        }
    }
    persistRegistry();
    trace(QStringLiteral("allocate kind=%1 leaseId=%2 path=%3").arg(cleanupArtifactKindToString(kind), lid, lease.stagingRoot));
    return lease.stagingRoot;
}

QString CleanupSubsystem::registerArtifact(CleanupArtifactKind kind,
                                           const QString &artifactPath,
                                           const QString &safetyRoot,
                                           bool allowRecursive,
                                           QString *leaseId)
{
    if (leaseId) {
        leaseId->clear();
    }
    const QString artifact = normalizedAbsolutePath(artifactPath);
    const QString root = normalizedAbsolutePath(safetyRoot);
    if (artifact.isEmpty() || root.isEmpty() || !isInsideSafetyRoot(artifact, root) || isForbiddenRoot(artifact)) {
        trace(QStringLiteral("register rejected path=%1 root=%2").arg(artifact, root));
        return {};
    }

    const QString lid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString marker;
    QString stagingRoot;
    if (allowRecursive) {
        QFileInfo info(artifact);
        if (!info.exists() || !info.isDir()) {
            trace(QStringLiteral("register recursive rejected: not a directory %1").arg(artifact));
            return {};
        }
        if (!writeOwnerMarker(artifact, lid, kind)) {
            trace(QStringLiteral("register recursive rejected: cannot write marker %1").arg(artifact));
            return {};
        }
        marker = QDir(artifact).filePath(QStringLiteral(".fm-cleanup-owner.json"));
        stagingRoot = artifact;
    }

    CleanupLease lease;
    lease.leaseId = lid;
    lease.kind = kind;
    lease.state = CleanupLeaseState::Active;
    lease.createdAt = QDateTime::currentDateTimeUtc();
    lease.touchedAt = lease.createdAt;
    lease.creatingPid = m_pid;
    lease.stagingRoot = stagingRoot;
    lease.artifactPaths = {artifact};
    lease.deletionSafetyRoot = root;
    lease.allowRecursive = allowRecursive;
    lease.ownerMarkerPath = marker;

    {
        QMutexLocker locker(&m_mutex);
        m_leases.append(lease);
        if (leaseId) {
            *leaseId = lid;
        }
    }
    persistRegistry();
    trace(QStringLiteral("register kind=%1 leaseId=%2 path=%3").arg(cleanupArtifactKindToString(kind), lid, artifact));
    return lid;
}

void CleanupSubsystem::scheduleDelete(const QString &leaseId)
{
    CleanupLease lease;
    {
        QMutexLocker locker(&m_mutex);
        for (CleanupLease &candidate : m_leases) {
            if (candidate.leaseId == leaseId) {
                candidate.state = CleanupLeaseState::DeletePending;
                candidate.touchedAt = QDateTime::currentDateTimeUtc();
                lease = candidate;
                break;
            }
        }
    }
    if (lease.leaseId.isEmpty()) {
        return;
    }
    persistRegistry();
    trace(QStringLiteral("schedule-delete leaseId=%1").arg(leaseId));
    deleteArtifactAsync(lease);
}

void CleanupSubsystem::scheduleDeleteOnFailure(const QString &leaseId)
{
    scheduleDelete(leaseId);
}

void CleanupSubsystem::completeWithoutDelete(const QString &leaseId)
{
    if (leaseId.isEmpty()) {
        return;
    }
    updateLeaseState(leaseId, CleanupLeaseState::Deleted);
    persistRegistry();
    trace(QStringLiteral("complete leaseId=%1").arg(leaseId));
}

void CleanupSubsystem::scheduleStartupCleanup()
{
    QTimer::singleShot(kStartupCleanupDelayMs, this, [this]() {
        QtConcurrent::run([this]() {
            loadRegistry();
            QList<CleanupLease> staleCandidates;
            {
                QMutexLocker locker(&m_mutex);
                for (CleanupLease &lease : m_leases) {
                    if (lease.creatingPid != m_pid
                        && (lease.state == CleanupLeaseState::Active
                            || lease.state == CleanupLeaseState::DeletePending
                            || lease.state == CleanupLeaseState::Failed)) {
                        lease.state = CleanupLeaseState::DeletePending;
                        lease.touchedAt = QDateTime::currentDateTimeUtc();
                        staleCandidates.append(lease);
                    }
                }
            }
            trace(QStringLiteral("startup stale candidates=%1").arg(staleCandidates.size()));
            persistRegistry();
            for (const CleanupLease &lease : staleCandidates) {
                deleteArtifactAsync(lease);
            }
        });
    });
}

QVariantList CleanupSubsystem::activeLeases() const
{
    QVariantList result;
    QMutexLocker locker(&m_mutex);
    for (const CleanupLease &lease : m_leases) {
        QVariantMap item;
        item.insert(QStringLiteral("leaseId"), lease.leaseId);
        item.insert(QStringLiteral("kind"), cleanupArtifactKindToString(lease.kind));
        item.insert(QStringLiteral("state"), cleanupLeaseStateToString(lease.state));
        item.insert(QStringLiteral("stagingRoot"), lease.stagingRoot);
        item.insert(QStringLiteral("artifactPaths"), lease.artifactPaths);
        item.insert(QStringLiteral("deletionSafetyRoot"), lease.deletionSafetyRoot);
        item.insert(QStringLiteral("allowRecursive"), lease.allowRecursive);
        result.append(item);
    }
    return result;
}

void CleanupSubsystem::deleteArtifactAsync(const CleanupLease &lease)
{
    QtConcurrent::run([this, lease]() {
        const bool deleted = deleteLeaseArtifacts(lease);
        updateLeaseState(lease.leaseId, deleted ? CleanupLeaseState::Deleted : CleanupLeaseState::Failed);
        trace(QStringLiteral("delete-result leaseId=%1 result=%2").arg(lease.leaseId, deleted ? QStringLiteral("deleted") : QStringLiteral("failed")));
        QMetaObject::invokeMethod(this, [this]() { persistRegistry(); }, Qt::QueuedConnection);
    });
}

void CleanupSubsystem::updateLeaseState(const QString &leaseId, CleanupLeaseState state)
{
    QMutexLocker locker(&m_mutex);
    for (CleanupLease &lease : m_leases) {
        if (lease.leaseId == leaseId) {
            lease.state = state;
            lease.touchedAt = QDateTime::currentDateTimeUtc();
            return;
        }
    }
}

bool CleanupSubsystem::safeToDelete(const CleanupLease &lease) const
{
    if (lease.leaseId.isEmpty() || lease.artifactPaths.isEmpty()) {
        return false;
    }
    for (const QString &path : lease.artifactPaths) {
        const QString normalized = normalizedAbsolutePath(path);
        if (normalized.isEmpty()
            || !isInsideSafetyRoot(normalized, lease.deletionSafetyRoot)
            || isForbiddenRoot(normalized)) {
            return false;
        }
        const QFileInfo info(normalized);
        if (info.exists() && info.isDir()) {
            if (!lease.allowRecursive || lease.ownerMarkerPath.isEmpty() || !ownerMarkerMatches(lease)) {
                return false;
            }
            if (!isInsideSafetyRoot(lease.ownerMarkerPath, normalized)) {
                return false;
            }
        }
    }
    return true;
}

bool CleanupSubsystem::deleteLeaseArtifacts(const CleanupLease &lease) const
{
    if (!safeToDelete(lease)) {
        return false;
    }
    bool allDeleted = true;
    for (const QString &path : lease.artifactPaths) {
        const QString normalized = normalizedAbsolutePath(path);
        const QFileInfo info(normalized);
        if (!info.exists()) {
            continue;
        }
        if (info.isDir()) {
            allDeleted = QDir(normalized).removeRecursively() && allDeleted;
        } else {
            allDeleted = QFile::remove(normalized) && allDeleted;
        }
    }
    return allDeleted;
}

void CleanupSubsystem::persistRegistry()
{
    if (m_registryPath.isEmpty()) {
        return;
    }
    QList<CleanupLease> snapshot;
    {
        QMutexLocker locker(&m_mutex);
        pruneDeletedLeasesLocked();
        snapshot = m_leases;
    }

    QDir().mkpath(QFileInfo(m_registryPath).absolutePath());
    QSaveFile file(m_registryPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        trace(QStringLiteral("registry write open failed %1").arg(file.errorString()));
        return;
    }
    QJsonArray array;
    for (const CleanupLease &lease : snapshot) {
        array.append(leaseToJson(lease));
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        trace(QStringLiteral("registry commit failed %1").arg(file.errorString()));
    }
}

void CleanupSubsystem::loadRegistry()
{
    if (m_registryPath.isEmpty() || !QFileInfo::exists(m_registryPath)) {
        return;
    }
    QFile file(m_registryPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isArray()) {
        return;
    }

    QList<CleanupLease> loaded;
    const QJsonArray array = doc.array();
    for (const QJsonValue &value : array) {
        if (!value.isObject()) {
            continue;
        }
        CleanupLease lease = leaseFromJson(value.toObject());
        if (!lease.leaseId.isEmpty()) {
            loaded.append(lease);
        }
    }

    QMutexLocker locker(&m_mutex);
    for (const CleanupLease &lease : loaded) {
        auto it = std::find_if(m_leases.begin(), m_leases.end(), [&lease](const CleanupLease &existing) {
            return existing.leaseId == lease.leaseId;
        });
        if (it == m_leases.end()) {
            m_leases.append(lease);
        }
    }
}

QList<CleanupLease> CleanupSubsystem::registrySnapshot() const
{
    QMutexLocker locker(&m_mutex);
    return m_leases;
}

void CleanupSubsystem::pruneDeletedLeasesLocked()
{
    qsizetype retainedDeleted = 0;
    for (auto it = m_leases.begin(); it != m_leases.end();) {
        if (it->state == CleanupLeaseState::Deleted) {
            ++retainedDeleted;
            if (retainedDeleted > kMaxRetainedDeletedLeases) {
                it = m_leases.erase(it);
                continue;
            }
        }
        ++it;
    }
}

bool CleanupSubsystem::writeOwnerMarker(const QString &stagingDir,
                                        const QString &leaseId,
                                        CleanupArtifactKind kind)
{
    if (!QDir().mkpath(stagingDir)) {
        return false;
    }
    const QString markerPath = QDir(stagingDir).filePath(QStringLiteral(".fm-cleanup-owner.json"));
    QSaveFile file(markerPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    QJsonObject marker;
    marker.insert(QStringLiteral("app"), QStringLiteral("FMQml"));
    marker.insert(QStringLiteral("schemaVersion"), kCleanupSchemaVersion);
    marker.insert(QStringLiteral("leaseId"), leaseId);
    marker.insert(QStringLiteral("createdAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    marker.insert(QStringLiteral("kind"), cleanupArtifactKindToString(kind));
    marker.insert(QStringLiteral("registryPath"), m_registryPath);
    file.write(QJsonDocument(marker).toJson(QJsonDocument::Indented));
    return file.commit();
}

bool CleanupSubsystem::ownerMarkerMatches(const CleanupLease &lease) const
{
    const QString markerPath = normalizedAbsolutePath(lease.ownerMarkerPath);
    if (markerPath.isEmpty() || !QFileInfo::exists(markerPath)) {
        return false;
    }
    QFile file(markerPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }
    const QJsonObject object = doc.object();
    return object.value(QStringLiteral("app")).toString() == QLatin1String("FMQml")
        && object.value(QStringLiteral("schemaVersion")).toInt() == kCleanupSchemaVersion
        && object.value(QStringLiteral("leaseId")).toString() == lease.leaseId
        && object.value(QStringLiteral("kind")).toString() == cleanupArtifactKindToString(lease.kind);
}

bool CleanupSubsystem::isInsideSafetyRoot(const QString &path, const QString &root) const
{
    const QString normalizedPathValue = normalizedAbsolutePath(path);
    const QString normalizedRoot = normalizedAbsolutePath(root);
    if (normalizedPathValue.isEmpty() || normalizedRoot.isEmpty() || normalizedPathValue == normalizedRoot) {
        return false;
    }
    const QString prefix = normalizedRoot.endsWith(QLatin1Char('/'))
        ? normalizedRoot
        : normalizedRoot + QLatin1Char('/');
#ifdef Q_OS_WIN
    return normalizedPathValue.startsWith(prefix, Qt::CaseInsensitive);
#else
    return normalizedPathValue.startsWith(prefix);
#endif
}

bool CleanupSubsystem::isForbiddenRoot(const QString &path) const
{
    const QString normalized = normalizedAbsolutePath(path);
    if (normalized.isEmpty()) {
        return true;
    }
    QDir dir(normalized);
    if (dir.isRoot()) {
        return true;
    }

    const QStringList forbidden = {
        QDir::homePath(),
        QDir::tempPath(),
        QDir::rootPath(),
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation),
        QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation),
    };
    for (const QString &candidate : forbidden) {
        const QString forbiddenPath = normalizedAbsolutePath(candidate);
        if (!forbiddenPath.isEmpty() && normalized == forbiddenPath) {
            return true;
        }
    }
#ifdef Q_OS_WIN
    if (normalized.size() <= 3 && normalized.endsWith(QLatin1Char('/'))) {
        return true;
    }
#endif
    return false;
}

void CleanupSubsystem::trace(const QString &message) const
{
    if (cleanupTraceEnabled()) {
        qInfo().noquote() << "[CleanupSubsystem]" << message;
    }
}
