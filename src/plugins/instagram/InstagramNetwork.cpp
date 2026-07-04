#include "InstagramInternal.h"

#include "InstagramAuth.h"

#include <QEventLoop>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

namespace InstagramProviderInternal {

void traceInstagramCookieMarkers(const QByteArray &cookie)
{
    if (!instagramTraceEnabled() || cookie.isEmpty()) {
        return;
    }

    traceInstagram(QStringLiteral("Cookie markers sessionid=%1 ds_user_id=%2 csrftoken=%3")
                       .arg(InstagramAuth::cookieValue(cookie, "sessionid").isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            InstagramAuth::cookieValue(cookie, "ds_user_id").isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            InstagramAuth::cookieValue(cookie, "csrftoken").isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
}

QString traceUrlLabel(const QUrl &url)
{
    const QString host = url.host().toLower();
    const QString path = url.path();
    if (host.endsWith(QStringLiteral("instagram.com"))) {
        if (path.startsWith(QStringLiteral("/api/v1/feed/user/"))) {
            return QStringLiteral("instagram://api/feed/user");
        }
        if (path.startsWith(QStringLiteral("/api/v1/feed/reels_media/"))) {
            return QStringLiteral("instagram://api/feed/reels_media");
        }
        if (path.startsWith(QStringLiteral("/api/v1/media/"))) {
            return QStringLiteral("instagram://api/media/info");
        }
        if (path.startsWith(QStringLiteral("/api/v1/users/web_profile_info/"))) {
            return QStringLiteral("instagram://api/profile/info");
        }
        if (path.startsWith(QStringLiteral("/graphql/query/"))) {
            return QStringLiteral("instagram://graphql/query");
        }
        if (path.startsWith(QStringLiteral("/api/graphql/"))) {
            return QStringLiteral("instagram://graphql/post");
        }
        if (path.startsWith(QStringLiteral("/web/search/topsearch/"))) {
            return QStringLiteral("instagram://web/search/topsearch");
        }
        return QStringLiteral("instagram://web/page");
    }
    return host.isEmpty() ? QStringLiteral("<invalid-url>") : host;
}

QString suffixForUrl(const QString &url, bool image)
{
    const QString path = QUrl(url).path();
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("jpg") || suffix == QLatin1String("jpeg")
        || suffix == QLatin1String("png") || suffix == QLatin1String("webp")
        || suffix == QLatin1String("gif") || suffix == QLatin1String("mp4")) {
        return suffix;
    }
    return image ? QStringLiteral("jpg") : QStringLiteral("mp4");
}

QString mimeForSuffix(const QString &suffix)
{
    if (suffix == QLatin1String("png")) {
        return QStringLiteral("image/png");
    }
    if (suffix == QLatin1String("webp")) {
        return QStringLiteral("image/webp");
    }
    if (suffix == QLatin1String("gif")) {
        return QStringLiteral("image/gif");
    }
    if (suffix == QLatin1String("mp4")) {
        return QStringLiteral("video/mp4");
    }
    return QStringLiteral("image/jpeg");
}

QString htmlDecode(QString value)
{
    value.replace(QStringLiteral("\\/"), QStringLiteral("/"));
    value.replace(QStringLiteral("\\u0026"), QStringLiteral("&"));
    value.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    value.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    value.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    return value;
}

QByteArray httpGetBytes(const QUrl &url,
                        const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progress,
                        QString *contentType,
                        QString *error,
                        bool includeCookie,
                        const QByteArray &userAgent)
{
    QNetworkAccessManager network;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QString::fromLatin1(userAgent.isEmpty() ? QByteArray(InstagramBrowserUserAgent) : userAgent));
    request.setRawHeader("X-IG-App-ID", "936619743392459");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setRawHeader("Accept", "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.8");
    const QByteArray cookie = includeCookie ? InstagramAuth::sessionCookieHeader() : QByteArray{};
    if (!cookie.isEmpty()) {
        traceInstagramCookieMarkers(cookie);
        request.setRawHeader("Cookie", cookie);
        request.setRawHeader("Referer", "https://www.instagram.com/");
        const QByteArray csrfToken = InstagramAuth::cookieValue(cookie, "csrftoken");
        if (!csrfToken.isEmpty()) {
            request.setRawHeader("X-CSRFToken", csrfToken);
        }
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QNetworkReply *reply = network.get(request);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    if (progress) {
        QObject::connect(reply, &QNetworkReply::downloadProgress,
                         [&progress, reply](qint64 processed, qint64 total) {
                             if (!progress(processed, total)) {
                                 reply->abort();
                             }
                         });
    }
    timeout.start(RequestTimeoutMs);
    loop.exec();

    if (timeout.isActive()) {
        timeout.stop();
    } else if (reply->isRunning()) {
        reply->abort();
        reply->deleteLater();
        if (error) {
            *error = QStringLiteral("Instagram request timed out");
        }
        return {};
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (contentType) {
        *contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    }
    traceInstagram(QStringLiteral("GET %1 status=%2 contentType=%3 cookie=%4")
                       .arg(traceUrlLabel(url))
                       .arg(status)
                       .arg(reply->header(QNetworkRequest::ContentTypeHeader).toString(),
                            includeCookie && !cookie.isEmpty() ? QStringLiteral("yes") : QStringLiteral("no")));
    if (reply->error() != QNetworkReply::NoError || status >= 400) {
        if (error) {
            *error = status > 0
                ? QStringLiteral("Instagram request failed with HTTP %1").arg(status)
                : reply->errorString();
        }
        reply->deleteLater();
        return {};
    }

    QByteArray data = reply->readAll();
    traceInstagram(QStringLiteral("GET body bytes=%1").arg(data.size()));
    reply->deleteLater();
    return data;
}

QByteArray httpGetBytes(const QUrl &url, QString *contentType, QString *error)
{
    return httpGetBytes(url, {}, contentType, error);
}

QByteArray httpGetBytesWithoutCookie(const QUrl &url, QString *contentType, QString *error)
{
    return httpGetBytes(url, {}, contentType, error, false);
}

QByteArray httpPostFormBytes(const QUrl &url,
                             const QByteArray &body,
                             const QString &referer,
                             const QByteArray &lsd,
                             QString *contentType,
                             QString *error)
{
    QNetworkAccessManager network;
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(InstagramBrowserUserAgent));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/x-www-form-urlencoded"));
    request.setRawHeader("X-IG-App-ID", "936619743392459");
    request.setRawHeader("X-IG-D", "www");
    request.setRawHeader("X-ASBD-ID", "129477");
    request.setRawHeader("X-Requested-With", "XMLHttpRequest");
    request.setRawHeader("X-FB-Friendly-Name", "PolarisProfilePostsTabContentQuery_connection");
    request.setRawHeader("Origin", "https://www.instagram.com");
    request.setRawHeader("Accept", "application/json, text/plain, */*");
    request.setRawHeader("Accept-Language", "en-US,en;q=0.8");
    if (!lsd.isEmpty()) {
        request.setRawHeader("X-FB-LSD", lsd);
    }
    if (!referer.isEmpty()) {
        request.setRawHeader("Referer", referer.toUtf8());
    }
    const QByteArray cookie = InstagramAuth::sessionCookieHeader();
    if (!cookie.isEmpty()) {
        traceInstagramCookieMarkers(cookie);
        request.setRawHeader("Cookie", cookie);
        const QByteArray csrfToken = InstagramAuth::cookieValue(cookie, "csrftoken");
        if (!csrfToken.isEmpty()) {
            request.setRawHeader("X-CSRFToken", csrfToken);
        }
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QNetworkReply *reply = network.post(request, body);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    timeout.start(RequestTimeoutMs);
    loop.exec();

    if (timeout.isActive()) {
        timeout.stop();
    } else if (reply->isRunning()) {
        reply->abort();
        reply->deleteLater();
        if (error) {
            *error = QStringLiteral("Instagram request timed out");
        }
        return {};
    }

    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (contentType) {
        *contentType = reply->header(QNetworkRequest::ContentTypeHeader).toString();
    }
    traceInstagram(QStringLiteral("POST %1 status=%2 contentType=%3 cookie=%4 bodyBytes=%5")
                       .arg(traceUrlLabel(url))
                       .arg(status)
                       .arg(reply->header(QNetworkRequest::ContentTypeHeader).toString(),
                            !cookie.isEmpty() ? QStringLiteral("yes") : QStringLiteral("no"))
                       .arg(body.size()));
    if (reply->error() != QNetworkReply::NoError || status >= 400) {
        if (error) {
            *error = status > 0
                ? QStringLiteral("Instagram request failed with HTTP %1").arg(status)
                : reply->errorString();
        }
        reply->deleteLater();
        return {};
    }

    QByteArray data = reply->readAll();
    traceInstagram(QStringLiteral("POST body bytes=%1").arg(data.size()));
    reply->deleteLater();
    return data;
}

} // namespace InstagramProviderInternal
