#include "AudioTagEditorBackend.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QMimeDatabase>
#include <QMimeType>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>

#include <algorithm>
#include <limits>

#include <taglib/attachedpictureframe.h>
#include <taglib/audioproperties.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/fileref.h>
#include <taglib/id3v2tag.h>
#include <taglib/id3v2framefactory.h>
#include <taglib/mpegfile.h>
#include <taglib/mp4coverart.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/tag.h>
#include <taglib/tbytevector.h>
#include <taglib/tpropertymap.h>

#include "CleanupSubsystem.h"

namespace {
constexpr int kLookupTimeoutMs = 6000;
constexpr int kCoverDownloadTimeoutMs = 15000;
constexpr int kLyricsTimeoutMs = 20000;
constexpr int kCoverDownloadLimit = 8 * 1024 * 1024;
constexpr int kCoverCandidateLimit = 6;
constexpr int kLyricsCandidateLimit = 8;
constexpr const char *kLookupUserAgent =
    "FMQml audio-tag-editor/0.2 ( https://musicbrainz.org/ )";

// Converts a QString path into a TagLib::FileName-compatible native pointer.
// Lifetime: the returned pointer is only valid while the NativeFilePath lives,
// which covers the TagLib file open + read/save usage we use here.
struct NativeFilePath
{
#ifdef Q_OS_WIN
    std::wstring storage;
    const wchar_t *ptr;
    explicit NativeFilePath(const QString &path)
        : storage(path.toStdWString()), ptr(storage.c_str()) {}
#else
    QByteArray storage;
    const char *ptr;
    explicit NativeFilePath(const QString &path)
        : storage(path.toUtf8()), ptr(storage.constData()) {}
#endif
};

bool isMp4Suffix(const QString &suffix)
{
    return suffix == QLatin1String("mp4")
        || suffix == QLatin1String("m4a")
        || suffix == QLatin1String("m4b");
}

bool coverWriteSupported(const QString &suffix)
{
    return suffix == QLatin1String("mp3")
        || suffix == QLatin1String("flac")
        || isMp4Suffix(suffix);
}

QString tagString(const TagLib::String &value)
{
    return QString::fromStdWString(value.toWString());
}

TagLib::String toTagString(const QVariantMap &item, QStringView key)
{
    const QByteArray utf8 = item.value(key.toString()).toString().toUtf8();
    return TagLib::String(utf8.constData(), TagLib::String::UTF8);
}

QString titleCaseWords(QString value)
{
    bool wordStart = true;
    for (qsizetype i = 0; i < value.size(); ++i) {
        if (value.at(i).isLetterOrNumber()) {
            if (wordStart) {
                value[i] = value.at(i).toUpper();
            }
            wordStart = false;
        } else {
            wordStart = true;
        }
    }
    return value;
}

bool saveTags(TagLib::FileRef &file)
{
    if (auto *mpeg = dynamic_cast<TagLib::MPEG::File *>(file.file())) {
        return mpeg->save(TagLib::MPEG::File::ID3v2,
                          TagLib::File::StripOthers,
                          TagLib::ID3v2::v4,
                          TagLib::File::DoNotDuplicate);
    }
    return file.save();
}

unsigned int uintField(const QVariantMap &item, QStringView key)
{
    bool ok = false;
    const uint value = item.value(key.toString()).toString().trimmed().toUInt(&ok);
    return ok ? value : 0;
}

QString propertyValue(const TagLib::PropertyMap &properties, const TagLib::String &key)
{
    if (!properties.contains(key)) {
        return {};
    }
    const TagLib::StringList values = properties[key];
    if (values.isEmpty()) {
        return {};
    }
    return tagString(values.front());
}

QString coverSourceForPath(const QString &path)
{
    return QStringLiteral("image://thumbnail/")
        + QString::fromUtf8(QUrl::toPercentEncoding(path + QStringLiteral("::cover")));
}

TagLib::ByteVector byteVectorFromByteArray(const QByteArray &bytes)
{
    return TagLib::ByteVector(bytes.constData(), static_cast<unsigned int>(bytes.size()));
}

QByteArray readCoverBytes(const QString &coverPath, QString *error)
{
    QFile file(coverPath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cover image could not be opened.");
        }
        return {};
    }

    const QByteArray data = file.readAll();
    if (data.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Cover image is empty.");
        }
        return {};
    }
    return data;
}

QString coverMimeType(const QString &coverPath, const QByteArray &bytes)
{
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForData(bytes);
    if (mime.name().startsWith(QStringLiteral("image/"))) {
        return mime.name();
    }

    const QString suffix = QFileInfo(coverPath).suffix().toLower();
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")) {
        return QStringLiteral("image/jpeg");
    }
    if (suffix == QLatin1String("png")) {
        return QStringLiteral("image/png");
    }
    return {};
}

bool hasEmbeddedCover(const QString &path, const QString &suffix)
{
    const NativeFilePath p(path);
    if (suffix == QLatin1String("mp3")) {
        TagLib::MPEG::File file(p.ptr);
        return file.isValid()
            && file.ID3v2Tag()
            && file.ID3v2Tag()->frameListMap().contains("APIC")
            && !file.ID3v2Tag()->frameListMap()["APIC"].isEmpty();
    }

    if (suffix == QLatin1String("flac")) {
        TagLib::FLAC::File file(p.ptr);
        return file.isValid() && !file.pictureList().isEmpty();
    }

    if (isMp4Suffix(suffix)) {
        TagLib::MP4::File file(p.ptr);
        return file.isValid()
            && file.tag()
            && file.tag()->itemMap().contains("covr")
            && !file.tag()->itemMap()["covr"].toCoverArtList().isEmpty();
    }

    return false;
}

bool writeMp3Cover(const QString &path, const QByteArray &bytes, const QString &mime,
                   bool removeCover, QString *error)
{
    const NativeFilePath p(path);
    TagLib::MPEG::File file(p.ptr);
    if (!file.isValid()) {
        if (error) {
            *error = QStringLiteral("MP3 file is invalid.");
        }
        return false;
    }

    TagLib::ID3v2::Tag *tag = file.ID3v2Tag(true);
    if (!tag) {
        if (error) {
            *error = QStringLiteral("ID3v2 tag is not available.");
        }
        return false;
    }

    tag->removeFrames("APIC");
    if (!removeCover) {
        auto *frame = new TagLib::ID3v2::AttachedPictureFrame;
        frame->setType(TagLib::ID3v2::AttachedPictureFrame::FrontCover);
        frame->setMimeType(TagLib::String(mime.toStdString(), TagLib::String::UTF8));
        frame->setPicture(byteVectorFromByteArray(bytes));
        tag->addFrame(frame);
    }
    if (!file.save(TagLib::MPEG::File::ID3v2,
                   TagLib::File::StripOthers,
                   TagLib::ID3v2::v4,
                   TagLib::File::DoNotDuplicate)) {
        return false;
    }

    TagLib::MPEG::File verification(p.ptr);
    const bool hasCover = verification.isValid()
        && verification.ID3v2Tag()
        && verification.ID3v2Tag()->frameListMap().contains("APIC")
        && !verification.ID3v2Tag()->frameListMap()["APIC"].isEmpty();
    if (hasCover == !removeCover) {
        return true;
    }
    if (error) {
        *error = QStringLiteral("TagLib reported success, but the MP3 cover was not written. The file container may be malformed.");
    }
    return false;
}

bool writeFlacCover(const QString &path, const QByteArray &bytes, const QString &mime,
                    bool removeCover, QString *error)
{
    const NativeFilePath p(path);
    TagLib::FLAC::File file(p.ptr);
    if (!file.isValid()) {
        if (error) {
            *error = QStringLiteral("FLAC file is invalid.");
        }
        return false;
    }

    file.removePictures();
    if (!removeCover) {
        auto *picture = new TagLib::FLAC::Picture;
        picture->setType(TagLib::FLAC::Picture::FrontCover);
        picture->setMimeType(TagLib::String(mime.toStdString(), TagLib::String::UTF8));
        picture->setData(byteVectorFromByteArray(bytes));
        file.addPicture(picture);
    }
    return file.save();
}

bool writeMp4Cover(const QString &path, const QByteArray &bytes, bool removeCover, QString *error)
{
    const NativeFilePath p(path);
    TagLib::MP4::File file(p.ptr);
    if (!file.isValid() || !file.tag()) {
        if (error) {
            *error = QStringLiteral("MP4 tag is not available.");
        }
        return false;
    }

    if (removeCover) {
        file.tag()->removeItem("covr");
    } else {
        TagLib::MP4::CoverArt::Format format = TagLib::MP4::CoverArt::JPEG;
        const QString mime = coverMimeType(path, bytes);
        if (mime == QLatin1String("image/png")) {
            format = TagLib::MP4::CoverArt::PNG;
        }
        TagLib::MP4::CoverArtList covers;
        covers.append(TagLib::MP4::CoverArt(format, byteVectorFromByteArray(bytes)));
        file.tag()->setItem("covr", TagLib::MP4::Item(covers));
    }
    return file.save();
}

bool applyCoverChange(const QString &audioPath, const QVariantMap &item, QString *error)
{
    const QString suffix = QFileInfo(audioPath).suffix().toLower();
    if (!coverWriteSupported(suffix)) {
        if (error) {
            *error = QStringLiteral("Cover writing is supported for MP3, FLAC, M4A, M4B, and MP4 only.");
        }
        return false;
    }

    const bool removeCover = item.value(QStringLiteral("removeCover")).toBool();
    QByteArray coverBytes;
    QString mime;
    if (!removeCover) {
        const QString coverPath = item.value(QStringLiteral("coverImagePath")).toString();
        coverBytes = readCoverBytes(coverPath, error);
        if (coverBytes.isEmpty()) {
            return false;
        }
        mime = coverMimeType(coverPath, coverBytes);
        if (mime != QLatin1String("image/jpeg") && mime != QLatin1String("image/png")) {
            if (error) {
                *error = QStringLiteral("Cover image must be JPEG or PNG.");
            }
            return false;
        }
    }

    if (suffix == QLatin1String("mp3")) {
        return writeMp3Cover(audioPath, coverBytes, mime, removeCover, error);
    }
    if (suffix == QLatin1String("flac")) {
        return writeFlacCover(audioPath, coverBytes, mime, removeCover, error);
    }
    return writeMp4Cover(audioPath, coverBytes, removeCover, error);
}

bool validateCoverChange(const QString &audioPath, const QVariantMap &item, QString *error)
{
    const QString suffix = QFileInfo(audioPath).suffix().toLower();
    if (!coverWriteSupported(suffix)) {
        if (error) {
            *error = QStringLiteral("Cover writing is supported for MP3, FLAC, M4A, M4B, and MP4 only.");
        }
        return false;
    }

    if (item.value(QStringLiteral("removeCover")).toBool()) {
        return true;
    }

    const QString coverPath = item.value(QStringLiteral("coverImagePath")).toString();
    const QByteArray coverBytes = readCoverBytes(coverPath, error);
    if (coverBytes.isEmpty()) {
        return false;
    }

    const QString mime = coverMimeType(coverPath, coverBytes);
    if (mime != QLatin1String("image/jpeg") && mime != QLatin1String("image/png")) {
        if (error) {
            *error = QStringLiteral("Cover image must be JPEG or PNG.");
        }
        return false;
    }

    return true;
}

QVariantMap failureResult(const QString &path, const QString &message)
{
    return {
        {QStringLiteral("path"), path},
        {QStringLiteral("ok"), false},
        {QStringLiteral("message"), message},
    };
}

QString jsonString(const QJsonObject &object, QStringView key)
{
    return object.value(key.toString()).toString();
}

QNetworkRequest jsonRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kLookupUserAgent));
    request.setRawHeader("Accept", "application/json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QNetworkRequest imageRequest(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kLookupUserAgent));
    request.setRawHeader("Accept", "image/jpeg,image/png,image/*;q=0.8");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    return request;
}

QVariantMap lookupFailure(const QString &message)
{
    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("message"), message},
        {QStringLiteral("candidates"), QVariantList{}},
    };
}

QString trimmed(const QString &value)
{
    return value.trimmed();
}

bool containsCi(const QString &haystack, const QString &needle)
{
    return !needle.isEmpty() && haystack.contains(needle, Qt::CaseInsensitive);
}

QString previewLyrics(const QString &full)
{
    if (full.isEmpty()) {
        return {};
    }
    QString simplified = full;
    simplified.replace(QLatin1Char('\r'), QLatin1String(""));
    if (simplified.length() <= 400) {
        return simplified;
    }
    return simplified.left(397) + QStringLiteral("…");
}
} // namespace

AudioTagEditorBackend::AudioTagEditorBackend(QObject *parent)
    : QObject(parent)
{
    TagLib::ID3v2::FrameFactory::instance()->setDefaultTextEncoding(TagLib::String::UTF8);
    m_network = new QNetworkAccessManager(this);
}

AudioTagEditorBackend::~AudioTagEditorBackend()
{
    abortReplies(m_coverReplies);
    abortReplies(m_lyricsReplies);
    abortReplies(m_tagReplies);
    if (m_coverDownloadReply) {
        m_coverDownloadReply->abort();
        m_coverDownloadReply = nullptr;
    }
    releaseCoverStaging();
}

void AudioTagEditorBackend::abortReplies(ReplyList &replies)
{
    for (const QPointer<QNetworkReply> &reply : replies) {
        if (reply) {
            reply->abort();
        }
    }
    replies.clear();
}

void AudioTagEditorBackend::dropReply(ReplyList &replies, QNetworkReply *reply)
{
    for (auto it = replies.begin(); it != replies.end(); ++it) {
        if (it->data() == reply) {
            replies.erase(it);
            return;
        }
    }
}

void AudioTagEditorBackend::registerCoverReply(QNetworkReply *reply)
{
    m_coverReplies.emplace_back(reply);
}

void AudioTagEditorBackend::registerLyricsReply(QNetworkReply *reply)
{
    m_lyricsReplies.emplace_back(reply);
}

void AudioTagEditorBackend::registerTagReply(QNetworkReply *reply)
{
    m_tagReplies.emplace_back(reply);
}

void AudioTagEditorBackend::registerCoverDownloadReply(QNetworkReply *reply)
{
    if (m_coverDownloadReply) {
        m_coverDownloadReply->abort();
        m_coverDownloadReply->deleteLater();
    }
    m_coverDownloadReply = reply;
}

QString AudioTagEditorBackend::ensureCoverStagingDir()
{
    if (!m_coverStagingDir.isEmpty()) {
        return m_coverStagingDir;
    }

    const QString parent = StagingLocationPolicy::defaultCleanupRoot();
    if (parent.isEmpty()) {
        return {};
    }

    const QString operationId = QStringLiteral("audio-tag-editor-%1")
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    m_coverStagingDir = CleanupSubsystem::instance().allocateStagingDirectory(
        CleanupArtifactKind::AudioTagCover,
        parent,
        operationId,
        &m_coverStagingLeaseId);
    return m_coverStagingDir;
}

void AudioTagEditorBackend::releaseCoverStaging()
{
    if (!m_coverStagingLeaseId.isEmpty()) {
        CleanupSubsystem::instance().scheduleDelete(m_coverStagingLeaseId);
    }
    m_coverStagingLeaseId.clear();
    m_coverStagingDir.clear();
}

QString AudioTagEditorBackend::localPathFromUrl(const QString &value) const
{
    const QUrl url(value);
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return value;
}

QVariantList AudioTagEditorBackend::loadTags(const QStringList &paths) const
{
    QVariantList result;
    result.reserve(paths.size());

    for (const QString &path : paths) {
        QVariantMap item{
            {QStringLiteral("path"), path},
            {QStringLiteral("name"), QFileInfo(path).fileName()},
            {QStringLiteral("ok"), false},
            {QStringLiteral("dirty"), false},
            {QStringLiteral("coverDirty"), false},
            {QStringLiteral("removeCover"), false},
        };

        const NativeFilePath native(path);
        TagLib::FileRef file(native.ptr);
        if (file.isNull() || !file.tag()) {
            item.insert(QStringLiteral("error"), QStringLiteral("Tags are not available for this file."));
            result.append(item);
            continue;
        }

        TagLib::Tag *tag = file.tag();
        const QString suffix = QFileInfo(path).suffix().toLower();
        item.insert(QStringLiteral("ok"), true);
        item.insert(QStringLiteral("title"), tagString(tag->title()));
        item.insert(QStringLiteral("artist"), tagString(tag->artist()));
        item.insert(QStringLiteral("album"), tagString(tag->album()));
        item.insert(QStringLiteral("year"), tag->year() > 0 ? QString::number(tag->year()) : QString());
        item.insert(QStringLiteral("track"), tag->track() > 0 ? QString::number(tag->track()) : QString());
        item.insert(QStringLiteral("genre"), tagString(tag->genre()));
        item.insert(QStringLiteral("comment"), tagString(tag->comment()));
        item.insert(QStringLiteral("lyrics"), propertyValue(file.properties(), "LYRICS"));
        item.insert(QStringLiteral("durationSec"),
                    file.audioProperties() ? file.audioProperties()->lengthInSeconds() : 0);
        item.insert(QStringLiteral("hasCover"), hasEmbeddedCover(path, suffix));
        item.insert(QStringLiteral("coverWriteSupported"), coverWriteSupported(suffix));
        item.insert(QStringLiteral("coverSource"), coverSourceForPath(path));
        result.append(item);
    }

    return result;
}

QVariantMap AudioTagEditorBackend::applyChanges(const QVariantList &items) const
{
    QVariantList fileResults;
    QStringList thumbnailInvalidationPaths;
    int changedCount = 0;
    int failedCount = 0;

    for (const QVariant &value : items) {
        const QVariantMap item = value.toMap();
        const QString path = item.value(QStringLiteral("path")).toString();
        if (path.isEmpty() || !item.value(QStringLiteral("dirty")).toBool()) {
            continue;
        }

        if (item.value(QStringLiteral("coverDirty")).toBool()) {
            QString coverError;
            if (!validateCoverChange(path, item, &coverError)) {
                ++failedCount;
                fileResults.append(failureResult(path, coverError.isEmpty()
                    ? QStringLiteral("Cover art could not be saved.")
                    : coverError));
                continue;
            }
        }

        // Finish and close the metadata handle before reopening the same file
        // for cover editing. Keeping two TagLib file objects alive across an
        // ID3 resize can leave the second operation working from stale offsets.
        {
            const NativeFilePath native(path);
            TagLib::FileRef file(native.ptr);
            if (file.isNull() || !file.tag()) {
                ++failedCount;
                fileResults.append(failureResult(path, QStringLiteral("Tags are not available for this file.")));
                continue;
            }

            TagLib::Tag *tag = file.tag();
            const bool clearAllTags = item.value(QStringLiteral("clearAllTags")).toBool();
            TagLib::PropertyMap properties = clearAllTags
                ? TagLib::PropertyMap{}
                : file.properties();
            const QString lyrics = item.value(QStringLiteral("lyrics")).toString();
            if (!clearAllTags && lyrics.trimmed().isEmpty()) {
                properties.erase("LYRICS");
            } else if (!lyrics.trimmed().isEmpty()) {
                const QByteArray lyricsUtf8 = lyrics.toUtf8();
                properties.replace("LYRICS",
                                   TagLib::StringList(TagLib::String(lyricsUtf8.constData(),
                                                                    TagLib::String::UTF8)));
            }
            file.setProperties(properties);

            tag->setTitle(toTagString(item, QStringLiteral("title")));
            tag->setArtist(toTagString(item, QStringLiteral("artist")));
            tag->setAlbum(toTagString(item, QStringLiteral("album")));
            tag->setYear(uintField(item, QStringLiteral("year")));
            tag->setTrack(uintField(item, QStringLiteral("track")));
            QVariantMap normalizedItem = item;
            normalizedItem.insert(QStringLiteral("genre"),
                                  titleCaseWords(item.value(QStringLiteral("genre")).toString()));
            tag->setGenre(toTagString(normalizedItem, QStringLiteral("genre")));
            tag->setComment(toTagString(item, QStringLiteral("comment")));

            if (!saveTags(file)) {
                ++failedCount;
                fileResults.append(failureResult(path, QStringLiteral("TagLib could not save this file.")));
                continue;
            }
        }

        if (item.value(QStringLiteral("coverDirty")).toBool()) {
            QString coverError;
            if (!applyCoverChange(path, item, &coverError)) {
                ++failedCount;
                fileResults.append(failureResult(path, coverError.isEmpty()
                    ? QStringLiteral("Cover art could not be saved.")
                    : coverError));
                continue;
            }
        }

        ++changedCount;
        thumbnailInvalidationPaths.append(path);
        fileResults.append(QVariantMap{
            {QStringLiteral("path"), path},
            {QStringLiteral("ok"), true},
            {QStringLiteral("message"), QStringLiteral("Saved.")},
        });
    }

    const bool ok = failedCount == 0;
    return {
        {QStringLiteral("ok"), ok},
        {QStringLiteral("refreshCurrentPath"), changedCount > 0},
        {QStringLiteral("changedCount"), changedCount},
        {QStringLiteral("failedCount"), failedCount},
        {QStringLiteral("thumbnailInvalidationPaths"), thumbnailInvalidationPaths},
        {QStringLiteral("results"), fileResults},
        {QStringLiteral("message"),
         failedCount > 0
             ? QStringLiteral("%1 file(s) saved, %2 failed.").arg(changedCount).arg(failedCount)
             : QStringLiteral("%1 file(s) saved.").arg(changedCount)},
    };
}

namespace {

QTimer *makeAbortTimer(int timeoutMs, QObject *parent, QNetworkReply *reply)
{
    auto *timer = new QTimer(parent);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, timer, [reply]() {
        if (reply) {
            reply->abort();
        }
    });
    timer->start(timeoutMs);
    return timer;
}

} // namespace

void AudioTagEditorBackend::lookupCoverArtAsync(const QVariantMap &item, int requestId)
{
    cancelCoverLookups();

    const QString artist = trimmed(item.value(QStringLiteral("artist")).toString());
    const QString album = trimmed(item.value(QStringLiteral("album")).toString());
    if (artist.isEmpty() || album.isEmpty()) {
        emit coverLookupFinished(requestId, lookupFailure(QStringLiteral("Artist and album are required for cover lookup.")));
        return;
    }

    QUrl url(QStringLiteral("https://musicbrainz.org/ws/2/release"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("limit"), QString::number(kCoverCandidateLimit));
    query.addQueryItem(QStringLiteral("query"),
                       QStringLiteral("artist:\"%1\" AND release:\"%2\"").arg(artist, album));
    url.setQuery(query);

    QNetworkReply *reply = m_network->get(jsonRequest(url));
    registerCoverReply(reply);
    auto *timer = makeAbortTimer(kLookupTimeoutMs, reply, reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, requestId, album] {
        timer->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            const QString message = reply->error() == QNetworkReply::OperationCanceledError
                ? QStringLiteral("Cover lookup timed out.")
                : reply->errorString();
            dropReply(m_coverReplies, reply);
            reply->deleteLater();
            emit coverLookupFinished(requestId, lookupFailure(message.isEmpty()
                ? QStringLiteral("Cover lookup failed.")
                : message));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        dropReply(m_coverReplies, reply);
        reply->deleteLater();
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit coverLookupFinished(requestId, lookupFailure(QStringLiteral("Cover lookup returned invalid data.")));
            return;
        }

        const QJsonArray releases = document.object().value(QStringLiteral("releases")).toArray();
        if (releases.isEmpty()) {
            emit coverLookupFinished(requestId, lookupFailure(QStringLiteral("No matching releases found on MusicBrainz.")));
            return;
        }

        // Build candidates with direct Cover Art Archive URLs — no second
        // round of JSON requests. The thumbnail (front-250) and full image
        // (front) URLs are constructed from the MusicBrainz release MBID.
        // QML loads thumbnails lazily via Image{asynchronous:true}; if a
        // release has no cover, CAA returns 404 and the delegate shows a
        // placeholder.
        QVariantList candidates;
        for (const QJsonValue &value : releases) {
            const QJsonObject release = value.toObject();
            const QString id = jsonString(release, QStringLiteral("id"));
            if (id.isEmpty()) {
                continue;
            }

            const QString title = jsonString(release, QStringLiteral("title"));
            const QString date = jsonString(release, QStringLiteral("date"));
            const int score = release.value(QStringLiteral("score")).toInt();

            candidates.append(QVariantMap{
                {QStringLiteral("id"), id},
                {QStringLiteral("title"), title.isEmpty() ? album : title},
                {QStringLiteral("subtitle"), date.isEmpty()
                    ? QStringLiteral("MusicBrainz score %1").arg(score)
                    : QStringLiteral("%1 — score %2").arg(date).arg(score)},
                {QStringLiteral("thumbnailSource"),
                 QStringLiteral("https://coverartarchive.org/release/%1/front-250").arg(id)},
                {QStringLiteral("imageUrl"),
                 QStringLiteral("https://coverartarchive.org/release/%1/front").arg(id)},
                {QStringLiteral("source"), QStringLiteral("Cover Art Archive")},
            });
        }

        emit coverLookupFinished(requestId, QVariantMap{
            {QStringLiteral("ok"), !candidates.isEmpty()},
            {QStringLiteral("finished"), true},
            {QStringLiteral("message"),
             candidates.isEmpty()
                 ? QStringLiteral("No cover candidates found.")
                 : QStringLiteral("%1 cover candidate(s) found.").arg(candidates.size())},
            {QStringLiteral("candidates"), candidates},
        });
    });
}

void AudioTagEditorBackend::cancelCoverLookups()
{
    abortReplies(m_coverReplies);
    if (m_coverDownloadReply) {
        m_coverDownloadReply->abort();
        m_coverDownloadReply->deleteLater();
        m_coverDownloadReply = nullptr;
    }
}

void AudioTagEditorBackend::downloadCoverArtAsync(const QString &imageUrl, int requestId)
{
    if (m_coverDownloadReply) {
        m_coverDownloadReply->abort();
        m_coverDownloadReply->deleteLater();
        m_coverDownloadReply = nullptr;
    }

    const QUrl url(imageUrl);
    if (!url.isValid()) {
        emit coverDownloadFinished(requestId, QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("message"), QStringLiteral("Cover URL is invalid.")},
        });
        return;
    }

    QNetworkReply *reply = m_network->get(imageRequest(url));
    registerCoverDownloadReply(reply);
    connect(reply, &QNetworkReply::downloadProgress, reply,
            [reply](qint64 processed, qint64 total) {
        if (processed > kCoverDownloadLimit || total > kCoverDownloadLimit) {
            reply->abort();
        }
    });
    auto *timer = makeAbortTimer(kCoverDownloadTimeoutMs, reply, reply);

    connect(reply, &QNetworkReply::finished, this, [reply, timer, requestId, imageUrl, this] {
        timer->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            const QString message = reply->error() == QNetworkReply::OperationCanceledError
                ? QStringLiteral("Cover download timed out.")
                : reply->errorString();
            if (m_coverDownloadReply == reply) {
                m_coverDownloadReply = nullptr;
            }
            reply->deleteLater();
            emit coverDownloadFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"),
                 message.isEmpty() ? QStringLiteral("Cover download failed.") : message},
            });
            return;
        }

        const QByteArray bytes = reply->readAll();
        if (m_coverDownloadReply == reply) {
            m_coverDownloadReply = nullptr;
        }
        reply->deleteLater();
        if (bytes.isEmpty() || bytes.size() > kCoverDownloadLimit) {
            emit coverDownloadFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("Downloaded cover is empty or too large.")},
            });
            return;
        }

        const QString mime = coverMimeType({}, bytes);
        QString extension;
        if (mime == QLatin1String("image/jpeg")) {
            extension = QStringLiteral("jpg");
        } else if (mime == QLatin1String("image/png")) {
            extension = QStringLiteral("png");
        } else {
            emit coverDownloadFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("Downloaded cover is not JPEG or PNG.")},
            });
            return;
        }

        const QByteArray hash =
            QCryptographicHash::hash(imageUrl.toUtf8(), QCryptographicHash::Sha256).toHex();
        const QString stagingDir = ensureCoverStagingDir();
        if (stagingDir.isEmpty()) {
            emit coverDownloadFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("Temporary cover storage is unavailable.")},
            });
            return;
        }
        const QString path = QDir(stagingDir)
                                 .filePath(QString::fromLatin1(hash) + QLatin1Char('.') + extension);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) {
            emit coverDownloadFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("Downloaded cover could not be cached.")},
            });
            return;
        }

        emit coverDownloadFinished(requestId, QVariantMap{
            {QStringLiteral("ok"), true},
            {QStringLiteral("path"), path},
            {QStringLiteral("source"), QUrl::fromLocalFile(path).toString(QUrl::FullyEncoded)},
            {QStringLiteral("message"), QStringLiteral("Cover art selected.")},
        });
    });
}

namespace {

struct LyricCandidate
{
    QString id;
    QString trackName;
    QString artistName;
    QString albumName;
    int duration = 0;
    bool hasSynced = false;
    bool hasPlain = false;
    QString plainLyrics;
    QString syncedLyrics;
    int score = 0;
};

QString lyricMatchKey(const QString &value)
{
    QString key = value.toCaseFolded();
    for (qsizetype i = 0; i < key.size(); ++i) {
        if (!key.at(i).isLetterOrNumber()) {
            key[i] = QLatin1Char(' ');
        }
    }
    return key.simplified();
}

int lyricMatchRank(const QString &candidate, const QString &query)
{
    const QString candidateKey = lyricMatchKey(candidate);
    const QString queryKey = lyricMatchKey(query);
    if (candidateKey.isEmpty() || queryKey.isEmpty()) {
        return 0;
    }
    if (candidateKey == queryKey) {
        return 2;
    }
    if (candidateKey.contains(queryKey) || queryKey.contains(candidateKey)) {
        return 1;
    }
    return 0;
}

int lyricScore(const LyricCandidate &c,
               const QString &artist,
               const QString &track,
               const QString &album,
               int duration)
{
    const int artistRank = lyricMatchRank(c.artistName, artist);
    const int trackRank = lyricMatchRank(c.trackName, track);
    if (artistRank == 0 || trackRank == 0) {
        return -1;
    }

    int score = artistRank * 100 + trackRank * 120;
    score += lyricMatchRank(c.albumName, album) * 25;
    if (duration > 0 && c.duration > 0) {
        const int difference = std::abs(duration - c.duration);
        if (difference <= 2) {
            score += 60;
        } else if (difference <= 5) {
            score += 30;
        } else if (difference <= 10) {
            score += 10;
        } else if (difference > 20) {
            score -= 30;
        }
    }
    return score + (c.hasSynced ? 5 : 0);
}

} // namespace

void AudioTagEditorBackend::lookupLyricsAsync(const QVariantMap &item, int requestId)
{
    cancelLyricsLookup();

    const QString artist = trimmed(item.value(QStringLiteral("artist")).toString());
    const QString album = trimmed(item.value(QStringLiteral("album")).toString());
    const QString title = trimmed(item.value(QStringLiteral("title")).toString());
    const int duration = item.value(QStringLiteral("durationSec")).toInt();

    // Prefer Title (track name) which most reliably maps to a track lookup; if
    // absent, fall back to using the file base name stem as a weak hint.
    QString trackName = title;
    if (trackName.isEmpty()) {
        const QString path = item.value(QStringLiteral("path")).toString();
        const QString base = QFileInfo(path).completeBaseName();
        if (!base.isEmpty()) {
            trackName = base;
        }
    }

    if (artist.isEmpty() || trackName.isEmpty()) {
        emit lyricsLookupFinished(requestId, QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("finished"), true},
            {QStringLiteral("message"), QStringLiteral("Artist and title are required for lyrics lookup.")},
            {QStringLiteral("candidates"), QVariantList{}},
        });
        return;
    }

    QUrl url(QStringLiteral("https://lrclib.net/api/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("artist_name"), artist);
    query.addQueryItem(QStringLiteral("track_name"), trackName);
    url.setQuery(query);

    QNetworkReply *reply = m_network->get(jsonRequest(url));
    registerLyricsReply(reply);
    auto *timer = makeAbortTimer(kLyricsTimeoutMs, reply, reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, requestId, artist, trackName, album, duration]() {
        timer->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            const QString message = reply->error() == QNetworkReply::OperationCanceledError
                ? QStringLiteral("Lyrics lookup timed out.")
                : reply->errorString();
            dropReply(m_lyricsReplies, reply);
            reply->deleteLater();
            emit lyricsLookupFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("finished"), true},
                {QStringLiteral("message"),
                 message.isEmpty() ? QStringLiteral("Lyrics lookup failed.") : message},
                {QStringLiteral("candidates"), QVariantList{}},
            });
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        dropReply(m_lyricsReplies, reply);
        reply->deleteLater();
        if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
            emit lyricsLookupFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("finished"), true},
                {QStringLiteral("message"), QStringLiteral("Lyrics lookup returned invalid data.")},
                {QStringLiteral("candidates"), QVariantList{}},
            });
            return;
        }

        const QJsonArray items = document.array();
        std::vector<LyricCandidate> candidates;
        candidates.reserve(static_cast<std::size_t>(items.size()));

        for (const QJsonValue &value : items) {
            const QJsonObject obj = value.toObject();
            LyricCandidate c;
            c.id = QString::number(obj.value(QStringLiteral("id")).toInt());
            c.trackName = jsonString(obj, QStringLiteral("trackName"));
            c.artistName = jsonString(obj, QStringLiteral("artistName"));
            c.albumName = jsonString(obj, QStringLiteral("albumName"));
            c.duration = obj.value(QStringLiteral("duration")).toInt();
            c.plainLyrics = jsonString(obj, QStringLiteral("plainLyrics"));
            c.syncedLyrics = jsonString(obj, QStringLiteral("syncedLyrics"));
            c.hasPlain = !c.plainLyrics.trimmed().isEmpty();
            c.hasSynced = !c.syncedLyrics.trimmed().isEmpty();
            if (!c.hasPlain && !c.hasSynced) {
                continue;
            }
            c.score = lyricScore(c, artist, trackName, album, duration);
            if (c.score < 0) {
                continue;
            }
            candidates.push_back(std::move(c));
        }

        std::sort(candidates.begin(), candidates.end(),
                  [](const LyricCandidate &a, const LyricCandidate &b) {
                      return a.score > b.score;
                  });
        if (candidates.size() > kLyricsCandidateLimit) {
            candidates.resize(kLyricsCandidateLimit);
        }

        QVariantList result;
        result.reserve(static_cast<int>(candidates.size()));
        for (const LyricCandidate &c : candidates) {
            const QString chosen = c.hasSynced ? c.syncedLyrics : c.plainLyrics;
            result.append(QVariantMap{
                {QStringLiteral("id"), c.id},
                {QStringLiteral("title"), c.trackName},
                {QStringLiteral("artist"), c.artistName},
                {QStringLiteral("album"), c.albumName},
                {QStringLiteral("durationSec"), c.duration},
                {QStringLiteral("hasSynced"), c.hasSynced},
                {QStringLiteral("hasPlain"), c.hasPlain},
                {QStringLiteral("source"), QStringLiteral("LRCLIB")},
                {QStringLiteral("preview"), previewLyrics(chosen)},
                {QStringLiteral("plainLyrics"), c.plainLyrics},
                {QStringLiteral("syncedLyrics"), c.syncedLyrics},
            });
        }

        emit lyricsLookupFinished(requestId, QVariantMap{
            {QStringLiteral("ok"), !result.isEmpty()},
            {QStringLiteral("finished"), true},
            {QStringLiteral("message"),
             result.isEmpty()
                 ? QStringLiteral("No reliable lyrics match found.")
                 : QStringLiteral("%1 lyric candidate(s) found.").arg(result.size())},
            {QStringLiteral("candidates"), result},
        });
    });
}

void AudioTagEditorBackend::cancelLyricsLookup()
{
    abortReplies(m_lyricsReplies);
}

namespace {

struct TagCandidate
{
    QString mbid;
    QString title;
    QString artist;
    QString album;
    QString year;
    QString track;
    QString genre;
    int completeness = 0;
    int queryScore = 0;
    bool artistMatches = false;
    bool albumMatches = false;
};

QString bestGenreName(const QJsonArray &values)
{
    int bestCount = std::numeric_limits<int>::min();
    QString bestName;
    for (const QJsonValue &value : values) {
        const QJsonObject entry = value.toObject();
        const QString name = jsonString(entry, QStringLiteral("name"));
        const int count = entry.value(QStringLiteral("count")).toInt();
        if (!name.isEmpty() && count > bestCount) {
            bestCount = count;
            bestName = name;
        }
    }
    return titleCaseWords(bestName);
}

QString genreFromRecordingDetails(const QJsonObject &recording)
{
    QString genre = bestGenreName(recording.value(QStringLiteral("genres")).toArray());
    if (genre.isEmpty()) {
        genre = bestGenreName(recording.value(QStringLiteral("tags")).toArray());
    }
    if (!genre.isEmpty()) {
        return genre;
    }

    const QJsonArray credits = recording.value(QStringLiteral("artist-credit")).toArray();
    for (const QJsonValue &value : credits) {
        const QJsonObject artist = value.toObject().value(QStringLiteral("artist")).toObject();
        genre = bestGenreName(artist.value(QStringLiteral("genres")).toArray());
        if (genre.isEmpty()) {
            genre = bestGenreName(artist.value(QStringLiteral("tags")).toArray());
        }
        if (!genre.isEmpty()) {
            return genre;
        }
    }
    return {};
}

QString albumMatchKey(const QString &value)
{
    QString key = value.toCaseFolded();
    for (qsizetype i = 0; i < key.size(); ++i) {
        if (!key.at(i).isLetterOrNumber()) {
            key[i] = QLatin1Char(' ');
        }
    }
    return key.simplified();
}

int albumMatchRank(const QString &candidate, const QString &query)
{
    const QString candidateKey = albumMatchKey(candidate);
    const QString queryKey = albumMatchKey(query);
    if (candidateKey.isEmpty() || queryKey.isEmpty()) {
        return 0;
    }
    if (candidateKey == queryKey) {
        return 3;
    }
    if (candidateKey.contains(queryKey) || queryKey.contains(candidateKey)) {
        return 2;
    }
    return 0;
}

int releaseTypeScore(const QJsonObject &release)
{
    const QJsonObject group = release.value(QStringLiteral("release-group")).toObject();
    int score = 0;
    const QString primaryType = jsonString(group, QStringLiteral("primary-type"));
    if (primaryType.compare(QStringLiteral("Album"), Qt::CaseInsensitive) == 0) {
        score += 40;
    } else if (primaryType.compare(QStringLiteral("Single"), Qt::CaseInsensitive) == 0) {
        score += 8;
    }
    const QJsonArray secondaryTypes = group.value(QStringLiteral("secondary-types")).toArray();
    for (const QJsonValue &value : secondaryTypes) {
        const QString type = value.toString();
        if (type.compare(QStringLiteral("Compilation"), Qt::CaseInsensitive) == 0) {
            score -= 50;
        } else if (type.compare(QStringLiteral("Live"), Qt::CaseInsensitive) == 0
                   || type.compare(QStringLiteral("Remix"), Qt::CaseInsensitive) == 0) {
            score -= 20;
        }
    }
    return score;
}

QString joinArtistCredit(const QJsonArray &credit)
{
    QString out;
    for (const QJsonValue &v : credit) {
        const QJsonObject c = v.toObject();
        out += jsonString(c, QStringLiteral("name"));
        out += jsonString(c, QStringLiteral("joinphrase"));
    }
    return out.trimmed();
}

QString yearFromDate(const QString &date)
{
    if (date.isEmpty()) {
        return {};
    }
    const int dash = date.indexOf(QLatin1Char('-'));
    const QString head = dash > 0 ? date.left(dash) : date;
    if (head.length() == 4) {
        return head;
    }
    return {};
}

TagCandidate buildTagCandidate(const QJsonObject &recording,
                               const QString &queryArtist,
                               const QString &queryAlbum)
{
    TagCandidate c;
    c.mbid = jsonString(recording, QStringLiteral("id"));
    c.title = jsonString(recording, QStringLiteral("title"));
    c.artist = joinArtistCredit(recording.value(QStringLiteral("artist-credit")).toArray());

    const QJsonArray releases = recording.value(QStringLiteral("releases")).toArray();
    if (!releases.isEmpty()) {
        // Prefer a known album match, then a proper album release. Compilation,
        // live and remix releases are poor automatic defaults for an ordinary
        // recording and must not win merely because their metadata is complete.
        int bestIdx = -1;
        QString bestYear;
        int bestScore = std::numeric_limits<int>::min();
        int bestAlbumMatch = 0;
        for (int i = 0; i < releases.size(); ++i) {
            const QJsonObject rel = releases.at(i).toObject();
            const QString y = yearFromDate(jsonString(rel, QStringLiteral("date")));
            const QString releaseTitle = jsonString(rel, QStringLiteral("title"));
            if (releaseTitle.isEmpty()) {
                continue;
            }
            const int match = albumMatchRank(releaseTitle, queryAlbum);
            int score = match * 100 + releaseTypeScore(rel);
            if (jsonString(rel, QStringLiteral("status")) == QStringLiteral("Official")) {
                score += 10;
            }
            if (!y.isEmpty()) {
                score += 2;
            }
            if (bestIdx == -1
                || score > bestScore
                || (score == bestScore && !y.isEmpty()
                    && (bestYear.isEmpty() || y < bestYear))) {
                bestIdx = i;
                bestYear = y;
                bestScore = score;
                bestAlbumMatch = match;
            }
        }
        if (bestIdx == -1) {
            for (int i = 0; i < releases.size(); ++i) {
                const QJsonObject rel = releases.at(i).toObject();
                if (!jsonString(rel, QStringLiteral("title")).isEmpty()) {
                    bestIdx = i;
                    break;
                }
            }
        }
        if (bestIdx >= 0) {
            const QJsonObject rel = releases.at(bestIdx).toObject();
            const int chosenTypeScore = releaseTypeScore(rel);
            if (bestAlbumMatch > 0 || chosenTypeScore >= 0) {
                c.album = jsonString(rel, QStringLiteral("title"));
            }
            c.albumMatches = bestAlbumMatch > 0;
            c.year = yearFromDate(jsonString(rel, QStringLiteral("date")));
            const QJsonObject medium = rel.value(QStringLiteral("medium")).toObject();
            const QJsonArray tracks = medium.value(QStringLiteral("track")).toArray();
            if (!tracks.isEmpty()) {
                const QJsonObject t = tracks.at(0).toObject();
                int num = t.value(QStringLiteral("number")).toInt();
                if (num <= 0) {
                    num = t.value(QStringLiteral("position")).toInt();
                }
                if (num > 0) {
                    c.track = QString::number(num);
                }
            }
        }
    }

    const QJsonArray tags = recording.value(QStringLiteral("tags")).toArray();
    c.genre = bestGenreName(tags);

    c.queryScore = recording.value(QStringLiteral("score")).toInt();
    c.artistMatches = !queryArtist.isEmpty() && containsCi(c.artist, queryArtist);
    c.completeness = (!c.title.isEmpty() ? 1 : 0)
                   + (!c.artist.isEmpty() ? 1 : 0)
                   + (!c.album.isEmpty() ? 2 : 0)
                   + (!c.year.isEmpty() ? 2 : 0)
                   + (!c.track.isEmpty() ? 1 : 0)
                   + (!c.genre.isEmpty() ? 1 : 0);
    return c;
}

QVariantMap candidateToFields(const TagCandidate &c)
{
    QVariantMap fields;
    if (!c.title.isEmpty()) fields[QStringLiteral("title")] = c.title;
    if (!c.artist.isEmpty()) fields[QStringLiteral("artist")] = c.artist;
    if (!c.album.isEmpty()) fields[QStringLiteral("album")] = c.album;
    if (!c.year.isEmpty()) fields[QStringLiteral("year")] = c.year;
    if (!c.track.isEmpty()) fields[QStringLiteral("track")] = c.track;
    if (!c.genre.isEmpty()) fields[QStringLiteral("genre")] = c.genre;
    return fields;
}

} // namespace

void AudioTagEditorBackend::lookupTagsAsync(const QVariantMap &item, int requestId)
{
    cancelTagsLookup();

    const QString artist = trimmed(item.value(QStringLiteral("artist")).toString());
    const QString album = trimmed(item.value(QStringLiteral("album")).toString());
    QString title = trimmed(item.value(QStringLiteral("title")).toString());
    if (title.isEmpty()) {
        const QString path = item.value(QStringLiteral("path")).toString();
        const QString base = QFileInfo(path).completeBaseName();
        if (!base.isEmpty()) {
            title = base;
        }
    }

    if (artist.isEmpty() || title.isEmpty()) {
        emit tagsLookupFinished(requestId, QVariantMap{
            {QStringLiteral("ok"), false},
            {QStringLiteral("message"), QStringLiteral("Artist and title are required for tag lookup.")},
            {QStringLiteral("fields"), QVariantMap{}},
        });
        return;
    }

    QUrl url(QStringLiteral("https://musicbrainz.org/ws/2/recording"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
    query.addQueryItem(QStringLiteral("limit"), QStringLiteral("8"));
    QString queryText = QStringLiteral("artist:\"%1\" AND recording:\"%2\"").arg(artist, title);
    if (!album.isEmpty()) {
        queryText += QStringLiteral(" AND release:\"%1\"").arg(album);
    }
    query.addQueryItem(QStringLiteral("query"), queryText);
    url.setQuery(query);

    QNetworkReply *reply = m_network->get(jsonRequest(url));
    registerTagReply(reply);
    auto *timer = makeAbortTimer(kLookupTimeoutMs, reply, reply);

    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, requestId, artist, album]() {
        timer->deleteLater();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError || status >= 400) {
            const QString message = reply->error() == QNetworkReply::OperationCanceledError
                ? QStringLiteral("Tag lookup timed out.")
                : reply->errorString();
            dropReply(m_tagReplies, reply);
            reply->deleteLater();
            emit tagsLookupFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"),
                 message.isEmpty() ? QStringLiteral("Tag lookup failed.") : message},
                {QStringLiteral("fields"), QVariantMap{}},
            });
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll(), &parseError);
        dropReply(m_tagReplies, reply);
        reply->deleteLater();
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            emit tagsLookupFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("Tag lookup returned invalid data.")},
                {QStringLiteral("fields"), QVariantMap{}},
            });
            return;
        }

        const QJsonArray recordings = document.object().value(QStringLiteral("recordings")).toArray();
        if (recordings.isEmpty()) {
            emit tagsLookupFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("No matching recordings on MusicBrainz.")},
                {QStringLiteral("fields"), QVariantMap{}},
            });
            return;
        }

        std::vector<TagCandidate> candidates;
        candidates.reserve(static_cast<std::size_t>(recordings.size()));
        for (const QJsonValue &v : recordings) {
            TagCandidate c = buildTagCandidate(v.toObject(), artist, album);
            if (!c.title.isEmpty()) {
                candidates.push_back(std::move(c));
            }
        }

        if (candidates.empty()) {
            emit tagsLookupFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"), QStringLiteral("No usable tag data in MusicBrainz results.")},
                {QStringLiteral("fields"), QVariantMap{}},
            });
            return;
        }

        // Prefer artist and known-album matches. MusicBrainz relevance must
        // beat formal field completeness, otherwise a richly tagged
        // compilation can displace the actual recording.
        std::sort(candidates.begin(), candidates.end(),
                  [](const TagCandidate &a, const TagCandidate &b) {
                      if (a.artistMatches != b.artistMatches) {
                          return a.artistMatches;
                      }
                      if (a.albumMatches != b.albumMatches) {
                          return a.albumMatches;
                      }
                      if (a.queryScore != b.queryScore) {
                          return a.queryScore > b.queryScore;
                      }
                      return a.completeness > b.completeness;
                  });

        const auto finishLookup = [this, requestId](const TagCandidate &candidate) {
            const QVariantMap fields = candidateToFields(candidate);
            QStringList filled;
            for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
                filled.append(it.key());
            }
            emit tagsLookupFinished(requestId, QVariantMap{
                {QStringLiteral("ok"), true},
                {QStringLiteral("message"),
                 QStringLiteral("Tags filled from MusicBrainz: %1.").arg(filled.join(QStringLiteral(", ")))},
                {QStringLiteral("fields"), fields},
                {QStringLiteral("source"), QStringLiteral("MusicBrainz")},
            });
        };

        TagCandidate best = candidates.front();
        if (!best.genre.isEmpty() || best.mbid.isEmpty()) {
            finishLookup(best);
            return;
        }

        QUrl detailsUrl(QStringLiteral("https://musicbrainz.org/ws/2/recording/%1").arg(best.mbid));
        QUrlQuery detailsQuery;
        detailsQuery.addQueryItem(QStringLiteral("fmt"), QStringLiteral("json"));
        detailsQuery.addQueryItem(QStringLiteral("inc"), QStringLiteral("genres+tags+artist-credits"));
        detailsUrl.setQuery(detailsQuery);

        QNetworkReply *detailsReply = m_network->get(jsonRequest(detailsUrl));
        registerTagReply(detailsReply);
        auto *detailsTimer = makeAbortTimer(kLookupTimeoutMs, detailsReply, detailsReply);
        connect(detailsReply, &QNetworkReply::finished, this,
                [this, detailsReply, detailsTimer, best = std::move(best), finishLookup]() mutable {
            detailsTimer->deleteLater();
            const int detailsStatus = detailsReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (detailsReply->error() == QNetworkReply::NoError && detailsStatus < 400) {
                QJsonParseError detailsParseError;
                const QJsonDocument detailsDocument =
                    QJsonDocument::fromJson(detailsReply->readAll(), &detailsParseError);
                if (detailsParseError.error == QJsonParseError::NoError && detailsDocument.isObject()) {
                    best.genre = genreFromRecordingDetails(detailsDocument.object());
                }
            }
            dropReply(m_tagReplies, detailsReply);
            detailsReply->deleteLater();
            finishLookup(best);
        });
    });
}

void AudioTagEditorBackend::cancelTagsLookup()
{
    abortReplies(m_tagReplies);
}
