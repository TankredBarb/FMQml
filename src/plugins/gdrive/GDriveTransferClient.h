#pragma once

#include <functional>

#include <QJsonObject>
#include <QHash>
#include <QString>
#include <QVector>

#include "FileProvider.h"

class QNetworkAccessManager;

namespace GDriveTransferClient {

struct UploadLogContext
{
    QString batchId;
    qsizetype batchIndex = -1;
    qsizetype batchCount = 0;
    qsizetype waveStart = -1;
    qsizetype waveEnd = -1;
    int attempt = 1;
};

struct BatchDownloadItem
{
    LocalFileMaterializeItem item;
    QString progressName;
    QString downloadPath;
    QString mimeType;
    QString resourceKey;
    QString partialPath;
};

struct BatchUploadItem
{
    LocalFileCopyItem item;
    QString parentPath;
    QString parentId;
    QString name;
};

bool downloadFileToLocalFile(QNetworkAccessManager &network,
                             const QString &sourcePath,
                             const QString &mimeType,
                             const QString &destinationFilePath,
                             const QString &accessToken,
                             const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                             QString *error,
                             const QString &resourceKey = {});
bool uploadFileBlockingWithRetry(QNetworkAccessManager &network,
                                 const QString &sourceFilePath,
                                 const QString &parentId,
                                 const QString &name,
                                 const QString &accessToken,
                                 const std::function<bool(qint64 sent, qint64 total)> &progress,
                                 QJsonObject *createdObject,
                                 QString *error,
                                 int *retryCount,
                                 const UploadLogContext &logContext = {});
bool uploadLoggingEnabled();
bool downloadLoggingEnabled();
bool downloadRangeLoggingEnabled();
int smallUploadConcurrency();
int downloadConcurrency();
bool downloadFiles(const QVector<BatchDownloadItem> &items,
                   const QString &accessToken,
                   qint64 authMs,
                   qint64 prepareMs,
                   const std::function<bool(const QString &, qint64, qint64)> &progress,
                   QString *error);
bool uploadFiles(const QVector<BatchUploadItem> &items,
                 const QString &accessToken,
                 qint64 targetMs,
                 qint64 prepareMs,
                 const std::function<bool(const QString &, qint64, qint64)> &progress,
                 const std::function<void(qsizetype, const QJsonObject &)> &completed,
                 QString *error);

} // namespace GDriveTransferClient
