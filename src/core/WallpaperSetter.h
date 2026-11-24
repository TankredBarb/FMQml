#pragma once

#include <QColor>
#include <QImage>
#include <QString>
#include <QVariantMap>

#include <functional>

namespace WallpaperSetter {

struct Options {
    QString fillMode = QStringLiteral("fill"); // fill, fit, stretch, tile, center
    QColor backgroundColor = QColor(12, 18, 28);
    bool blurBackground = false;
    int screenIndex = -1; // -1 = all screens
};

struct PreviewLoadResult {
    bool valid = false;
    QString filePath;
    QString fileUrl;
    QString fileName;
    QString fileSizeText;
    int imageWidth = 0;
    int imageHeight = 0;
    QImage previewImage;
    QImage blurImage;
    QString errorText;
    QString previewErrorText;
};

bool canSetWallpaperForPath(const QString &path);
QVariantMap screenCapabilitiesInfo();
PreviewLoadResult probeFileMetadata(const QString &path);
void decodePreviewImages(PreviewLoadResult &result,
                         int maxPreviewDimension = 1600,
                         int maxBlurDimension = 256,
                         const std::function<bool()> &isCancelled = {});
QVariantMap previewInfo(const QString &path);
bool setWallpaper(const QString &path, QString *errorMessage = nullptr);
bool setWallpaper(const QString &path, const Options &options, QString *errorMessage = nullptr);

}
