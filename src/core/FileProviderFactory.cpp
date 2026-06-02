#include "FileProviderFactory.h"

#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "LocalFileProvider.h"

std::unique_ptr<FileProvider> FileProviderFactory::createProvider(const QString &path)
{
    if (ArchiveSupport::isArchivePath(path)) {
        return std::make_unique<ArchiveFileProvider>();
    }

    return std::make_unique<LocalFileProvider>();
}
