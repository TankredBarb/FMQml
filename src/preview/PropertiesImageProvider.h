#pragma once

#include "../core/ThumbnailProvider.h"
#include <QThreadPool>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

// Keeps Properties decoding out of Qt's shared synchronous image-provider lane.
class PropertiesImageProvider : public QQuickAsyncImageProvider
{
public:
    using Loader = std::function<QImage(const VisualThumbnailRequest &)>;
    explicit PropertiesImageProvider(Loader loader);
    ~PropertiesImageProvider() override;
    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    Loader m_loader;
    QThreadPool m_pool;
    std::atomic_int m_priority{0};
    std::mutex m_mutex;
    std::vector<std::weak_ptr<std::atomic_bool>> m_cancellations;
};
