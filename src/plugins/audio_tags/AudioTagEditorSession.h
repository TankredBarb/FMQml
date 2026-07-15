#pragma once

#include "AudioTagEditModel.h"
#include "AudioTagEditorBackend.h"

#include <QFutureWatcher>

class AudioTagEditorSession : public AudioTagEditorBackend
{
    Q_OBJECT
    Q_PROPERTY(AudioTagEditModel *editModel READ editModel CONSTANT)
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QVariantMap currentRecord READ currentRecord NOTIFY currentRecordChanged)
    Q_PROPERTY(int dirtyCount READ dirtyCount NOTIFY dirtyCountChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QVariantList coverLookupCandidates READ coverLookupCandidates NOTIFY lookupStateChanged)
    Q_PROPERTY(QString coverLookupStatus READ coverLookupStatus NOTIFY lookupStateChanged)
    Q_PROPERTY(bool coverLookupStatusIsError READ coverLookupStatusIsError NOTIFY lookupStateChanged)
    Q_PROPERTY(bool coverLookupBusy READ coverLookupBusy NOTIFY lookupStateChanged)
    Q_PROPERTY(bool coverDownloadBusy READ coverDownloadBusy NOTIFY lookupStateChanged)
    Q_PROPERTY(QString downloadingImageUrl READ downloadingImageUrl NOTIFY lookupStateChanged)
    Q_PROPERTY(QVariantList lyricsCandidates READ lyricsCandidates NOTIFY lookupStateChanged)
    Q_PROPERTY(QString lyricsLookupStatus READ lyricsLookupStatus NOTIFY lookupStateChanged)
    Q_PROPERTY(bool lyricsLookupStatusIsError READ lyricsLookupStatusIsError NOTIFY lookupStateChanged)
    Q_PROPERTY(bool lyricsLookupBusy READ lyricsLookupBusy NOTIFY lookupStateChanged)
    Q_PROPERTY(QString tagLookupStatus READ tagLookupStatus NOTIFY lookupStateChanged)
    Q_PROPERTY(bool tagLookupStatusIsError READ tagLookupStatusIsError NOTIFY lookupStateChanged)
    Q_PROPERTY(bool tagLookupBusy READ tagLookupBusy NOTIFY lookupStateChanged)
    Q_PROPERTY(bool lookupBusy READ lookupBusy NOTIFY lookupStateChanged)

public:
    explicit AudioTagEditorSession(QObject *parent = nullptr);
    ~AudioTagEditorSession() override;
    AudioTagEditModel *editModel();
    int currentIndex() const;
    void setCurrentIndex(int index);
    QVariantMap currentRecord() const;
    int dirtyCount() const;
    bool busy() const;
    QVariantList coverLookupCandidates() const;
    QString coverLookupStatus() const;
    bool coverLookupStatusIsError() const;
    bool coverLookupBusy() const;
    bool coverDownloadBusy() const;
    QString downloadingImageUrl() const;
    QVariantList lyricsCandidates() const;
    QString lyricsLookupStatus() const;
    bool lyricsLookupStatusIsError() const;
    bool lyricsLookupBusy() const;
    QString tagLookupStatus() const;
    bool tagLookupStatusIsError() const;
    bool tagLookupBusy() const;
    bool lookupBusy() const;

    Q_INVOKABLE void load(const QStringList &paths);
    Q_INVOKABLE bool updateCurrentField(const QString &field, const QVariant &value);
    Q_INVOKABLE bool setCurrentCover(const QString &coverPath, const QString &previewSource, bool removeCover = false);
    Q_INVOKABLE bool clearCurrentTags();
    Q_INVOKABLE int applyCurrentCoverToAll();
    Q_INVOKABLE int applyLookupFields(const QVariantMap &fields);
    Q_INVOKABLE void applyCurrent();
    Q_INVOKABLE void applyAll();
    Q_INVOKABLE bool hasPendingCover() const;
    Q_INVOKABLE void fetchCoverCandidates();
    Q_INVOKABLE void useCoverCandidate(const QVariantMap &candidate);
    Q_INVOKABLE void fetchLyricsCandidates();
    Q_INVOKABLE bool useLyricsCandidate(const QVariantMap &candidate);
    Q_INVOKABLE void fetchTags();

signals:
    void currentIndexChanged();
    void currentRecordChanged();
    void dirtyCountChanged();
    void busyChanged();
    void loadFinished();
    void applyFinished(const QVariantMap &result);
    void lookupStateChanged();
    void lyricsApplied();

private:
    void applyItems(const QVariantList &items);
    void resetLookupState();

    AudioTagEditModel m_model;
    int m_currentIndex = -1;
    bool m_busy = false;
    QFutureWatcher<QVariantList> m_loadWatcher;
    QFutureWatcher<QVariantMap> m_applyWatcher;
    QVariantList m_coverLookupCandidates;
    QString m_coverLookupStatus;
    bool m_coverLookupStatusIsError = false;
    bool m_coverLookupBusy = false;
    bool m_coverDownloadBusy = false;
    QString m_downloadingImageUrl;
    int m_coverLookupRequestId = 0;
    int m_coverDownloadRequestId = 0;
    QVariantList m_lyricsCandidates;
    QString m_lyricsLookupStatus;
    bool m_lyricsLookupStatusIsError = false;
    bool m_lyricsLookupBusy = false;
    int m_lyricsLookupRequestId = 0;
    QString m_tagLookupStatus;
    bool m_tagLookupStatusIsError = false;
    bool m_tagLookupBusy = false;
    int m_tagLookupRequestId = 0;
};
