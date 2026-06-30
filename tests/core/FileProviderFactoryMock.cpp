#include "FileProviderFactory.h"

std::unique_ptr<FileProvider> FileProviderFactory::createProvider(const QString &path)
{
    Q_UNUSED(path)
    return nullptr;
}
