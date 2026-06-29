#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QSet>
#include <QVariantList>
#include <QVariantMap>
#include <QLatin1String>
#include <atomic>
#include <memory>

#include "../core/FileProvider.h"
#include "../core/FileAccessResolver.h"
#include "../models/DirectoryModel.h"
#include "../core/ChecksumCalculator.h"
#include "../core/BatchRenameEngine.h"

class VolumeMonitor;

class FilePanelController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(DirectoryModel::SortRole panelSortRole READ panelSortRole WRITE setPanelSortRole NOTIFY panelSortRoleChanged)
    Q_PROPERTY(Qt::SortOrder panelSortOrder READ panelSortOrder WRITE setPanelSortOrder NOTIFY panelSortOrderChanged)
    Q_PROPERTY(DirectoryModel::SortRole detailsSortRole READ detailsSortRole WRITE setDetailsSortRole NOTIFY detailsSortRoleChanged)
    Q_PROPERTY(Qt::SortOrder detailsSortOrder READ detailsSortOrder WRITE setDetailsSortOrder NOTIFY detailsSortOrderChanged)
    Q_PROPERTY(DirectoryModel *directoryModel READ directoryModel CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(int backStackCount READ backStackCount NOTIFY historyChanged)
    Q_PROPERTY(int forwardStackCount READ forwardStackCount NOTIFY historyChanged)
    Q_PROPERTY(QString hoveredPath READ hoveredPath WRITE setHoveredPath NOTIFY hoveredPathChanged)
    Q_PROPERTY(QString currentItemPath READ currentItemPath WRITE setCurrentItemPath NOTIFY currentItemPathChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(QVariantMap lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(bool scrolling READ scrolling WRITE setScrolling NOTIFY scrollingChanged)
    Q_PROPERTY(bool navigationPending READ navigationPending NOTIFY navigationPendingChanged)
    Q_PROPERTY(QString pendingNavigationPath READ pendingNavigationPath NOTIFY pendingNavigationPathChanged)
    Q_PROPERTY(bool isDeviceRoot READ isDeviceRoot NOTIFY isDeviceRootChanged)
    Q_PROPERTY(bool isFavoritesRoot READ isFavoritesRoot NOTIFY isFavoritesRootChanged)
    Q_PROPERTY(bool isVirtualRoot READ isVirtualRoot NOTIFY virtualRootChanged)
    Q_PROPERTY(bool canCreateInCurrentPath READ canCreateInCurrentPath NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canCopySelection READ canCopySelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canRenameSelection READ canRenameSelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canDeleteSelection READ canDeleteSelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canDuplicateSelection READ canDuplicateSelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canCompressSelection READ canCompressSelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canPasteIntoCurrentPath READ canPasteIntoCurrentPath NOTIFY capabilitiesChanged)
    Q_PROPERTY(int storageInfoRevision READ storageInfoRevision NOTIFY storageInfoChanged)
    Q_PROPERTY(int categoryFilter READ categoryFilter NOTIFY categoryFilterStateChanged)
    Q_PROPERTY(bool categoryFilterActive READ categoryFilterActive NOTIFY categoryFilterStateChanged)
    Q_PROPERTY(bool categoryFilterSuspended READ categoryFilterSuspended NOTIFY categoryFilterStateChanged)
    Q_PROPERTY(QString categoryFilterSummary READ categoryFilterSummary NOTIFY categoryFilterStateChanged)
    Q_PROPERTY(ChecksumCalculator* checksumCalculator READ checksumCalculator CONSTANT)

    static constexpr QLatin1String DEVICE_ROOT{"devices://"};
    static constexpr QLatin1String FAVORITES_ROOT{"favorites://"};

public:
    explicit FilePanelController(QObject *parent = nullptr);
    void setVolumeMonitor(VolumeMonitor *monitor);

    int viewMode() const;
    void setViewMode(int mode);
    DirectoryModel::SortRole panelSortRole() const;
    void setPanelSortRole(DirectoryModel::SortRole role);
    Qt::SortOrder panelSortOrder() const;
    void setPanelSortOrder(Qt::SortOrder order);
    Q_INVOKABLE void setPanelSortPolicy(int role, int order);
    DirectoryModel::SortRole detailsSortRole() const;
    void setDetailsSortRole(DirectoryModel::SortRole role);
    Qt::SortOrder detailsSortOrder() const;
    void setDetailsSortOrder(Qt::SortOrder order);

    bool isDeviceRoot() const;
    bool isFavoritesRoot() const;
    bool isVirtualRoot() const;

    DirectoryModel *directoryModel();
    QString currentPath() const;
    bool canGoBack() const;
    bool canGoForward() const;
    int backStackCount() const;
    int forwardStackCount() const;
    QString hoveredPath() const;
    void setHoveredPath(const QString &path);
    QString currentItemPath() const;
    void setCurrentItemPath(const QString &path);
    QString statusMessage() const;
    QVariantMap lastError() const;
    bool scrolling() const;
    void setScrolling(bool scrolling);
    bool navigationPending() const;
    QString pendingNavigationPath() const;
    bool canCreateInCurrentPath() const;
    bool canCopySelection() const;
    bool canCopyPaths(const QStringList &paths) const;
    bool canRenameSelection() const;
    bool canDeleteSelection() const;
    bool canDeletePaths(const QStringList &paths) const;
    bool canDuplicateSelection() const;
    bool canCompressSelection() const;
    bool canPasteIntoCurrentPath() const;
    int storageInfoRevision() const;
    int categoryFilter() const;
    bool categoryFilterActive() const;
    bool categoryFilterSuspended() const;
    QString categoryFilterSummary() const;
    Q_INVOKABLE QString fileNameForPath(const QString &path) const;
    Q_INVOKABLE QString parentPathForPath(const QString &path) const;
    Q_INVOKABLE QString childPathForCurrent(const QString &name) const;
    Q_INVOKABLE QString childPathForPath(const QString &parentPath, const QString &name) const;
    Q_INVOKABLE QStringList breadcrumbPathsForPath(const QString &path) const;
    Q_INVOKABLE QVariantList breadcrumbEntriesForPath(const QString &path) const;
    Q_INVOKABLE QString pathKindFor(const QString &path) const;
    Q_INVOKABLE void showStatusMessage(const QString &message);
    Q_INVOKABLE QString fileTypeLabelFor(const QString &suffix, bool isDirectory) const;
    Q_INVOKABLE bool isArchiveFilePath(const QString &path) const;
    Q_INVOKABLE bool isIsoImageFilePath(const QString &path) const;
    Q_INVOKABLE QString archiveExtractionFolderNameForPath(const QString &path) const;
    
    ChecksumCalculator* checksumCalculator() { return &m_checksumCalculator; }

    Q_INVOKABLE bool openPath(const QString &path);
    bool openStartupRestoredFolder(const QString &path);
    Q_INVOKABLE bool canOpenPath(const QString &path) const;
    Q_INVOKABLE QStringList getDirectorySuggestions(const QString &inputPath) const;
    Q_INVOKABLE void requestDirectorySuggestions(const QString &inputPath, int requestId, int maxSuggestions = 160) const;
    Q_INVOKABLE void requestDirectorySuggestionEntries(const QString &inputPath, int requestId, int maxSuggestions = 160) const;
    Q_INVOKABLE void cancelDirectorySuggestions() const;
    Q_INVOKABLE bool openSearchResult(const QString &path, bool isDirectory);
    Q_INVOKABLE bool openNestedArchivePath(const QString &path);
    Q_INVOKABLE void submitArchivePassword(const QString &path, const QString &password);
    Q_INVOKABLE void cancelArchivePassword(const QString &path);
    Q_INVOKABLE void cancelCurrentLoad();
    Q_INVOKABLE void openRow(int row);
    Q_INVOKABLE void openItem(int row);
    Q_INVOKABLE QVariantMap launchCapabilitiesForPath(const QString &path) const;
    Q_INVOKABLE void openPathWithWine(const QString &path);
    Q_INVOKABLE void openPathWithSteamProton(const QString &path);
    Q_INVOKABLE QVariantMap steamProtonLaunchOptionsForPath(const QString &path) const;
    Q_INVOKABLE QVariantMap launchPathWithSteamProton(const QString &path,
                                                      const QString &runtimeId,
                                                      bool enableVkBasalt,
                                                      bool captureLog,
                                                      bool clearXModifiers);
    Q_INVOKABLE void revealInFileManager(int row);
    Q_INVOKABLE void openInTerminal();
    Q_INVOKABLE bool canSetWallpaperPath(const QString &path) const;
    Q_INVOKABLE void setAsWallpaper(int row);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void setCategoryFilter(int filter);
    Q_INVOKABLE QStringList selectedPaths() const;
    Q_INVOKABLE QVariantList selectedItems() const;
    Q_INVOKABLE QVariantMap storageInfoForPath(const QString &rootPath) const;
    Q_INVOKABLE void ejectDrive(const QString &rootPath);
    void handleDeviceRemoved(const QString &rootPath, const QString &displayName);
    Q_INVOKABLE void syncStateFrom(FilePanelController *other);

    Q_INVOKABLE bool rename(int row, const QString &newName);
    Q_INVOKABLE bool renamePath(const QString &oldPath, const QString &newName);
    Q_INVOKABLE bool renameAsAdministrator(int row, const QString &newName);
    
    Q_INVOKABLE QVariantList previewBatchRename(const QStringList &paths, const QVariantList &rules);
    Q_INVOKABLE QVariantList applyBatchRename(const QStringList &paths, const QVariantList &rules);

    Q_INVOKABLE bool createFolder(const QString &name);
    Q_INVOKABLE bool createFolderAsAdministrator(const QString &name);
    Q_INVOKABLE bool createFile(const QString &name);
    Q_INVOKABLE bool createFileAsAdministrator(const QString &name);
    Q_INVOKABLE void showProperties(int row);

    // Async media metadata fetch for Details View columns
    // Returns immediately; emits metadataReady(path, map) when done.
    // map keys: "dimensions", "resolution", "duration", "artist", "album", "bitrate"
    Q_INVOKABLE void fetchMetadataAsync(const QString &path);

signals:
    void pathAboutToChange(const QString &from, const QString &to, bool preserveScroll);
    void currentPathChanged();
    void historyChanged();
    void hoveredPathChanged();
    void currentItemPathChanged();
    void viewModeChanged();
    void panelSortRoleChanged();
    void panelSortOrderChanged();
    void detailsSortRoleChanged();
    void detailsSortOrderChanged();
    void isDeviceRootChanged();
    void isFavoritesRootChanged();
    void virtualRootChanged();
    void revealProperties(const QStringList &paths);
    void revealBatchRename(const QStringList &paths);
    void entryRenamed(const QString &oldPath, const QString &newPath);
    void entryCreated(const QString &path);
    void administratorOperationSucceeded();
    void createdEntryRevealRequested(const QString &path);
    void pathNavigated(const QString &path);
    void pathNavigationFailed(const QString &path);
    void contentsChanged(const QString &path);
    void statusMessageChanged();
    void lastErrorChanged();
    void scrollingChanged();
    void navigationPendingChanged();
    void pendingNavigationPathChanged();
    void capabilitiesChanged();
    void storageInfoChanged();
    void categoryFilterStateChanged();
    void ejectFinished(const QString &rootPath, bool success);
    void isoMountRequested(const QString &path);
    void nestedArchiveOpenRequested(const QString &path, const QString &displayName, const QString &sizeText);
    void archivePasswordRequested(const QString &path, const QString &displayName, const QString &message);
    void directorySuggestionsReady(int requestId, const QStringList &suggestions);
    void directorySuggestionEntriesReady(int requestId, const QVariantList &suggestions);
    // Emitted on the GUI thread when async metadata finishes
    void metadataReady(const QString &path, const QVariantMap &meta);

private:
    bool isReadOnlyContainerPath(const QString &path) const;
    bool pathCanCopy(const QString &path) const;
    bool pathCanCreateChildren(const QString &path) const;
    bool pathCanDelete(const QString &path) const;
    bool openPathInternal(const QString &path, bool addToHistory, bool preserveScroll = false);
    QString filterScopeForPath(const QString &path) const;
    QString comparisonPathForFilterScope(const QString &path) const;
    QString filterContextForPath(const QString &path) const;
    bool isPathInsideCategoryFilterScope(const QString &path) const;
    void clearCategoryFilterScope();
    void updateCategoryFilterForPath(const QString &path);
    void pushHistory(const QString &path);
    bool removeLastHistoryEntryIfPath(const QString &path);
    void setStatusMessage(const QString &message);
    void setLastError(const QVariantMap &error);
    void setOperationError(const QString &message, const QString &path, const QString &operation);
    bool requestOpenPath(const QString &path, bool addToHistory, bool preserveScroll = false);
    void setNavigationPending(bool pending, const QString &path = {});
    void recoverFromMissingPath(const QString &path, const QString &error);
    void scheduleCreatedEntryReveal(const QString &path);

    DirectoryModel m_directoryModel;
    std::unique_ptr<FileProvider> m_fileProvider;
    QString m_hoveredPath;
    QString m_currentItemPath;
    QString m_statusMessage;
    QVariantMap m_lastError;
    QString m_pendingNavigationPath;
    QStringList m_backStack;
    QStringList m_forwardStack;
    int m_viewMode = 0;
    int m_storageInfoRevision = 0;
    DirectoryModel::SortRole m_panelSortRole = DirectoryModel::SortByName;
    Qt::SortOrder m_panelSortOrder = Qt::AscendingOrder;
    bool m_scrolling = false;
    bool m_isDeviceRoot = false;
    bool m_isFavoritesRoot = false;
    bool m_navigationPending = false;
    int m_navigationRequestId = 0;
    mutable std::atomic<int> m_directorySuggestionGeneration{0};
    QTimer m_createdEntryRevealTimer;
    QString m_pendingCreatedEntryRevealPath;
    int m_createdEntryRevealAttempts = 0;
    QSet<QString> m_approvedNestedArchiveScopeKeys;
    VolumeMonitor *m_volumeMonitor = nullptr;
    DirectoryModel::CategoryFilter m_categoryFilter = DirectoryModel::FilterAll;
    QString m_categoryFilterScopePath;
    QString m_categoryFilterContext;
    ChecksumCalculator m_checksumCalculator;
    BatchRenameEngine m_renameEngine;

    void setIsDeviceRoot(bool value);
    void setIsFavoritesRoot(bool value);
};
