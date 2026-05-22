#pragma once

#include <memory>
#include <QString>

#include "FileProvider.h"

class FileProviderFactory final {
public:
    static std::unique_ptr<FileProvider> createProvider(const QString &path);
};
