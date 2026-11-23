#include "WallpaperPreviewController.h"

#include "../core/WallpaperSetter.h"

#include <QMetaObject>
#include <QtMath>

namespace {

QVariantMap makeMode(const QString &id, const QString &label, const QString &description)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("label"), label},
        {QStringLiteral("description"), description}
    };
}

QVariantMap makeColorPreset(const QString &label, const QString &hex)
{
    return {
        {QStringLiteral("label"), label},
        {QStringLiteral("color"), QColor(hex)}
    };
}

} // namespace

WallpaperPreviewController::WallpaperPreviewController(QObject *parent)
    : QObject(parent)
{
    m_fillModes = {
        makeMode(QStringLiteral("fill"), QStringLiteral("Fill"), QStringLiteral("Crop to fill screen")),
        makeMode(QStringLiteral("fit"), QStringLiteral("Fit"), QStringLiteral("Keep aspect ratio inside screen")),
        makeMode(QStringLiteral("stretch"), QStringLiteral("Stretch"), QStringLiteral("Stretch to screen edges")),
        makeMode(QStringLiteral("tile"), QStringLiteral("Tile"), QStringLiteral("Repeat pattern across screen")),
        makeMode(QStringLiteral("center"), QStringLiteral("Center"), QStringLiteral("Original size, centered"))
    };

    m_backgroundPresets = {
        makeColorPreset(QStringLiteral("Obsidian"), QStringLiteral("#000000")),
        makeColorPreset(QStringLiteral("Midnight"), QStringLiteral("#0B1220")),
        makeColorPreset(QStringLiteral("Deep Slate"), QStringLiteral("#142234")),
        makeColorPreset(QStringLiteral("Charcoal"), QStringLiteral("#1E2430")),
        makeColorPreset(QStringLiteral("Espresso"), QStringLiteral("#1F1A18")),
        makeColorPreset(QStringLiteral("Graphite"), QStringLiteral("#374151")),
        makeColorPreset(QStringLiteral("Mist"), QStringLiteral("#D8DEE9")),
        makeColorPreset(QStringLiteral("White"), QStringLiteral("#FFFFFF"))
    };
}

bool WallpaperPreviewController::visible() const { return m_visible; }

void WallpaperPreviewController::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    emit visibleChanged();
}

QString WallpaperPreviewController::filePath() const { return m_filePath; }
QString WallpaperPreviewController::fileUrl() const { return m_fileUrl; }
QString WallpaperPreviewController::fileName() const { return m_fileName; }
QString WallpaperPreviewController::fileSizeText() const { return m_fileSizeText; }
int WallpaperPreviewController::imageWidth() const { return m_imageWidth; }
int WallpaperPreviewController::imageHeight() const { return m_imageHeight; }

QString WallpaperPreviewController::imageResolutionText() const
{
    if (m_imageWidth <= 0 || m_imageHeight <= 0) {
        return QStringLiteral("Unknown resolution");
    }
    return QStringLiteral("%1 × %2").arg(m_imageWidth).arg(m_imageHeight);
}

int WallpaperPreviewController::targetScreenWidth() const { return m_targetScreenWidth; }
int WallpaperPreviewController::targetScreenHeight() const { return m_targetScreenHeight; }

QString WallpaperPreviewController::targetScreenResolutionText() const
{
    return QStringLiteral("%1 × %2").arg(m_targetScreenWidth).arg(m_targetScreenHeight);
}

qreal WallpaperPreviewController::targetAspectRatio() const { return m_targetAspectRatio; }
bool WallpaperPreviewController::upscalingWarning() const { return m_upscalingWarning; }
QString WallpaperPreviewController::resolutionHintText() const { return m_resolutionHintText; }
QString WallpaperPreviewController::fillMode() const { return m_fillMode; }
QVariantList WallpaperPreviewController::fillModes() const { return m_fillModes; }

bool WallpaperPreviewController::backgroundOptionsEnabled() const
{
    return m_fillMode == QLatin1String("fit") || m_fillMode == QLatin1String("center");
}

bool WallpaperPreviewController::blurBackground() const { return m_blurBackground; }
bool WallpaperPreviewController::supportsBlur() const { return m_supportsBlur; }
QColor WallpaperPreviewController::backgroundColor() const { return m_backgroundColor; }
QVariantList WallpaperPreviewController::backgroundPresets() const { return m_backgroundPresets; }
int WallpaperPreviewController::screenIndex() const { return m_screenIndex; }
QVariantList WallpaperPreviewController::screens() const { return m_screens; }
bool WallpaperPreviewController::supportsPerScreen() const { return m_supportsPerScreen; }
bool WallpaperPreviewController::showDesktopChrome() const { return m_showDesktopChrome; }
QString WallpaperPreviewController::errorText() const { return m_errorText; }
bool WallpaperPreviewController::canApply() const { return m_valid; }

void WallpaperPreviewController::openFor(const QString &path, QObject *panelController)
{
    m_panelController = panelController;
    m_errorText.clear();

    const QVariantMap info = WallpaperSetter::previewInfo(path);
    m_valid = info.value(QStringLiteral("valid")).toBool();
    m_filePath = info.value(QStringLiteral("filePath")).toString();
    m_fileUrl = info.value(QStringLiteral("fileUrl")).toString();
    m_fileName = info.value(QStringLiteral("fileName")).toString();
    m_fileSizeText = info.value(QStringLiteral("fileSizeText")).toString();
    m_imageWidth = info.value(QStringLiteral("imageWidth")).toInt();
    m_imageHeight = info.value(QStringLiteral("imageHeight")).toInt();
    m_supportsBlur = info.value(QStringLiteral("supportsBlur")).toBool();
    m_supportsPerScreen = info.value(QStringLiteral("supportsPerScreen")).toBool();
    m_rawScreens = info.value(QStringLiteral("screens")).toList();

    m_screens.clear();
    if (m_supportsPerScreen && m_rawScreens.size() > 1) {
        m_screens.append(QVariantMap{
            {QStringLiteral("index"), -1},
            {QStringLiteral("label"), QStringLiteral("All displays")},
            {QStringLiteral("subtitle"), QStringLiteral("%1 displays").arg(m_rawScreens.size())}
        });
        for (const QVariant &entryVar : m_rawScreens) {
            const QVariantMap entry = entryVar.toMap();
            const int idx = entry.value(QStringLiteral("index")).toInt();
            const QString name = entry.value(QStringLiteral("name")).toString();
            const int w = entry.value(QStringLiteral("width")).toInt();
            const int h = entry.value(QStringLiteral("height")).toInt();
            const bool primary = entry.value(QStringLiteral("primary")).toBool();
            m_screens.append(QVariantMap{
                {QStringLiteral("index"), idx},
                {QStringLiteral("label"), primary ? QStringLiteral("%1 (Primary)").arg(name) : name},
                {QStringLiteral("subtitle"), QStringLiteral("%1 × %2").arg(w).arg(h)}
            });
        }
    }

    if (!m_supportsPerScreen || m_screenIndex >= m_rawScreens.size()) {
        m_screenIndex = -1;
    }
    if (!m_supportsBlur) {
        m_blurBackground = false;
    }
    if (!m_valid) {
        m_errorText = QStringLiteral("Wallpaper is only supported for local PNG, JPG, JPEG, or BMP files.");
    }

    recomputeDerivedState();
    emit stateChanged();
    setVisible(true);
}

void WallpaperPreviewController::close()
{
    setVisible(false);
}

void WallpaperPreviewController::setFillMode(const QString &mode)
{
    const QString lower = mode.trimmed().toLower();
    if (lower != QLatin1String("fill")
        && lower != QLatin1String("fit")
        && lower != QLatin1String("stretch")
        && lower != QLatin1String("tile")
        && lower != QLatin1String("center")) {
        return;
    }
    if (m_fillMode == lower) {
        return;
    }
    m_fillMode = lower;
    m_errorText.clear();
    recomputeDerivedState();
    emit stateChanged();
}

void WallpaperPreviewController::setBackgroundColor(const QColor &color)
{
    if (!color.isValid() || m_backgroundColor == color) {
        return;
    }
    m_backgroundColor = color;
    m_errorText.clear();
    emit stateChanged();
}

void WallpaperPreviewController::setBlurBackground(bool enabled)
{
    const bool effective = m_supportsBlur && enabled;
    if (m_blurBackground == effective) {
        return;
    }
    m_blurBackground = effective;
    m_errorText.clear();
    emit stateChanged();
}

void WallpaperPreviewController::setScreenIndex(int index)
{
    const int normalized = (m_supportsPerScreen && index >= 0 && index < m_rawScreens.size()) ? index : -1;
    if (m_screenIndex == normalized) {
        return;
    }
    m_screenIndex = normalized;
    m_errorText.clear();
    recomputeDerivedState();
    emit stateChanged();
}

void WallpaperPreviewController::setShowDesktopChrome(bool visible)
{
    if (m_showDesktopChrome == visible) {
        return;
    }
    m_showDesktopChrome = visible;
    emit stateChanged();
}

bool WallpaperPreviewController::apply()
{
    if (!m_valid || m_filePath.isEmpty()) {
        m_errorText = QStringLiteral("Wallpaper can only be set from a local image file.");
        emit stateChanged();
        return false;
    }

    WallpaperSetter::Options options;
    options.fillMode = m_fillMode;
    options.backgroundColor = m_backgroundColor;
    options.blurBackground = m_blurBackground;
    options.screenIndex = m_screenIndex;

    QString errorMessage;
    if (!WallpaperSetter::setWallpaper(m_filePath, options, &errorMessage)) {
        m_errorText = errorMessage.isEmpty() ? QStringLiteral("Failed to set wallpaper.") : errorMessage;
        emit stateChanged();
        return false;
    }

    m_errorText.clear();
    if (m_panelController) {
        QMetaObject::invokeMethod(m_panelController,
                                  "setPathAsWallpaperStatus",
                                  Q_ARG(QString, QStringLiteral("Wallpaper updated")));
    }
    emit stateChanged();
    setVisible(false);
    return true;
}

QVariantMap WallpaperPreviewController::computeMonitorRect(qreal stageWidth, qreal stageHeight) const
{
    const qreal availW = qMax<qreal>(120.0, stageWidth);
    const qreal availH = qMax<qreal>(80.0, stageHeight);
    const qreal aspect = qBound<qreal>(0.5, m_targetAspectRatio, 4.0);

    qreal width = availW;
    qreal height = width / aspect;
    if (height > availH) {
        height = availH;
        width = height * aspect;
    }
    return {
        {QStringLiteral("width"), qRound(width)},
        {QStringLiteral("height"), qRound(height)}
    };
}

QVariantMap WallpaperPreviewController::computeImagePreviewMetrics(qreal monitorWidth, qreal monitorHeight) const
{
    const qreal monW = qMax<qreal>(1.0, monitorWidth);
    const qreal monH = qMax<qreal>(1.0, monitorHeight);
    const qreal screenW = qMax<qreal>(1.0, m_targetScreenWidth);
    const qreal screenH = qMax<qreal>(1.0, m_targetScreenHeight);
    const qreal scaleX = monW / screenW;
    const qreal scaleY = monH / screenH;
    const qreal imgW = m_imageWidth > 0 ? static_cast<qreal>(m_imageWidth) : screenW;
    const qreal imgH = m_imageHeight > 0 ? static_cast<qreal>(m_imageHeight) : screenH;

    // For Center and Tile, scale image proportionally to the preview monitor size
    const qreal scaledW = qMax<qreal>(16.0, qRound(imgW * scaleX));
    const qreal scaledH = qMax<qreal>(16.0, qRound(imgH * scaleY));

    return {
        {QStringLiteral("scaledWidth"), scaledW},
        {QStringLiteral("scaledHeight"), scaledH}
    };
}

void WallpaperPreviewController::recomputeDerivedState()
{
    QVariantMap selectedScreen;
    if (m_screenIndex >= 0 && m_screenIndex < m_rawScreens.size()) {
        selectedScreen = m_rawScreens.at(m_screenIndex).toMap();
    } else {
        for (const QVariant &entryVar : m_rawScreens) {
            const QVariantMap entry = entryVar.toMap();
            if (entry.value(QStringLiteral("primary")).toBool()) {
                selectedScreen = entry;
                break;
            }
        }
        if (selectedScreen.isEmpty() && !m_rawScreens.isEmpty()) {
            selectedScreen = m_rawScreens.first().toMap();
        }
    }

    m_targetScreenWidth = qMax(1, selectedScreen.value(QStringLiteral("width"), 1920).toInt());
    m_targetScreenHeight = qMax(1, selectedScreen.value(QStringLiteral("height"), 1080).toInt());
    m_targetAspectRatio = static_cast<qreal>(m_targetScreenWidth) / static_cast<qreal>(m_targetScreenHeight);

    m_upscalingWarning = false;
    m_resolutionHintText.clear();
    if (m_imageWidth > 0 && m_imageHeight > 0) {
        if (m_fillMode == QLatin1String("center") || m_fillMode == QLatin1String("tile")) {
            m_resolutionHintText = QStringLiteral("Displayed at native pixel scale");
        } else if (m_imageWidth < m_targetScreenWidth || m_imageHeight < m_targetScreenHeight) {
            m_upscalingWarning = true;
            m_resolutionHintText = QStringLiteral("Smaller than display — will upscale");
        } else if (m_imageWidth == m_targetScreenWidth && m_imageHeight == m_targetScreenHeight) {
            m_resolutionHintText = QStringLiteral("Exact display match");
        } else {
            m_resolutionHintText = QStringLiteral("High resolution for display");
        }
    }
}
