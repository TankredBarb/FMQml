#pragma once

#include "FileProvider.h"
#include <QString>
#include <memory>

class FileProviderFactory {
public:
    static std::unique_ptr<FileProvider> createProvider(const QString &path, QObject *parent = nullptr);
};
