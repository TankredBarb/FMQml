#pragma once

#include <QObject>
#include <QFont>
#include <QStringList>
#include <QVariantMap>

class ThemeController;

class AppSettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool useNativeIcons READ useNativeIcons WRITE setUseNativeIcons NOTIFY useNativeIconsChanged)
    Q_PROPERTY(bool useHighQualitySystemIcons READ useHighQualitySystemIcons WRITE setUseHighQualitySystemIcons NOTIFY useHighQualitySystemIconsChanged)
    Q_PROPERTY(bool showThumbnails READ showThumbnails WRITE setShowThumbnails NOTIFY showThumbnailsChanged)
    Q_PROPERTY(bool ultraLightMode READ ultraLightMode WRITE setUltraLightMode NOTIFY ultraLightModeChanged)
    Q_PROPERTY(bool shellFirstQmlRestore READ shellFirstQmlRestore WRITE setShellFirstQmlRestore NOTIFY shellFirstQmlRestoreChanged)
    Q_PROPERTY(bool previewDetailsRaised READ previewDetailsRaised WRITE setPreviewDetailsRaised NOTIFY previewDetailsRaisedChanged)
    Q_PROPERTY(bool useSystemTrayIcon READ useSystemTrayIcon WRITE setUseSystemTrayIcon NOTIFY useSystemTrayIconChanged)
    Q_PROPERTY(bool allowOnlyOneInstance READ allowOnlyOneInstance WRITE setAllowOnlyOneInstance NOTIFY allowOnlyOneInstanceChanged)
    Q_PROPERTY(QString fontFamily READ fontFamily WRITE setFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(QString resolvedFontFamily READ resolvedFontFamily NOTIFY fontFamilyChanged)
    Q_PROPERTY(int fontScale READ fontScale WRITE setFontScale NOTIFY fontScaleChanged)
    Q_PROPERTY(QStringList availableFontFamilies READ availableFontFamilies CONSTANT)
    Q_PROPERTY(QString appDataLocation READ appDataLocation NOTIFY appDataLocationChanged)
    Q_PROPERTY(QString settingsMaintenanceStatus READ settingsMaintenanceStatus NOTIFY settingsMaintenanceStatusChanged)
    Q_PROPERTY(int settingsFormatVersion READ settingsFormatVersion CONSTANT)

public:
    explicit AppSettingsController(QObject *parent = nullptr);
    void setThemeController(ThemeController *themeController);

    bool useNativeIcons() const;
    void setUseNativeIcons(bool enabled);
    bool useHighQualitySystemIcons() const;
    void setUseHighQualitySystemIcons(bool enabled);
    bool showThumbnails() const;
    void setShowThumbnails(bool enabled);
    bool ultraLightMode() const;
    void setUltraLightMode(bool enabled);
    bool shellFirstQmlRestore() const;
    void setShellFirstQmlRestore(bool enabled);
    bool previewDetailsRaised() const;
    void setPreviewDetailsRaised(bool enabled);
    bool useSystemTrayIcon() const;
    void setUseSystemTrayIcon(bool enabled);
    bool allowOnlyOneInstance() const;
    void setAllowOnlyOneInstance(bool enabled);
    QString fontFamily() const;
    QString resolvedFontFamily() const;
    void setFontFamily(const QString &family);
    int fontScale() const;
    void setFontScale(int scale);
    QStringList availableFontFamilies() const;

    Q_INVOKABLE QVariantMap workspaceState() const;
    Q_INVOKABLE void saveWorkspaceState(const QVariantMap &state);
    Q_INVOKABLE QString safeFolderPath(const QString &path) const;
    Q_INVOKABLE QVariantMap sanitizedWindowGeometry(const QVariantMap &state,
                                                    int fallbackWidth,
                                                    int fallbackHeight) const;
    Q_INVOKABLE void resetWorkspaceState();
    Q_INVOKABLE bool exportSettings(const QString &filePath);
    Q_INVOKABLE bool importSettings(const QString &filePath);
    Q_INVOKABLE bool openAppDataFolder() const;
    Q_INVOKABLE QVariantMap commandUsageStats() const;
    Q_INVOKABLE void recordCommandExecuted(const QString &commandId);
    Q_INVOKABLE void resetCommandUsageStats();
    QString appDataLocation() const;
    QString settingsMaintenanceStatus() const;
    int settingsFormatVersion() const;

signals:
    void workspaceStateChanged();
    void useNativeIconsChanged();
    void useHighQualitySystemIconsChanged();
    void showThumbnailsChanged();
    void ultraLightModeChanged();
    void shellFirstQmlRestoreChanged();
    void previewDetailsRaisedChanged();
    void useSystemTrayIconChanged();
    void allowOnlyOneInstanceChanged();
    void fontFamilyChanged();
    void fontScaleChanged();
    void appDataLocationChanged();
    void settingsMaintenanceStatusChanged();

private:
    QVariantMap appearanceSettings() const;
    void applyAppearanceSettings(const QVariantMap &appearance);
    void applyApplicationFont() const;
    QVariantMap exportWorkspaceState(const QVariantMap &workspace) const;
    QVariantMap importWorkspaceState(const QVariantMap &workspace) const;
    QVariantMap exportableSettings() const;
    void setSettingsMaintenanceStatus(const QString &status);
    QString normalizeLocalPath(const QString &filePath) const;
    QString fallbackFolderPath() const;
    QString safeFolderPathForSave(const QString &path, const QString &previousPath) const;
    bool isRestorableFolderPath(const QString &path) const;
    bool m_useNativeIcons = true;
    bool m_useHighQualitySystemIcons = true;
    bool m_showThumbnails = true;
    bool m_ultraLightMode = false;
    bool m_shellFirstQmlRestore = false;
    bool m_previewDetailsRaised = false;
    bool m_useSystemTrayIcon = false;
    bool m_allowOnlyOneInstance = false;
    QString m_fontFamily;
    int m_fontScale = 100;
    QStringList m_availableFontFamilies;
    QFont m_defaultApplicationFont;
    QString m_settingsMaintenanceStatus;
    ThemeController *m_themeController = nullptr;
};
