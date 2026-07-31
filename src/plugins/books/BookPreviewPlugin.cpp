#include "BookPreviewPlugin.h"

#include "../../core/ArchiveFileProvider.h"
#include "../../core/ArchiveSupport.h"
#include "BookPagination.h"
#include "EpubPreviewLoader.h"
#include "Fb2PreviewLoader.h"

#include <QFileInfo>

using namespace PreviewInternal;

int BuiltinBookPreviewPlugin::bookPreviewApiVersion() const { return FM_BOOK_PREVIEW_PLUGIN_API_VERSION; }
QString BuiltinBookPreviewPlugin::bookPreviewPluginId() const { return QStringLiteral("fm.book-preview"); }
QString BuiltinBookPreviewPlugin::bookPreviewDisplayName() const { return QStringLiteral("EPUB and FB2 Preview"); }

bool BuiltinBookPreviewPlugin::supportsBookPath(const QString &path) const
{
    const QString name = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::archiveFileName(path) : QFileInfo(path).fileName();
    const QString lower = name.toLower();
    return lower.endsWith(QStringLiteral(".epub"))
        || lower.endsWith(QStringLiteral(".fb2"))
        || lower.endsWith(QStringLiteral(".fb2.zip"));
}

BookPreviewData BuiltinBookPreviewPlugin::loadBookPreview(const QString &path, bool includeContent) const
{
    BookPreviewData data;
    if (path.endsWith(QStringLiteral(".epub"), Qt::CaseInsensitive)) {
        data = loadEpubPreviewData(path, includeContent);
        data.format = QStringLiteral("epub");
        return data;
    }
#ifdef HAS_UNOFFICIAL_BIT7Z
    if (ArchiveSupport::isArchivePath(path)) {
        data = loadFb2ArchiveEntryPreviewData(path, includeContent);
        data.format = QStringLiteral("fb2");
        return data;
    }
    if (isFb2ZipPath(path)) {
        data = loadFb2ZipPreviewData(path, includeContent);
        data.format = QStringLiteral("fb2.zip");
        return data;
    }
#endif
    data = loadFb2PreviewData(path, includeContent);
    data.format = QStringLiteral("fb2");
    return data;
}

QImage BuiltinBookPreviewPlugin::extractBookCover(const QString &path) const
{
    if (path.endsWith(QStringLiteral(".epub"), Qt::CaseInsensitive)) return extractEpubCoverArt(path);
#ifdef HAS_UNOFFICIAL_BIT7Z
    if (ArchiveSupport::isArchivePath(path)) {
        ArchiveFileProvider provider;
        auto device = provider.openRead(path);
        return device ? extractFb2CoverArt(device.get()) : QImage{};
    }
    if (isFb2ZipPath(path)) return extractFb2ZipCoverArt(path);
#endif
    return extractFb2CoverArt(path);
}

QStringList BuiltinBookPreviewPlugin::paginateBook(const QStringList &paragraphs, int readerPixelSize) const
{
    return buildBookPages(paragraphs, bookPageCharLimitForPixelSize(readerPixelSize));
}
