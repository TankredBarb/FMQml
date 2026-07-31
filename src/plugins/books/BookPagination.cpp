#include "BookPagination.h"
#include "../../preview/PreviewData.h"

#include <QtGlobal>

namespace PreviewInternal {
namespace {
constexpr qsizetype kPageCharLimit = 3500;
constexpr qsizetype kMaxPages = 2000;
}

int bookPageCharLimitForPixelSize(int pixelSize)
{
    const int normalizedSize = qBound(10, pixelSize, 28);
    return qBound(1200, (static_cast<int>(kPageCharLimit) * kBookDefaultReaderPixelSize) / normalizedSize, 7000);
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
            if (pages.size() >= kMaxPages) break;
        }
        if (!page.isEmpty()) page.append(QStringLiteral("\n\n"));
        page.append(paragraph);
    }
    if (!page.trimmed().isEmpty() && pages.size() < kMaxPages) pages.append(page.trimmed());
    return pages;
}
}
