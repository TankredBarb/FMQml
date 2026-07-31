#pragma once

#include <QImage>
#include <QString>
#include <QtPlugin>

#include "../preview/PreviewData.h"

inline constexpr int FM_BOOK_PREVIEW_PLUGIN_API_VERSION = 1;

class BookPreviewPlugin
{
public:
    virtual ~BookPreviewPlugin() = default;

    virtual int bookPreviewApiVersion() const = 0;
    virtual QString bookPreviewPluginId() const = 0;
    virtual QString bookPreviewDisplayName() const = 0;
    virtual bool supportsBookPath(const QString &path) const = 0;
    virtual PreviewInternal::BookPreviewData loadBookPreview(const QString &path, bool includeContent) const = 0;
    virtual QImage extractBookCover(const QString &path) const = 0;
};

#define FM_BOOK_PREVIEW_PLUGIN_IID "FM.BookPreviewPlugin/1.0"
Q_DECLARE_INTERFACE(BookPreviewPlugin, FM_BOOK_PREVIEW_PLUGIN_IID)
