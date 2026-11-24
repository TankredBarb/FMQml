#pragma once

#include <QColor>
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThreadPool>
#include <QVariantList>
#include <QVariantMap>

#include <atomic>
#include <memory>

class WallpaperPreviewController final : public QObject {
    Q_OBJECT

    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY stateChanged)
    Q_PROPERTY(QString fileUrl READ fileUrl NOTIFY stateChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY stateChanged)
    Q_PROPERTY(QString fileSizeText READ fileSizeText NOTIFY stateChanged)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY stateChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY stateChanged)
    Q_PROPERTY(QString imageResolutionText READ imageResolutionText NOTIFY stateChanged)
    Q_PROPERTY(int targetScreenWidth READ targetScreenWidth NOTIFY stateChanged)
    Q_PROPERTY(int targetScreenHeight READ targetScreenHeight NOTIFY stateChanged)
    Q_PROPERTY(QString targetScreenResolutionText READ targetScreenResolutionText NOTIFY stateChanged)
    Q_PROPERTY(qreal targetAspectRatio READ targetAspectRatio NOTIFY stateChanged)
    Q_PROPERTY(bool upscalingWarning READ upscalingWarning NOTIFY stateChanged)
    Q_PROPERTY(QString resolutionHintText READ resolutionHintText NOTIFY stateChanged)
    Q_PROPERTY(QString fillMode READ fillMode WRITE setFillMode NOTIFY stateChanged)
    Q_PROPERTY(QVariantList fillModes READ fillModes CONSTANT)
    Q_PROPERTY(bool backgroundOptionsEnabled READ backgroundOptionsEnabled NOTIFY stateChanged)
    Q_PROPERTY(bool blurBackground READ blurBackground WRITE setBlurBackground NOTIFY stateChanged)
    Q_PROPERTY(bool supportsBlur READ supportsBlur NOTIFY stateChanged)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor NOTIFY stateChanged)
    Q_PROPERTY(QVariantList backgroundPresets READ backgroundPresets CONSTANT)
    Q_PROPERTY(int screenIndex READ screenIndex WRITE setScreenIndex NOTIFY stateChanged)
    Q_PROPERTY(QVariantList screens READ screens NOTIFY stateChanged)
    Q_PROPERTY(bool supportsPerScreen READ supportsPerScreen NOTIFY stateChanged)
    Q_PROPERTY(bool showDesktopChrome READ showDesktopChrome WRITE setShowDesktopChrome NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(bool canApply READ canApply NOTIFY stateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
    Q_PROPERTY(bool applying READ applying NOTIFY stateChanged)
    Q_PROPERTY(bool previewReady READ previewReady NOTIFY stateChanged)
    Q_PROPERTY(QString previewImageUrl READ previewImageUrl NOTIFY stateChanged)
    Q_PROPERTY(QString blurImageUrl READ blurImageUrl NOTIFY stateChanged)
    Q_PROPERTY(QString previewErrorText READ previewErrorText NOTIFY stateChanged)

public:
    struct PreviewImageStore {
        QMutex mutex;
        quint64 generation = 0;
        QImage previewImage;
        QImage blurImage;
    };

    explicit WallpaperPreviewController(QObject *parent = nullptr);
    ~WallpaperPreviewController() override;

    std::shared_ptr<PreviewImageStore> previewImageStore() const;

    bool visible() const;
    void setVisible(bool visible);

    QString filePath() const;
    QString fileUrl() const;
    QString fileName() const;
    QString fileSizeText() const;
    int imageWidth() const;
    int imageHeight() const;
    QString imageResolutionText() const;
    int targetScreenWidth() const;
    int targetScreenHeight() const;
    QString targetScreenResolutionText() const;
    qreal targetAspectRatio() const;
    bool upscalingWarning() const;
    QString resolutionHintText() const;
    QString fillMode() const;
    QVariantList fillModes() const;
    bool backgroundOptionsEnabled() const;
    bool blurBackground() const;
    bool supportsBlur() const;
    QColor backgroundColor() const;
    QVariantList backgroundPresets() const;
    int screenIndex() const;
    QVariantList screens() const;
    bool supportsPerScreen() const;
    bool showDesktopChrome() const;
    QString errorText() const;
    bool canApply() const;
    bool loading() const;
    bool applying() const;
    bool previewReady() const;
    QString previewImageUrl() const;
    QString blurImageUrl() const;
    QString previewErrorText() const;

    Q_INVOKABLE void openFor(const QString &path, QObject *panelController = nullptr);
    Q_INVOKABLE void close();
    Q_INVOKABLE void setFillMode(const QString &mode);
    Q_INVOKABLE void setBackgroundColor(const QColor &color);
    Q_INVOKABLE void setBlurBackground(bool enabled);
    Q_INVOKABLE void setScreenIndex(int index);
    Q_INVOKABLE void setShowDesktopChrome(bool visible);
    Q_INVOKABLE bool apply();
    Q_INVOKABLE QVariantMap computeMonitorRect(qreal stageWidth, qreal stageHeight) const;
    Q_INVOKABLE QVariantMap computeImagePreviewMetrics(qreal monitorWidth, qreal monitorHeight) const;

signals:
    void visibleChanged();
    void stateChanged();

private:
    void recomputeDerivedState();
    void clearPreviewImageStore(quint64 generation);

    bool m_visible = false;
    QPointer<QObject> m_panelController;
    QString m_filePath;
    QString m_fileUrl;
    QString m_fileName;
    QString m_fileSizeText;
    int m_imageWidth = 0;
    int m_imageHeight = 0;
    int m_targetScreenWidth = 1920;
    int m_targetScreenHeight = 1080;
    qreal m_targetAspectRatio = 16.0 / 9.0;
    bool m_upscalingWarning = false;
    QString m_resolutionHintText;
    QString m_fillMode = QStringLiteral("fill");
    QVariantList m_fillModes;
    bool m_blurBackground = false;
    bool m_supportsBlur = false;
    QColor m_backgroundColor = QColor(12, 18, 28);
    QVariantList m_backgroundPresets;
    int m_screenIndex = -1;
    QVariantList m_screens;
    QVariantList m_rawScreens;
    bool m_supportsPerScreen = false;
    bool m_showDesktopChrome = true;
    QString m_errorText;
    bool m_valid = false;
    bool m_loading = false;
    bool m_applying = false;
    bool m_previewReady = false;
    QString m_previewImageUrl;
    QString m_blurImageUrl;
    QString m_previewErrorText;

    std::shared_ptr<PreviewImageStore> m_imageStore = std::make_shared<PreviewImageStore>();
    std::shared_ptr<std::atomic<quint64>> m_loadGeneration = std::make_shared<std::atomic<quint64>>(0);
    std::shared_ptr<std::atomic<quint64>> m_applyGeneration = std::make_shared<std::atomic<quint64>>(0);
    QThreadPool m_workerPool;
};
