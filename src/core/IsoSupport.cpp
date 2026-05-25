#include "IsoSupport.h"

#include <QFileInfo>

namespace IsoSupport {

bool isIsoImageExtension(const QString &suffix)
{
    const QString lower = suffix.toLower();
    return lower == QLatin1String("iso") || lower == QLatin1String("udf");
}

bool isIsoImagePath(const QString &path)
{
    const QFileInfo info(path);
    return info.isFile() && isIsoImageExtension(info.suffix());
}

QString displayNameForImagePath(const QString &path)
{
    const QFileInfo info(path);
    const QString baseName = info.completeBaseName();
    return baseName.isEmpty() ? info.fileName() : baseName;
}

}
