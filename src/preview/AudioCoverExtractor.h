#pragma once

#include <QString>

namespace PreviewInternal {
QString materializeAudioCoverSource(const QString &audioPath, const QString &cleanupDir, const QString &suffix);
} // namespace PreviewInternal
