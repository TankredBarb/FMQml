#pragma once

#include <QStringList>

namespace PreviewInternal {
int bookPageCharLimitForPixelSize(int pixelSize);
QStringList buildBookPages(const QStringList &paragraphs, int pageCharLimit);
}
