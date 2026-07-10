#pragma once

#include <QStringList>

class LocalMountPointIndex final {
public:
    static void setMountRoots(const QStringList &roots);
    static bool isMountPoint(const QString &path);
};
