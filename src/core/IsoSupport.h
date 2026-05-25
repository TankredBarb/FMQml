#pragma once

#include <QString>

namespace IsoSupport {

bool isIsoImageExtension(const QString &suffix);
bool isIsoImagePath(const QString &path);
QString displayNameForImagePath(const QString &path);

}
