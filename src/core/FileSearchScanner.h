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
    bool isMountBoundary = false;
};

struct FileSearchRequest {
    enum SearchTarget { NameTarget = 0, ContentsTarget = 1, NameAndContentsTarget = 2 };
    enum MatchMode { ContainsMatch = 0, ExactMatch = 1, WildcardMatch = 2 };
    enum KindFilter { AllKinds = 0, FoldersKind, FilesKind, ImagesKind, VideoKind, AudioKind, DocumentsKind, ArchivesKind };

    QString rootPath;
    QString query;
    bool includeHidden = false;
    int searchTarget = NameTarget;
    bool caseSensitive = false;
    int matchMode = ContainsMatch;
    bool includeFolders = true;
    int kindFilter = AllKinds;
    QString extension;
    QDateTime modifiedSince;
    qint64 minimumSize = -1;
    qint64 maximumSize = -1;
    int generation = 0;
};

class FileSearchScanner final : public QObject, public QRunnable {
    Q_OBJECT

public:
    using MatchMode = FileSearchRequest::MatchMode;
    using SearchTarget = FileSearchRequest::SearchTarget;
    static constexpr int ContainsMatch = FileSearchRequest::ContainsMatch;
    static constexpr int ExactMatch = FileSearchRequest::ExactMatch;
    static constexpr int WildcardMatch = FileSearchRequest::WildcardMatch;
    static constexpr int NameTarget = FileSearchRequest::NameTarget;
    static constexpr int ContentsTarget = FileSearchRequest::ContentsTarget;
    static constexpr int NameAndContentsTarget = FileSearchRequest::NameAndContentsTarget;

    explicit FileSearchScanner(FileSearchRequest request);

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
    bool entryPassesFilters(const FileSearchScannerEntry &entry) const;
    int nameRelevance(const QString &fileName) const;
    bool canSearchFileContents(const FileSearchScannerEntry &entry) const;
    bool enumerateFolder(const QString &folderPath, QStack<QString> &pending);
    void appendResultBatch(const FileSearchResult &result);
    void addSkippedDetail(QStringList &details, const QString &detail);
    void emitBatchIfNeeded(bool force);

    FileSearchRequest m_request;
    QRegularExpression m_wildcardExpression;
    bool m_useWildcardNameMatch = false;
    int m_discoveryOrder = 0;
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
