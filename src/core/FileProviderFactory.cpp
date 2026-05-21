#include "FileProviderFactory.h"
#include "LocalFileProvider.h"
#include "ArchiveFileProvider.h"

std::unique_ptr<FileProvider> FileProviderFactory::createProvider(const QString &path, QObject *parent) {
    if (path.startsWith(QLatin1String("archive://"))) {
        return std::make_unique<ArchiveFileProvider>(parent);
    }
    
    return std::make_unique<LocalFileProvider>(parent);
}
