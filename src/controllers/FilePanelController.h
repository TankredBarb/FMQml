#pragma once

#include <QObject>
#include <QStringList>

#include "../models/DirectoryModel.h"

class FilePanelController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(DirectoryModel *directoryModel READ directoryModel CONSTANT)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool canGoForward READ canGoForward NOTIFY historyChanged)
    Q_PROPERTY(QString hoveredPath READ hoveredPath WRITE setHoveredPath NOTIFY hoveredPathChanged)

public:
    explicit FilePanelController(QObject *parent = nullptr);

    DirectoryModel *directoryModel();
    QString currentPath() const;
    bool canGoBack() const;
    bool canGoForward() const;
    QString hoveredPath() const;
    void setHoveredPath(const QString &path);

    Q_INVOKABLE void openPath(const QString &path);
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
    Q_INVOKABLE bool createFolder(const QString &name);
    Q_INVOKABLE void showProperties(int row);

signals:
    void currentPathChanged();
    void historyChanged();
    void hoveredPathChanged();
    void revealProperties(const QString &path);

private:
    void openPathInternal(const QString &path, bool addToHistory);
    void pushHistory(const QString &path);

    DirectoryModel m_directoryModel;
    QString m_hoveredPath;
    QStringList m_backStack;
    QStringList m_forwardStack;
};
