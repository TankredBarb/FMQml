#pragma once

#include "../controllers/WallpaperPreviewController.h"

#include <QQuickAsyncImageProvider>
#include <QThreadPool>

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

class WallpaperPreviewImageProvider : public QQuickAsyncImageProvider
{
public:
    explicit WallpaperPreviewImageProvider(std::shared_ptr<WallpaperPreviewController::PreviewImageStore> store);
    ~WallpaperPreviewImageProvider() override;

    QQuickImageResponse *requestImageResponse(const QString &id, const QSize &requestedSize) override;

private:
    std::shared_ptr<WallpaperPreviewController::PreviewImageStore> m_store;
    QThreadPool m_pool;
    std::atomic_int m_priority{0};
    std::mutex m_mutex;
    std::vector<std::weak_ptr<std::atomic_bool>> m_cancellations;
};
