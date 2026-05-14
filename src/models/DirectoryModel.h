#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QSet>
#include <QFileSystemWatcher>

#include "../core/DirectoryScanner.h"

struct FileEntry {
    QString name;
    QString path;
    QString suffix;
    qint64 size = 0;
    QDateTime modified;
    bool isDirectory = false;
    bool isHidden = false;
    bool isSelected = false;
};

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
        SuffixRole
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
    Q_INVOKABLE void toggleSelected(int row);
    Q_INVOKABLE void selectOnly(int row);
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE QString pathAt(int row) const;
    Q_INVOKABLE bool isDirectoryAt(int row) const;
    Q_INVOKABLE int indexOfPath(const QString &path) const;
    Q_INVOKABLE QStringList selectedPaths() const;

signals:
    void currentPathChanged();
    void loadingChanged();
    void showHiddenChanged();
    void errorChanged();
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
    void onScannerBatchReady(const QList<FileEntry> &entries);
    void onScannerFinished(const QString &path, bool success, const QString &error);
    void onDirectoryChanged(const QString &path);

    QString m_currentPath;
    bool m_loading = false;
    bool m_showHidden = false;
    QString m_error;
    QString m_filterText;
    QList<FileEntry> m_entries;
    QList<int> m_filteredIndices;
    QHash<QString, int> m_entryIndex;
    QSet<QString> m_foundNames;
    DirectoryScanner m_scanner;
    QFileSystemWatcher m_watcher;
};
