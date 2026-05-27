#pragma once

#include <QObject>
#include <QVariantMap>

class AppSettingsController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool useNativeIcons READ useNativeIcons WRITE setUseNativeIcons NOTIFY useNativeIconsChanged)
    Q_PROPERTY(bool useHighQualitySystemIcons READ useHighQualitySystemIcons WRITE setUseHighQualitySystemIcons NOTIFY useHighQualitySystemIconsChanged)
    Q_PROPERTY(bool showThumbnails READ showThumbnails WRITE setShowThumbnails NOTIFY showThumbnailsChanged)
    Q_PROPERTY(bool simplifyVisualsForPerformance READ simplifyVisualsForPerformance WRITE setSimplifyVisualsForPerformance NOTIFY simplifyVisualsForPerformanceChanged)

public:
    explicit AppSettingsController(QObject *parent = nullptr);

    bool useNativeIcons() const;
    void setUseNativeIcons(bool enabled);
    bool useHighQualitySystemIcons() const;
    void setUseHighQualitySystemIcons(bool enabled);
    bool showThumbnails() const;
    void setShowThumbnails(bool enabled);
    bool simplifyVisualsForPerformance() const;
    void setSimplifyVisualsForPerformance(bool enabled);

    Q_INVOKABLE QVariantMap workspaceState() const;
    Q_INVOKABLE void saveWorkspaceState(const QVariantMap &state);
    Q_INVOKABLE QString safeFolderPath(const QString &path) const;
    Q_INVOKABLE QVariantMap sanitizedWindowGeometry(const QVariantMap &state,
                                                    int fallbackWidth,
                                                    int fallbackHeight) const;
    Q_INVOKABLE void resetWorkspaceState();

signals:
    void workspaceStateChanged();
    void useNativeIconsChanged();
    void useHighQualitySystemIconsChanged();
    void showThumbnailsChanged();
    void simplifyVisualsForPerformanceChanged();

private:
    QString fallbackFolderPath() const;
    bool isRestorableFolderPath(const QString &path) const;
    bool m_useNativeIcons = true;
    bool m_useHighQualitySystemIcons = true;
    bool m_showThumbnails = true;
    bool m_simplifyVisualsForPerformance = true;
};
