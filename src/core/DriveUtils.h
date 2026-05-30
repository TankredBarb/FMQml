#pragma once

#include <QString>
#include <QStorageInfo>

namespace DriveUtils {

/// Returns one of: "hdd", "ssd", "usb", "optical", "network", "unknown"
QString detectDriveType(const QStorageInfo &info);

/// Human-readable size string: "120 GB", "1.5 TB", "450 MB", "900 KB"
QString formatSize(qint64 bytes);

/// Short display label for filesystem roots: "D:" on Windows, "/" on Linux.
QString rootDisplayName(const QString &rootPath);

} // namespace DriveUtils
