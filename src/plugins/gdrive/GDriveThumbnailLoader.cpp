#include "GDriveThumbnailLoader.h"

#include <atomic>

#include <QCache>
#include <QEventLoop>
#include <QMutex>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QVector>

namespace GDriveThumbnailLoader {
namespace {

constexpr qint64 kProviderThumbnailMaxBytes = 2 * 1024 * 1024;
constexpr int kProviderThumbnailTimeoutMs = 5000;
constexpr int kGDriveThumbnailByteCacheLimitKb = 32 * 1024;
constexpr int kGDriveThumbnailWorkerCount = 4;

class NetworkWorker final : public QObject
{
public:
    DownloadResult download(const QUrl &url, const QString &accessToken)
    {
        DownloadResult result;
        if (!url.isValid()) {
            return result;
        }

        QNetworkRequest request(url);
        request.setRawHeader("Authorization", QByteArrayLiteral("Bearer ") + accessToken.toUtf8());
        request.setHeader(QNetworkRequest::UserAgentHeader,
                          QStringLiteral("Mozilla/5.0 FMQml/1.0 GoogleDriveNativeThumbnail"));
        request.setRawHeader("Accept", "image/jpeg,image/png,image/*;q=0.8,*/*;q=0.5");
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

        // Keep the network manager local to the worker-thread method. A member
        // QNetworkAccessManager may be constructed in the caller thread before
        // the worker is moved, which makes QNetworkReply creation warn/fail
        // when thumbnails are requested from QQuickPixmapReader.
        QNetworkAccessManager network;
        QNetworkReply *reply = network.get(request);
        if (!reply) {
            return result;
        }

        bool oversize = false;
        QObject::connect(reply, &QNetworkReply::downloadProgress,
                         [reply, &oversize](qint64 processed, qint64 total) {
                             if (processed > kProviderThumbnailMaxBytes
                                 || (total > 0 && total > kProviderThumbnailMaxBytes)) {
                                 oversize = true;
                                 reply->abort();
                             }
                         });

        QEventLoop loop;
        QTimer timeout;
        bool timedOut = false;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
            timedOut = true;
            reply->abort();
        });
        QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        timeout.start(kProviderThumbnailTimeoutMs);
        loop.exec();
        timeout.stop();

        result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        result.networkError = reply->error();
        result.body = reply->readAll();
        result.timedOut = timedOut;
        result.oversize = oversize;
        delete reply;
        return result;
    }
};

const QVector<NetworkWorker *> &networkWorkers()
{
    static QVector<NetworkWorker *> workers = [] {
        QVector<NetworkWorker *> result;
        result.reserve(kGDriveThumbnailWorkerCount);
        for (int i = 0; i < kGDriveThumbnailWorkerCount; ++i) {
            auto *workerThread = new QThread;
            workerThread->setObjectName(QStringLiteral("GDriveThumbnailNetwork%1").arg(i + 1));
            workerThread->start();

            auto *networkWorker = new NetworkWorker;
            networkWorker->moveToThread(workerThread);
            result.append(networkWorker);
        }
        return result;
    }();
    return workers;
}

NetworkWorker *networkWorker()
{
    static std::atomic<int> nextWorker{0};
    const QVector<NetworkWorker *> &workers = networkWorkers();
    if (workers.isEmpty()) {
        return nullptr;
    }
    const int index = nextWorker.fetch_add(1, std::memory_order_relaxed);
    const int slot = int(static_cast<unsigned int>(index) % static_cast<unsigned int>(workers.size()));
    return workers.at(slot);
}

QCache<QString, QByteArray> &byteCache()
{
    static QCache<QString, QByteArray> cache(kGDriveThumbnailByteCacheLimitKb);
    return cache;
}

QMutex &byteCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

} // namespace

DownloadResult downloadBytes(const QUrl &url, const QString &accessToken)
{
    DownloadResult result;
    if (!url.isValid() || accessToken.isEmpty()) {
        return result;
    }

    NetworkWorker *worker = networkWorker();
    if (!worker) {
        return result;
    }
    if (QThread::currentThread() == worker->thread()) {
        result = worker->download(url, accessToken);
    } else {
        QMetaObject::invokeMethod(worker,
                                  [&result, worker, url, accessToken]() {
                                      result = worker->download(url, accessToken);
                                  },
                                  Qt::BlockingQueuedConnection);
    }
    return result;
}

QByteArray cachedBytes(const QString &cacheIdentity)
{
    if (cacheIdentity.isEmpty()) {
        return {};
    }
    QMutexLocker locker(&byteCacheMutex());
    if (const QByteArray *bytes = byteCache().object(cacheIdentity)) {
        return *bytes;
    }
    return {};
}

void cacheBytes(const QString &cacheIdentity, const QByteArray &bytes)
{
    if (cacheIdentity.isEmpty() || bytes.isEmpty()) {
        return;
    }
    const int costKb = qMax(1, int((bytes.size() + 1023) / 1024));
    QMutexLocker locker(&byteCacheMutex());
    byteCache().insert(cacheIdentity, new QByteArray(bytes), costKb);
}

} // namespace GDriveThumbnailLoader
