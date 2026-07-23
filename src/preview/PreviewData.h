#pragma once

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace PreviewInternal {

inline constexpr qint64 kTextPreviewLimit = 8192;
inline constexpr qint64 kTextFullLoadLimit = 1024 * 1024;
inline constexpr qint64 kTextChunkSize = 384 * 1024;
inline constexpr qint64 kArchivePreviewExtractLimit = 1024 * 1024;
inline constexpr qint64 kRemotePreviewMaterializeLimit = 40LL * 1024 * 1024;
inline constexpr int kFb2DefaultReaderPixelSize = 17;
inline constexpr qsizetype kFb2PageCharLimit = 3500;
inline constexpr qsizetype kFb2MaxPages = 2000;
inline constexpr int kAudioMetadataRetryCount = 2;
inline constexpr int kAudioMetadataRetryBaseDelayMs = 140;

struct PreviewData {
    QString content;
    int lines = 0;
    bool truncated = false;
    bool fullTextAvailable = false;
    bool chunked = false;
    int chunkIndex = 0;
    int chunkCount = 0;
};

struct DevicesPreviewData {
    QString sizeText;
    QVariantList extraProperties;
};

struct DrivePreviewData {
    QString name;
    QString extension;
    QString sizeText;
    QString modifiedText;
    QString mimeName;
    QVariantList extraProperties;
};

struct ImageMetadataData {
    QVariantList extraProperties;
    int width = 0;
    int height = 0;
    QString formatText;
    QString colorDepthText;
    QString alphaChannelText;
    QString dpiText;
    QString colorSpaceText;
    QString pixelFormatText;
};

struct LocalPreviewData {
    QString content;
    QString type;
    QString extension;
    QString name;
    QString sizeText;
    QString modifiedText;
    QString mimeName;
    QString absolutePath;
    QString parentPath;
    QString permissionsText;
    QString attributesText;
    QVariantList extraProperties;
    QStringList bookPages;
    QStringList bookParagraphs;
    QString bookCoverSource;
    QString bookTitle;
    QString bookAuthor;
    QString cleanupDir;
    QString cleanupLeaseId;
    QString materializedPath;
    QString metadataPath;
    QString audioCoverSource;
    bool directory = false;
    bool hidden = false;
    bool symlink = false;
    bool readable = false;
    bool writable = false;
    bool executable = false;
    int lines = 0;
    bool textTruncated = false;
    bool fullTextAvailable = false;
    bool textChunked = false;
    int textChunkIndex = 0;
    int textChunkCount = 0;
    int bookPageIndex = 0;
    bool requestMetadata = false;
    bool requestImageMetadata = false;
};

struct Fb2PreviewData {
    QString content;
    QVariantList extraProperties;
    QStringList pages;
    QStringList paragraphs;
    QString coverSource;
    QString title;
    QString author;
    int lines = 0;
    int pageIndex = 0;
};

struct EpubPreviewData {
    QString content;
    QVariantList extraProperties;
    QStringList pages;
    QStringList paragraphs;
    QString coverSource;
    QString title;
    QString author;
    int lines = 0;
    int pageIndex = 0;
};

} // namespace PreviewInternal
