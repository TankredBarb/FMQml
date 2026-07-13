#pragma once

#include <QDateTime>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace StagingLocationPolicy {
QString defaultCleanupRoot();
bool isStagingParentUsable(const QString &parentPath);
QString resolveStagingParent(const QString &destinationPath = {},
                             const QString &sourcePath = {},
                             const QString &callerProvided = {},
                             bool allowDefault = true);
QString resolveStagingParentDirectory(const QString &candidateParent,
                                      const QString &sourcePath = {},
                                      const QString &callerProvided = {},
                                      bool allowDefault = true);
}

enum class CleanupArtifactKind {
    PartFile,
    ProviderTransfer,
    ArchiveExtract,
    ArchiveBrowse,
    RemotePreview,
    ThumbnailAdapter,
    YakuakeSession,
};

enum class CleanupLeaseState {
    Active,
    FinalizePending,
    DeletePending,
    Deleted,
    Failed,
};

struct CleanupLease {
    QString leaseId;
    CleanupArtifactKind kind = CleanupArtifactKind::PartFile;
    CleanupLeaseState state = CleanupLeaseState::Active;
    QDateTime createdAt;
    QDateTime touchedAt;
    qint64 creatingPid = 0;
    QString stagingRoot;
    QStringList artifactPaths;
    QString deletionSafetyRoot;
    bool allowRecursive = false;
    QString ownerMarkerPath;
};

class CleanupSubsystem : public QObject {
    Q_OBJECT
public:
    static CleanupSubsystem &instance();

    QString allocateStagingDirectory(CleanupArtifactKind kind,
                                     const QString &preferredParent,
                                     const QString &operationId,
                                     QString *leaseId = nullptr);
    QString registerArtifact(CleanupArtifactKind kind,
                             const QString &artifactPath,
                             const QString &safetyRoot,
                             bool allowRecursive = false,
                             QString *leaseId = nullptr);

    void scheduleDelete(const QString &leaseId);
    void scheduleDeleteOnFailure(const QString &leaseId);
    void completeWithoutDelete(const QString &leaseId);
    void scheduleStartupCleanup();


private:
    explicit CleanupSubsystem(QObject *parent = nullptr);

    void deleteArtifactAsync(const CleanupLease &lease);
    void updateLeaseState(const QString &leaseId, CleanupLeaseState state);
    bool safeToDelete(const CleanupLease &lease) const;
    bool deleteLeaseArtifacts(const CleanupLease &lease) const;

    void persistRegistry();
    void loadRegistry();
    void pruneDeletedLeasesLocked();

    bool writeOwnerMarker(const QString &stagingDir,
                          const QString &leaseId,
                          CleanupArtifactKind kind);
    bool ownerMarkerMatches(const CleanupLease &lease) const;

    bool isInsideSafetyRoot(const QString &path, const QString &root) const;
    bool isForbiddenRoot(const QString &path) const;
    void trace(const QString &message) const;

    mutable QMutex m_mutex;
    QList<CleanupLease> m_leases;
    QString m_registryPath;
    qint64 m_pid = 0;
};

QString cleanupArtifactKindToString(CleanupArtifactKind kind);
QString cleanupLeaseStateToString(CleanupLeaseState state);
