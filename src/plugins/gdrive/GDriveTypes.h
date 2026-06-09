#pragma once

#include <QDateTime>
#include <QString>

struct GDriveItemCapabilities {
    bool canDownload = false;
    bool canEdit = false;
    bool canAddChildren = false;
    bool canListChildren = false;
    bool canRename = false;
    bool canTrash = false;
    bool canDelete = false;
    bool canCopy = false;
};

struct GDriveStorageQuota {
    qint64 total = -1;
    qint64 used = -1;
    qint64 free = -1;
    bool valid = false;
    QDateTime cachedAt;
};
