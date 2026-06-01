#pragma once

#include "../models/DiskUsageModel.h"

#include <QObject>
#include <QRunnable>
#include <QString>
#include <QStringList>
#include <atomic>

class DiskUsageScanner final : public QObject, public QRunnable {
    Q_OBJECT

public:
    struct Totals {
        qint64 bytes = 0;
        int files = 0;
        int folders = 0;
    };

    DiskUsageScanner(const QString &rootPath, int generation, int maxResults = 200);

    void run() override;
    void cancel();

signals:
    void snapshotReady(QList<DiskUsageEntry> folders,
                       QList<DiskUsageEntry> files,
                       QList<DiskUsageEntry> rootChildren,
                       qint64 totalBytes,
                       int scannedFiles,
                       int scannedFolders,
                       int skippedPaths,
                       int inaccessiblePaths,
                       int reparsePaths,
                       QStringList inaccessiblePathDetails,
                       QStringList reparsePathDetails,
                       QString currentPath,
                       QString lastError,
                       int generation);
    void finished(bool success,
                  QString error,
                  QList<DiskUsageEntry> folders,
                  QList<DiskUsageEntry> files,
                  QList<DiskUsageEntry> rootChildren,
                  qint64 totalBytes,
                  int scannedFiles,
                  int scannedFolders,
                  int skippedPaths,
                  int inaccessiblePaths,
                  int reparsePaths,
                  QStringList inaccessiblePathDetails,
                  QStringList reparsePathDetails,
                  int generation);

private:
    void addFolderCandidate(const DiskUsageEntry &entry);
    void addFileCandidate(const DiskUsageEntry &entry);
    void addRootChildCandidate(const DiskUsageEntry &entry);
    void addSkippedDetail(QStringList &details, const QString &detail);
    void emitSnapshotIfNeeded(bool force);

    QString m_rootPath;
    int m_generation = 0;
    int m_maxResults = 200;
    std::atomic_bool m_cancelled{false};
    QList<DiskUsageEntry> m_topFolders;
    QList<DiskUsageEntry> m_topFiles;
    QList<DiskUsageEntry> m_rootChildren;
    qint64 m_totalBytes = 0;
    int m_scannedFiles = 0;
    int m_scannedFolders = 0;
    int m_skippedPaths = 0;
    int m_inaccessiblePaths = 0;
    int m_reparsePaths = 0;
    QStringList m_inaccessiblePathDetails;
    QStringList m_reparsePathDetails;
    QString m_currentPath;
    QString m_lastError;
    qint64 m_lastSnapshotMsec = 0;
};
