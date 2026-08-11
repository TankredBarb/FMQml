#pragma once

#include <QObject>
#include <QVariantMap>

class AdminController;
class AppSettingsController;
class DiskUsageController;
class FileSearchController;
class FolderCompareController;
class PluginActionController;
class QuickLookController;
class ThemeController;
class WorkspaceController;

class DebugInformationController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap snapshot READ snapshot NOTIFY snapshotChanged)
    Q_PROPERTY(QString generatedAtText READ generatedAtText NOTIFY snapshotChanged)

public:
    explicit DebugInformationController(QObject *parent = nullptr);

    void setSources(WorkspaceController *workspace,
                    ThemeController *theme,
                    AppSettingsController *settings,
                    AdminController *admin,
                    QuickLookController *quickLook,
                    FileSearchController *fileSearch,
                    DiskUsageController *diskUsage,
                    FolderCompareController *folderCompare,
                    PluginActionController *pluginActions);

    QVariantMap snapshot() const;
    QString generatedAtText() const;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE QString reportText(bool includePaths) const;
    Q_INVOKABLE void copyReport(bool includePaths) const;

    static QString sanitizeText(QString text);

signals:
    void snapshotChanged();

private:
    QVariantMap panelSnapshot(const QString &name, QObject *panel, bool active) const;

    WorkspaceController *m_workspace = nullptr;
    ThemeController *m_theme = nullptr;
    AppSettingsController *m_settings = nullptr;
    AdminController *m_admin = nullptr;
    QuickLookController *m_quickLook = nullptr;
    FileSearchController *m_fileSearch = nullptr;
    DiskUsageController *m_diskUsage = nullptr;
    FolderCompareController *m_folderCompare = nullptr;
    PluginActionController *m_pluginActions = nullptr;
    QVariantMap m_snapshot;
    QString m_generatedAtText;
    int m_fileSearchCount = 0;
    int m_diskAnalysisCount = 0;
    int m_folderComparisonCount = 0;
    int m_fileOperationCount = 0;
    bool m_fileSearchActive = false;
    bool m_diskAnalysisActive = false;
    bool m_folderComparisonActive = false;
};
