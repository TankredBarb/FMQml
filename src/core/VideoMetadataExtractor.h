#pragma once

#include <QVariantList>
#include <QString>

class VideoMetadataExtractor {
public:
    static QVariantList extract(const QString &path);
};
