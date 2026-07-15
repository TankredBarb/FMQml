#include "AudioTagEditorSession.h"

#include <QtConcurrentRun>

AudioTagEditorSession::AudioTagEditorSession(QObject *parent)
    : AudioTagEditorBackend(parent), m_model(this)
{
    connect(&m_model, &AudioTagEditModel::dirtyCountChanged, this, &AudioTagEditorSession::dirtyCountChanged);
    connect(&m_model, &AudioTagEditModel::recordChanged, this, [this](int index) {
        if (index == m_currentIndex) emit currentRecordChanged();
    });
    connect(&m_loadWatcher, &QFutureWatcher<QVariantList>::finished, this, [this]() {
        m_model.setRecords(m_loadWatcher.result());
        m_currentIndex = m_model.rowCount() > 0 ? 0 : -1;
        m_busy = false;
        emit currentIndexChanged();
        emit currentRecordChanged();
        emit busyChanged();
        emit loadFinished();
    });
    connect(&m_applyWatcher, &QFutureWatcher<QVariantMap>::finished, this, [this]() {
        const QVariantMap result = m_applyWatcher.result();
        m_model.reconcileApplyResults(result.value(QStringLiteral("results")).toList());
        if (!m_model.hasPendingCover()) releaseCoverStaging();
        m_busy = false;
        emit busyChanged();
        emit currentRecordChanged();
        emit applyFinished(result);
    });
    connect(this, &AudioTagEditorBackend::coverLookupFinished, this,
            [this](int requestId, const QVariantMap &result) {
        if (requestId != m_coverLookupRequestId) return;
        m_coverLookupBusy = !result.value(QStringLiteral("finished"), true).toBool();
        m_coverLookupStatus = result.value(QStringLiteral("message")).toString();
        m_coverLookupStatusIsError = !result.value(QStringLiteral("ok")).toBool();
        m_coverLookupCandidates = result.value(QStringLiteral("candidates")).toList();
        emit lookupStateChanged();
    });
    connect(this, &AudioTagEditorBackend::coverDownloadFinished, this,
            [this](int requestId, const QVariantMap &result) {
        if (requestId != m_coverDownloadRequestId) return;
        m_coverDownloadBusy = false;
        m_downloadingImageUrl.clear();
        const bool ok = result.value(QStringLiteral("ok")).toBool();
        m_coverLookupStatusIsError = !ok;
        m_coverLookupStatus = result.value(QStringLiteral("message"),
            ok ? QStringLiteral("Cover art selected.") : QStringLiteral("Cover art download failed.")).toString();
        if (ok) {
            m_model.setCover(m_currentIndex,
                             result.value(QStringLiteral("path")).toString(),
                             result.value(QStringLiteral("source")).toString(), false);
        }
        emit lookupStateChanged();
    });
    connect(this, &AudioTagEditorBackend::lyricsLookupFinished, this,
            [this](int requestId, const QVariantMap &result) {
        if (requestId != m_lyricsLookupRequestId) return;
        m_lyricsLookupBusy = false;
        m_lyricsLookupStatusIsError = !result.value(QStringLiteral("ok")).toBool();
        m_lyricsLookupStatus = result.value(QStringLiteral("message")).toString();
        m_lyricsCandidates = result.value(QStringLiteral("candidates")).toList();
        emit lookupStateChanged();
    });
    connect(this, &AudioTagEditorBackend::tagsLookupFinished, this,
            [this](int requestId, const QVariantMap &result) {
        if (requestId != m_tagLookupRequestId) return;
        m_tagLookupBusy = false;
        const bool ok = result.value(QStringLiteral("ok")).toBool();
        m_tagLookupStatusIsError = !ok;
        const int count = ok
            ? m_model.applyLookupFields(m_currentIndex, result.value(QStringLiteral("fields")).toMap())
            : 0;
        m_tagLookupStatus = ok
            ? result.value(QStringLiteral("message"), QStringLiteral("Tags filled.")).toString()
                + (count > 0 ? QString() : QStringLiteral(" (no fields to fill)"))
            : result.value(QStringLiteral("message"), QStringLiteral("Tag lookup failed.")).toString();
        emit lookupStateChanged();
    });
}

AudioTagEditorSession::~AudioTagEditorSession()
{
    resetLookupState();
    m_loadWatcher.cancel();
    m_applyWatcher.cancel();
    m_loadWatcher.waitForFinished();
    m_applyWatcher.waitForFinished();
}

AudioTagEditModel *AudioTagEditorSession::editModel() { return &m_model; }
int AudioTagEditorSession::currentIndex() const { return m_currentIndex; }
void AudioTagEditorSession::setCurrentIndex(int index)
{
    const int normalized = index >= 0 && index < m_model.rowCount() ? index : -1;
    if (m_currentIndex == normalized) return;
    resetLookupState();
    m_currentIndex = normalized;
    emit currentIndexChanged();
    emit currentRecordChanged();
}
QVariantMap AudioTagEditorSession::currentRecord() const { return m_model.record(m_currentIndex); }
int AudioTagEditorSession::dirtyCount() const { return m_model.dirtyCount(); }
bool AudioTagEditorSession::busy() const { return m_busy; }
QVariantList AudioTagEditorSession::coverLookupCandidates() const { return m_coverLookupCandidates; }
QString AudioTagEditorSession::coverLookupStatus() const { return m_coverLookupStatus; }
bool AudioTagEditorSession::coverLookupStatusIsError() const { return m_coverLookupStatusIsError; }
bool AudioTagEditorSession::coverLookupBusy() const { return m_coverLookupBusy; }
bool AudioTagEditorSession::coverDownloadBusy() const { return m_coverDownloadBusy; }
QString AudioTagEditorSession::downloadingImageUrl() const { return m_downloadingImageUrl; }
QVariantList AudioTagEditorSession::lyricsCandidates() const { return m_lyricsCandidates; }
QString AudioTagEditorSession::lyricsLookupStatus() const { return m_lyricsLookupStatus; }
bool AudioTagEditorSession::lyricsLookupStatusIsError() const { return m_lyricsLookupStatusIsError; }
bool AudioTagEditorSession::lyricsLookupBusy() const { return m_lyricsLookupBusy; }
QString AudioTagEditorSession::tagLookupStatus() const { return m_tagLookupStatus; }
bool AudioTagEditorSession::tagLookupStatusIsError() const { return m_tagLookupStatusIsError; }
bool AudioTagEditorSession::tagLookupBusy() const { return m_tagLookupBusy; }
bool AudioTagEditorSession::lookupBusy() const
{
    return m_coverLookupBusy || m_coverDownloadBusy || m_lyricsLookupBusy || m_tagLookupBusy;
}

void AudioTagEditorSession::load(const QStringList &paths)
{
    if (m_busy) return;
    m_busy = true;
    emit busyChanged();
    m_loadWatcher.setFuture(QtConcurrent::run([this, paths]() { return loadTags(paths); }));
}

bool AudioTagEditorSession::updateCurrentField(const QString &field, const QVariant &value)
{
    return m_model.updateField(m_currentIndex, field, value);
}
bool AudioTagEditorSession::setCurrentCover(const QString &path, const QString &source, bool removeCover)
{
    return m_model.setCover(m_currentIndex, path, source, removeCover);
}
bool AudioTagEditorSession::clearCurrentTags() { return m_model.clearTags(m_currentIndex); }
int AudioTagEditorSession::applyCurrentCoverToAll() { return m_model.applyCoverToAll(m_currentIndex); }
int AudioTagEditorSession::applyLookupFields(const QVariantMap &fields) { return m_model.applyLookupFields(m_currentIndex, fields); }
bool AudioTagEditorSession::hasPendingCover() const { return m_model.hasPendingCover(); }

void AudioTagEditorSession::fetchCoverCandidates()
{
    m_coverLookupCandidates.clear();
    m_coverLookupStatus = QStringLiteral("Searching cover art…");
    m_coverLookupStatusIsError = false;
    m_coverLookupBusy = true;
    emit lookupStateChanged();
    lookupCoverArtAsync(currentRecord(), ++m_coverLookupRequestId);
}

void AudioTagEditorSession::useCoverCandidate(const QVariantMap &candidate)
{
    const QString imageUrl = candidate.value(QStringLiteral("imageUrl")).toString();
    if (imageUrl.isEmpty()) return;
    m_downloadingImageUrl = imageUrl;
    m_coverLookupStatus = QStringLiteral("Downloading cover art…");
    m_coverLookupStatusIsError = false;
    m_coverDownloadBusy = true;
    emit lookupStateChanged();
    downloadCoverArtAsync(imageUrl, ++m_coverDownloadRequestId);
}

void AudioTagEditorSession::fetchLyricsCandidates()
{
    m_lyricsCandidates.clear();
    m_lyricsLookupStatus = QStringLiteral("Searching lyrics…");
    m_lyricsLookupStatusIsError = false;
    m_lyricsLookupBusy = true;
    emit lookupStateChanged();
    lookupLyricsAsync(currentRecord(), ++m_lyricsLookupRequestId);
}

bool AudioTagEditorSession::useLyricsCandidate(const QVariantMap &candidate)
{
    QString lyrics = candidate.value(QStringLiteral("syncedLyrics")).toString();
    if (lyrics.isEmpty()) lyrics = candidate.value(QStringLiteral("plainLyrics")).toString();
    if (lyrics.isEmpty() || !m_model.updateField(m_currentIndex, QStringLiteral("lyrics"), lyrics)) return false;
    m_lyricsCandidates.clear();
    m_lyricsLookupStatus.clear();
    m_lyricsLookupStatusIsError = false;
    emit lookupStateChanged();
    emit lyricsApplied();
    return true;
}

void AudioTagEditorSession::fetchTags()
{
    m_tagLookupStatus = QStringLiteral("Looking up tags on MusicBrainz…");
    m_tagLookupStatusIsError = false;
    m_tagLookupBusy = true;
    emit lookupStateChanged();
    lookupTagsAsync(currentRecord(), ++m_tagLookupRequestId);
}

void AudioTagEditorSession::resetLookupState()
{
    ++m_coverLookupRequestId;
    ++m_coverDownloadRequestId;
    ++m_lyricsLookupRequestId;
    ++m_tagLookupRequestId;
    cancelCoverLookups();
    cancelLyricsLookup();
    cancelTagsLookup();
    m_coverLookupCandidates.clear();
    m_coverLookupStatus.clear();
    m_coverLookupStatusIsError = false;
    m_coverLookupBusy = false;
    m_coverDownloadBusy = false;
    m_downloadingImageUrl.clear();
    m_lyricsCandidates.clear();
    m_lyricsLookupStatus.clear();
    m_lyricsLookupStatusIsError = false;
    m_lyricsLookupBusy = false;
    m_tagLookupStatus.clear();
    m_tagLookupStatusIsError = false;
    m_tagLookupBusy = false;
    emit lookupStateChanged();
}

void AudioTagEditorSession::applyCurrent() { applyItems(m_model.dirtyRecords(m_currentIndex)); }
void AudioTagEditorSession::applyAll() { applyItems(m_model.dirtyRecords()); }

void AudioTagEditorSession::applyItems(const QVariantList &items)
{
    if (items.isEmpty() || m_busy) return;
    m_busy = true;
    emit busyChanged();
    m_applyWatcher.setFuture(QtConcurrent::run([this, items]() { return applyChanges(items); }));
}
