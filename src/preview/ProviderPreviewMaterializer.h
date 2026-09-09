#pragma once

#include "PreviewData.h"

#include <functional>

namespace PreviewInternal {
LocalPreviewData loadProviderPreviewData(
    const QString &path,
    const std::function<void(const QString &)> &nameReady = {},
    const std::function<bool(qint64, qint64, bool)> &progressReady = {},
    bool allowCache = true);
} // namespace PreviewInternal
