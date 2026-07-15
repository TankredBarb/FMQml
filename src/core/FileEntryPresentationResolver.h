#pragma once

#include "FileProvider.h"

class FileEntryPresentationResolver final {
public:
    static QString breadcrumbIconNameForPath(const QString &path);
    static QString previewIconNameForPath(const QString &path);
    static QString previewIconSource(const QString &path,
                                     bool directory,
                                     const QString &suffix,
                                     const QString &mimeName,
                                     bool useNativeIcons);
    static QString menuIconName(const FileEntry &entry);
    static bool menuUsesAvatar(const FileEntry &entry);
    static bool isRemotePreviewContentPath(const QString &path);
    static bool canRequestThumbnail(const QString &path);
};
