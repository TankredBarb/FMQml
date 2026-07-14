#pragma once

#include <functional>

#include <QByteArray>
#include <QJsonObject>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

#include "GDriveAuth.h"
#include "GDriveTypes.h"

class QIODevice;
class QNetworkAccessManager;
class QNetworkReply;
class QNetworkRequest;

namespace GDriveApiClient {

QByteArray safeReadAll(QIODevice *device);
QString errorMessage(const QByteArray &body, const QString &fallback);
QNetworkRequest authorizedJsonRequest(const QUrl &url, const QString &accessToken);
bool waitForReply(QNetworkReply *reply,
                  int timeoutMs,
                  QString timeoutMessage,
                  QByteArray *body,
                  QString *error,
                  QHash<QByteArray, QByteArray> *rawHeaders = nullptr);
bool parseFileResponse(const QByteArray &body, QJsonObject *fileObject, QString *error);
bool fetchFileMetadataBlocking(QNetworkAccessManager &network,
                               const QString &fileId,
                               const QString &accessToken,
                               const QString &resourceKey,
                               QJsonObject *fileObject,
                               QString *error);
GDriveStorageQuota quotaFromAboutObject(const QJsonObject &object);
void rememberAccountInfoFromAboutObject(const QJsonObject &object);
bool refreshStorageQuotaBlocking(QNetworkAccessManager &network, const QString &accessToken, QString *error);
QVariantMap storageInfoForQuota(const GDriveStorageQuota &quota);
bool createMetadataBlocking(QNetworkAccessManager &network,
                            const QString &parentId,
                            const QString &name,
                            const QString &mimeType,
                            const QString &accessToken,
                            QJsonObject *createdObject,
                            QString *error);
bool trashFileBlocking(QNetworkAccessManager &network,
                       const QString &fileId,
                       const QString &accessToken,
                       QJsonObject *trashedObject,
                       QString *error);
bool restoreFileBlocking(QNetworkAccessManager &network,
                         const QString &fileId,
                         const QString &accessToken,
                         QJsonObject *restoredObject,
                         QString *error);
QStringList listChildrenBlocking(QNetworkAccessManager &network, const QString &path, QString *error);

} // namespace GDriveApiClient
