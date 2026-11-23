#include "WallpaperPreviewController.h"
#include "WallpaperSetter.h"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
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

    // 2. WallpaperPreviewController invalid open
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
                "openFor must set visible and emit visibleChanged")) {
        return 1;
    }
    if (!expect(!controller.canApply() && !controller.errorText().isEmpty(),
                "invalid image must disable apply and populate errorText")) {
        return 1;
    }
    if (!expect(!controller.apply(), "apply must fail on invalid file")) {
        return 1;
    }

    // 3. Valid small image: upscaling warning and fillMode transitions
    controller.openFor(smallImagePath);
    if (!expect(controller.canApply() && controller.errorText().isEmpty(),
                "valid PNG must be applicable with empty errorText")) {
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

    // 5. Large image: no upscaling warning on standard displays
    controller.setFillMode(QStringLiteral("fill"));
    controller.openFor(largeImagePath);
    if (controller.targetScreenWidth() <= 2560 && controller.targetScreenHeight() <= 1440) {
        if (!expect(!controller.upscalingWarning(),
                    "2560x1440 image must not warn about upscaling on <= 2560x1440 display")) {
            return 1;
        }
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

    // 7. Close
    controller.close();
    if (!expect(!controller.visible(), "close must hide controller")) {
        return 1;
    }

    return 0;
}
