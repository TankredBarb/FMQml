#pragma once

#include "../models/FileSearchModel.h"

#include <QDateTime>
#include <QObject>
#include <QRunnable>
#include <QRegularExpression>
#include <QStack>
#include <QString>
#include <QStringList>
#include <atomic>

struct FileSearchScannerEntry {
    QString path;
    QString name;
    QString parentPath;
    qint64 size = 0;
    QDateTime modified;
    bool isDirectory = false;
    bool isHidden = false;
    bool isReparseDirectory = false;
};

class FileSearchScanner final : public QObject, public QRunnable {
    Q_OBJECT

public:
    enum MatchMode {
        ContainsMatch = 0,
        ExactMatch = 1,
        WildcardMatch = 2
    };

    FileSearchScanner(const QString &rootPath, const QString &query, bool includeHidden, bool searchContents, bool caseSensitive, int matchMode, bool includeFolders, int generation);

    void run() override;
    void cancel();

signals:
    void resultsReady(QList<FileSearchResult> results,
                      int scannedFiles,
                      int scannedFolders,
                      int skippedPaths,
                      int inaccessiblePaths,
                      int reparsePaths,
                      int contentFilesScanned,
                      int contentFilesSkipped,
                      QStringList inaccessiblePathDetails,
                      QStringList reparsePathDetails,
                      QString currentPath,
                      QString lastError,
                      int generation);
    void finished(bool success,
                  QString error,
                  int scannedFiles,
                  int scannedFolders,
                  int skippedPaths,
                  int inaccessiblePaths,
                  int reparsePaths,
                  int contentFilesScanned,
                  int contentFilesSkipped,
                  QStringList inaccessiblePathDetails,
                  QStringList reparsePathDetails,
                  int generation);

private:
    void processEntry(const FileSearchScannerEntry &entry, QStack<QString> &pending);
    void appendNameMatch(const FileSearchScannerEntry &entry);
    void appendContentMatches(const FileSearchScannerEntry &entry);
    bool fileNameMatches(const QString &fileName) const;
    bool canSearchFileContents(const FileSearchScannerEntry &entry) const;
    bool enumerateFolder(const QString &folderPath, QStack<QString> &pending);
    void appendResultBatch(const FileSearchResult &result);
    void addSkippedDetail(QStringList &details, const QString &detail);
    void emitBatchIfNeeded(bool force);

    QString m_rootPath;
    QString m_query;
    QRegularExpression m_wildcardExpression;
    bool m_includeHidden = false;
    bool m_searchContents = false;
    bool m_caseSensitive = false;
    bool m_includeFolders = true;
    bool m_useWildcardNameMatch = false;
    int m_matchMode = ContainsMatch;
    int m_generation = 0;
    std::atomic_bool m_cancelled{false};
    QList<FileSearchResult> m_pendingResults;
    int m_scannedFiles = 0;
    int m_scannedFolders = 0;
    int m_skippedPaths = 0;
    int m_inaccessiblePaths = 0;
    int m_reparsePaths = 0;
    int m_contentFilesScanned = 0;
    int m_contentFilesSkipped = 0;
    QStringList m_inaccessiblePathDetails;
    QStringList m_reparsePathDetails;
    QString m_currentPath;
    QString m_lastError;
    qint64 m_lastBatchMsec = 0;
};
