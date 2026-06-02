#pragma once

#include <QObject>
#include <QStringList>
#include <QTimer>
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

class FilePanelController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(DirectoryModel::SortRole detailsSortRole READ detailsSortRole WRITE setDetailsSortRole NOTIFY detailsSortRoleChanged)
    Q_PROPERTY(Qt::SortOrder detailsSortOrder READ detailsSortOrder WRITE setDetailsSortOrder NOTIFY detailsSortOrderChanged)
    Q_PROPERTY(DirectoryModel *directoryModel READ directoryModel CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
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
    Q_PROPERTY(bool canRenameSelection READ canRenameSelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canDeleteSelection READ canDeleteSelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canDuplicateSelection READ canDuplicateSelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canCompressSelection READ canCompressSelection NOTIFY capabilitiesChanged)
    Q_PROPERTY(bool canPasteIntoCurrentPath READ canPasteIntoCurrentPath NOTIFY capabilitiesChanged)
    Q_PROPERTY(int categoryFilter READ categoryFilter NOTIFY categoryFilterStateChanged)
    Q_PROPERTY(bool categoryFilterActive READ categoryFilterActive NOTIFY categoryFilterStateChanged)
    Q_PROPERTY(bool categoryFilterSuspended READ categoryFilterSuspended NOTIFY categoryFilterStateChanged)
    Q_PROPERTY(QString categoryFilterSummary READ categoryFilterSummary NOTIFY categoryFilterStateChanged)
    Q_PROPERTY(ChecksumCalculator* checksumCalculator READ checksumCalculator CONSTANT)

    static constexpr QLatin1String DEVICE_ROOT{"devices://"};
    static constexpr QLatin1String FAVORITES_ROOT{"favorites://"};

public:
    explicit FilePanelController(QObject *parent = nullptr);

    int viewMode() const;
    void setViewMode(int mode);
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
    bool canRenameSelection() const;
    bool canDeleteSelection() const;
    bool canDuplicateSelection() const;
    bool canCompressSelection() const;
    bool canPasteIntoCurrentPath() const;
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
    Q_INVOKABLE QString fileTypeLabelFor(const QString &suffix, bool isDirectory) const;
    Q_INVOKABLE bool isArchiveFilePath(const QString &path) const;
    Q_INVOKABLE bool isIsoImageFilePath(const QString &path) const;
    Q_INVOKABLE QString archiveExtractionFolderNameForPath(const QString &path) const;
    
    ChecksumCalculator* checksumCalculator() { return &m_checksumCalculator; }

    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE bool canOpenPath(const QString &path) const;
    Q_INVOKABLE QStringList getDirectorySuggestions(const QString &inputPath) const;
    Q_INVOKABLE void requestDirectorySuggestions(const QString &inputPath, int requestId, int maxSuggestions = 160) const;
    Q_INVOKABLE void requestDirectorySuggestionEntries(const QString &inputPath, int requestId, int maxSuggestions = 160) const;
    Q_INVOKABLE void cancelDirectorySuggestions() const;
    Q_INVOKABLE void openRow(int row);
    Q_INVOKABLE void openItem(int row);
    Q_INVOKABLE void revealInFileManager(int row);
    Q_INVOKABLE void openInTerminal();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void setCategoryFilter(int filter);
    Q_INVOKABLE QStringList selectedPaths() const;
    Q_INVOKABLE QVariantMap storageInfoForPath(const QString &rootPath) const;
    Q_INVOKABLE void ejectDrive(const QString &rootPath);
    Q_INVOKABLE void syncStateFrom(FilePanelController *other);

    Q_INVOKABLE bool rename(int row, const QString &newName);
    Q_INVOKABLE bool renamePath(const QString &oldPath, const QString &newName);
    
    Q_INVOKABLE QVariantList previewBatchRename(const QStringList &paths, const QVariantList &rules);
    Q_INVOKABLE QVariantList applyBatchRename(const QStringList &paths, const QVariantList &rules);

    Q_INVOKABLE bool createFolder(const QString &name);
    Q_INVOKABLE bool createFile(const QString &name);
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
    void detailsSortRoleChanged();
    void detailsSortOrderChanged();
    void isDeviceRootChanged();
    void isFavoritesRootChanged();
    void virtualRootChanged();
    void revealProperties(const QStringList &paths);
    void revealBatchRename(const QStringList &paths);
    void entryRenamed(const QString &oldPath, const QString &newPath);
    void entryCreated(const QString &path);
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
    void categoryFilterStateChanged();
    void ejectFinished(const QString &rootPath, bool success);
    void isoMountRequested(const QString &path);
    void directorySuggestionsReady(int requestId, const QStringList &suggestions);
    void directorySuggestionEntriesReady(int requestId, const QVariantList &suggestions);
    // Emitted on the GUI thread when async metadata finishes
    void metadataReady(const QString &path, const QVariantMap &meta);

private:
    bool isReadOnlyContainerPath(const QString &path) const;
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
    void applySortPolicyForCurrentView();
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
    DirectoryModel::SortRole m_detailsSortRole = DirectoryModel::SortByName;
    Qt::SortOrder m_detailsSortOrder = Qt::AscendingOrder;
    bool m_scrolling = false;
    bool m_isDeviceRoot = false;
    bool m_isFavoritesRoot = false;
    bool m_navigationPending = false;
    int m_navigationRequestId = 0;
    mutable std::atomic<int> m_directorySuggestionGeneration{0};
    QTimer m_createdEntryRevealTimer;
    QString m_pendingCreatedEntryRevealPath;
    int m_createdEntryRevealAttempts = 0;
    DirectoryModel::CategoryFilter m_categoryFilter = DirectoryModel::FilterAll;
    QString m_categoryFilterScopePath;
    QString m_categoryFilterContext;
    ChecksumCalculator m_checksumCalculator;
    BatchRenameEngine m_renameEngine;

    void setIsDeviceRoot(bool value);
    void setIsFavoritesRoot(bool value);
};
