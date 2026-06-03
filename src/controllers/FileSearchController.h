#pragma once

#include "../core/FileSearchScanner.h"
#include "../models/FileSearchModel.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>

class FileSearchController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QString rootPath READ rootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QString displayRootPath READ displayRootPath NOTIFY rootPathChanged)
    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY progressChanged)
    Q_PROPERTY(QString currentDisplayPath READ currentDisplayPath NOTIFY progressChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY progressChanged)
    Q_PROPERTY(int scannedFiles READ scannedFiles NOTIFY progressChanged)
    Q_PROPERTY(int scannedFolders READ scannedFolders NOTIFY progressChanged)
    Q_PROPERTY(int skippedPaths READ skippedPaths NOTIFY progressChanged)
    Q_PROPERTY(int inaccessiblePaths READ inaccessiblePaths NOTIFY progressChanged)
    Q_PROPERTY(int reparsePaths READ reparsePaths NOTIFY progressChanged)
    Q_PROPERTY(int contentFilesScanned READ contentFilesScanned NOTIFY progressChanged)
    Q_PROPERTY(int contentFilesSkipped READ contentFilesSkipped NOTIFY progressChanged)
    Q_PROPERTY(QString coverageStatusText READ coverageStatusText NOTIFY progressChanged)
    Q_PROPERTY(QVariantList skippedDetailEntries READ skippedDetailEntries NOTIFY progressChanged)
    Q_PROPERTY(FileSearchModel *resultsModel READ resultsModel CONSTANT)
    Q_PROPERTY(bool holdResultUpdates READ holdResultUpdates WRITE setHoldResultUpdates NOTIFY holdResultUpdatesChanged)

public:
    enum class State {
        Idle,
        Searching,
        Canceling,
        Finished,
        Failed
    };
    Q_ENUM(State)

    explicit FileSearchController(QObject *parent = nullptr);
    ~FileSearchController() override;

    State state() const;
    bool busy() const;
    QString rootPath() const;
    QString displayRootPath() const;
    QString query() const;
    QString currentPath() const;
    QString currentDisplayPath() const;
    QString error() const;
    QString lastError() const;
    int scannedFiles() const;
    int scannedFolders() const;
    int skippedPaths() const;
    int inaccessiblePaths() const;
    int reparsePaths() const;
    int contentFilesScanned() const;
    int contentFilesSkipped() const;
    QString coverageStatusText() const;
    QVariantList skippedDetailEntries() const;
    FileSearchModel *resultsModel();
    bool holdResultUpdates() const;
    void setHoldResultUpdates(bool hold);

    Q_INVOKABLE bool canSearchPath(const QString &path) const;
    Q_INVOKABLE void search(const QString &rootPath, const QString &query, bool includeHidden = false, bool searchContents = false, bool caseSensitive = false, int matchMode = 0, bool includeFolders = true);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void clear();
    Q_INVOKABLE bool revealPath(const QString &path) const;

signals:
    void stateChanged();
    void rootPathChanged();
    void queryChanged();
    void progressChanged();
    void errorChanged();
    void holdResultUpdatesChanged();

private:
    void appendOrQueueResults(const QList<FileSearchResult> &results);
    void flushPendingResults();
    void setState(State state);
    void setError(const QString &error);
    void resetProgress();
    void applyProgress(int scannedFiles,
                       int scannedFolders,
                       int skippedPaths,
                       int inaccessiblePaths,
                       int reparsePaths,
                       int contentFilesScanned,
                       int contentFilesSkipped,
                       const QStringList &inaccessiblePathDetails,
                       const QStringList &reparsePathDetails,
                       const QString &currentPath,
                       const QString &lastError);

    State m_state = State::Idle;
    QString m_rootPath;
    QString m_query;
    QString m_currentPath;
    QString m_error;
    QString m_lastError;
    int m_scannedFiles = 0;
    int m_scannedFolders = 0;
    int m_skippedPaths = 0;
    int m_inaccessiblePaths = 0;
    int m_reparsePaths = 0;
    int m_contentFilesScanned = 0;
    int m_contentFilesSkipped = 0;
    QStringList m_inaccessiblePathDetails;
    QStringList m_reparsePathDetails;
    int m_generation = 0;
    QPointer<FileSearchScanner> m_scanner;
    QList<FileSearchResult> m_pendingModelResults;
    bool m_holdResultUpdates = false;
    FileSearchModel m_resultsModel;
};
