#include "BookPagination.h"
#include "PreviewData.h"

#include <QtGlobal>

namespace PreviewInternal {
int bookPageCharLimitForPixelSize(int pixelSize)
{
    const int normalizedSize = qBound(10, pixelSize, 28);
    return qBound(1200, (static_cast<int>(kFb2PageCharLimit) * kFb2DefaultReaderPixelSize) / normalizedSize, 7000);
}

QStringList buildBookPages(const QStringList &paragraphs, int pageCharLimit)
{
    QStringList pages;
    QString page;
    for (const QString &paragraph : paragraphs) {
        if (paragraph.isEmpty()) continue;
        const qsizetype nextSize = page.size() + paragraph.size() + (page.isEmpty() ? 0 : 2);
        if (!page.isEmpty() && nextSize > pageCharLimit) {
            pages.append(page.trimmed());
            page.clear();
            if (pages.size() >= kFb2MaxPages) break;
        }
        if (!page.isEmpty()) page.append(QStringLiteral("\n\n"));
        page.append(paragraph);
    }
    if (!page.trimmed().isEmpty() && pages.size() < kFb2MaxPages) pages.append(page.trimmed());
    return pages;
}
}
