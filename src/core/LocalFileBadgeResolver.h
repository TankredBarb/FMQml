#pragma once

#include <QString>

class QFileInfo;

struct LocalFileBadgeState {
    bool isSymLink = false;
    bool isBrokenSymLink = false;
    bool isMountPoint = false;
    bool isLocked = false;
    QString primaryBadgeKind;
};

class LocalFileBadgeResolver final {
public:
    static LocalFileBadgeState resolve(const QFileInfo &fileInfo, bool isSymLink,
                                       bool isMountPoint = false);
    static QString primaryBadgeKind(bool isBrokenSymLink, bool isSymLink,
                                    bool isMountPoint, bool isLocked, bool isArchive);
};
