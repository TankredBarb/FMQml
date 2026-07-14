#include "FileProviderFactory.h"

#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "FileProviderPluginRegistry.h"
#include "LocalFileProvider.h"
#include "PathSemantics.h"

std::unique_ptr<FileProvider> FileProviderFactory::createProvider(const QString &path)
{
    const QString preprocessed = preprocessPath(path);
    if (ArchiveSupport::isArchivePath(preprocessed)) {
        return std::make_unique<ArchiveFileProvider>();
    }

    auto &pluginRegistry = FileProviderPluginRegistry::instance();
    if (pluginRegistry.hasProviderForPath(preprocessed)) {
        return pluginRegistry.createProvider(preprocessed);
    }
    if (PathSemantics::hasExplicitNonLocalScheme(preprocessed)) {
        return nullptr;
    }

    return std::make_unique<LocalFileProvider>();
}

bool FileProviderFactory::hasPluginProviderForPath(const QString &path)
{
    return FileProviderPluginRegistry::instance().hasProviderForPath(preprocessPath(path));
}

QString FileProviderFactory::normalizePath(const QString &path)
{
    const QString preprocessed = preprocessPath(path);
    if (ArchiveSupport::isArchivePath(preprocessed)) {
        return ArchiveSupport::normalizeArchivePath(preprocessed);
    }

    const std::unique_ptr<FileProvider> provider = createProvider(preprocessed);
    if (provider) {
        return provider->normalizedPath(preprocessed);
    }

    return preprocessed.trimmed();
}

QString FileProviderFactory::preprocessPath(const QString &path)
{
    return FileProviderPluginRegistry::instance().preprocessPath(path);
}
