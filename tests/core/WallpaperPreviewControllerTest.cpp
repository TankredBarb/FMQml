#include "WallpaperPreviewController.h"
#include "WallpaperSetter.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QMutexLocker>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtMath>
#include <cstdio>

namespace {

bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        return false;
    }
    return true;
}

template <typename Predicate>
bool waitUntil(Predicate &&predicate, int timeoutMs = 5000)
{
    QElapsedTimer timer;
    timer.start();
    while (!predicate() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
    }
    return predicate();
}

} // namespace

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!expect(tempDir.isValid(), "QTemporaryDir must be valid")) {
        return 1;
    }

    const QString textPath = tempDir.filePath(QStringLiteral("notes.txt"));
    {
        QFile file(textPath);
        if (!expect(file.open(QIODevice::WriteOnly), "must create text fixture")) {
            return 1;
        }
        file.write("not an image");
    }

    const QString smallImagePath = tempDir.filePath(QStringLiteral("small.png"));
    {
        QImage img(320, 180, QImage::Format_RGB32);
        img.fill(Qt::darkCyan);
        if (!expect(img.save(smallImagePath), "must save small PNG fixture")) {
            return 1;
        }
    }

    const QString largeImagePath = tempDir.filePath(QStringLiteral("large.png"));
    {
        QImage img(2560, 1440, QImage::Format_RGB32);
        img.fill(Qt::darkBlue);
        if (!expect(img.save(largeImagePath), "must save large PNG fixture")) {
            return 1;
        }
    }

    const QString hugeJpegPath = tempDir.filePath(QStringLiteral("huge.jpg"));
    {
        QImage img(3200, 1800, QImage::Format_RGB32);
        img.fill(Qt::darkGreen);
        if (!expect(img.save(hugeJpegPath, "JPG", 85), "must save huge JPEG fixture")) {
            return 1;
        }
    }

    // 1. WallpaperSetter::previewInfo & validation
    if (!expect(!WallpaperSetter::canSetWallpaperForPath(textPath),
                "text file must not be eligible as wallpaper")) {
        return 1;
    }
    if (!expect(!WallpaperSetter::canSetWallpaperForPath(tempDir.filePath(QStringLiteral("missing.png"))),
                "missing file must not be eligible as wallpaper")) {
        return 1;
    }

    const QVariantMap invalidInfo = WallpaperSetter::previewInfo(textPath);
    if (!expect(!invalidInfo.value(QStringLiteral("valid")).toBool(),
                "previewInfo must mark non-image as invalid")) {
        return 1;
    }

    const QVariantMap smallInfo = WallpaperSetter::previewInfo(smallImagePath);
    if (!expect(smallInfo.value(QStringLiteral("valid")).toBool(),
                "previewInfo must mark valid PNG as valid")) {
        return 1;
    }
    if (!expect(smallInfo.value(QStringLiteral("imageWidth")).toInt() == 320
                && smallInfo.value(QStringLiteral("imageHeight")).toInt() == 180,
                "previewInfo must read exact PNG dimensions")) {
        return 1;
    }
    if (!expect(smallInfo.value(QStringLiteral("fileName")).toString() == QStringLiteral("small.png"),
                "previewInfo must expose fileName")) {
        return 1;
    }
    if (!expect(smallInfo.value(QStringLiteral("fileUrl")).toString().startsWith(QStringLiteral("file://")),
                "previewInfo must expose file:// URL")) {
        return 1;
    }

    // 2. WallpaperPreviewController invalid open (async)
    WallpaperPreviewController controller;
    if (!expect(!controller.visible(), "controller starts hidden")) {
        return 1;
    }
    if (!expect(controller.fillModes().size() == 5, "controller exposes 5 scaling modes")) {
        return 1;
    }
    if (!expect(controller.backgroundPresets().size() == 8, "controller exposes 8 color presets")) {
        return 1;
    }

    QSignalSpy visibleSpy(&controller, &WallpaperPreviewController::visibleChanged);
    QSignalSpy stateSpy(&controller, &WallpaperPreviewController::stateChanged);

    controller.openFor(textPath);
    if (!expect(controller.visible() && visibleSpy.count() == 1,
                "openFor must set visible and emit visibleChanged immediately")) {
        return 1;
    }
    if (!expect(controller.loading(), "openFor must enter async loading state immediately")) {
        return 1;
    }
    if (!expect(waitUntil([&]() { return !controller.loading(); }),
                "invalid image open must finish loading asynchronously")) {
        return 1;
    }
    if (!expect(!controller.canApply() && !controller.errorText().isEmpty(),
                "invalid image must disable apply and populate errorText")) {
        return 1;
    }
    if (!expect(!controller.apply() && !controller.applying(),
                "apply must fail synchronously on invalid file without entering applying state")) {
        return 1;
    }

    // 3. Valid small image: async load, upscaling warning, and fillMode transitions
    controller.openFor(smallImagePath);
    if (!expect(controller.loading(), "openFor must enter loading state immediately")) {
        return 1;
    }
    if (!expect(waitUntil([&]() { return !controller.loading(); }),
                "small PNG must finish loading asynchronously")) {
        return 1;
    }
    if (!expect(controller.canApply() && controller.errorText().isEmpty(),
                "valid PNG must be applicable with empty errorText")) {
        return 1;
    }
    if (!expect(controller.previewReady()
                && controller.previewImageUrl().startsWith(QStringLiteral("image://wallpaperpreview/"))
                && controller.blurImageUrl().startsWith(QStringLiteral("image://wallpaperpreview/")),
                "loaded PNG must expose previewReady and image provider URLs")) {
        return 1;
    }
    if (!expect(controller.imageResolutionText() == QStringLiteral("320 × 180"),
                "imageResolutionText must format dimensions")) {
        return 1;
    }
    if (!expect(controller.fillMode() == QStringLiteral("fill"),
                "default fillMode must be fill")) {
        return 1;
    }
    if (!expect(!controller.backgroundOptionsEnabled(),
                "fill mode must not enable background options")) {
        return 1;
    }
    if (controller.targetScreenWidth() > 320 || controller.targetScreenHeight() > 180) {
        if (!expect(controller.upscalingWarning(),
                    "small image on larger display must trigger upscaling warning in fill mode")) {
            return 1;
        }
    }

    controller.setFillMode(QStringLiteral("fit"));
    if (!expect(controller.fillMode() == QStringLiteral("fit") && controller.backgroundOptionsEnabled(),
                "fit mode must enable background options")) {
        return 1;
    }

    controller.setFillMode(QStringLiteral("center"));
    if (!expect(controller.fillMode() == QStringLiteral("center")
                && controller.backgroundOptionsEnabled()
                && !controller.upscalingWarning(),
                "center mode must enable background options and clear upscaling warning")) {
        return 1;
    }

    controller.setFillMode(QStringLiteral("tile"));
    if (!expect(controller.fillMode() == QStringLiteral("tile")
                && !controller.backgroundOptionsEnabled()
                && !controller.upscalingWarning(),
                "tile mode must disable background options and clear upscaling warning")) {
        return 1;
    }

    controller.setFillMode(QStringLiteral("stretch"));
    if (!expect(controller.fillMode() == QStringLiteral("stretch") && !controller.backgroundOptionsEnabled(),
                "stretch mode must disable background options")) {
        return 1;
    }

    const int stateCountBeforeInvalid = stateSpy.count();
    controller.setFillMode(QStringLiteral("bogus-mode"));
    if (!expect(controller.fillMode() == QStringLiteral("stretch")
                && stateSpy.count() == stateCountBeforeInvalid,
                "invalid fillMode must be ignored without emitting stateChanged")) {
        return 1;
    }

    // 4. Background color & desktop chrome toggles
    controller.setBackgroundColor(QColor(QStringLiteral("#1E2430")));
    if (!expect(controller.backgroundColor() == QColor(QStringLiteral("#1E2430")),
                "valid background color must be applied")) {
        return 1;
    }
    controller.setBackgroundColor(QColor());
    if (!expect(controller.backgroundColor() == QColor(QStringLiteral("#1E2430")),
                "invalid QColor must be ignored")) {
        return 1;
    }

    const bool initialChrome = controller.showDesktopChrome();
    controller.setShowDesktopChrome(!initialChrome);
    if (!expect(controller.showDesktopChrome() == !initialChrome,
                "showDesktopChrome must toggle")) {
        return 1;
    }

    // 5. Large PNG image: async downscaled preview + no upscaling warning on standard displays
    controller.setFillMode(QStringLiteral("fill"));
    controller.openFor(largeImagePath);
    if (!expect(waitUntil([&]() { return !controller.loading(); }),
                "large PNG must finish loading asynchronously")) {
        return 1;
    }
    if (!expect(controller.previewReady() && controller.imageWidth() == 2560 && controller.imageHeight() == 1440,
                "large PNG must report original dimensions while preparing preview")) {
        return 1;
    }
    {
        const auto store = controller.previewImageStore();
        QMutexLocker locker(&store->mutex);
        if (!expect(!store->previewImage.isNull()
                    && store->previewImage.width() <= 1600
                    && store->previewImage.height() <= 1600
                    && (store->previewImage.format() == QImage::Format_RGB32
                        || store->previewImage.format() == QImage::Format_ARGB32_Premultiplied),
                    "large PNG previewImage must be capped at <= 1600px in native 32-bit format")) {
            return 1;
        }
        if (!expect(!store->blurImage.isNull()
                    && store->blurImage.width() <= 256
                    && store->blurImage.height() <= 256,
                    "large PNG blurImage must be capped at <= 256px")) {
            return 1;
        }
    }
    if (controller.targetScreenWidth() <= 2560 && controller.targetScreenHeight() <= 1440) {
        if (!expect(!controller.upscalingWarning(),
                    "2560x1440 image must not warn about upscaling on <= 2560x1440 display")) {
            return 1;
        }
    }

    // 5b. Huge JPEG: fast ScaledSize decode + downscaled store
    controller.openFor(hugeJpegPath);
    if (!expect(waitUntil([&]() { return !controller.loading(); }),
                "huge JPEG must finish loading asynchronously")) {
        return 1;
    }
    if (!expect(controller.previewReady() && controller.imageWidth() == 3200 && controller.imageHeight() == 1800,
                "huge JPEG must report full dimensions and previewReady")) {
        return 1;
    }
    {
        const auto store = controller.previewImageStore();
        QMutexLocker locker(&store->mutex);
        if (!expect(!store->previewImage.isNull()
                    && store->previewImage.width() <= 1600
                    && store->previewImage.height() <= 1600
                    && !store->blurImage.isNull()
                    && store->blurImage.width() <= 256
                    && store->blurImage.height() <= 256,
                    "huge JPEG preview and blur images must be bounded")) {
            return 1;
        }
    }

    // 5c. Rapid generation cancellation: second openFor must win
    controller.openFor(hugeJpegPath);
    controller.openFor(smallImagePath);
    if (!expect(waitUntil([&]() { return !controller.loading(); }),
                "rapid openFor switch must settle")) {
        return 1;
    }
    if (!expect(controller.filePath() == smallImagePath
                && controller.imageWidth() == 320
                && controller.imageHeight() == 180,
                "stale huge JPEG load must not overwrite newer small PNG openFor")) {
        return 1;
    }

    // 6. Preview geometry math
    const QVariantMap wideMonitor = controller.computeMonitorRect(600.0, 300.0);
    const qreal wideW = wideMonitor.value(QStringLiteral("width")).toReal();
    const qreal wideH = wideMonitor.value(QStringLiteral("height")).toReal();
    if (!expect(wideW <= 600.0 && wideH <= 300.0 && wideW > 0.0 && wideH > 0.0,
                "computeMonitorRect must fit within wide stage")) {
        return 1;
    }
    const qreal expectedAspect = qBound<qreal>(0.5, controller.targetAspectRatio(), 4.0);
    if (!expect(std::abs((wideW / wideH) - expectedAspect) < 0.05,
                "computeMonitorRect must preserve target display aspect ratio")) {
        return 1;
    }

    const QVariantMap tallMonitor = controller.computeMonitorRect(240.0, 500.0);
    if (!expect(tallMonitor.value(QStringLiteral("width")).toReal() <= 240.0
                && tallMonitor.value(QStringLiteral("height")).toReal() <= 500.0,
                "computeMonitorRect must fit within tall stage")) {
        return 1;
    }

    const QVariantMap imgMetrics = controller.computeImagePreviewMetrics(wideW, wideH);
    if (!expect(imgMetrics.value(QStringLiteral("scaledWidth")).toReal() >= 16.0
                && imgMetrics.value(QStringLiteral("scaledHeight")).toReal() >= 16.0,
                "computeImagePreviewMetrics must respect minimum size floor")) {
        return 1;
    }

    // 7. Close clears cached preview images and resets loading/preview state
    controller.close();
    if (!expect(!controller.visible()
                && !controller.loading()
                && !controller.previewReady()
                && controller.previewImageUrl().isEmpty()
                && controller.blurImageUrl().isEmpty(),
                "close must hide controller and reset preview state")) {
        return 1;
    }
    {
        const auto store = controller.previewImageStore();
        QMutexLocker locker(&store->mutex);
        if (!expect(store->previewImage.isNull() && store->blurImage.isNull(),
                    "close must release cached preview images")) {
            return 1;
        }
    }

    const int stateCountAfterClose = stateSpy.count();
    controller.close();
    if (!expect(stateSpy.count() == stateCountAfterClose,
                "redundant close on already hidden controller must be idempotent")) {
        return 1;
    }

    return 0;
}
