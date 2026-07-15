#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <atomic>

class IsoMountManager;

class QuickLookController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString path READ path NOTIFY pathChanged)
    Q_PROPERTY(QString content READ content NOTIFY contentChanged)
    Q_PROPERTY(QString type READ type NOTIFY typeChanged)
    Q_PROPERTY(QString extension READ extension NOTIFY extensionChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY sizeTextChanged)
    Q_PROPERTY(QString modifiedText READ modifiedText NOTIFY modifiedTextChanged)
    Q_PROPERTY(QString mimeName READ mimeName NOTIFY mimeNameChanged)
    Q_PROPERTY(bool directory READ directory NOTIFY directoryChanged)
    Q_PROPERTY(bool hidden READ hidden NOTIFY hiddenChanged)
    Q_PROPERTY(bool symlink READ symlink NOTIFY symlinkChanged)
    Q_PROPERTY(bool readable READ readable NOTIFY readableChanged)
    Q_PROPERTY(bool writable READ writable NOTIFY writableChanged)
    Q_PROPERTY(bool executable READ executable NOTIFY executableChanged)
    Q_PROPERTY(QString absolutePath READ absolutePath NOTIFY absolutePathChanged)
    Q_PROPERTY(QString parentPath READ parentPath NOTIFY parentPathChanged)
    Q_PROPERTY(QString permissionsText READ permissionsText NOTIFY permissionsTextChanged)
    Q_PROPERTY(QString attributesText READ attributesText NOTIFY attributesTextChanged)
    Q_PROPERTY(int lines READ lines NOTIFY linesChanged)
    Q_PROPERTY(bool textTruncated READ textTruncated NOTIFY textStateChanged)
    Q_PROPERTY(bool fullTextAvailable READ fullTextAvailable NOTIFY textStateChanged)
    Q_PROPERTY(bool textChunked READ textChunked NOTIFY textStateChanged)
    Q_PROPERTY(int textChunkIndex READ textChunkIndex NOTIFY textStateChanged)
    Q_PROPERTY(int textChunkCount READ textChunkCount NOTIFY textStateChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(bool visible READ visible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(QVariantList extraProperties READ extraProperties NOTIFY extraPropertiesChanged)
    Q_PROPERTY(QString audioTitle READ audioTitle NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioArtist READ audioArtist NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioAlbum READ audioAlbum NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioYear READ audioYear NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioTrack READ audioTrack NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioGenre READ audioGenre NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioComment READ audioComment NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioDuration READ audioDuration NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioBitrate READ audioBitrate NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioSampleRate READ audioSampleRate NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioChannels READ audioChannels NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString audioCoverSource READ audioCoverSource NOTIFY audioPropertiesChanged)
    Q_PROPERTY(QString mediaSourceUrl READ mediaSourceUrl NOTIFY mediaSourceUrlChanged)
    Q_PROPERTY(bool hasPdfSupport READ hasPdfSupport CONSTANT)
    Q_PROPERTY(bool hasMultimediaSupport READ hasMultimediaSupport CONSTANT)
    Q_PROPERTY(int imageWidth READ imageWidth NOTIFY imageSizeChanged)
    Q_PROPERTY(int imageHeight READ imageHeight NOTIFY imageSizeChanged)
    Q_PROPERTY(QString imageFormatText READ imageFormatText NOTIFY imageInfoChanged)
    Q_PROPERTY(QString imageColorDepthText READ imageColorDepthText NOTIFY imageInfoChanged)
    Q_PROPERTY(QString imageAlphaChannelText READ imageAlphaChannelText NOTIFY imageInfoChanged)
    Q_PROPERTY(QString imageDpiText READ imageDpiText NOTIFY imageInfoChanged)
    Q_PROPERTY(QString imageColorSpaceText READ imageColorSpaceText NOTIFY imageInfoChanged)
    Q_PROPERTY(QString imagePixelFormatText READ imagePixelFormatText NOTIFY imageInfoChanged)
    Q_PROPERTY(int bookPageIndex READ bookPageIndex NOTIFY bookPageStateChanged)
    Q_PROPERTY(int bookPageCount READ bookPageCount NOTIFY bookPageStateChanged)
    Q_PROPERTY(QString bookCoverSource READ bookCoverSource NOTIFY bookPageStateChanged)
    Q_PROPERTY(QString bookTitle READ bookTitle NOTIFY bookPageStateChanged)
    Q_PROPERTY(QString bookAuthor READ bookAuthor NOTIFY bookPageStateChanged)

public:
    explicit QuickLookController(QObject *parent = nullptr);
    ~QuickLookController() override;

    QString path() const;
    QString content() const;
    QString type() const;
    QString extension() const;
    QString name() const;
    QString sizeText() const;
    QString modifiedText() const;
    QString mimeName() const;
    bool directory() const;
    bool hidden() const;
    bool symlink() const;
    bool readable() const;
    bool writable() const;
    bool executable() const;
    QString absolutePath() const;
    QString parentPath() const;
    QString permissionsText() const;
    QString attributesText() const;
    int lines() const;
    bool textTruncated() const;
    bool fullTextAvailable() const;
    bool textChunked() const;
    int textChunkIndex() const;
    int textChunkCount() const;
    bool loading() const;
    bool visible() const;
    QVariantList extraProperties() const;
    QString audioTitle() const;
    QString audioArtist() const;
    QString audioAlbum() const;
    QString audioYear() const;
    QString audioTrack() const;
    QString audioGenre() const;
    QString audioComment() const;
    QString audioDuration() const;
    QString audioBitrate() const;
    QString audioSampleRate() const;
    QString audioChannels() const;
    QString audioCoverSource() const;
    QString mediaSourceUrl() const;
    bool hasPdfSupport() const;
    bool hasMultimediaSupport() const;
    int imageWidth() const;
    int imageHeight() const;
    QString imageFormatText() const;
    QString imageColorDepthText() const;
    QString imageAlphaChannelText() const;
    QString imageDpiText() const;
    QString imageColorSpaceText() const;
    QString imagePixelFormatText() const;
    int bookPageIndex() const;
    int bookPageCount() const;
    QString bookCoverSource() const;
    QString bookTitle() const;
    QString bookAuthor() const;

    Q_INVOKABLE void preview(const QString &path);
    Q_INVOKABLE QString presentationIconSourceForPath(const QString &path,
                                                      bool directory,
                                                      const QString &suffix,
                                                      const QString &mimeName,
                                                      bool useNativeIcons) const;
    Q_INVOKABLE bool isRemotePreviewContentPath(const QString &path) const;
    Q_INVOKABLE bool canRequestThumbnailForPath(const QString &path) const;
    Q_INVOKABLE void previewDrive(const QVariantMap &drive);
    Q_INVOKABLE void previewSelection(const QStringList &paths);
    Q_INVOKABLE void loadFullText();
    Q_INVOKABLE void loadTextChunk(int chunkIndex);
    Q_INVOKABLE void loadBookContent();
    Q_INVOKABLE void loadBookPage(int pageIndex);
    Q_INVOKABLE void setBookReaderPixelSize(int pixelSize);
    Q_INVOKABLE void unloadBookContent();
    Q_INVOKABLE void setImageMetadataRequested(const QString &scope, bool requested);
    Q_INVOKABLE void refresh();
    void setVisible(bool visible);
    void setIsoMountManager(IsoMountManager *manager);

signals:
    void pathChanged();
    void contentChanged();
    void typeChanged();
    void extensionChanged();
    void nameChanged();
    void sizeTextChanged();
    void modifiedTextChanged();
    void mimeNameChanged();
    void directoryChanged();
    void hiddenChanged();
    void symlinkChanged();
    void readableChanged();
    void writableChanged();
    void executableChanged();
    void absolutePathChanged();
    void parentPathChanged();
    void permissionsTextChanged();
    void attributesTextChanged();
    void linesChanged();
    void textStateChanged();
    void loadingChanged();
    void visibleChanged();
    void extraPropertiesChanged();
    void audioPropertiesChanged();
    void mediaSourceUrlChanged();
    void imageSizeChanged();
    void imageInfoChanged();
    void bookPageStateChanged();

private:
    QString m_path;
    QString m_content;
    QString m_type;
    QString m_extension;
    QString m_name;
    QString m_sizeText;
    QString m_modifiedText;
    QString m_mimeName;
    bool m_directory = false;
    bool m_hidden = false;
    bool m_symlink = false;
    bool m_readable = false;
    bool m_writable = false;
    bool m_executable = false;
    QString m_absolutePath;
    QString m_parentPath;
    QString m_permissionsText;
    QString m_attributesText;
    int m_lines = 0;
    bool m_textTruncated = false;
    bool m_fullTextAvailable = false;
    bool m_textChunked = false;
    int m_textChunkIndex = 0;
    int m_textChunkCount = 0;
    bool m_loading = false;
    bool m_visible = false;
    QVariantList m_extraProperties;
    QString m_audioTitle;
    QString m_audioArtist;
    QString m_audioAlbum;
    QString m_audioYear;
    QString m_audioTrack;
    QString m_audioGenre;
    QString m_audioComment;
    QString m_audioDuration;
    QString m_audioBitrate;
    QString m_audioSampleRate;
    QString m_audioChannels;
    QString m_audioCoverSource;
    int m_imageWidth = 0;
    int m_imageHeight = 0;
    QString m_imageFormatText;
    QString m_imageColorDepthText;
    QString m_imageAlphaChannelText;
    QString m_imageDpiText;
    QString m_imageColorSpaceText;
    QString m_imagePixelFormatText;
    QStringList m_bookPages;
    QStringList m_bookParagraphs;
    int m_bookPageIndex = 0;
    int m_bookReaderPixelSize = 17;
    QString m_bookCoverSource;
    QString m_bookTitle;
    QString m_bookAuthor;
    bool m_bookContentLoading = false;
    int m_bookContentGeneration = 0;
    std::atomic<int> m_previewGeneration{0};
    IsoMountManager *m_isoMountManager = nullptr;
    bool m_previewPaneImageMetadataRequested = true;
    bool m_quickLookImageMetadataRequested = false;
    bool m_imageMetadataLoading = false;
    QString m_imageMetadataLoadedPath;
    QString m_materializedPreviewDir;
    QString m_materializedPreviewLeaseId;
    QString m_materializedPreviewFile;

    void previewPath(const QString &path, bool forceReload);
    bool previewVirtualRoot(const QString &path);
    void previewLocalOrMaterializedFile(const QString &path, int previewGeneration);
    void previewArchiveEntry(const QString &path, int previewGeneration);
    bool imageMetadataRequested() const;
    void requestImageMetadata();
    void requestMetadata(const QString &path, int previewGeneration, int retryAttempt = 0, const QString &expectedPath = {});
    void clearMaterializedPreview();
    void resetAudioProperties();
    void syncAudioProperties(const QVariantList &properties);
    void resetImageInfo();
    void resetBookInfo();
    void syncImageProperties(const QVariantList &properties);
};
