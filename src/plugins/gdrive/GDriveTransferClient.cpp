#include "GDriveTransferClient.h"

#include "GDriveApiClient.h"
#include "GDriveExportPolicy.h"
#include "GDrivePath.h"
#include "GDriveRequestPolicy.h"

#include <algorithm>
#include <atomic>

#include <QCryptographicHash>
#include <QDir>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QJsonDocument>
#include <QJsonArray>
#include <QMimeDatabase>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QtConcurrent>

namespace GDriveTransferClient {
namespace {
constexpr qint64 SmallMultipartUploadThresholdBytes = 1 * 1024 * 1024;
constexpr qint64 ResumableUploadChunkBytes = 8 * 1024 * 1024;
constexpr int TransferIdleTimeoutMs = 120000;
constexpr int DefaultUploadStallLogMs = 15000;
constexpr int DefaultSmallUploadConcurrency = 6;
constexpr int MaxSmallUploadConcurrency = 12;
constexpr int DefaultDownloadConcurrency = 4;
constexpr int MaxDownloadConcurrency = 8;
constexpr int MaxResumableChunkAttempts = 5;
constexpr QLatin1StringView DriveFileFields{
    "id,name,mimeType,size,modifiedTime,createdTime,parents,webViewLink,ownedByMe,shared,"
    "thumbnailLink,shortcutDetails(targetId,targetMimeType,targetResourceKey),"
    "capabilities(canDownload,canEdit,canAddChildren,canListChildren,canRename,canTrash,canDelete,canCopy)"};
using GDriveExportPolicy::exportFormatForGoogleAppsDownload;
using GDriveExportPolicy::isGoogleAppsMimeType;

struct GDriveUploadReplyResult {
    QByteArray body;
    QHash<QByteArray, QByteArray> headers;
    QString error;
    int status = 0;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
    qint64 elapsedMs = 0;
    bool timedOut = false;
    bool canceled = false;
};

QString uploadContextText(const UploadLogContext &context)
{
    if (context.batchId.isEmpty()) {
        return QStringLiteral("single");
    }
    return QStringLiteral("batch=%1 index=%2/%3 wave=%4-%5 attempt=%6")
        .arg(context.batchId)
        .arg(context.batchIndex + 1)
        .arg(context.batchCount)
        .arg(context.waveStart + 1)
        .arg(context.waveEnd)
        .arg(context.attempt);
}


bool isRetryableDriveUploadStatus(int status);
bool isRetryableDriveUploadNetworkError(QNetworkReply::NetworkError error);

qint64 acknowledgedResumableOffset(const QHash<QByteArray, QByteArray> &headers);
GDriveUploadReplyResult waitForUploadReply(QNetworkReply *reply,
                                           int timeoutMs,
                                           const QString &timeoutMessage,
                                           const std::function<bool(qint64 sent, qint64 total)> &progress);
bool queryResumableUploadOffset(QNetworkAccessManager &network,
                                const QUrl &sessionUrl,
                                qint64 fileSize,
                                const QString &accessToken,
                                qint64 *offset,
                                QString *error);

QByteArray resourceKeyHeaderValue(const QString &path, const QString &resourceKey)
{
    const QString cleanResourceKey = resourceKey.trimmed();
    if (cleanResourceKey.isEmpty()) {
        return {};
    }
    const QString fileId = GDrivePath::idForItemPath(path).trimmed();
    if (fileId.isEmpty()) {
        return {};
    }
    return QStringLiteral("%1/%2").arg(fileId, cleanResourceKey).toUtf8();
}

void applyResourceKeyHeader(QNetworkRequest *request, const QString &path, const QString &resourceKey)
{
    if (!request) {
        return;
    }
    const QByteArray headerValue = resourceKeyHeaderValue(path, resourceKey);
    if (!headerValue.isEmpty()) {
        request->setRawHeader("X-Goog-Drive-Resource-Keys", headerValue);
    }
}

QUrl driveDownloadUrl(const QString &path,
                      const QString &mimeType,
                      const QString &destinationFilePath)
{
    const QString id = GDrivePath::idForItemPath(path);
    if (id.isEmpty()) {
        return {};
    }

    QUrl url;
    QUrlQuery query;
    if (isGoogleAppsMimeType(mimeType)) {
        url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1/export")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(id))));
        query.addQueryItem(QStringLiteral("mimeType"),
                           exportFormatForGoogleAppsDownload(mimeType, destinationFilePath).mimeType);
    } else {
        url = QUrl(QStringLiteral("https://www.googleapis.com/drive/v3/files/%1")
                       .arg(QString::fromLatin1(QUrl::toPercentEncoding(id))));
        query.addQueryItem(QStringLiteral("alt"), QStringLiteral("media"));
        query.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
    }
    url.setQuery(query);
    return url;
}

bool downloadFileToLocalFileImpl(QNetworkAccessManager &network,
                                  const QString &sourcePath,
                                  const QString &mimeType,
                                  const QString &destinationFilePath,
                                  const QString &accessToken,
                                  const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                  QString *error,
                                  const QString &resourceKey)
{
    const QUrl url = driveDownloadUrl(sourcePath, mimeType, destinationFilePath);
    if (!url.isValid()) {
        if (error) {
            *error = QStringLiteral("Google Drive file path is invalid");
        }
        return false;
    }

    QFileInfo destinationInfo(destinationFilePath);
    if (!destinationInfo.absoluteDir().exists()) {
        if (error) {
            *error = QStringLiteral("Google Drive download destination does not exist: %1")
                .arg(destinationInfo.absolutePath());
        }
        return false;
    }

    QFile output(destinationFilePath);
    if (!output.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) {
            *error = QStringLiteral("Cannot open Google Drive destination: %1").arg(output.errorString());
        }
        return false;
    }

    QNetworkRequest request(url);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    applyResourceKeyHeader(&request, sourcePath, resourceKey);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = network.get(request);
    QEventLoop loop;
    QByteArray errorBody;
    QString writeError;
    bool canceled = false;
    bool writeFailed = false;
    bool timedOut = false;
    QTimer idleTimeout;
    idleTimeout.setSingleShot(true);

    auto consumeReadyRead = [&]() {
        if (!reply->isOpen()) {
            return;
        }
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QByteArray data = GDriveApiClient::safeReadAll(reply);
        if (data.isEmpty()) {
            return;
        }
        if (status >= 400) {
            errorBody.append(data);
            return;
        }
        if (writeFailed) {
            return;
        }
        if (output.write(data) != data.size()) {
            writeFailed = true;
            writeError = output.errorString();
            reply->abort();
        }
    };

    QObject::connect(&idleTimeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
    });
    QObject::connect(reply, &QIODevice::readyRead, &loop, [&]() {
        idleTimeout.start(TransferIdleTimeoutMs);
        consumeReadyRead();
    });
    QObject::connect(reply, &QNetworkReply::downloadProgress, &loop, [&](qint64 received, qint64 total) {
        idleTimeout.start(TransferIdleTimeoutMs);
        if (progress && !progress(received, total)) {
            canceled = true;
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    idleTimeout.start(TransferIdleTimeoutMs);
    loop.exec();
    idleTimeout.stop();
    consumeReadyRead();

    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorString = reply->errorString();
    delete reply;

    const bool flushed = output.flush();
    const QString flushError = output.errorString();
    output.close();

    if (writeFailed) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = QStringLiteral("Google Drive download write failed: %1").arg(writeError);
        }
        return false;
    }

    if (!flushed) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = QStringLiteral("Google Drive download flush failed: %1").arg(flushError);
        }
        return false;
    }

    if (timedOut) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = QStringLiteral("Google Drive download timed out");
        }
        return false;
    }

    if (canceled || networkError == QNetworkReply::OperationCanceledError) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = QStringLiteral("Google Drive download canceled");
        }
        return false;
    }

    if (networkError != QNetworkReply::NoError) {
        QFile::remove(destinationFilePath);
        if (error) {
            *error = GDriveApiClient::errorMessage(errorBody,
                                       QStringLiteral("Google Drive download failed: %1").arg(networkErrorString));
        }
        return false;
    }

    return true;
}

void disableHttp2ForUpload(QNetworkRequest *request)
{
    if (!request) {
        return;
    }
    request->setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
}

QString mimeTypeForLocalUpload(const QString &sourceFilePath)
{
    const QString mimeType = QMimeDatabase().mimeTypeForFile(sourceFilePath, QMimeDatabase::MatchExtension).name();
    return mimeType.isEmpty() ? QStringLiteral("application/octet-stream") : mimeType;
}

QUrl driveMultipartUploadUrl()
{
    QUrl url(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("multipart"));
    query.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("fields"), QString(DriveFileFields));
    url.setQuery(query);
    return url;
}

QUrl driveUploadSessionUrl()
{
    QUrl url(QStringLiteral("https://www.googleapis.com/upload/drive/v3/files"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("uploadType"), QStringLiteral("resumable"));
    query.addQueryItem(QStringLiteral("supportsAllDrives"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("fields"), QString(DriveFileFields));
    url.setQuery(query);
    return url;
}

bool uploadLoggingEnabledImpl();
bool downloadLoggingEnabledImpl();
bool downloadRangeLoggingEnabledImpl();
int gdriveUploadStallLogIntervalMs();
int downloadConcurrencyImpl();

bool uploadSmallLocalFileToDriveBlocking(QNetworkAccessManager &network,
                                         QFile &file,
                                         const QString &parentId,
                                         const QString &name,
                                         const QString &mimeType,
                                         const QString &accessToken,
                                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                         QJsonObject *createdObject,
                                         QString *error,
                                         const UploadLogContext &logContext)
{
    if (!file.seek(0)) {
        if (error) {
            *error = QStringLiteral("Cannot rewind local file for Google Drive upload: %1").arg(file.errorString());
        }
        return false;
    }

    const QByteArray fileBytes = file.readAll();
    if (file.error() != QFile::NoError) {
        if (error) {
            *error = QStringLiteral("Cannot read local file for Google Drive upload: %1").arg(file.errorString());
        }
        return false;
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("name"), name);
    metadata.insert(QStringLiteral("parents"), QJsonArray{parentId});

    const QByteArray boundary = QByteArrayLiteral("fmqml-gdrive-") + QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    QByteArray body;
    body.reserve(QJsonDocument(metadata).toJson(QJsonDocument::Compact).size() + fileBytes.size() + 512);
    body += QByteArrayLiteral("--") + boundary + QByteArrayLiteral("\r\n");
    body += QByteArrayLiteral("Content-Type: application/json; charset=UTF-8\r\n\r\n");
    body += QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    body += QByteArrayLiteral("\r\n--") + boundary + QByteArrayLiteral("\r\n");
    body += QByteArrayLiteral("Content-Type: ") + mimeType.toUtf8() + QByteArrayLiteral("\r\n\r\n");
    body += fileBytes;
    body += QByteArrayLiteral("\r\n--") + boundary + QByteArrayLiteral("--\r\n");

    QNetworkRequest uploadRequest = GDriveApiClient::authorizedJsonRequest(driveMultipartUploadUrl(), accessToken);
    uploadRequest.setHeader(QNetworkRequest::ContentTypeHeader,
                            QStringLiteral("multipart/related; boundary=%1").arg(QString::fromLatin1(boundary)));
    uploadRequest.setHeader(QNetworkRequest::ContentLengthHeader, body.size());

    if (!GDriveRequestPolicy::waitForCooldown(QLatin1StringView("multipartUpload"), [&]() {
            return progress && !progress(0, file.size());
        })) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }
    QNetworkReply *uploadReply = network.post(uploadRequest, body);
    QEventLoop uploadLoop;
    QTimer uploadIdleTimeout;
    QTimer uploadStallLogTimer;
    QElapsedTimer uploadTimer;
    QElapsedTimer uploadProgressTimer;
    bool canceled = false;
    bool timedOut = false;
    qint64 lastSent = 0;
    qint64 lastTotal = 0;
    int stallLogCount = 0;
    const bool logging = uploadLoggingEnabledImpl();
    const int stallLogMs = logging ? gdriveUploadStallLogIntervalMs() : 0;
    uploadTimer.start();
    uploadProgressTimer.start();
    if (logging) {
        qInfo() << "GDrive upload attempt started"
                << "mode" << "multipart"
                << "context" << uploadContextText(logContext)
                << "name" << name
                << "bytes" << file.size();
    }
    uploadIdleTimeout.setSingleShot(true);
    QObject::connect(&uploadIdleTimeout, &QTimer::timeout, &uploadLoop, [&]() {
        timedOut = true;
        uploadReply->abort();
    });
    if (stallLogMs > 0) {
        QObject::connect(&uploadStallLogTimer, &QTimer::timeout, &uploadLoop, [&]() {
            ++stallLogCount;
            if (progress && !progress(file.size(), file.size())) {
                canceled = true;
                uploadReply->abort();
                return;
            }
            qInfo() << "GDrive upload no progress"
                    << "mode" << "multipart"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "sent" << lastSent
                    << "total" << lastTotal
                    << "idleMs" << uploadProgressTimer.elapsed()
                    << "elapsedMs" << uploadTimer.elapsed()
                    << "count" << stallLogCount;
        });
        uploadStallLogTimer.start(stallLogMs);
    }
    QObject::connect(uploadReply, &QNetworkReply::uploadProgress, &uploadLoop, [&](qint64 sent, qint64 total) {
        uploadIdleTimeout.start(TransferIdleTimeoutMs);
        lastSent = sent;
        lastTotal = total;
        uploadProgressTimer.restart();
        stallLogCount = 0;
        const qint64 fileSize = file.size();
        qint64 processed = fileSize;
        if (total > 0 && fileSize > 0) {
            processed = std::clamp<qint64>((sent * fileSize) / total, 0, fileSize);
        } else if (fileSize > 0) {
            processed = std::clamp<qint64>(sent, 0, fileSize);
        }
        if (progress && !progress(processed, fileSize)) {
            canceled = true;
            uploadReply->abort();
        }
    });
    QObject::connect(uploadReply, &QNetworkReply::finished, &uploadLoop, &QEventLoop::quit);
    uploadIdleTimeout.start(TransferIdleTimeoutMs);
    uploadLoop.exec();
    uploadIdleTimeout.stop();
    uploadStallLogTimer.stop();

    const QByteArray uploadBody = GDriveApiClient::safeReadAll(uploadReply);
    QHash<QByteArray, QByteArray> uploadHeaders;
    for (const QByteArray &header : uploadReply->rawHeaderList()) {
        const QByteArray value = uploadReply->rawHeader(header);
        uploadHeaders.insert(header, value);
        uploadHeaders.insert(header.toLower(), value);
    }
    const QNetworkReply::NetworkError networkError = uploadReply->error();
    const QString networkErrorString = uploadReply->errorString();
    const int status = uploadReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    delete uploadReply;

    if (timedOut) {
        if (error) {
            *error = QStringLiteral("Google Drive upload timed out");
        }
        if (logging) {
            qInfo() << "GDrive upload attempt failed"
                    << "mode" << "multipart"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "elapsedMs" << uploadTimer.elapsed()
                    << "reason" << "timeout";
        }
        return false;
    }
    if (canceled || networkError == QNetworkReply::OperationCanceledError) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        if (logging) {
            qInfo() << "GDrive upload attempt failed"
                    << "mode" << "multipart"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "elapsedMs" << uploadTimer.elapsed()
                    << "reason" << "canceled";
        }
        return false;
    }
    if (networkError != QNetworkReply::NoError || status >= 400) {
        const QString message = status > 0
            ? GDriveApiClient::errorMessage(uploadBody, QStringLiteral("Google Drive upload failed with HTTP %1").arg(status))
            : GDriveApiClient::errorMessage(uploadBody, QStringLiteral("Google Drive upload failed: %1").arg(networkErrorString));
        if (error) {
            *error = message;
        }
        if (logging) {
            qInfo() << "GDrive upload attempt failed"
                    << "mode" << "multipart"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "elapsedMs" << uploadTimer.elapsed()
                    << "httpStatus" << status
                    << "networkError" << networkError
                    << "message" << message.left(180);
        }
        if (GDriveRequestPolicy::isRetryableError(message)) {
            GDriveRequestPolicy::noteThrottle(QLatin1StringView("multipartUpload"), uploadHeaders, 1, message);
        }
        return false;
    }

    if (progress && !progress(file.size(), file.size())) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }

    if (logging) {
        qInfo() << "GDrive upload attempt finished"
                << "mode" << "multipart"
                << "context" << uploadContextText(logContext)
                << "name" << name
                << "bytes" << file.size()
                << "elapsedMs" << uploadTimer.elapsed();
    }
    GDriveRequestPolicy::noteSuccess();
    return GDriveApiClient::parseFileResponse(uploadBody, createdObject, error);
}

bool uploadLocalFileToDriveBlocking(QNetworkAccessManager &network,
                                    const QString &sourceFilePath,
                                    const QString &parentId,
                                    const QString &name,
                                    const QString &accessToken,
                                    const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                    QJsonObject *createdObject,
                                    QString *error,
                                    const UploadLogContext &logContext)
{
    QFile file(sourceFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) {
            *error = QStringLiteral("Cannot read local file for Google Drive upload: %1").arg(file.errorString());
        }
        return false;
    }

    const QString mimeType = mimeTypeForLocalUpload(sourceFilePath);
    if (file.size() <= SmallMultipartUploadThresholdBytes) {
        return uploadSmallLocalFileToDriveBlocking(network,
                                                   file,
                                                   parentId,
                                                   name,
                                                   mimeType,
                                                   accessToken,
                                                   progress,
                                                   createdObject,
                                                   error,
                                                   logContext);
    }

    QJsonObject metadata;
    metadata.insert(QStringLiteral("name"), name);
    metadata.insert(QStringLiteral("parents"), QJsonArray{parentId});

    QNetworkRequest sessionRequest = GDriveApiClient::authorizedJsonRequest(driveUploadSessionUrl(), accessToken);
    sessionRequest.setRawHeader("X-Upload-Content-Type", mimeType.toUtf8());
    sessionRequest.setRawHeader("X-Upload-Content-Length", QByteArray::number(file.size()));
    disableHttp2ForUpload(&sessionRequest);

    QByteArray sessionBody;
    QHash<QByteArray, QByteArray> sessionHeaders;
    if (!GDriveRequestPolicy::waitForCooldown(QLatin1StringView("uploadSession"), [&]() {
            return progress && !progress(0, file.size());
        })) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }
    QNetworkReply *sessionReply = network.post(sessionRequest, QJsonDocument(metadata).toJson(QJsonDocument::Compact));
    if (!GDriveApiClient::waitForReply(sessionReply,
                      60000,
                      QStringLiteral("Google Drive upload session timed out"),
                      &sessionBody,
                      error,
                      &sessionHeaders)) {
        if (uploadLoggingEnabledImpl()) {
            qInfo() << "GDrive upload session failed"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "bytes" << file.size()
                    << "message" << (error ? error->left(180) : QString{});
        }
        if (error && GDriveRequestPolicy::isRetryableError(*error)) {
            GDriveRequestPolicy::noteThrottle(QLatin1StringView("uploadSession"), sessionHeaders, 1, *error);
        }
        return false;
    }
    GDriveRequestPolicy::noteSuccess();

    const QByteArray sessionUrlBytes = sessionHeaders.value(QByteArrayLiteral("location")).trimmed();
    const QUrl sessionUrl(QString::fromUtf8(sessionUrlBytes));
    if (sessionUrlBytes.isEmpty() || !sessionUrl.isValid() || sessionUrl.isRelative()) {
        if (error) {
            *error = QStringLiteral("Google Drive upload session response has no upload URL");
        }
        return false;
    }

    const bool logging = uploadLoggingEnabledImpl();
    QElapsedTimer uploadTimer;
    uploadTimer.start();
    if (logging) {
        qInfo() << "GDrive upload attempt started"
                << "mode" << "resumable"
                << "context" << uploadContextText(logContext)
                << "name" << name
                << "bytes" << file.size()
                << "chunkBytes" << ResumableUploadChunkBytes;
    }

    QByteArray finalBody;
    qint64 offset = 0;
    int consecutiveRetry = 0;

    while (offset < file.size()) {
        if (!file.seek(offset)) {
            if (error) {
                *error = QStringLiteral("Cannot seek local file for Google Drive upload: %1").arg(file.errorString());
            }
            return false;
        }

        const qint64 chunkSize = (std::min<qint64>)(ResumableUploadChunkBytes, file.size() - offset);
        const QByteArray chunk = file.read(chunkSize);
        if (chunk.size() != chunkSize) {
            if (error) {
                *error = QStringLiteral("Cannot read local file chunk for Google Drive upload: %1").arg(file.errorString());
            }
            return false;
        }

        QNetworkRequest uploadRequest(sessionUrl);
        uploadRequest.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
        uploadRequest.setHeader(QNetworkRequest::ContentTypeHeader, mimeType);
        uploadRequest.setHeader(QNetworkRequest::ContentLengthHeader, chunk.size());
        uploadRequest.setRawHeader("Content-Range",
                                   QByteArrayLiteral("bytes ")
                                       + QByteArray::number(offset)
                                       + QByteArrayLiteral("-")
                                       + QByteArray::number(offset + chunk.size() - 1)
                                       + QByteArrayLiteral("/")
                                       + QByteArray::number(file.size()));
        uploadRequest.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        disableHttp2ForUpload(&uploadRequest);

        const qint64 chunkOffset = offset;
        if (logging) {
            qInfo() << "GDrive upload chunk started"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "offset" << chunkOffset
                    << "bytes" << chunk.size()
                    << "attempt" << (consecutiveRetry + 1);
        }

        if (!GDriveRequestPolicy::waitForCooldown(QLatin1StringView("uploadChunk"), [&]() {
                return progress && !progress(offset, file.size());
            })) {
            if (error) {
                *error = QStringLiteral("Google Drive upload canceled");
            }
            return false;
        }
        const GDriveUploadReplyResult reply = waitForUploadReply(
            network.put(uploadRequest, chunk),
            TransferIdleTimeoutMs,
            QStringLiteral("Google Drive upload timed out"),
            [&, chunkOffset](qint64 sent, qint64 total) -> bool {
                Q_UNUSED(total)
                return !progress || progress(chunkOffset + sent, file.size());
            });

        if (reply.canceled) {
            if (error) {
                *error = reply.error;
            }
            if (logging) {
                qInfo() << "GDrive upload attempt failed"
                        << "mode" << "resumable"
                        << "context" << uploadContextText(logContext)
                        << "name" << name
                        << "bytes" << file.size()
                        << "elapsedMs" << uploadTimer.elapsed()
                        << "reason" << "canceled";
            }
            return false;
        }

        if (reply.status == 308) {
            const qint64 nextOffset = std::clamp<qint64>(acknowledgedResumableOffset(reply.headers), offset, file.size());
            offset = nextOffset > offset ? nextOffset : offset + chunk.size();
            consecutiveRetry = 0;
            if (logging) {
                qInfo() << "GDrive upload chunk accepted"
                        << "context" << uploadContextText(logContext)
                        << "name" << name
                        << "nextOffset" << offset
                        << "elapsedMs" << reply.elapsedMs;
            }
            continue;
        }

        if ((reply.status == 200 || reply.status == 201) && reply.error.isEmpty()) {
            finalBody = reply.body;
            offset = file.size();
            if (progress && !progress(file.size(), file.size())) {
                if (error) {
                    *error = QStringLiteral("Google Drive upload canceled");
                }
                return false;
            }
            break;
        }

        const QString failureMessage = reply.error.trimmed().isEmpty()
            ? QStringLiteral("Google Drive upload failed with HTTP %1").arg(reply.status)
            : reply.error;
        const bool rateLimited = GDriveRequestPolicy::isRateLimitError(failureMessage);
        if (rateLimited) {
            if (error) {
                *error = failureMessage;
            }
            if (logging) {
                qInfo() << "GDrive upload attempt failed"
                        << "mode" << "resumable"
                        << "context" << uploadContextText(logContext)
                        << "name" << name
                        << "bytes" << file.size()
                        << "elapsedMs" << uploadTimer.elapsed()
                        << "httpStatus" << reply.status
                        << "networkError" << reply.networkError
                        << "message" << failureMessage.left(180)
                        << "reason" << "rate-limit-abort";
            }
            GDriveRequestPolicy::noteThrottle(QLatin1StringView("uploadChunk"), reply.headers, consecutiveRetry + 1, failureMessage);
            return false;
        }

        const bool retryable = reply.timedOut
            || isRetryableDriveUploadStatus(reply.status)
            || isRetryableDriveUploadNetworkError(reply.networkError)
            || GDriveRequestPolicy::isRetryableError(reply.error);
        if (!retryable || consecutiveRetry >= MaxResumableChunkAttempts - 1) {
            if (error) {
                *error = failureMessage;
            }
            if (logging) {
                qInfo() << "GDrive upload attempt failed"
                        << "mode" << "resumable"
                        << "context" << uploadContextText(logContext)
                        << "name" << name
                        << "bytes" << file.size()
                        << "elapsedMs" << uploadTimer.elapsed()
                        << "httpStatus" << reply.status
                        << "networkError" << reply.networkError
                        << "message" << failureMessage.left(180);
            }
            if (retryable) {
                GDriveRequestPolicy::noteThrottle(QLatin1StringView("uploadChunk"), reply.headers, consecutiveRetry + 1, failureMessage);
            }
            return false;
        }

        ++consecutiveRetry;
        qint64 confirmedOffset = offset;
        QString queryError;
        queryResumableUploadOffset(network, sessionUrl, file.size(), accessToken, &confirmedOffset, &queryError);
        offset = std::clamp<qint64>(confirmedOffset, 0, file.size());
        const int delayMs = GDriveRequestPolicy::retryDelayMs(reply.headers, consecutiveRetry);
        GDriveRequestPolicy::noteThrottle(QLatin1StringView("uploadChunk"), reply.headers, consecutiveRetry, reply.error);
        if (logging) {
            qInfo() << "GDrive upload chunk retry scheduled"
                    << "context" << uploadContextText(logContext)
                    << "name" << name
                    << "offset" << offset
                    << "delayMs" << delayMs
                    << "httpStatus" << reply.status
                    << "message" << reply.error.left(180);
        }
        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    if (logging) {
        qInfo() << "GDrive upload attempt finished"
                << "mode" << "resumable"
                << "context" << uploadContextText(logContext)
                << "name" << name
                << "bytes" << file.size()
                << "elapsedMs" << uploadTimer.elapsed();
    }
    GDriveRequestPolicy::noteSuccess();
    return GDriveApiClient::parseFileResponse(finalBody, createdObject, error);
}


bool uploadLoggingEnabledImpl()
{
    return qEnvironmentVariableIntValue("FMQML_GDRIVE_UPLOAD_LOG") > 0;
}

bool downloadLoggingEnabledImpl()
{
    return qEnvironmentVariableIntValue("FMQML_GDRIVE_DOWNLOAD_LOG") > 0;
}

bool downloadRangeLoggingEnabledImpl()
{
    return qEnvironmentVariableIntValue("FMQML_GDRIVE_DOWNLOAD_RANGE_LOG") > 0;
}

int gdriveUploadStallLogIntervalMs()
{
    bool ok = false;
    const int requested = qEnvironmentVariableIntValue("FMQML_GDRIVE_UPLOAD_STALL_LOG_MS", &ok);
    if (!ok) {
        return DefaultUploadStallLogMs;
    }
    if (requested <= 0) {
        return 0;
    }
    return std::clamp(requested, 1000, TransferIdleTimeoutMs);
}

int smallUploadConcurrencyImpl()
{
    bool ok = false;
    const int requested = qEnvironmentVariableIntValue("FMQML_GDRIVE_UPLOAD_CONCURRENCY", &ok);
    if (!ok) {
        return DefaultSmallUploadConcurrency;
    }
    return std::clamp(requested, 1, MaxSmallUploadConcurrency);
}

int downloadConcurrencyImpl()
{
    bool ok = false;
    const int requested = qEnvironmentVariableIntValue("FMQML_GDRIVE_DOWNLOAD_CONCURRENCY", &ok);
    if (!ok) {
        return DefaultDownloadConcurrency;
    }
    return std::clamp(requested, 1, MaxDownloadConcurrency);
}





bool isRetryableDriveUploadStatus(int status)
{
    return status == 403
        || status == 408
        || status == 429
        || status == 500
        || status == 502
        || status == 503
        || status == 504;
}

bool isRetryableDriveUploadNetworkError(QNetworkReply::NetworkError error)
{
    return error == QNetworkReply::RemoteHostClosedError
        || error == QNetworkReply::TimeoutError
        || error == QNetworkReply::TemporaryNetworkFailureError
        || error == QNetworkReply::NetworkSessionFailedError
        || error == QNetworkReply::ProtocolFailure
        || error == QNetworkReply::ProtocolUnknownError
        || error == QNetworkReply::UnknownNetworkError;
}











qint64 acknowledgedResumableOffset(const QHash<QByteArray, QByteArray> &headers)
{
    const QString range = QString::fromLatin1(headers.value(QByteArrayLiteral("range")).trimmed());
    if (range.isEmpty()) {
        return 0;
    }

    static const QRegularExpression expression(QStringLiteral("^bytes=\\d+-(\\d+)$"));
    const QRegularExpressionMatch match = expression.match(range);
    if (!match.hasMatch()) {
        return 0;
    }

    bool ok = false;
    const qint64 lastByte = match.captured(1).toLongLong(&ok);
    return ok && lastByte >= 0 ? lastByte + 1 : 0;
}

GDriveUploadReplyResult waitForUploadReply(QNetworkReply *reply,
                                           int timeoutMs,
                                           const QString &timeoutMessage,
                                           const std::function<bool(qint64 sent, qint64 total)> &progress)
{
    GDriveUploadReplyResult result;
    QEventLoop loop;
    QTimer idleTimeout;
    QTimer cancelPollTimer;
    QElapsedTimer timer;
    qint64 lastSent = 0;
    qint64 lastTotal = 0;
    timer.start();
    idleTimeout.setSingleShot(true);
    cancelPollTimer.setInterval(250);

    QObject::connect(&idleTimeout, &QTimer::timeout, &loop, [&]() {
        result.timedOut = true;
        reply->abort();
    });
    QObject::connect(&cancelPollTimer, &QTimer::timeout, &loop, [&]() {
        if (progress && !progress(lastSent, lastTotal)) {
            result.canceled = true;
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::uploadProgress, &loop, [&](qint64 sent, qint64 total) {
        idleTimeout.start(timeoutMs);
        lastSent = sent;
        lastTotal = total;
        if (progress && !progress(sent, total)) {
            result.canceled = true;
            reply->abort();
        }
    });
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    idleTimeout.start(timeoutMs);
    cancelPollTimer.start();
    loop.exec();
    cancelPollTimer.stop();
    idleTimeout.stop();

    result.elapsedMs = timer.elapsed();
    result.body = GDriveApiClient::safeReadAll(reply);
    for (const QByteArray &header : reply->rawHeaderList()) {
        const QByteArray value = reply->rawHeader(header);
        result.headers.insert(header, value);
        result.headers.insert(header.toLower(), value);
    }
    result.networkError = reply->error();
    result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString networkErrorString = reply->errorString();
    delete reply;

    if (result.timedOut) {
        result.error = timeoutMessage;
    } else if (result.canceled || result.networkError == QNetworkReply::OperationCanceledError) {
        result.canceled = true;
        result.error = QStringLiteral("Google Drive upload canceled");
    } else if (result.networkError != QNetworkReply::NoError) {
        const QString fallback = result.status > 0
            ? QStringLiteral("Google Drive upload failed with HTTP %1").arg(result.status)
            : QStringLiteral("Google Drive upload failed: %1").arg(networkErrorString);
        result.error = GDriveApiClient::errorMessage(result.body, fallback);
    } else if (result.status >= 400) {
        result.error = GDriveApiClient::errorMessage(result.body, QStringLiteral("Google Drive upload failed with HTTP %1").arg(result.status));
    }

    return result;
}

bool queryResumableUploadOffset(QNetworkAccessManager &network,
                                const QUrl &sessionUrl,
                                qint64 fileSize,
                                const QString &accessToken,
                                qint64 *offset,
                                QString *error)
{
    QNetworkRequest request(sessionUrl);
    request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
    request.setHeader(QNetworkRequest::ContentLengthHeader, 0);
    request.setRawHeader("Content-Range", QByteArrayLiteral("bytes */") + QByteArray::number(fileSize));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    disableHttp2ForUpload(&request);

    GDriveRequestPolicy::waitForCooldown(QLatin1StringView("uploadStatus"));
    const GDriveUploadReplyResult reply = waitForUploadReply(
        network.put(request, QByteArray{}),
        TransferIdleTimeoutMs,
        QStringLiteral("Google Drive upload status query timed out"),
        nullptr);
    if (reply.canceled) {
        if (error) {
            *error = reply.error;
        }
        return false;
    }
    if (reply.status == 308) {
        if (offset) {
            *offset = std::clamp<qint64>(acknowledgedResumableOffset(reply.headers), 0, fileSize);
        }
        GDriveRequestPolicy::noteSuccess();
        return true;
    }
    if (reply.status == 200 || reply.status == 201) {
        if (offset) {
            *offset = fileSize;
        }
        GDriveRequestPolicy::noteSuccess();
        return true;
    }
    if (error) {
        *error = reply.error.trimmed().isEmpty()
            ? QStringLiteral("Google Drive upload status query failed with HTTP %1").arg(reply.status)
            : reply.error;
    }
    if (isRetryableDriveUploadStatus(reply.status) || GDriveRequestPolicy::isRetryableError(reply.error)) {
        GDriveRequestPolicy::noteThrottle(QLatin1StringView("uploadStatus"), reply.headers, 1, reply.error);
    }
    return false;
}

bool uploadFileBlockingWithRetryImpl(QNetworkAccessManager &network,
                                             const QString &sourceFilePath,
                                             const QString &parentId,
                                             const QString &name,
                                             const QString &accessToken,
                                             const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                                             QJsonObject *createdObject,
                                             QString *error,
                                             int *retryCount,
                                             const UploadLogContext &logContext = {})
{
    constexpr int maxAttempts = 3;
    QString lastError;
    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        QJsonObject attemptObject;
        QString attemptError;
        UploadLogContext attemptContext = logContext;
        attemptContext.attempt = attempt;
        if (uploadLocalFileToDriveBlocking(network,
                                           sourceFilePath,
                                           parentId,
                                           name,
                                           accessToken,
                                           progress,
                                           &attemptObject,
                                           &attemptError,
                                           attemptContext)) {
            if (createdObject) {
                *createdObject = attemptObject;
            }
            if (error) {
                error->clear();
            }
            return true;
        }

        lastError = attemptError;
        if (GDriveRequestPolicy::isRateLimitError(attemptError)) {
            if (retryCount) {
                ++(*retryCount);
            }
            if (uploadLoggingEnabledImpl()) {
                qInfo() << "GDrive upload retry suppressed"
                        << "context" << uploadContextText(attemptContext)
                        << "name" << name
                        << "message" << attemptError.left(180)
                        << "reason" << "rate-limit-abort";
            }
            break;
        }
        if (retryCount && attempt < maxAttempts && GDriveRequestPolicy::isRetryableError(attemptError)) {
            ++(*retryCount);
        }
        if (attempt >= maxAttempts || !GDriveRequestPolicy::isRetryableError(attemptError)) {
            break;
        }

        if (uploadLoggingEnabledImpl()) {
            qInfo() << "GDrive upload retry scheduled"
                    << "context" << uploadContextText(attemptContext)
                    << "name" << name
                    << "message" << attemptError.left(180);
        }
        const int delayMs = GDriveRequestPolicy::isRateLimitError(attemptError)
            ? GDriveRequestPolicy::retryDelayMs({}, attempt + 3)
            : GDriveRequestPolicy::retryDelayMs({}, attempt);
        QThread::msleep(static_cast<unsigned long>(delayMs));
    }

    if (error) {
        *error = lastError;
    }
    return false;
}

} // namespace









bool downloadFileToLocalFile(QNetworkAccessManager &network,
                             const QString &sourcePath,
                             const QString &mimeType,
                             const QString &destinationFilePath,
                             const QString &accessToken,
                             const std::function<bool(qint64, qint64)> &progress,
                             QString *error,
                             const QString &resourceKey)
{
    return downloadFileToLocalFileImpl(network, sourcePath, mimeType, destinationFilePath,
                                       accessToken, progress, error, resourceKey);
}

bool uploadFileBlockingWithRetry(QNetworkAccessManager &network,
                                 const QString &sourceFilePath,
                                 const QString &parentId,
                                 const QString &name,
                                 const QString &accessToken,
                                 const std::function<bool(qint64, qint64)> &progress,
                                 QJsonObject *createdObject,
                                 QString *error,
                                 int *retryCount,
                                 const UploadLogContext &logContext)
{
    return uploadFileBlockingWithRetryImpl(network, sourceFilePath, parentId, name, accessToken,
                                           progress, createdObject, error, retryCount, logContext);
}

bool uploadLoggingEnabled() { return uploadLoggingEnabledImpl(); }
bool downloadLoggingEnabled() { return downloadLoggingEnabledImpl(); }
bool downloadRangeLoggingEnabled() { return downloadRangeLoggingEnabledImpl(); }
int smallUploadConcurrency() { return smallUploadConcurrencyImpl(); }
int downloadConcurrency() { return downloadConcurrencyImpl(); }



bool downloadFiles(const QVector<BatchDownloadItem> &items,
                   const QString &accessToken,
                   qint64 authMs,
                   qint64 prepareMs,
                   const std::function<bool(const QString &, qint64, qint64)> &progress,
                   QString *error)
{
    qint64 totalBytes = 0;
    for (const BatchDownloadItem &item : items) {
        totalBytes += item.item.size;
    }

    const int concurrency = downloadConcurrencyImpl();
    const bool logging = downloadLoggingEnabledImpl();
    const QString batchId = logging
        ? QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
        : QString{};
    QElapsedTimer timer;
    timer.start();
    if (logging) {
        qInfo() << "GDrive parallel download scheduler started"
                << "batch" << batchId
                << "files" << items.size()
                << "bytes" << totalBytes
                << "concurrency" << concurrency
                << "authMs" << authMs
                << "prepareMs" << prepareMs;
    }

    QThreadPool pool;
    pool.setMaxThreadCount(concurrency);
    QVector<qint64> itemProgress(items.size(), 0);
    QMutex progressMutex;
    QMutex progressCallbackMutex;
    qint64 aggregateProgress = 0;
    std::atomic_bool canceled{false};

    struct Result {
        qsizetype index = -1;
        bool success = false;
        QString error;
    };
    QVector<QFuture<Result>> futures;
    futures.reserve(items.size());
    const bool rangeLogging = downloadRangeLoggingEnabledImpl();
    for (qsizetype offset = 0; offset < items.size(); offset += concurrency) {
        const qsizetype waveEnd = (std::min)(items.size(), offset + concurrency);
        if (rangeLogging) {
            qInfo() << "GDrive parallel download scheduler queue range"
                    << "batch" << batchId
                    << "range" << QStringLiteral("%1-%2").arg(offset + 1).arg(waveEnd)
                    << "files" << (waveEnd - offset);
        }
        for (qsizetype i = offset; i < waveEnd; ++i) {
            futures.push_back(QtConcurrent::run(&pool, [&, i]() -> Result {
                const BatchDownloadItem downloadItem = items.at(i);
                Result result;
                result.index = i;
                if (canceled.load()) {
                    result.error = QStringLiteral("Google Drive download canceled");
                    return result;
                }
                thread_local QNetworkAccessManager network;
                QString downloadError;
                result.success = downloadFileToLocalFileImpl(
                    network, downloadItem.downloadPath, downloadItem.mimeType,
                    downloadItem.partialPath, accessToken,
                    [&, i, downloadItem](qint64 processed, qint64 total) -> bool {
                        Q_UNUSED(total)
                        if (canceled.load()) {
                            return false;
                        }
                        qint64 aggregate = 0;
                        {
                            QMutexLocker locker(&progressMutex);
                            const qint64 itemSize = downloadItem.item.size > 0 ? downloadItem.item.size : processed;
                            const qint64 boundedProcessed = std::clamp<qint64>(processed, 0, (std::max<qint64>)(0, itemSize));
                            aggregateProgress += boundedProcessed - itemProgress[i];
                            itemProgress[i] = boundedProcessed;
                            aggregate = aggregateProgress;
                        }
                        QMutexLocker callbackLocker(&progressCallbackMutex);
                        return !progress || progress(downloadItem.progressName, aggregate, totalBytes);
                    },
                    &downloadError, downloadItem.resourceKey);
                result.error = downloadError;
                if (!result.success) {
                    canceled = true;
                }
                return result;
            }));
        }
    }

    for (QFuture<Result> &future : futures) {
        future.waitForFinished();
        const Result result = future.result();
        if (!result.success) {
            for (const BatchDownloadItem &item : items) {
                QFile::remove(item.partialPath);
            }
            if (error) {
                *error = result.error.trimmed().isEmpty()
                    ? QStringLiteral("Google Drive download scheduler failed")
                    : result.error.trimmed();
            }
            return false;
        }
        const BatchDownloadItem item = items.at(result.index);
        QMutexLocker locker(&progressMutex);
        const qint64 finalProgress = item.item.size > 0 ? item.item.size : itemProgress[result.index];
        aggregateProgress += finalProgress - itemProgress[result.index];
        itemProgress[result.index] = finalProgress;
    }

    if (canceled.load()) {
        for (const BatchDownloadItem &item : items) {
            QFile::remove(item.partialPath);
        }
        if (error) {
            *error = QStringLiteral("Google Drive download canceled");
        }
        return false;
    }
    for (const BatchDownloadItem &item : items) {
        QFile::remove(item.item.destinationFilePath);
        if (!QFile::rename(item.partialPath, item.item.destinationFilePath)) {
            for (const BatchDownloadItem &cleanupItem : items) {
                QFile::remove(cleanupItem.partialPath);
            }
            if (error) {
                *error = QStringLiteral("Could not move Google Drive download into place");
            }
            return false;
        }
    }
    if (progress && !progress(QString{}, totalBytes, totalBytes)) {
        if (error) {
            *error = QStringLiteral("Google Drive download canceled");
        }
        return false;
    }
    if (logging) {
        qInfo() << "GDrive parallel download scheduler finished"
                << "batch" << batchId << "files" << items.size()
                << "bytes" << totalBytes << "ms" << timer.elapsed();
    }
    if (error) {
        error->clear();
    }
    return true;
}

bool uploadFiles(const QVector<BatchUploadItem> &items,
                 const QString &accessToken,
                 qint64 targetMs,
                 qint64 prepareMs,
                 const std::function<bool(const QString &, qint64, qint64)> &progress,
                 const std::function<void(qsizetype, const QJsonObject &)> &completed,
                 QString *error)
{
    qint64 totalBytes = 0;
    for (const BatchUploadItem &item : items) {
        totalBytes += item.item.size;
    }
    const bool logging = uploadLoggingEnabledImpl();
    const int concurrency = smallUploadConcurrencyImpl();
    const QString batchId = logging
        ? QUuid::createUuid().toString(QUuid::WithoutBraces).left(8)
        : QString{};
    QElapsedTimer timer;
    timer.start();
    if (logging) {
        qInfo() << "GDrive parallel upload scheduler prepared"
                << "files" << items.size() << "bytes" << totalBytes
                << "targetMs" << targetMs << "prepareMs" << prepareMs;
        qInfo() << "GDrive parallel upload scheduler started"
                << "batch" << batchId << "files" << items.size()
                << "bytes" << totalBytes << "concurrency" << concurrency;
    }

    QThreadPool pool;
    pool.setMaxThreadCount(concurrency);
    QVector<qint64> itemProgress(items.size(), 0);
    QMutex progressMutex;
    QMutex progressCallbackMutex;
    qint64 aggregateProgress = 0;
    std::atomic_bool canceled{false};
    int totalRetries = 0;
    struct Result {
        qsizetype index = -1;
        bool success = false;
        QJsonObject createdObject;
        QString error;
        int retries = 0;
    };
    QVector<QFuture<Result>> futures;
    futures.reserve(items.size());
    for (qsizetype offset = 0; offset < items.size(); offset += concurrency) {
        const qsizetype waveEnd = (std::min)(items.size(), offset + concurrency);
        if (logging) {
            qInfo() << "GDrive parallel upload scheduler queue range"
                    << "batch" << batchId
                    << "range" << QStringLiteral("%1-%2").arg(offset + 1).arg(waveEnd)
                    << "files" << (waveEnd - offset);
        }
        for (qsizetype i = offset; i < waveEnd; ++i) {
            futures.push_back(QtConcurrent::run(&pool, [&, i]() -> Result {
                const BatchUploadItem uploadItem = items.at(i);
                Result result;
                result.index = i;
                if (canceled.load()) {
                    result.error = QStringLiteral("Google Drive upload canceled");
                    return result;
                }
                thread_local QNetworkAccessManager network;
                QString uploadError;
                const qsizetype rangeStart = (i / concurrency) * concurrency;
                const qsizetype rangeEnd = (std::min)(items.size(), rangeStart + concurrency);
                result.success = uploadFileBlockingWithRetryImpl(
                    network, uploadItem.item.sourceFilePath, uploadItem.parentId,
                    uploadItem.name, accessToken,
                    [&, i](qint64 processed, qint64 total) -> bool {
                        Q_UNUSED(total)
                        if (canceled.load()) {
                            return false;
                        }
                        qint64 aggregate = 0;
                        {
                            QMutexLocker locker(&progressMutex);
                            const qint64 boundedProcessed = std::clamp<qint64>(processed, 0, items.at(i).item.size);
                            aggregateProgress += boundedProcessed - itemProgress[i];
                            itemProgress[i] = boundedProcessed;
                            aggregate = aggregateProgress;
                        }
                        QMutexLocker callbackLocker(&progressCallbackMutex);
                        return !progress || progress(uploadItem.item.sourceFilePath, aggregate, totalBytes);
                    },
                    &result.createdObject, &uploadError, &result.retries,
                    UploadLogContext{batchId, i, items.size(), rangeStart, rangeEnd, 1});
                result.error = uploadError;
                if (!result.success) {
                    canceled = true;
                }
                return result;
            }));
        }
    }

    for (QFuture<Result> &future : futures) {
        future.waitForFinished();
        const Result result = future.result();
        totalRetries += result.retries;
        if (!result.success) {
            if (error) {
                *error = result.error.trimmed().isEmpty()
                    ? QStringLiteral("Google Drive upload scheduler failed")
                    : result.error.trimmed();
            }
            return false;
        }
        if (completed) {
            completed(result.index, result.createdObject);
        }
        QMutexLocker locker(&progressMutex);
        aggregateProgress += items.at(result.index).item.size - itemProgress[result.index];
        itemProgress[result.index] = items.at(result.index).item.size;
    }
    if (canceled.load()) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }
    if (progress && !progress(QString{}, totalBytes, totalBytes)) {
        if (error) {
            *error = QStringLiteral("Google Drive upload canceled");
        }
        return false;
    }
    if (logging) {
        qInfo() << "GDrive parallel upload scheduler finished"
                << "batch" << batchId << "files" << items.size()
                << "bytes" << totalBytes << "ms" << timer.elapsed()
                << "retries" << totalRetries;
    }
    if (error) {
        error->clear();
    }
    return true;
}

} // namespace GDriveTransferClient
