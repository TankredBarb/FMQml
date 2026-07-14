#pragma once

#include <QByteArray>
#include <QNetworkReply>
#include <QString>
#include <QUrl>

namespace GDriveThumbnailLoader {

struct DownloadResult
{
    int httpStatus = 0;
    QByteArray body;
    bool timedOut = false;
    bool oversize = false;
    QNetworkReply::NetworkError networkError = QNetworkReply::NoError;
};

DownloadResult downloadBytes(const QUrl &url, const QString &accessToken);
QByteArray cachedBytes(const QString &cacheIdentity);
void cacheBytes(const QString &cacheIdentity, const QByteArray &bytes);

} // namespace GDriveThumbnailLoader
