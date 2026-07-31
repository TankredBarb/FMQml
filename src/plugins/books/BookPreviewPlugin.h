#pragma once

#include <QObject>

#include "../../core/BookPreviewPlugin.h"

class BuiltinBookPreviewPlugin final : public QObject, public BookPreviewPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FM_BOOK_PREVIEW_PLUGIN_IID)
    Q_INTERFACES(BookPreviewPlugin)

public:
    int bookPreviewApiVersion() const override;
    QString bookPreviewPluginId() const override;
    QString bookPreviewDisplayName() const override;
    bool supportsBookPath(const QString &path) const override;
    PreviewInternal::BookPreviewData loadBookPreview(const QString &path, bool includeContent) const override;
    QImage extractBookCover(const QString &path) const override;
    QStringList paginateBook(const QStringList &paragraphs, int readerPixelSize) const override;
};
