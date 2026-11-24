#include "WallpaperPreviewImageProvider.h"

#include <QMutexLocker>
#include <QRunnable>
#include <algorithm>

namespace {

class PreviewResponse final : public QQuickImageResponse
{
public:
    std::shared_ptr<std::atomic_bool> cancelled = std::make_shared<std::atomic_bool>(false);
    QImage image;
    QString error;

    void cancel() override { *cancelled = true; }
    QString errorString() const override { return error; }
    QQuickTextureFactory *textureFactory() const override
    {
        return QQuickTextureFactory::textureFactoryForImage(image);
    }
};

} // namespace

WallpaperPreviewImageProvider::WallpaperPreviewImageProvider(
    std::shared_ptr<WallpaperPreviewController::PreviewImageStore> store)
    : m_store(std::move(store))
{
    m_pool.setMaxThreadCount(2);
}

WallpaperPreviewImageProvider::~WallpaperPreviewImageProvider()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto &weak : m_cancellations) {
            if (auto flag = weak.lock()) {
                *flag = true;
            }
        }
    }
    m_pool.waitForDone();
}

QQuickImageResponse *WallpaperPreviewImageProvider::requestImageResponse(
    const QString &id, const QSize &requestedSize)
{
    auto *response = new PreviewResponse;
    const auto cancelled = response->cancelled;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_cancellations.erase(
            std::remove_if(m_cancellations.begin(), m_cancellations.end(),
                           [](const auto &weak) { return weak.expired(); }),
            m_cancellations.end());
        m_cancellations.push_back(cancelled);
    }

    const auto store = m_store;
    m_pool.start(QRunnable::create([response, cancelled, store, id, requestedSize]() {
        const auto isCancelled = [cancelled]() { return cancelled->load(); };

        QImage image;
        if (!isCancelled() && store) {
            const qsizetype slash = id.indexOf(QLatin1Char('/'));
            if (slash > 0) {
                bool ok = false;
                const quint64 generation = id.left(slash).toULongLong(&ok);
                const QString variant = id.mid(slash + 1);
                if (ok) {
                    QMutexLocker locker(&store->mutex);
                    if (store->generation == generation) {
                        image = (variant == QLatin1String("blur"))
                            ? store->blurImage
                            : store->previewImage;
                    }
                }
            }
        }

        if (!isCancelled() && !image.isNull()
            && requestedSize.isValid()
            && (requestedSize.width() > 0 || requestedSize.height() > 0)) {
            QSize targetSize = requestedSize;
            if (targetSize.width() <= 0) {
                targetSize.setWidth(qMax(1, qRound(static_cast<qreal>(image.width())
                                                   * targetSize.height() / qMax(1, image.height()))));
            } else if (targetSize.height() <= 0) {
                targetSize.setHeight(qMax(1, qRound(static_cast<qreal>(image.height())
                                                    * targetSize.width() / qMax(1, image.width()))));
            }
            targetSize.setWidth(qBound(1, targetSize.width(), 2048));
            targetSize.setHeight(qBound(1, targetSize.height(), 2048));
            if (targetSize != image.size()) {
                image = image.scaled(targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }

        if (isCancelled()) {
            image = {};
        } else if (image.isNull()) {
            response->error = QStringLiteral("Wallpaper preview image is not available.");
        } else {
            image.setDevicePixelRatio(1.0);
        }

        response->image = std::move(image);
        emit response->finished();
    }), ++m_priority);

    return response;
}
