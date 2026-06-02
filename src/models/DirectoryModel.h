#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <memory>

#include "../core/DirectoryChangeWatcher.h"
#include "../core/FileProvider.h"

// #define FM_DEBUG_LOAD_TIMING

class DirectoryModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(bool mixFilesAndFolders READ mixFilesAndFolders WRITE setMixFilesAndFolders NOTIFY mixFilesAndFoldersChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QVariantMap lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(CategoryFilter categoryFilter READ categoryFilter WRITE setCategoryFilter NOTIFY filtersChanged)
    Q_PROPERTY(bool hasActiveFilters READ hasActiveFilters NOTIFY filtersChanged)
    Q_PROPERTY(QString activeFiltersSummary READ activeFiltersSummary NOTIFY filtersChanged)
    Q_PROPERTY(SortRole sortRole READ sortRole WRITE setSortRole NOTIFY sortRoleChanged)
    Q_PROPERTY(Qt::SortOrder sortOrder READ sortOrder WRITE setSortOrder NOTIFY sortOrderChanged)

public:
    enum SortRole {
        SortByName = 0,
        SortBySize,
        SortByType,
        SortByDate,
        SortByDateCreated,
        SortByExtension
    };
    Q_ENUM(SortRole)
    enum CategoryFilter {
        FilterAll = 0,
        FilterExecutables,
        FilterLibraries,
        FilterImages,
        FilterArchives,
        FilterMedia,
        FilterDocuments
    };
    Q_ENUM(CategoryFilter)
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        SizeRole,
        SizeTextRole,
        ModifiedTextRole,
        CreatedTextRole,
        AttributesRole,
        IsDirectoryRole,
        IsHiddenRole,
        IsSelectedRole,
        IconNameRole,
        SuffixRole,
        IsImageRole,
        HasThumbnailRole,
        IsArchiveFileRole,
        IsIsoImageFileRole
    };
    Q_ENUM(Role)

    explicit DirectoryModel(QObject *parent = nullptr);

    bool mixFilesAndFolders() const;
    void setMixFilesAndFolders(bool mix);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentPath() const;
    bool loading() const;
    QString error() const;
    QVariantMap lastError() const;
    int count() const;
    int selectedCount() const;
    QString searchText() const;
    void setSearchText(const QString &text);
    CategoryFilter categoryFilter() const;
    void setCategoryFilter(CategoryFilter filter);
    bool hasActiveFilters() const;
    QString activeFiltersSummary() const;

    SortRole sortRole() const;
    void setSortRole(SortRole role);

    Qt::SortOrder sortOrder() const;
    void setSortOrder(Qt::SortOrder order);

    bool showHidden() const;
    void setShowHidden(bool show);

    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clearError();
    void noteLocalMutation();
    void suppressNextWatchRestart();
    bool upsertPath(const QString &path);
    bool insertPath(const QString &path);
    bool removePath(const QString &path);
    bool renamePath(const QString &oldPath, const QString &newPath);
    Q_INVOKABLE void toggleSelected(int row);
    Q_INVOKABLE void selectOnly(int row);
    Q_INVOKABLE void selectRange(int from, int to);
    Q_INVOKABLE void selectRows(const QVariantList &rows);
    Q_INVOKABLE void invertSelection();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirectoryAt(int row) const;
    Q_INVOKABLE int indexOfPath(const QString &path) const;
    Q_INVOKABLE QStringList selectedPaths() const;
    Q_INVOKABLE void clearFilters();

signals:
    void mixFilesAndFoldersChanged();
    void currentPathChanged();
    void loadingChanged();
    void showHiddenChanged();
    void errorChanged();
    void lastErrorChanged();
    void directoryUnavailable(const QString &path, const QString &error);
    void countChanged();
    void selectionChanged();
    void searchTextChanged();
    void filtersChanged();
    void sortRoleChanged();
    void sortOrderChanged();

private slots:
    void onScannerStarted();
    void onScannerBatchReady(const QList<FileEntry> &entries, int generation);
    void onScannerFinished(const QString &path, bool success, int generation, const QString &error);
    void onDirectoryEventsReady(const QList<DirectoryChangeEvent> &events);
    void onDirectoryWatchFailed(const QString &path, const QString &error);
    void onDebounceTimeout();
    void processPendingDirectoryEvents();
    void processPendingInserts();

private:
    static QString formatSize(qint64 bytes);
    static QString iconNameFor(const FileEntry &entry);
    void replaceProvider(std::unique_ptr<FileProvider> provider);
    void setLoading(bool loading);
    void setError(const QString &error);
    void setLastError(const QVariantMap &error);
    bool matchesFilter(const FileEntry &entry) const;
    void notifyFiltersChanged();
    void applyFilter();
    void applyFilterInternal(bool keepSelection);
    void sortModel();
    bool compareEntries(const FileEntry &a, const FileEntry &b) const;
    int filteredRowForAbsoluteIndex(int absoluteIdx) const;
    void updatePathIndex();
    void finalizeScannerFinished(const QString &path, bool success, const QString &error);
    void processAllPendingInsertsFast();
    void applyDirectoryChangeEvents(const QList<DirectoryChangeEvent> &events);
    bool canWatchPath(const QString &path) const;
    void restartChangeWatcherForCurrentPath();
    void scheduleDeferredWatchRestart();
    bool currentPathExists() const;
    void notifyCurrentPathUnavailable(const QString &error);

#ifdef FM_DEBUG_LOAD_TIMING
    void dumpLoadTiming() const;
#endif

    QString m_currentPath;
    bool m_loading = false;
    bool m_showHidden = false;
    bool m_mixFilesAndFolders = false;
    bool m_freshLoad = false;
    bool m_recoveringUnavailablePath = false;
    bool m_suppressNextWatchRestart = false;
    bool m_deferredWatchRestartPending = false;
    QString m_deferredWatchRestartPath;
    int m_currentScanGeneration = 0; 
    QTimer m_debounceTimer;
    QTimer m_directoryEventTimer;
    QList<DirectoryChangeEvent> m_pendingDirectoryEvents;
    qint64 m_watchEventsReceived = 0;
    qint64 m_watchBatchesApplied = 0;
    qint64 m_watchOverflowRefreshes = 0;
    QElapsedTimer m_localMutationThrottle;
    QString m_error;
    QVariantMap m_lastError;
    QString m_searchText;
    CategoryFilter m_categoryFilter = FilterAll;
    QString m_previousPath;
    SortRole m_sortRole = SortByName;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;

    QList<FileEntry> m_entries;
    QList<int> m_filteredIndices;
    
    QList<FileEntry> m_pendingInserts;
    qsizetype m_pendingInsertOffset = 0;
    QTimer m_insertTimer;
    bool m_pendingScannerFinish = false;
    QString m_pendingScannerPath;
    QString m_pendingScannerError;
    bool m_pendingScannerSuccess = false;
    
    QHash<QString, int> m_pathIndex;
    QSet<QString> m_foundPaths;
    
    int m_selectedCount = 0;
    std::unique_ptr<FileProvider> m_provider;
    std::unique_ptr<DirectoryChangeWatcher> m_changeWatcher;

    static constexpr int SmallDirectoryThreshold = 100;
    static constexpr int LargeDirectoryBulkFinishThreshold = 1000;

#ifdef FM_DEBUG_LOAD_TIMING
    QElapsedTimer m_loadTimingTimer;
    bool m_loadTimingFirstRowInserted = false;
    bool m_loadTimingRailShown = false;
#endif
};
