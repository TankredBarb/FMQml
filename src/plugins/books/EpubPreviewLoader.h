#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>

#include "../../preview/PreviewData.h"

class QImage;

namespace PreviewInternal {

struct EpubPackageData {
    QString packagePath;
    QString title;
    QString author;
    QString genre;
    QString date;
    QString language;
    QString annotation;
    QString coverPath;
    QStringList spinePaths;
    QString error;
};

EpubPackageData parseEpubPackageData(const QByteArray &containerXml,
                                     const QByteArray &packageXml);
QStringList parseEpubXhtmlParagraphs(const QByteArray &contents);
BookPreviewData loadEpubPreviewData(const QString &path, bool includeContent);
QImage extractEpubCoverArt(const QString &path);

} // namespace PreviewInternal
