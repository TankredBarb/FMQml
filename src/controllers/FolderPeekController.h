#pragma once

#include <QObject>
#include <QPointer>
#include <QThreadPool>
#include <QVariantList>

#include <atomic>

class FilePanelController;

class FolderPeekController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool open READ isOpen NOTIFY openChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QVariantList breadcrumbs READ breadcrumbs NOTIFY currentPathChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY historyChanged)
    Q_PROPERTY(bool hasMore READ hasMore NOTIFY entriesChanged)
    Q_PROPERTY(quint64 openCount READ openCount NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 cancellationCount READ cancellationCount NOTIFY statisticsChanged)
    Q_PROPERTY(quint64 failureCount READ failureCount NOTIFY statisticsChanged)

public:
    explicit FolderPeekController(QObject *parent = nullptr);
    ~FolderPeekController() override;

    void setSourcePanel(FilePanelController *panel);
    bool isOpen() const;
    QString currentPath() const;
    QString state() const;
    bool loading() const { return m_loading; }
    QVariantList entries() const;
    QVariantList breadcrumbs() const;
    bool canGoBack() const;
    bool hasMore() const { return m_hasMore; }
    quint64 openCount() const { return m_openCount; }
    quint64 cancellationCount() const { return m_cancellationCount; }
    quint64 failureCount() const { return m_failureCount; }

    Q_INVOKABLE void openPath(const QString &path, bool showHidden);
    Q_INVOKABLE void navigate(const QString &path);
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goUp();
    Q_INVOKABLE void close();
    Q_INVOKABLE bool openInSourcePanel();
    Q_INVOKABLE void openEntry(const QString &path, bool isDirectory);

signals:
    void openChanged();
    void currentPathChanged();
    void stateChanged();
    void entriesChanged();
    void historyChanged();
    void statisticsChanged();
    void loadingChanged();

private:
    void load(const QString &path, bool addToHistory, bool popBackOnSuccess = false);
    void publish(quint64 generation, const QString &path, const QString &state,
                 const QVariantList &entries, bool hasMore);
    void startRemoteWarmup(quint64 generation, const QString &path, bool showHidden,
                           int sortRole, Qt::SortOrder sortOrder, bool mixFilesAndFolders,
                           bool commitPending);

    QPointer<FilePanelController> m_sourcePanel;
    QThreadPool m_pool;
    QThreadPool m_warmPool;
    std::atomic<quint64> m_generation{0};
    QString m_currentPath;
    QString m_state = QStringLiteral("idle");
    QString m_pendingPath;
    QVariantList m_entries;
    QStringList m_backStack;
    bool m_open = false;
    bool m_loading = false;
    bool m_pendingAddToHistory = false;
    bool m_pendingPopBack = false;
    bool m_showHidden = false;
    bool m_hasMore = false;
    quint64 m_openCount = 0;
    quint64 m_cancellationCount = 0;
    quint64 m_failureCount = 0;
};
