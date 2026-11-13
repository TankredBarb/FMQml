#include "PropertiesImageProvider.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QMimeType>
#include <QRunnable>
#include <QUrl>
#include <algorithm>

namespace {
class Response : public QQuickImageResponse
{
public:
    std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
    QImage image;
    void cancel() override { *cancelled = true; }
    QQuickTextureFactory *textureFactory() const override { return QQuickTextureFactory::textureFactoryForImage(image); }
};

bool isMedia(const QMimeType &mime)
{
    const QString name = mime.name();
    return name.startsWith(QStringLiteral("image/")) || name.startsWith(QStringLiteral("audio/"))
           || name.startsWith(QStringLiteral("video/"));
}
}

PropertiesImageProvider::PropertiesImageProvider(Loader loader)
    : m_loader(std::move(loader))
{
    // A raster decoder cannot be interrupted mid-read; reserve capacity for its replacement.
    m_pool.setMaxThreadCount(2);
}

PropertiesImageProvider::~PropertiesImageProvider()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto &weak : m_cancellations) {
            if (auto flag = weak.lock()) *flag = true;
        }
    }
    m_pool.waitForDone();
}

QQuickImageResponse *PropertiesImageProvider::requestImageResponse(const QString &id, const QSize &requestedSize)
{
    auto *response = new Response;
    const auto cancelled = response->cancelled;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cancellations.erase(std::remove_if(m_cancellations.begin(), m_cancellations.end(),
                                            [](const auto &weak) { return weak.expired(); }), m_cancellations.end());
        m_cancellations.push_back(cancelled);
    }
    const auto loader = m_loader;
    m_pool.start(QRunnable::create([response, cancelled, loader, id, requestedSize] {
        const auto isCancelled = [cancelled] { return cancelled->load(); };
        const QString path = QUrl::fromPercentEncoding(id.toUtf8());
        QImage image;
        const QFileInfo info(path);
        // Never sniff devices/FIFOs. Inaccessible paths may still use the approved admin loader.
        if (!isCancelled() && !path.isEmpty() && !path.contains(QStringLiteral("://"))
            && (!info.exists() || info.isFile())) {
            QMimeDatabase database;
            QMimeType mime = database.mimeTypeForFile(path, QMimeDatabase::MatchContent);
            if (!isMedia(mime)) mime = database.mimeTypeForFile(path, QMimeDatabase::MatchExtension);
            if (!isCancelled() && isMedia(mime)) {
                QString suffix = mime.preferredSuffix();
                if (mime.inherits(QStringLiteral("audio/mpeg"))) suffix = QStringLiteral("mp3");
                else if (mime.inherits(QStringLiteral("audio/flac"))) suffix = QStringLiteral("flac");
                else if (mime.inherits(QStringLiteral("audio/mp4"))) suffix = QStringLiteral("m4a");
                else if (mime.inherits(QStringLiteral("audio/ogg"))) suffix = QStringLiteral("ogg");
                if (suffix.isEmpty()) suffix = QFileInfo(path).suffix().toLower();
                image = loader({path, requestedSize.isValid() ? requestedSize : QSize(128, 128),
                                mime.name(), suffix, isCancelled});
            }
        }
        // Keep the established 1x1 missing-thumbnail contract, without Qt warning spam.
        // A genuine one-pixel image is enlarged so it remains distinguishable from a miss.
        if (!isCancelled() && !image.isNull()) {
            if (image.size() == QSize(1, 1)) image = image.scaled(2, 2);
        } else {
            image = {};
        }
        if (image.isNull()) {
            image = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::transparent);
        }
        image.setDevicePixelRatio(1.0);
        response->image = std::move(image);
        emit response->finished();
    }), ++m_priority);
    return response;
}
