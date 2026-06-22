#include "FileProviderFactory.h"

#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "FileProviderPluginRegistry.h"
#include "LocalFileProvider.h"

#include <QRegularExpression>

namespace {

bool hasExplicitNonLocalScheme(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return false;
    }

    static const QRegularExpression schemePattern(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*$"));
    const QString scheme = trimmed.left(separatorIndex).toLower();
    return schemePattern.match(scheme).hasMatch() && scheme != QStringLiteral("file");
}

} // namespace

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
    if (hasExplicitNonLocalScheme(preprocessed)) {
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
