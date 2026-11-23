#pragma once

#include <QColor>
#include <QString>
#include <QVariantMap>

namespace WallpaperSetter {

struct Options {
    QString fillMode = QStringLiteral("fill"); // fill, fit, stretch, tile, center
    QColor backgroundColor = QColor(12, 18, 28);
    bool blurBackground = false;
    int screenIndex = -1; // -1 = all screens
};

bool canSetWallpaperForPath(const QString &path);
QVariantMap previewInfo(const QString &path);
bool setWallpaper(const QString &path, QString *errorMessage = nullptr);
bool setWallpaper(const QString &path, const Options &options, QString *errorMessage = nullptr);

}
