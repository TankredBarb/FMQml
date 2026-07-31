#pragma once

#include <QStringList>

namespace PreviewInternal {
inline constexpr int kBookDefaultReaderPixelSize = 17;
int bookPageCharLimitForPixelSize(int pixelSize);
QStringList buildBookPages(const QStringList &paragraphs, int pageCharLimit);
}
