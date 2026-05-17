#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QSet>
#include <QTimer>
#include <QFileSystemWatcher>
#include <memory>

#include "../core/FileProvider.h"

class DirectoryModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY currentPathChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool showHidden READ showHidden WRITE setShowHidden NOTIFY showHiddenChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        PathRole,
        SizeRole,
        SizeTextRole,
        ModifiedTextRole,
        IsDirectoryRole,
        IsHiddenRole,
        IsSelectedRole,
        IconNameRole,
        SuffixRole,
        IsImageRole
    };
    Q_ENUM(Role)

    explicit DirectoryModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentPath() const;
    bool loading() const;
    QString error() const;
    int count() const;
    int selectedCount() const;
    QString filterText() const;
    void setFilterText(const QString &text);

    bool showHidden() const;
    void setShowHidden(bool show);

    Q_INVOKABLE bool openPath(const QString &path);
    Q_INVOKABLE void refresh();
    bool insertPath(const QString &path);
    bool removePath(const QString &path);
    bool renamePath(const QString &oldPath, const QString &newPath);
    Q_INVOKABLE void toggleSelected(int row);
    Q_INVOKABLE void selectOnly(int row);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirectoryAt(int row) const;
    Q_INVOKABLE int indexOfPath(const QString &path) const;
    Q_INVOKABLE QStringList selectedPaths() const;

signals:
    void currentPathChanged();
    void loadingChanged();
    void showHiddenChanged();
    void errorChanged();
    void directoryUnavailable(const QString &path, const QString &error);
    void countChanged();
    void selectionChanged();
    void filterTextChanged();

private:
    static QString formatSize(qint64 bytes);
    static QString iconNameFor(const FileEntry &entry);
    void setLoading(bool loading);
    void setError(const QString &error);
    void applyFilter();

    void onScannerStarted();
    void onScannerBatchReady(const QList<FileEntry> &entries, int generation);
    void onScannerFinished(const QString &path, bool success, int generation, const QString &error);
    void onDirectoryChanged(const QString &path);
    void onDebounceTimeout();

    QString m_currentPath;
    bool m_loading = false;
    bool m_showHidden = false;
    bool m_freshLoad = false;
    int m_currentScanGeneration = 0; // copied from scanner on scan start, for rejecting stale signals
    QTimer m_debounceTimer;
    QString m_error;
    QString m_filterText;
    QList<FileEntry> m_entries;
    QList<int> m_filteredIndices;
    QList<FileEntry> m_freshLoadBuffer;
    QHash<QString, int> m_entryIndex;
    QSet<QString> m_foundNames;
    int m_selectedCount = 0;
    std::unique_ptr<FileProvider> m_provider;
    QFileSystemWatcher m_watcher;
};
