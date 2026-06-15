#pragma once

#include <QString>

namespace WallpaperSetter {

bool canSetWallpaperForPath(const QString &path);
bool setWallpaper(const QString &path, QString *errorMessage = nullptr);

}
