#include "WallpaperPreviewController.h"

#include "../core/WallpaperSetter.h"

#include <QMetaObject>
#include <QMutexLocker>
#include <QRunnable>
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

QString extractFileNameFromPath(const QString &path)
{
    const qsizetype slash = qMax(path.lastIndexOf(QLatin1Char('/')), path.lastIndexOf(QLatin1Char('\\')));
    return slash >= 0 ? path.mid(slash + 1) : path;
}

} // namespace

WallpaperPreviewController::WallpaperPreviewController(QObject *parent)
    : QObject(parent)
{
    m_workerPool.setMaxThreadCount(2);

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

WallpaperPreviewController::~WallpaperPreviewController()
{
    ++(*m_loadGeneration);
    ++(*m_applyGeneration);
    clearPreviewImageStore(m_loadGeneration->load());
    m_workerPool.clear();
    m_workerPool.waitForDone();
}

std::shared_ptr<WallpaperPreviewController::PreviewImageStore> WallpaperPreviewController::previewImageStore() const
{
    return m_imageStore;
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
        return m_loading ? QStringLiteral("Loading…") : QStringLiteral("Unknown resolution");
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
bool WallpaperPreviewController::canApply() const { return m_valid && !m_applying; }
bool WallpaperPreviewController::loading() const { return m_loading; }
bool WallpaperPreviewController::applying() const { return m_applying; }
bool WallpaperPreviewController::previewReady() const { return m_previewReady; }
QString WallpaperPreviewController::previewImageUrl() const { return m_previewImageUrl; }
QString WallpaperPreviewController::blurImageUrl() const { return m_blurImageUrl; }
QString WallpaperPreviewController::previewErrorText() const { return m_previewErrorText; }

void WallpaperPreviewController::clearPreviewImageStore(quint64 generation)
{
    if (!m_imageStore) {
        return;
    }
    QMutexLocker locker(&m_imageStore->mutex);
    m_imageStore->generation = generation;
    m_imageStore->previewImage = {};
    m_imageStore->blurImage = {};
}

void WallpaperPreviewController::openFor(const QString &path, QObject *panelController)
{
    const quint64 gen = ++(*m_loadGeneration);
    ++(*m_applyGeneration);
    clearPreviewImageStore(gen);

    m_panelController = panelController;
    m_filePath = path;
    m_fileUrl.clear();
    m_fileName = extractFileNameFromPath(path);
    m_fileSizeText.clear();
    m_imageWidth = 0;
    m_imageHeight = 0;
    m_errorText.clear();
    m_previewErrorText.clear();
    m_previewImageUrl.clear();
    m_blurImageUrl.clear();
    m_valid = false;
    m_previewReady = false;
    m_loading = true;
    m_applying = false;

    const QVariantMap capabilities = WallpaperSetter::screenCapabilitiesInfo();
    m_supportsBlur = capabilities.value(QStringLiteral("supportsBlur")).toBool();
    m_supportsPerScreen = capabilities.value(QStringLiteral("supportsPerScreen")).toBool();
    m_rawScreens = capabilities.value(QStringLiteral("screens")).toList();

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

    recomputeDerivedState();
    emit stateChanged();
    setVisible(true);

    QPointer<WallpaperPreviewController> self(this);
    const std::shared_ptr<PreviewImageStore> store = m_imageStore;
    const std::shared_ptr<std::atomic<quint64>> loadGen = m_loadGeneration;

    m_workerPool.start(QRunnable::create([self, path, gen, store, loadGen]() {
        const auto isCancelled = [loadGen, gen]() {
            return loadGen->load() != gen;
        };
        if (isCancelled()) {
            return;
        }

        WallpaperSetter::PreviewLoadResult meta = WallpaperSetter::probeFileMetadata(path);
        if (isCancelled()) {
            return;
        }

        const bool valid = meta.valid;
        const QString filePath = meta.filePath;
        const QString fileUrl = meta.fileUrl;
        const QString fileName = meta.fileName;
        const QString fileSizeText = meta.fileSizeText;
        const int width = meta.imageWidth;
        const int height = meta.imageHeight;
        const QString errorText = meta.errorText;

        QMetaObject::invokeMethod(self, [self, gen, loadGen, valid, filePath, fileUrl, fileName,
                                         fileSizeText, width, height, errorText]() {
            if (!self || loadGen->load() != gen) {
                return;
            }
            self->m_valid = valid;
            self->m_filePath = filePath;
            self->m_fileUrl = fileUrl;
            self->m_fileName = fileName;
            self->m_fileSizeText = fileSizeText;
            self->m_imageWidth = width;
            self->m_imageHeight = height;
            self->m_errorText = errorText;
            if (!valid) {
                self->m_loading = false;
            }
            self->recomputeDerivedState();
            emit self->stateChanged();
        }, Qt::QueuedConnection);

        if (!valid) {
            return;
        }

        WallpaperSetter::decodePreviewImages(meta, 1600, 256, isCancelled);
        if (isCancelled()) {
            return;
        }

        const bool hasPreview = !meta.previewImage.isNull();
        const bool hasBlur = !meta.blurImage.isNull();
        const QString previewErr = meta.previewErrorText;
        const int decodedWidth = meta.imageWidth;
        const int decodedHeight = meta.imageHeight;

        if (store) {
            QMutexLocker locker(&store->mutex);
            if (isCancelled()) {
                return;
            }
            store->generation = gen;
            store->previewImage = std::move(meta.previewImage);
            store->blurImage = std::move(meta.blurImage);
        }

        QMetaObject::invokeMethod(self, [self, gen, loadGen, hasPreview, hasBlur, previewErr,
                                         decodedWidth, decodedHeight]() {
            if (!self || loadGen->load() != gen) {
                return;
            }
            self->m_loading = false;
            self->m_previewReady = hasPreview;
            self->m_previewErrorText = previewErr;
            if (hasPreview) {
                self->m_previewImageUrl = QStringLiteral("image://wallpaperpreview/%1/preview").arg(gen);
            }
            if (hasBlur) {
                self->m_blurImageUrl = QStringLiteral("image://wallpaperpreview/%1/blur").arg(gen);
            }
            if ((self->m_imageWidth <= 0 || self->m_imageHeight <= 0)
                && decodedWidth > 0 && decodedHeight > 0) {
                self->m_imageWidth = decodedWidth;
                self->m_imageHeight = decodedHeight;
                self->recomputeDerivedState();
            }
            emit self->stateChanged();
        }, Qt::QueuedConnection);
    }));
}

void WallpaperPreviewController::close()
{
    if (!m_visible && !m_loading && !m_previewReady && m_previewImageUrl.isEmpty() && m_blurImageUrl.isEmpty()) {
        return;
    }
    const quint64 gen = ++(*m_loadGeneration);
    clearPreviewImageStore(gen);
    m_loading = false;
    m_previewReady = false;
    m_previewImageUrl.clear();
    m_blurImageUrl.clear();
    emit stateChanged();
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
    if (m_applying) {
        return false;
    }
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

    const QString filePath = m_filePath;
    const quint64 gen = ++(*m_applyGeneration);
    m_applying = true;
    m_errorText.clear();
    emit stateChanged();

    QPointer<WallpaperPreviewController> self(this);
    const std::shared_ptr<std::atomic<quint64>> applyGen = m_applyGeneration;

    m_workerPool.start(QRunnable::create([self, filePath, options, gen, applyGen]() {
        QString errorMessage;
        const bool ok = WallpaperSetter::setWallpaper(filePath, options, &errorMessage);

        QMetaObject::invokeMethod(self, [self, gen, applyGen, ok, errorMessage]() {
            if (!self || applyGen->load() != gen) {
                return;
            }
            self->m_applying = false;
            if (!ok) {
                self->m_errorText = errorMessage.isEmpty()
                    ? QStringLiteral("Failed to set wallpaper.")
                    : errorMessage;
                emit self->stateChanged();
                return;
            }

            self->m_errorText.clear();
            if (self->m_panelController) {
                QMetaObject::invokeMethod(self->m_panelController,
                                          "setPathAsWallpaperStatus",
                                          Q_ARG(QString, QStringLiteral("Wallpaper updated")));
            }
            const quint64 loadGen = ++(*self->m_loadGeneration);
            self->clearPreviewImageStore(loadGen);
            self->m_loading = false;
            self->m_previewReady = false;
            self->m_previewImageUrl.clear();
            self->m_blurImageUrl.clear();
            emit self->stateChanged();
            self->setVisible(false);
        }, Qt::QueuedConnection);
    }));

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
