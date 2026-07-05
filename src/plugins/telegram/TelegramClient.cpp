#include "TelegramClient.h"

#include "TelegramAuth.h"

#include <td/telegram/Client.h>
#include <td/telegram/td_api.h>

#include <QCoreApplication>
#include <QDir>
#include <QDeadlineTimer>
#include <QFileInfo>
#include <QDebug>
#include <QMimeDatabase>
#include <QMutexLocker>
#include <QSet>
#include <QStandardPaths>
#include <QStringList>
#include <QSysInfo>
#include <QTimeZone>
#include <QTimer>

namespace TelegramProviderInternal {

namespace {

QString stateLabel(TelegramClient::State state)
{
    switch (state) {
    case TelegramClient::State::NotStarted:
        return QStringLiteral("TDLib client not started");
    case TelegramClient::State::Starting:
        return QStringLiteral("TDLib client starting");
    case TelegramClient::State::WaitTdlibParameters:
        return QStringLiteral("TDLib waits for application parameters");
    case TelegramClient::State::WaitPhoneNumber:
        return QStringLiteral("Telegram waits for phone number");
    case TelegramClient::State::WaitCode:
        return QStringLiteral("Telegram waits for login code");
    case TelegramClient::State::WaitPassword:
        return QStringLiteral("Telegram waits for 2FA password");
    case TelegramClient::State::Ready:
        return QStringLiteral("Telegram authorization is ready");
    case TelegramClient::State::Closing:
        return QStringLiteral("TDLib client closing");
    case TelegramClient::State::Closed:
        return QStringLiteral("TDLib client closed");
    }
    return QStringLiteral("Unknown TDLib client state");
}

bool traceEnabled()
{
    return qEnvironmentVariableIntValue("FM_TELEGRAM_TRACE") != 0;
}

bool tdlibTraceEnabled()
{
    return qEnvironmentVariableIntValue("FM_TELEGRAM_TDLIB_TRACE") != 0;
}

int idleTimeoutMs()
{
    bool ok = false;
    const int seconds = qEnvironmentVariableIntValue("FM_TELEGRAM_IDLE_TIMEOUT_SECONDS", &ok);
    if (ok) {
        return seconds > 0 ? seconds * 1000 : 0;
    }
    return 120000;
}

void traceTelegram(const QString &message)
{
    if (traceEnabled()) {
        qInfo().noquote() << "[TelegramProvider]" << message;
    }
}

void configureTdlibLogging()
{
    const int verbosity = tdlibTraceEnabled() ? 3 : 0;
    td::ClientManager::set_log_message_callback(verbosity, nullptr);
    td::ClientManager::execute(td::td_api::make_object<td::td_api::setLogVerbosityLevel>(verbosity));
}

QString objectLabel(int objectId)
{
    using namespace td::td_api;
    switch (objectId) {
    case ok::ID:
        return QStringLiteral("ok");
    case error::ID:
        return QStringLiteral("error");
    case updateAuthorizationState::ID:
        return QStringLiteral("updateAuthorizationState");
    case authorizationStateWaitTdlibParameters::ID:
        return QStringLiteral("authorizationStateWaitTdlibParameters");
    case authorizationStateWaitPhoneNumber::ID:
        return QStringLiteral("authorizationStateWaitPhoneNumber");
    case authorizationStateWaitCode::ID:
        return QStringLiteral("authorizationStateWaitCode");
    case authorizationStateWaitPassword::ID:
        return QStringLiteral("authorizationStateWaitPassword");
    case authorizationStateReady::ID:
        return QStringLiteral("authorizationStateReady");
    case authorizationStateClosed::ID:
        return QStringLiteral("authorizationStateClosed");
    case user::ID:
        return QStringLiteral("user");
    case chat::ID:
        return QStringLiteral("chat");
    case messages::ID:
        return QStringLiteral("messages");
    case file::ID:
        return QStringLiteral("file");
    case updateFile::ID:
        return QStringLiteral("updateFile");
    default:
        return QStringLiteral("object:%1").arg(objectId);
    }
}

bool isAuthorizationStateObject(int objectId)
{
    using namespace td::td_api;
    switch (objectId) {
    case authorizationStateWaitTdlibParameters::ID:
    case authorizationStateWaitPhoneNumber::ID:
    case authorizationStateWaitCode::ID:
    case authorizationStateWaitEmailAddress::ID:
    case authorizationStateWaitEmailCode::ID:
    case authorizationStateWaitOtherDeviceConfirmation::ID:
    case authorizationStateWaitPassword::ID:
    case authorizationStateReady::ID:
    case authorizationStateLoggingOut::ID:
    case authorizationStateClosing::ID:
    case authorizationStateClosed::ID:
        return true;
    default:
        return false;
    }
}

QString sanitizedTdError(const td::td_api::error &tdError)
{
    QString message = QString::fromStdString(tdError.message_).trimmed();
    if (message.size() > 160) {
        message = message.left(157) + QStringLiteral("...");
    }
    return message;
}

TelegramClient::State authStateFromTdObject(const td::td_api::Object &object)
{
    using namespace td::td_api;
    switch (object.get_id()) {
    case authorizationStateWaitTdlibParameters::ID:
        return TelegramClient::State::WaitTdlibParameters;
    case authorizationStateWaitPhoneNumber::ID:
        return TelegramClient::State::WaitPhoneNumber;
    case authorizationStateWaitCode::ID:
    case authorizationStateWaitEmailAddress::ID:
    case authorizationStateWaitEmailCode::ID:
    case authorizationStateWaitOtherDeviceConfirmation::ID:
        return TelegramClient::State::WaitCode;
    case authorizationStateWaitPassword::ID:
        return TelegramClient::State::WaitPassword;
    case authorizationStateReady::ID:
        return TelegramClient::State::Ready;
    case authorizationStateLoggingOut::ID:
    case authorizationStateClosing::ID:
        return TelegramClient::State::Closing;
    case authorizationStateClosed::ID:
        return TelegramClient::State::Closed;
    default:
        return TelegramClient::State::Starting;
    }
}

QString fileNameSafe(QString name)
{
    name = name.trimmed();
    name.replace(QLatin1Char('\\'), QLatin1Char('_'));
    name.replace(QLatin1Char('/'), QLatin1Char('_'));
    return name.isEmpty() ? QStringLiteral("telegram-file") : name;
}

QString fileNameWithFallback(QString name, const QString &fallback)
{
    name = fileNameSafe(std::move(name));
    return name == QLatin1String("telegram-file") ? fallback : name;
}

QString mimeWithFallback(QString mimeType, const QString &fallback)
{
    mimeType = mimeType.trimmed();
    return mimeType.isEmpty() ? fallback : mimeType;
}

QString chatPath(qint64 chatId)
{
    return QStringLiteral("telegram://chat/%1").arg(QString::number(chatId));
}

QString filePathForMessage(const QString &parentPath, const td::td_api::message &message, const td::td_api::file &file, const QString &name)
{
    return QStringLiteral("%1/%2-%3-%4").arg(parentPath,
                                             QString::number(message.id_),
                                             QString::number(file.id_),
                                             name);
}

const td::td_api::file *tdFileFromPhoto(const td::td_api::photo &photo)
{
    const td::td_api::photoSize *best = nullptr;
    for (const auto &size : photo.sizes_) {
        if (!size || !size->photo_) {
            continue;
        }
        if (!best || (size->width_ * size->height_) > (best->width_ * best->height_)) {
            best = size.get();
        }
    }
    return best ? best->photo_.get() : nullptr;
}

TelegramEntry entryFromFile(const td::td_api::message &message,
                            const td::td_api::file &file,
                            QString name,
                            const QString &mimeType,
                            const QString &label,
                            const QString &parentPath,
                            QByteArray thumbnailData = {},
                            QString thumbnailLocalPath = {},
                            int thumbnailFileId = 0)
{
    name = fileNameSafe(std::move(name));
    TelegramEntry entry;
    entry.name = name;
    entry.path = filePathForMessage(parentPath, message, file, name);
    entry.mimeType = mimeType;
    entry.providerLabel = label;
    entry.localPath = file.local_ && file.local_->is_downloading_completed_
        ? QString::fromStdString(file.local_->path_)
        : QString{};
    entry.chatId = message.chat_id_;
    entry.messageId = message.id_;
    entry.fileId = file.id_;
    entry.thumbnailFileId = thumbnailFileId;
    entry.size = file.size_ > 0 ? static_cast<qint64>(file.size_) : static_cast<qint64>(file.expected_size_);
    entry.date = QDateTime::fromSecsSinceEpoch(message.date_, QTimeZone::UTC);
    entry.directory = false;
    entry.downloaded = file.local_ && file.local_->is_downloading_completed_;
    entry.providerLabel = entry.downloaded ? QStringLiteral("Telegram downloaded") : label;
    entry.thumbnailLocalPath = std::move(thumbnailLocalPath);
    entry.thumbnailData = std::move(thumbnailData);
    entry.hasThumbnail = !entry.thumbnailLocalPath.isEmpty() || !entry.thumbnailData.isEmpty() || entry.thumbnailFileId > 0;
    return entry;
}

QString localPathFromTdFile(const td::td_api::file *file)
{
    if (!file || !file->local_ || !file->local_->is_downloading_completed_ || file->local_->path_.empty()) {
        return {};
    }
    return QString::fromStdString(file->local_->path_);
}

QByteArray thumbnailDataFromMiniThumbnail(const td::td_api::minithumbnail *thumbnail)
{
    if (!thumbnail || thumbnail->data_.empty()) {
        return {};
    }
    return QByteArray(thumbnail->data_.data(), static_cast<qsizetype>(thumbnail->data_.size()));
}

QString localPathFromThumbnail(const td::td_api::thumbnail *thumbnail)
{
    return thumbnail ? localPathFromTdFile(thumbnail->file_.get()) : QString{};
}

int fileIdFromThumbnail(const td::td_api::thumbnail *thumbnail)
{
    return thumbnail && thumbnail->file_ ? thumbnail->file_->id_ : 0;
}

QString localPathFromChatPhotoInfo(const td::td_api::chatPhotoInfo *photo)
{
    return photo ? localPathFromTdFile(photo->small_.get()) : QString{};
}

int fileIdFromChatPhotoInfo(const td::td_api::chatPhotoInfo *photo)
{
    return photo && photo->small_ ? photo->small_->id_ : 0;
}

QByteArray thumbnailDataFromChatPhotoInfo(const td::td_api::chatPhotoInfo *photo)
{
    return photo ? thumbnailDataFromMiniThumbnail(photo->minithumbnail_.get()) : QByteArray{};
}

QString localPathFromSmallestPhotoSize(const td::td_api::photo *photo)
{
    if (!photo) {
        return {};
    }

    const td::td_api::photoSize *best = nullptr;
    for (const auto &size : photo->sizes_) {
        if (!size || !size->photo_) {
            continue;
        }
        const QString localPath = localPathFromTdFile(size->photo_.get());
        if (localPath.isEmpty()) {
            continue;
        }
        if (!best || (size->width_ * size->height_) < (best->width_ * best->height_)) {
            best = size.get();
        }
    }
    return best ? localPathFromTdFile(best->photo_.get()) : QString{};
}

int fileIdFromSmallestPhotoSize(const td::td_api::photo *photo)
{
    if (!photo) {
        return 0;
    }

    const td::td_api::photoSize *best = nullptr;
    for (const auto &size : photo->sizes_) {
        if (!size || !size->photo_) {
            continue;
        }
        if (!best || (size->width_ * size->height_) < (best->width_ * best->height_)) {
            best = size.get();
        }
    }
    return best && best->photo_ ? best->photo_->id_ : 0;
}

QStringList linksFromFormattedText(const td::td_api::formattedText *text)
{
    QStringList links;
    if (!text) {
        return links;
    }

    const QString body = QString::fromStdString(text->text_);
    for (const auto &entity : text->entities_) {
        if (!entity || !entity->type_) {
            continue;
        }

        QString link;
        switch (entity->type_->get_id()) {
        case td::td_api::textEntityTypeUrl::ID:
            link = body.mid(entity->offset_, entity->length_);
            break;
        case td::td_api::textEntityTypeTextUrl::ID:
            link = QString::fromStdString(static_cast<const td::td_api::textEntityTypeTextUrl &>(*entity->type_).url_);
            break;
        default:
            break;
        }

        link = link.trimmed();
        if (!link.isEmpty() && !links.contains(link)) {
            links.append(link);
        }
    }
    return links;
}

td::td_api::object_ptr<td::td_api::formattedText> emptyCaption()
{
    return td::td_api::make_object<td::td_api::formattedText>(std::string(), std::vector<td::td_api::object_ptr<td::td_api::textEntity>>{});
}

td::td_api::object_ptr<td::td_api::InputFile> inputFileLocal(const QString &path)
{
    return td::td_api::make_object<td::td_api::inputFileLocal>(path.toStdString());
}

constexpr qint64 TelegramClientPhotoUploadLimit = 10 * 1024 * 1024;

td::td_api::object_ptr<td::td_api::InputMessageContent> inputMessageContentForLocalFile(const QString &path, QString mimeType, qint64 size = 0)
{
    mimeType = mimeType.trimmed().toLower();
    if (mimeType.isEmpty()) {
        mimeType = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchExtension).name().toLower();
    }

    if (mimeType.startsWith(QStringLiteral("image/")) && (size <= 0 || size <= TelegramClientPhotoUploadLimit)) {
        return td::td_api::make_object<td::td_api::inputMessagePhoto>(
            inputFileLocal(path),
            nullptr,
            nullptr,
            std::vector<int32_t>{},
            0,
            0,
            emptyCaption(),
            false,
            nullptr,
            false);
    }
    if (mimeType.startsWith(QStringLiteral("video/"))) {
        return td::td_api::make_object<td::td_api::inputMessageVideo>(
            inputFileLocal(path),
            nullptr,
            nullptr,
            0,
            std::vector<int32_t>{},
            0,
            0,
            0,
            true,
            emptyCaption(),
            false,
            nullptr,
            false);
    }
    if (mimeType.startsWith(QStringLiteral("audio/"))) {
        return td::td_api::make_object<td::td_api::inputMessageAudio>(
            inputFileLocal(path),
            nullptr,
            0,
            std::string(),
            std::string(),
            emptyCaption());
    }
    return td::td_api::make_object<td::td_api::inputMessageDocument>(
        inputFileLocal(path),
        nullptr,
        false,
        emptyCaption());
}

bool isTelegramAlbumCompatibleMime(QString mimeType, qint64 size = 0)
{
    mimeType = mimeType.trimmed().toLower();
    return (mimeType.startsWith(QStringLiteral("image/")) && (size <= 0 || size <= TelegramClientPhotoUploadLimit))
        || mimeType.startsWith(QStringLiteral("video/"));
}

TelegramEntry entryFromLinks(const td::td_api::message &message, const QStringList &links, const QString &parentPath)
{
    TelegramEntry entry;
    entry.name = QStringLiteral("link_%1.url").arg(message.id_);
    entry.path = filePathForMessage(parentPath, message, td::td_api::file(), entry.name);
    entry.mimeType = QStringLiteral("application/x-mswinurl");
    entry.providerLabel = QStringLiteral("Telegram link");
    entry.iconName = QStringLiteral("text");
    entry.openUrl = links.first();
    entry.chatId = message.chat_id_;
    entry.messageId = message.id_;
    entry.size = 0;
    entry.date = QDateTime::fromSecsSinceEpoch(message.date_, QTimeZone::UTC);
    entry.directory = false;
    QByteArray data("[InternetShortcut]\n");
    for (const QString &link : links) {
        data += "URL=" + link.toUtf8() + '\n';
    }
    entry.virtualContent = data;
    entry.size = entry.virtualContent.size();
    return entry;
}

std::optional<TelegramEntry> entryFromMessage(const td::td_api::message &message, const QString &parentPath)
{
    if (!message.content_) {
        return std::nullopt;
    }

    using namespace td::td_api;
    switch (message.content_->get_id()) {
    case messageText::ID: {
        const auto &content = static_cast<const messageText &>(*message.content_);
        QStringList links = linksFromFormattedText(content.text_.get());
        if (content.link_preview_ && !content.link_preview_->url_.empty()) {
            const QString previewUrl = QString::fromStdString(content.link_preview_->url_).trimmed();
            if (!previewUrl.isEmpty() && !links.contains(previewUrl)) {
                links.append(previewUrl);
            }
        }
        if (links.isEmpty()) {
            return std::nullopt;
        }
        return entryFromLinks(message, links, parentPath);
    }
    case messageAnimation::ID: {
        const auto &content = static_cast<const messageAnimation &>(*message.content_);
        if (!content.animation_ || !content.animation_->animation_) {
            return std::nullopt;
        }
        return entryFromFile(message,
                             *content.animation_->animation_,
                             fileNameWithFallback(QString::fromStdString(content.animation_->file_name_),
                                                  QStringLiteral("animation_%1.mp4").arg(message.id_)),
                             mimeWithFallback(QString::fromStdString(content.animation_->mime_type_), QStringLiteral("video/mp4")),
                             QStringLiteral("Telegram animation"),
                             parentPath,
                             thumbnailDataFromMiniThumbnail(content.animation_->minithumbnail_.get()),
                             localPathFromThumbnail(content.animation_->thumbnail_.get()),
                             fileIdFromThumbnail(content.animation_->thumbnail_.get()));
    }
    case messageDocument::ID: {
        const auto &content = static_cast<const messageDocument &>(*message.content_);
        if (!content.document_ || !content.document_->document_) {
            return std::nullopt;
        }
        return entryFromFile(message,
                             *content.document_->document_,
                             QString::fromStdString(content.document_->file_name_),
                             QString::fromStdString(content.document_->mime_type_),
                             QStringLiteral("Telegram document"),
                             parentPath,
                             thumbnailDataFromMiniThumbnail(content.document_->minithumbnail_.get()),
                             localPathFromThumbnail(content.document_->thumbnail_.get()),
                             fileIdFromThumbnail(content.document_->thumbnail_.get()));
    }
    case messagePhoto::ID: {
        const auto &content = static_cast<const messagePhoto &>(*message.content_);
        if (!content.photo_) {
            return std::nullopt;
        }
        const file *photoFile = tdFileFromPhoto(*content.photo_);
        if (!photoFile) {
            return std::nullopt;
        }
        return entryFromFile(message,
                             *photoFile,
                             QStringLiteral("photo_%1.jpg").arg(message.id_),
                             QStringLiteral("image/jpeg"),
                             QStringLiteral("Telegram photo"),
                             parentPath,
                             thumbnailDataFromMiniThumbnail(content.photo_->minithumbnail_.get()),
                             localPathFromSmallestPhotoSize(content.photo_.get()),
                             fileIdFromSmallestPhotoSize(content.photo_.get()));
    }
    case messageVideo::ID: {
        const auto &content = static_cast<const messageVideo &>(*message.content_);
        if (!content.video_ || !content.video_->video_) {
            return std::nullopt;
        }
        return entryFromFile(message,
                             *content.video_->video_,
                             fileNameWithFallback(QString::fromStdString(content.video_->file_name_),
                                                  QStringLiteral("video_%1.mp4").arg(message.id_)),
                             mimeWithFallback(QString::fromStdString(content.video_->mime_type_), QStringLiteral("video/mp4")),
                             QStringLiteral("Telegram video"),
                             parentPath,
                             thumbnailDataFromMiniThumbnail(content.video_->minithumbnail_.get()),
                             localPathFromThumbnail(content.video_->thumbnail_.get()),
                             fileIdFromThumbnail(content.video_->thumbnail_.get()));
    }
    case messageAudio::ID: {
        const auto &content = static_cast<const messageAudio &>(*message.content_);
        if (!content.audio_ || !content.audio_->audio_) {
            return std::nullopt;
        }
        QString name = QString::fromStdString(content.audio_->file_name_);
        if (name.isEmpty()) {
            name = QStringLiteral("audio_%1.mp3").arg(message.id_);
        }
        return entryFromFile(message,
                             *content.audio_->audio_,
                             name,
                             QString::fromStdString(content.audio_->mime_type_),
                             QStringLiteral("Telegram audio"),
                             parentPath,
                             thumbnailDataFromMiniThumbnail(content.audio_->album_cover_minithumbnail_.get()),
                             localPathFromThumbnail(content.audio_->album_cover_thumbnail_.get()),
                             fileIdFromThumbnail(content.audio_->album_cover_thumbnail_.get()));
    }
    case messageVideoNote::ID: {
        const auto &content = static_cast<const messageVideoNote &>(*message.content_);
        if (!content.video_note_ || !content.video_note_->video_) {
            return std::nullopt;
        }
        return entryFromFile(message,
                             *content.video_note_->video_,
                             QStringLiteral("video_note_%1.mp4").arg(message.id_),
                             QStringLiteral("video/mp4"),
                             QStringLiteral("Telegram video note"),
                             parentPath,
                             thumbnailDataFromMiniThumbnail(content.video_note_->minithumbnail_.get()),
                             localPathFromThumbnail(content.video_note_->thumbnail_.get()),
                             fileIdFromThumbnail(content.video_note_->thumbnail_.get()));
    }
    case messageVoiceNote::ID: {
        const auto &content = static_cast<const messageVoiceNote &>(*message.content_);
        if (!content.voice_note_ || !content.voice_note_->voice_) {
            return std::nullopt;
        }
        QString suffix = QStringLiteral("ogg");
        const QString mimeType = QString::fromStdString(content.voice_note_->mime_type_);
        if (mimeType.contains(QStringLiteral("mpeg"), Qt::CaseInsensitive)) {
            suffix = QStringLiteral("mp3");
        }
        return entryFromFile(message,
                             *content.voice_note_->voice_,
                             QStringLiteral("voice_%1.%2").arg(QString::number(message.id_), suffix),
                             mimeType.isEmpty() ? QStringLiteral("audio/ogg") : mimeType,
                             QStringLiteral("Telegram voice note"),
                             parentPath);
    }
    default:
        return std::nullopt;
    }
}

} // namespace

class TelegramClient::ActivityScope
{
public:
    explicit ActivityScope(TelegramClient &client)
        : m_client(&client)
    {
        m_client->beginActivity();
    }

    ~ActivityScope()
    {
        if (m_client) {
            m_client->endActivity();
        }
    }

    Q_DISABLE_COPY_MOVE(ActivityScope)

private:
    TelegramClient *m_client = nullptr;
};

TelegramClient::~TelegramClient()
{
    close();
}

bool TelegramClient::start(QString *error)
{
    ActivityScope activity(*this);
    QMutexLocker locker(&m_clientMutex);
    if (error) {
        error->clear();
    }
    if (m_state != State::NotStarted && m_state != State::Closed) {
        return true;
    }

    configureTdlibLogging();
    m_manager = std::make_unique<td::ClientManager>();
    m_clientId = m_manager->create_client_id();
    setState(State::Starting);
    sendGetAuthorizationState();
    pollUntilStateChanges(State::Starting, 5000, error);
    return true;
}

void TelegramClient::close()
{
    QMutexLocker locker(&m_clientMutex);
    ++m_idleGeneration;
    if (!m_manager) {
        setState(State::Closed);
        return;
    }

    m_manager->send(m_clientId, m_nextRequestId++, td::td_api::make_object<td::td_api::close>());
    QString ignoredError;
    poll(100, &ignoredError);
    m_manager.reset();
    m_clientId = 0;
    setState(State::Closed);
}

TelegramClient::State TelegramClient::state() const
{
    return m_state;
}

QString TelegramClient::sanitizedStatus() const
{
    return m_lastStatus;
}

bool TelegramClient::poll(int timeoutMs, QString *error)
{
    QMutexLocker locker(&m_clientMutex);
    if (error) {
        error->clear();
    }
    if (!m_manager) {
        if (error) {
            *error = QStringLiteral("TDLib client is not started");
        }
        return false;
    }

    const td::ClientManager::Response response = m_manager->receive(double(timeoutMs) / 1000.0);
    if (!response.object) {
        return true;
    }
    if (response.client_id != m_clientId) {
        return true;
    }
    if (response.object->get_id() == td::td_api::error::ID) {
        const auto &tdError = static_cast<const td::td_api::error &>(*response.object);
        m_lastStatus = QStringLiteral("TDLib request failed with code %1: %2")
            .arg(QString::number(tdError.code_), sanitizedTdError(tdError));
        if (error) {
            *error = m_lastStatus;
        }
        return false;
    }

    handleObject(response.object->get_id());

    if (response.object->get_id() == td::td_api::updateAuthorizationState::ID) {
        const auto &update = static_cast<const td::td_api::updateAuthorizationState &>(*response.object);
        if (update.authorization_state_) {
            setState(authStateFromTdObject(*update.authorization_state_));
        }
    } else if (isAuthorizationStateObject(response.object->get_id())) {
        setState(authStateFromTdObject(*response.object));
    }
    return true;
}

bool TelegramClient::configureFromEnvironment(QString *error)
{
    ActivityScope activity(*this);
    if (!ensureStarted(error)) {
        return false;
    }
    if (m_state != State::WaitTdlibParameters) {
        return true;
    }

    const TelegramApiCredentials savedCredentials = savedApiCredentials();
    if (savedCredentials.apiId > 0 && !savedCredentials.apiHash.trimmed().isEmpty()) {
        return sendTdlibParameters(savedCredentials.apiId, savedCredentials.apiHash, error);
    }

    bool envOk = false;
    const int envApiId = qEnvironmentVariableIntValue("FM_TELEGRAM_API_ID", &envOk);
    const QString envApiHash = qEnvironmentVariable("FM_TELEGRAM_API_HASH").trimmed();
    if (!envOk || envApiId <= 0 || envApiHash.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Enter Telegram API ID and API hash to continue authorization.");
        }
        return false;
    }
    return sendTdlibParameters(envApiId, envApiHash, error);
}

bool TelegramClient::configureWithCredentials(int apiId, const QString &apiHash, QString *error)
{
    ActivityScope activity(*this);
    if (!ensureStarted(error)) {
        return false;
    }
    if (m_state != State::WaitTdlibParameters) {
        return true;
    }
    const QString cleanHash = apiHash.trimmed();
    if (apiId <= 0 || cleanHash.isEmpty()) {
        return configureFromEnvironment(error);
    }
    m_pendingApiId = apiId;
    m_pendingApiHash = cleanHash;
    if (!sendTdlibParameters(apiId, cleanHash, error)) {
        return false;
    }
    return rememberPendingApiCredentialsIfReady(error);
}

bool TelegramClient::setPhoneNumber(const QString &phoneNumber, int apiId, const QString &apiHash, QString *error)
{
    ActivityScope activity(*this);
    if (!configureWithCredentials(apiId, apiHash, error)) {
        return false;
    }
    if (m_state != State::WaitPhoneNumber) {
        if (error) {
            *error = m_lastStatus;
        }
        return m_state == State::WaitCode || m_state == State::WaitPassword || m_state == State::Ready;
    }
    const QString cleanPhone = phoneNumber.trimmed();
    if (cleanPhone.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Telegram phone number is required.");
        }
        return false;
    }
    const State previousState = m_state;
    auto response = sendBlocking(td::td_api::make_object<td::td_api::setAuthenticationPhoneNumber>(
                                     cleanPhone.toStdString(),
                                     td::td_api::make_object<td::td_api::phoneNumberAuthenticationSettings>(
                                         false,
                                         false,
                                         false,
                                         false,
                                         false,
                                         nullptr,
                                         std::vector<std::string>{})),
                                 QStringLiteral("setAuthenticationPhoneNumber"),
                                 error,
                                 30000);
    if (!response) {
        return false;
    }
    if (!pollUntilStateChanges(previousState, 15000, error)) {
        return false;
    }
    return rememberPendingApiCredentialsIfReady(error);
}

bool TelegramClient::checkCode(const QString &code, QString *error)
{
    ActivityScope activity(*this);
    if (!ensureStarted(error)) {
        return false;
    }
    const QString cleanCode = code.trimmed();
    if (cleanCode.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Telegram login code is required.");
        }
        return false;
    }
    const State previousState = m_state;
    auto response = sendBlocking(td::td_api::make_object<td::td_api::checkAuthenticationCode>(cleanCode.toStdString()),
                                 QStringLiteral("checkAuthenticationCode"),
                                 error,
                                 30000);
    if (!response) {
        return false;
    }
    if (!pollUntilStateChanges(previousState, 15000, error)) {
        return false;
    }
    return rememberPendingApiCredentialsIfReady(error);
}

bool TelegramClient::checkPassword(const QString &password, QString *error)
{
    ActivityScope activity(*this);
    if (!ensureStarted(error)) {
        return false;
    }
    if (password.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Telegram 2FA password is required.");
        }
        return false;
    }
    const State previousState = m_state;
    auto response = sendBlocking(td::td_api::make_object<td::td_api::checkAuthenticationPassword>(password.toStdString()),
                                 QStringLiteral("checkAuthenticationPassword"),
                                 error,
                                 30000);
    if (!response) {
        return false;
    }
    if (!pollUntilStateChanges(previousState, 15000, error)) {
        return false;
    }
    return rememberPendingApiCredentialsIfReady(error);
}

bool TelegramClient::logOut(QString *error)
{
    ActivityScope activity(*this);
    if (!ensureStarted(error)) {
        return false;
    }
    auto response = sendBlocking(td::td_api::make_object<td::td_api::logOut>(), QStringLiteral("logOut"), error, 30000);
    if (!response) {
        return false;
    }
    pollBriefly(error);
    if (error && !error->isEmpty()) {
        return false;
    }
    return clearSavedApiCredentials();
}

QList<TelegramEntry> TelegramClient::chats(QString *error)
{
    ActivityScope activity(*this);
    QList<TelegramEntry> entries;
    if (!configureFromEnvironment(error)) {
        return entries;
    }
    if (m_state != State::Ready) {
        if (error) {
            *error = m_lastStatus;
        }
        return entries;
    }

    auto chatsObject = sendBlocking(td::td_api::make_object<td::td_api::getChats>(
                                        td::td_api::make_object<td::td_api::chatListMain>(),
                                        100),
                                    QStringLiteral("getChats"),
                                    error);
    if (!chatsObject || chatsObject->get_id() != td::td_api::chats::ID) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Telegram chats are unavailable.");
        }
        return entries;
    }

    const auto &chats = static_cast<const td::td_api::chats &>(*chatsObject);
    entries.reserve(static_cast<int>(chats.chat_ids_.size()));
    for (const qint64 chatId : chats.chat_ids_) {
        auto chatObject = sendBlocking(td::td_api::make_object<td::td_api::getChat>(chatId),
                                       QStringLiteral("getChat"),
                                       error);
        if (!chatObject || chatObject->get_id() != td::td_api::chat::ID) {
            continue;
        }

        const auto &chat = static_cast<const td::td_api::chat &>(*chatObject);
        TelegramEntry entry;
        entry.name = QString::fromStdString(chat.title_).trimmed();
        if (entry.name.isEmpty()) {
            entry.name = QStringLiteral("Chat %1").arg(QString::number(chat.id_));
        }
        entry.path = chatPath(chat.id_);
        entry.providerLabel = QStringLiteral("Telegram chat");
        entry.iconName = QStringLiteral("telegram-badge-chat");
        entry.chatId = chat.id_;
        entry.thumbnailLocalPath = localPathFromChatPhotoInfo(chat.photo_.get());
        entry.thumbnailFileId = fileIdFromChatPhotoInfo(chat.photo_.get());
        entry.thumbnailData = thumbnailDataFromChatPhotoInfo(chat.photo_.get());
        entry.hasThumbnail = !entry.thumbnailLocalPath.isEmpty() || entry.thumbnailFileId > 0 || !entry.thumbnailData.isEmpty();
        entry.directory = true;
        entries.append(entry);
    }
    traceTelegram(QStringLiteral("chats count=%1").arg(entries.size()));
    return entries;
}

qint64 TelegramClient::publicChatId(const QString &username, QString *error)
{
    ActivityScope activity(*this);
    const QString cleanUsername = username.trimmed();
    if (cleanUsername.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Telegram channel username is unavailable.");
        }
        return 0;
    }
    if (!configureFromEnvironment(error)) {
        return 0;
    }
    if (m_state != State::Ready) {
        if (error) {
            *error = m_lastStatus;
        }
        return 0;
    }

    auto chatObject = sendBlocking(td::td_api::make_object<td::td_api::searchPublicChat>(cleanUsername.toStdString()),
                                   QStringLiteral("searchPublicChat"),
                                   error);
    if (!chatObject || chatObject->get_id() != td::td_api::chat::ID) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Telegram public chat is unavailable.");
        }
        return 0;
    }

    const auto &chat = static_cast<const td::td_api::chat &>(*chatObject);
    return chat.id_;
}

qint64 TelegramClient::savedMessagesChatId(QString *error)
{
    ActivityScope activity(*this);
    if (!configureFromEnvironment(error)) {
        return 0;
    }
    if (m_state != State::Ready) {
        if (error) {
            *error = m_lastStatus;
        }
        return 0;
    }

    auto meObject = sendBlocking(td::td_api::make_object<td::td_api::getMe>(), QStringLiteral("getMe"), error);
    if (!meObject || meObject->get_id() != td::td_api::user::ID) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Telegram user metadata is unavailable.");
        }
        return 0;
    }
    const auto &me = static_cast<const td::td_api::user &>(*meObject);

    auto chatObject = sendBlocking(td::td_api::make_object<td::td_api::createPrivateChat>(me.id_, false),
                                   QStringLiteral("createPrivateChat"),
                                   error);
    if (!chatObject || chatObject->get_id() != td::td_api::chat::ID) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Telegram Saved Messages chat is unavailable.");
        }
        return 0;
    }
    const auto &chat = static_cast<const td::td_api::chat &>(*chatObject);
    if (error) {
        error->clear();
    }
    return chat.id_;
}

TelegramSavedMessagesPage TelegramClient::savedMessageFiles(qint64 fromMessageId, QString *error)
{
    TelegramSavedMessagesPage page;
    const qint64 chatId = savedMessagesChatId(error);
    return chatId == 0 ? page : chatMessageFiles(chatId, QStringLiteral("telegram://saved"), fromMessageId, error);
}

TelegramFilesPage TelegramClient::chatMessageFiles(qint64 chatId, const QString &parentPath, qint64 fromMessageId, QString *error)
{
    ActivityScope activity(*this);
    TelegramFilesPage page;
    if (chatId == 0) {
        if (error) {
            *error = QStringLiteral("Telegram chat id is unavailable.");
        }
        return page;
    }
    if (!configureFromEnvironment(error)) {
        return page;
    }
    if (m_state != State::Ready) {
        if (error) {
            *error = m_lastStatus;
        }
        return page;
    }

    constexpr int filePageLimit = 48;
    constexpr int historyChunkLimit = 100;
    qint64 cursor = fromMessageId;
    for (int chunk = 0; chunk < 8 && page.entries.size() < filePageLimit; ++chunk) {
        const qint64 requestCursor = cursor;
        auto historyObject = sendBlocking(td::td_api::make_object<td::td_api::getChatHistory>(chatId, cursor, 0, historyChunkLimit, false),
                                          QStringLiteral("getChatHistory"),
                                          error);
        if (!historyObject || historyObject->get_id() != td::td_api::messages::ID) {
            if (error && error->isEmpty()) {
                *error = QStringLiteral("Telegram chat history is unavailable.");
            }
            return page;
        }

        const auto &messages = static_cast<const td::td_api::messages &>(*historyObject);
        if (messages.messages_.empty()) {
            page.hasMore = false;
            break;
        }

        qint64 nextCursor = cursor;
        bool sawNewMessage = false;
        bool stoppedOnEntryLimit = false;
        qint64 firstMessageId = 0;
        qint64 lastMessageId = 0;
        for (const auto &message : messages.messages_) {
            if (!message) {
                continue;
            }
            if (firstMessageId == 0) {
                firstMessageId = message->id_;
            }
            lastMessageId = message->id_;
            nextCursor = message->id_;
            if (cursor != 0 && message->id_ == cursor) {
                continue;
            }
            sawNewMessage = true;
            if (const std::optional<TelegramEntry> entry = entryFromMessage(*message, parentPath)) {
                page.entries.append(*entry);
                if (page.entries.size() >= filePageLimit) {
                    stoppedOnEntryLimit = true;
                    break;
                }
            }
        }

        page.nextFromMessageId = nextCursor;
        page.hasMore = sawNewMessage && (stoppedOnEntryLimit || messages.messages_.size() > 1);
        traceTelegram(QStringLiteral("history chunk=%1 cursor=%2 tdMessages=%3 firstMessage=%4 lastMessage=%5 newFiles=%6 pageFiles=%7 nextCursor=%8 stoppedOnEntryLimit=%9 sawNewMessage=%10 hasMore=%11")
                          .arg(QString::number(chunk),
                               requestCursor != 0 ? QStringLiteral("set") : QStringLiteral("initial"),
                               QString::number(messages.messages_.size()),
                               firstMessageId != 0 ? QStringLiteral("set") : QStringLiteral("empty"),
                               lastMessageId != 0 ? QStringLiteral("set") : QStringLiteral("empty"),
                               QString::number(page.entries.size()),
                               QString::number(page.entries.size()),
                               nextCursor != 0 ? QStringLiteral("set") : QStringLiteral("empty"),
                               stoppedOnEntryLimit ? QStringLiteral("true") : QStringLiteral("false"),
                               sawNewMessage ? QStringLiteral("true") : QStringLiteral("false"),
                               page.hasMore ? QStringLiteral("true") : QStringLiteral("false")));
        if (!sawNewMessage || nextCursor == cursor || stoppedOnEntryLimit) {
            break;
        }
        cursor = nextCursor;
        if (fromMessageId == 0 && page.entries.isEmpty()) {
            QString ignoredError;
            poll(500, &ignoredError);
        }
    }
    return page;
}

QString TelegramClient::downloadFile(int fileId, QString *error)
{
    return downloadFile(fileId, {}, error);
}

QString TelegramClient::downloadFile(int fileId,
                                     const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                     QString *error,
                                     int timeoutMs)
{
    ActivityScope activity(*this);
    if (fileId <= 0) {
        if (error) {
            *error = QStringLiteral("Telegram file id is unavailable.");
        }
        return {};
    }
    if (!configureFromEnvironment(error)) {
        return {};
    }
    if (m_state != State::Ready) {
        if (error) {
            *error = m_lastStatus;
        }
        return {};
    }

    QMutexLocker locker(&m_clientMutex);
    const quint64 requestId = m_nextRequestId++;
    m_manager->send(m_clientId, requestId, td::td_api::make_object<td::td_api::downloadFile>(fileId, 32, 0, 0, false));

    auto reportFileProgress = [&](const td::td_api::file &file, QString *completedPath) -> bool {
        if (file.id_ != fileId) {
            return true;
        }
        const qint64 total = file.size_ > 0 ? static_cast<qint64>(file.size_) : static_cast<qint64>(file.expected_size_);
        qint64 downloaded = file.local_ ? static_cast<qint64>(file.local_->downloaded_size_) : 0;
        if (file.local_ && file.local_->is_downloading_completed_ && total > 0) {
            downloaded = total;
        }
        if (progress && !progress(downloaded, total)) {
            m_manager->send(m_clientId, m_nextRequestId++, td::td_api::make_object<td::td_api::cancelDownloadFile>(fileId, false));
            if (error) {
                *error = QStringLiteral("Telegram file download cancelled. Retry will resume from TDLib cache when possible.");
            }
            return false;
        }
        if (file.local_ && file.local_->is_downloading_completed_) {
            const QString localPath = QString::fromStdString(file.local_->path_);
            if (!localPath.isEmpty()) {
                if (progress) {
                    progress(total > 0 ? total : downloaded, total > 0 ? total : downloaded);
                }
                if (completedPath) {
                    *completedPath = localPath;
                }
                return false;
            }
        }
        return true;
    };

    QString completedPath;
    const QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        const td::ClientManager::Response response = m_manager->receive(0.1);
        if (!response.object || response.client_id != m_clientId) {
            continue;
        }

        if (response.request_id == 0) {
            if (response.object->get_id() == td::td_api::updateAuthorizationState::ID) {
                const auto &update = static_cast<const td::td_api::updateAuthorizationState &>(*response.object);
                if (update.authorization_state_) {
                    setState(authStateFromTdObject(*update.authorization_state_));
                }
            } else if (response.object->get_id() == td::td_api::updateFile::ID) {
                const auto &update = static_cast<const td::td_api::updateFile &>(*response.object);
                if (update.file_ && !reportFileProgress(*update.file_, &completedPath)) {
                    return completedPath;
                }
            }
            continue;
        }

        if (response.request_id != requestId) {
            traceTelegram(QStringLiteral("skip response request=%1 object=%2")
                              .arg(QString::number(response.request_id), objectLabel(response.object->get_id())));
            continue;
        }

        traceTelegram(QStringLiteral("response downloadFile request=%1 object=%2")
                          .arg(QString::number(requestId), objectLabel(response.object->get_id())));
        if (response.object->get_id() == td::td_api::error::ID) {
            const auto &tdError = static_cast<const td::td_api::error &>(*response.object);
            const QString message = QStringLiteral("TDLib downloadFile failed with code %1: %2")
                .arg(QString::number(tdError.code_), sanitizedTdError(tdError));
            m_lastStatus = message;
            if (error) {
                *error = message;
            }
            return {};
        }
        if (response.object->get_id() == td::td_api::file::ID) {
            const auto &file = static_cast<const td::td_api::file &>(*response.object);
            if (!reportFileProgress(file, &completedPath)) {
                return completedPath;
            }
        }
    }

    m_manager->send(m_clientId, m_nextRequestId++, td::td_api::make_object<td::td_api::cancelDownloadFile>(fileId, false));
    const QString message = QStringLiteral("TDLib downloadFile timed out while state was: %1").arg(m_lastStatus);
    m_lastStatus = message;
    if (error) {
        *error = message;
    }
    return {};
}

bool TelegramClient::sendFile(qint64 chatId,
                              const QString &localFilePath,
                              const QString &mimeType,
                              const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                              QString *error)
{
    ActivityScope activity(*this);
    const QFileInfo fileInfo(localFilePath);
    if (chatId == 0) {
        if (error) {
            *error = QStringLiteral("Telegram chat id is unavailable.");
        }
        return false;
    }
    if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
        if (error) {
            *error = QStringLiteral("Telegram upload source file is unavailable.");
        }
        return false;
    }
    if (!configureFromEnvironment(error)) {
        return false;
    }
    if (m_state != State::Ready) {
        if (error) {
            *error = m_lastStatus;
        }
        return false;
    }

    const qint64 total = fileInfo.size();
    if (progress && !progress(0, total)) {
        if (error) {
            *error = QStringLiteral("Telegram upload cancelled.");
        }
        return false;
    }

    auto response = sendBlocking(td::td_api::make_object<td::td_api::sendMessage>(
                                     chatId,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     inputMessageContentForLocalFile(fileInfo.absoluteFilePath(), mimeType, fileInfo.size())),
                                 QStringLiteral("sendMessage"),
                                 error,
                                 600000);
    if (!response || response->get_id() != td::td_api::message::ID) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Telegram upload failed.");
        }
        return false;
    }
    const auto &sentMessage = static_cast<const td::td_api::message &>(*response);
    if (sentMessage.sending_state_) {
        return waitForMessageSendResults({sentMessage.id_}, total, progress, error);
    }

    if (progress && !progress(total, total)) {
        if (error) {
            *error = QStringLiteral("Telegram upload cancelled.");
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool TelegramClient::sendFileAlbum(qint64 chatId,
                                   const QList<TelegramUploadFile> &files,
                                   const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                   QString *error)
{
    ActivityScope activity(*this);
    if (chatId == 0) {
        if (error) {
            *error = QStringLiteral("Telegram chat id is unavailable.");
        }
        return false;
    }
    if (files.size() < 2 || files.size() > 8) {
        if (error) {
            *error = QStringLiteral("Telegram albums require 2 to 8 files.");
        }
        return false;
    }

    qint64 total = 0;
    std::vector<td::td_api::object_ptr<td::td_api::InputMessageContent>> contents;
    contents.reserve(static_cast<size_t>(files.size()));
    for (const TelegramUploadFile &file : files) {
        const QFileInfo fileInfo(file.localFilePath);
        if (!fileInfo.exists() || !fileInfo.isFile() || !fileInfo.isReadable()) {
            if (error) {
                *error = QStringLiteral("Telegram upload source file is unavailable.");
            }
            return false;
        }
        if (!isTelegramAlbumCompatibleMime(file.mimeType, file.size > 0 ? file.size : fileInfo.size())) {
            if (error) {
                *error = QStringLiteral("Telegram album upload supports image and video files only.");
            }
            return false;
        }
        total += file.size > 0 ? file.size : fileInfo.size();
        contents.push_back(inputMessageContentForLocalFile(fileInfo.absoluteFilePath(), file.mimeType, file.size > 0 ? file.size : fileInfo.size()));
    }

    if (!configureFromEnvironment(error)) {
        return false;
    }
    if (m_state != State::Ready) {
        if (error) {
            *error = m_lastStatus;
        }
        return false;
    }
    if (progress && !progress(0, total)) {
        if (error) {
            *error = QStringLiteral("Telegram upload cancelled.");
        }
        return false;
    }

    auto response = sendBlocking(td::td_api::make_object<td::td_api::sendMessageAlbum>(
                                     chatId,
                                     nullptr,
                                     nullptr,
                                     nullptr,
                                     std::move(contents)),
                                 QStringLiteral("sendMessageAlbum"),
                                 error,
                                 600000);
    if (!response || response->get_id() != td::td_api::messages::ID) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Telegram album upload failed.");
        }
        return false;
    }

    const auto &messages = static_cast<const td::td_api::messages &>(*response);
    QList<qint64> pendingIds;
    for (const auto &message : messages.messages_) {
        if (message && message->sending_state_) {
            pendingIds.append(message->id_);
        }
    }
    if (!pendingIds.isEmpty()) {
        return waitForMessageSendResults(pendingIds, total, progress, error);
    }
    if (progress && !progress(total, total)) {
        if (error) {
            *error = QStringLiteral("Telegram upload cancelled.");
        }
        return false;
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool TelegramClient::ensureStarted(QString *error)
{
    if (m_state == State::NotStarted || m_state == State::Closed) {
        return start(error);
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool TelegramClient::rememberPendingApiCredentialsIfReady(QString *error)
{
    if (m_state != State::Ready || m_pendingApiId <= 0 || m_pendingApiHash.trimmed().isEmpty()) {
        if (error) {
            error->clear();
        }
        return true;
    }
    QString accountError;
    const QString accountLabel = currentUserAccountLabel(&accountError);
    Q_UNUSED(accountError)
    if (!rememberApiCredentials(m_pendingApiId, m_pendingApiHash, accountLabel)) {
        if (error) {
            *error = QStringLiteral("Telegram API credentials could not be saved to secret storage.");
        }
        return false;
    }
    m_pendingApiId = 0;
    m_pendingApiHash.clear();
    if (error) {
        error->clear();
    }
    return true;
}

QString TelegramClient::currentUserAccountLabel(QString *error)
{
    if (m_state != State::Ready) {
        if (error) {
            *error = QStringLiteral("Telegram authorization is not ready.");
        }
        return {};
    }

    auto meObject = sendBlocking(td::td_api::make_object<td::td_api::getMe>(), QStringLiteral("getMe"), error);
    if (!meObject || meObject->get_id() != td::td_api::user::ID) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("Telegram user metadata is unavailable.");
        }
        return {};
    }

    const auto &me = static_cast<const td::td_api::user &>(*meObject);
    QStringList parts;
    const QString firstName = QString::fromStdString(me.first_name_).trimmed();
    const QString lastName = QString::fromStdString(me.last_name_).trimmed();
    if (!firstName.isEmpty()) {
        parts.append(firstName);
    }
    if (!lastName.isEmpty()) {
        parts.append(lastName);
    }
    if (error) {
        error->clear();
    }
    const QString fullName = parts.join(QLatin1Char(' ')).trimmed();
    return fullName.isEmpty() ? QStringLiteral("Telegram account") : fullName;
}

bool TelegramClient::sendTdlibParameters(int apiId, const QString &apiHash, QString *error)
{
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/telegram");
    const QString databaseDirectory = dataRoot + QStringLiteral("/tdlib-db");
    const QString filesDirectory = dataRoot + QStringLiteral("/tdlib-files");
    QDir().mkpath(databaseDirectory);
    QDir().mkpath(filesDirectory);

    const QByteArray encryptionKey = qgetenv("FM_TELEGRAM_DATABASE_KEY");
    auto response = sendBlocking(td::td_api::make_object<td::td_api::setTdlibParameters>(
                                     false,
                                     databaseDirectory.toStdString(),
                                     filesDirectory.toStdString(),
                                     std::string(encryptionKey.constData(), size_t(encryptionKey.size())),
                                     true,
                                     true,
                                     true,
                                     false,
                                     apiId,
                                     apiHash.toStdString(),
                                     QStringLiteral("en").toStdString(),
                                     QSysInfo::prettyProductName().toStdString(),
                                     QSysInfo::kernelVersion().toStdString(),
                                     QStringLiteral("FMQml 0.1.0").toStdString()),
                                 QStringLiteral("setTdlibParameters"),
                                 error,
                                 30000);
    if (!response) {
        return false;
    }
    pollBriefly(error);
    return error ? error->isEmpty() : true;
}

td::td_api::object_ptr<td::td_api::Object> TelegramClient::sendBlocking(td::td_api::object_ptr<td::td_api::Function> &&request,
                                                                         const QString &label,
                                                                         QString *error,
                                                                         int timeoutMs)
{
    QMutexLocker locker(&m_clientMutex);
    if (!ensureStarted(error)) {
        return nullptr;
    }
    const quint64 requestId = m_nextRequestId++;
    traceTelegram(QStringLiteral("send %1 request=%2 state=%3").arg(label, QString::number(requestId), m_lastStatus));
    m_manager->send(m_clientId, requestId, std::move(request));

    const QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        td::ClientManager::Response response = m_manager->receive(0.1);
        if (!response.object || response.client_id != m_clientId) {
            continue;
        }

        if (response.request_id == 0) {
            if (response.object->get_id() == td::td_api::updateAuthorizationState::ID) {
                const auto &update = static_cast<const td::td_api::updateAuthorizationState &>(*response.object);
                if (update.authorization_state_) {
                    setState(authStateFromTdObject(*update.authorization_state_));
                    traceTelegram(QStringLiteral("update auth state=%1").arg(m_lastStatus));
                }
            }
            continue;
        }

        if (response.request_id != requestId) {
            traceTelegram(QStringLiteral("skip response request=%1 object=%2")
                              .arg(QString::number(response.request_id), objectLabel(response.object->get_id())));
            continue;
        }
        traceTelegram(QStringLiteral("response %1 request=%2 object=%3")
                          .arg(label, QString::number(requestId), objectLabel(response.object->get_id())));
        if (response.object->get_id() == td::td_api::error::ID) {
            const auto &tdError = static_cast<const td::td_api::error &>(*response.object);
            const QString message = QStringLiteral("TDLib %1 failed with code %2: %3")
                .arg(label, QString::number(tdError.code_), sanitizedTdError(tdError));
            m_lastStatus = message;
            if (error) {
                *error = message;
            }
            return nullptr;
        }
        if (error) {
            error->clear();
        }
        return std::move(response.object);
    }

    const QString message = QStringLiteral("TDLib %1 timed out while state was: %2").arg(label, m_lastStatus);
    m_lastStatus = message;
    if (error) {
        *error = message;
    }
    return nullptr;
}

void TelegramClient::sendGetAuthorizationState()
{
    if (!m_manager || m_clientId == 0) {
        return;
    }
    m_manager->send(m_clientId, m_nextRequestId++, td::td_api::make_object<td::td_api::getAuthorizationState>());
}

void TelegramClient::handleObject(int objectId)
{
    Q_UNUSED(objectId)
}

void TelegramClient::setState(State state)
{
    m_state = state;
    m_lastStatus = stateLabel(state);
}

void TelegramClient::pollBriefly(QString *error)
{
    for (int i = 0; i < 8; ++i) {
        if (!poll(100, error)) {
            return;
        }
    }
}

bool TelegramClient::pollUntilStateChanges(State previousState, int timeoutMs, QString *error)
{
    if (error) {
        error->clear();
    }
    const QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired()) {
        if (!poll(250, error)) {
            return false;
        }
        if (m_state != previousState || m_state == State::Ready || m_state == State::Closed) {
            return true;
        }
    }
    const QString message = QStringLiteral("TDLib state did not change from: %1").arg(stateLabel(previousState));
    m_lastStatus = message;
    if (error) {
        *error = message;
    }
    return false;
}

bool TelegramClient::waitForMessageSendResults(const QList<qint64> &pendingMessageIds,
                                               qint64 totalBytes,
                                               const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                               QString *error)
{
    QSet<qint64> pending;
    for (const qint64 messageId : pendingMessageIds) {
        pending.insert(messageId);
    }
    const int initialCount = pending.size();
    if (initialCount == 0) {
        if (progress && !progress(totalBytes, totalBytes)) {
            if (error) {
                *error = QStringLiteral("Telegram upload cancelled.");
            }
            return false;
        }
        if (error) {
            error->clear();
        }
        return true;
    }

    const QDeadlineTimer deadline(600000);
    while (!deadline.hasExpired()) {
        const qint64 completed = initialCount - pending.size();
        const qint64 processed = initialCount > 0
            ? (totalBytes * completed) / initialCount
            : totalBytes;
        if (progress && !progress(processed, totalBytes)) {
            if (error) {
                *error = QStringLiteral("Telegram upload cancelled.");
            }
            return false;
        }

        QMutexLocker locker(&m_clientMutex);
        const td::ClientManager::Response updateResponse = m_manager->receive(0.1);
        if (!updateResponse.object || updateResponse.client_id != m_clientId) {
            continue;
        }
        if (updateResponse.object->get_id() == td::td_api::updateAuthorizationState::ID) {
            const auto &update = static_cast<const td::td_api::updateAuthorizationState &>(*updateResponse.object);
            if (update.authorization_state_) {
                setState(authStateFromTdObject(*update.authorization_state_));
            }
            continue;
        }
        if (updateResponse.object->get_id() == td::td_api::updateMessageSendSucceeded::ID) {
            const auto &update = static_cast<const td::td_api::updateMessageSendSucceeded &>(*updateResponse.object);
            pending.remove(update.old_message_id_);
            if (pending.isEmpty()) {
                if (progress) {
                    progress(totalBytes, totalBytes);
                }
                if (error) {
                    error->clear();
                }
                return true;
            }
        } else if (updateResponse.object->get_id() == td::td_api::updateMessageSendFailed::ID) {
            const auto &update = static_cast<const td::td_api::updateMessageSendFailed &>(*updateResponse.object);
            if (pending.contains(update.old_message_id_)) {
                const QString message = update.error_
                    ? QStringLiteral("Telegram upload failed with code %1: %2")
                          .arg(QString::number(update.error_->code_), sanitizedTdError(*update.error_))
                    : QStringLiteral("Telegram upload failed.");
                if (error) {
                    *error = message;
                }
                return false;
            }
        }
    }

    if (error) {
        *error = QStringLiteral("Telegram upload timed out.");
    }
    return false;
}

void TelegramClient::beginActivity()
{
    QMutexLocker locker(&m_clientMutex);
    ++m_activeOperations;
    ++m_idleGeneration;
}

void TelegramClient::endActivity()
{
    QMutexLocker locker(&m_clientMutex);
    if (m_activeOperations > 0) {
        --m_activeOperations;
    }
    scheduleIdleCloseLocked();
}

bool TelegramClient::canIdleCloseLocked() const
{
    return m_manager && (m_state == State::Ready || m_state == State::WaitTdlibParameters);
}

void TelegramClient::scheduleIdleCloseLocked()
{
    if (m_activeOperations > 0 || !canIdleCloseLocked()) {
        return;
    }

    const int timeoutMs = idleTimeoutMs();
    if (timeoutMs <= 0) {
        return;
    }

    QObject *context = QCoreApplication::instance();
    if (!context) {
        return;
    }

    const quint64 generation = ++m_idleGeneration;
    QTimer::singleShot(timeoutMs, context, [this, generation]() {
        QMutexLocker locker(&m_clientMutex);
        if (generation != m_idleGeneration || m_activeOperations > 0 || !canIdleCloseLocked()) {
            return;
        }
        traceTelegram(QStringLiteral("idle close state=%1 timeoutMs=%2").arg(m_lastStatus, QString::number(idleTimeoutMs())));
        close();
    });
}

TelegramClient &sharedTelegramClient()
{
    static TelegramClient client;
    return client;
}

} // namespace TelegramProviderInternal
