#pragma once

#include <QObject>
#include <QStringList>
#include <memory>

#include "../core/FileProvider.h"
#include "../models/DirectoryModel.h"

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

public:
    explicit FilePanelController(QObject *parent = nullptr);

    int viewMode() const;
    void setViewMode(int mode);

    DirectoryModel *directoryModel();
    QString currentPath() const;
    bool canGoBack() const;
    bool canGoForward() const;
    QString hoveredPath() const;
    void setHoveredPath(const QString &path);
    QString statusMessage() const;
    bool scrolling() const;
    void setScrolling(bool scrolling);
    QString fileNameForPath(const QString &path) const;
    QString parentPathForPath(const QString &path) const;
    QString childPathForCurrent(const QString &name) const;
    QString childPathForPath(const QString &parentPath, const QString &name) const;

    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE void openRow(int row);
    Q_INVOKABLE void openItem(int row);
    Q_INVOKABLE void revealInFileManager(int row);
    Q_INVOKABLE void openInTerminal();
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList selectedPaths() const;

    Q_INVOKABLE bool rename(int row, const QString &newName);
    Q_INVOKABLE bool renamePath(const QString &oldPath, const QString &newName);
    Q_INVOKABLE bool createFolder(const QString &name);
    Q_INVOKABLE bool createFile(const QString &name);
    Q_INVOKABLE void showProperties(int row);

signals:
    void currentPathChanged();
    void historyChanged();
    void hoveredPathChanged();
    void viewModeChanged();
    void revealProperties(const QString &path);
    void entryRenamed(const QString &oldPath, const QString &newPath);
    void entryCreated(const QString &path);
    void pathNavigated(const QString &path);
    void contentsChanged(const QString &path);
    void statusMessageChanged();
    void scrollingChanged();

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
};
