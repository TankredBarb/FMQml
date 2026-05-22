#pragma once

#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QLatin1String>
#include <memory>

#include "../core/FileProvider.h"
#include "../models/DirectoryModel.h"
#include "../core/ChecksumCalculator.h"
#include "../core/BatchRenameEngine.h"

class FilePanelController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int viewMode READ viewMode WRITE setViewMode NOTIFY viewModeChanged)
    Q_PROPERTY(DirectoryModel *directoryModel READ directoryModel CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(QString hoveredPath READ hoveredPath WRITE setHoveredPath NOTIFY hoveredPathChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool scrolling READ scrolling WRITE setScrolling NOTIFY scrollingChanged)
    Q_PROPERTY(bool isDeviceRoot READ isDeviceRoot NOTIFY isDeviceRootChanged)
    Q_PROPERTY(ChecksumCalculator* checksumCalculator READ checksumCalculator CONSTANT)

    static constexpr QLatin1String DEVICE_ROOT{"devices://"};

public:
    explicit FilePanelController(QObject *parent = nullptr);

    int viewMode() const;
    void setViewMode(int mode);

    bool isDeviceRoot() const;

    DirectoryModel *directoryModel();
    QString currentPath() const;
    bool canGoBack() const;
    bool canGoForward() const;
    QString hoveredPath() const;
    void setHoveredPath(const QString &path);
    QString statusMessage() const;
    bool scrolling() const;
    void setScrolling(bool scrolling);
    Q_INVOKABLE QString fileNameForPath(const QString &path) const;
    Q_INVOKABLE QString parentPathForPath(const QString &path) const;
    Q_INVOKABLE QString childPathForCurrent(const QString &name) const;
    Q_INVOKABLE QString childPathForPath(const QString &parentPath, const QString &name) const;
    Q_INVOKABLE QStringList breadcrumbPathsForPath(const QString &path) const;
    Q_INVOKABLE QVariantList breadcrumbEntriesForPath(const QString &path) const;
    
    ChecksumCalculator* checksumCalculator() { return &m_checksumCalculator; }

    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE QStringList getDirectorySuggestions(const QString &inputPath) const;
    Q_INVOKABLE void openRow(int row);
    Q_INVOKABLE void openItem(int row);
    Q_INVOKABLE void revealInFileManager(int row);
    Q_INVOKABLE void openInTerminal();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList selectedPaths() const;
    Q_INVOKABLE QVariantMap storageInfoForPath(const QString &rootPath) const;
    Q_INVOKABLE void ejectDrive(const QString &rootPath);

    Q_INVOKABLE bool rename(int row, const QString &newName);
    Q_INVOKABLE bool renamePath(const QString &oldPath, const QString &newName);
    
    Q_INVOKABLE QVariantList previewBatchRename(const QStringList &paths, const QVariantList &rules);
    Q_INVOKABLE QVariantList applyBatchRename(const QStringList &paths, const QVariantList &rules);

    Q_INVOKABLE bool createFolder(const QString &name);
    Q_INVOKABLE bool createFile(const QString &name);
    Q_INVOKABLE void showProperties(int row);

    // Async media metadata fetch for Details View columns
    // Returns immediately; emits metadataReady(path, map) when done.
    // map keys: "resolution", "duration", "artist", "album", "bitrate"
    Q_INVOKABLE void fetchMetadataAsync(const QString &path);

signals:
    void currentPathChanged();
    void historyChanged();
    void hoveredPathChanged();
    void viewModeChanged();
    void isDeviceRootChanged();
    void revealProperties(const QStringList &paths);
    void revealBatchRename(const QStringList &paths);
    void entryRenamed(const QString &oldPath, const QString &newPath);
    void entryCreated(const QString &path);
    void pathNavigated(const QString &path);
    void contentsChanged(const QString &path);
    void statusMessageChanged();
    void scrollingChanged();
    void ejectFinished(const QString &rootPath, bool success);
    // Emitted on the GUI thread when async metadata finishes
    void metadataReady(const QString &path, const QVariantMap &meta);

private:
    bool openPathInternal(const QString &path, bool addToHistory);
    void pushHistory(const QString &path);
    void setStatusMessage(const QString &message);
    QString fallbackPathForMissing(const QString &path) const;
    void recoverFromMissingPath(const QString &path, const QString &error);

    DirectoryModel m_directoryModel;
    std::unique_ptr<FileProvider> m_fileProvider;
    QString m_hoveredPath;
    QString m_statusMessage;
    QStringList m_backStack;
    QStringList m_forwardStack;
    int m_viewMode = 0;
    bool m_scrolling = false;
    bool m_isDeviceRoot = false;
    ChecksumCalculator m_checksumCalculator;
    BatchRenameEngine m_renameEngine;

    void setIsDeviceRoot(bool value);
};
