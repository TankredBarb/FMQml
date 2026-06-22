#pragma once

#include <memory>
#include <QString>

#include "FileProvider.h"

class FileProviderFactory final {
public:
    static std::unique_ptr<FileProvider> createProvider(const QString &path);
    static bool hasPluginProviderForPath(const QString &path);
    static QString normalizePath(const QString &path);
    static QString preprocessPath(const QString &path);
};
