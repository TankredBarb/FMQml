#include "FileProviderPluginRegistry.h"

#include <algorithm>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFileInfoList>
#include <QLibrary>
#include <QMutexLocker>
#include <QObject>
#include <QRegularExpression>
#include <QSet>
#include <QDebug>
#include <QSettings>

namespace {

QString canonicalPluginPath(const QString &path)
{
    const QFileInfo info(path);
    const QString canonicalPath = info.canonicalFilePath();
    if (!canonicalPath.isEmpty()) {
        return QDir::cleanPath(canonicalPath);
    }
    return QDir::cleanPath(info.absoluteFilePath());
}

QString schemeForPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return {};
    }

    static const QRegularExpression schemePattern(QStringLiteral("^[A-Za-z][A-Za-z0-9+.-]*$"));
    const QString scheme = trimmed.left(separatorIndex);
    if (!schemePattern.match(scheme).hasMatch()) {
        return {};
    }
    return scheme.toLower();
}

QStringList normalizedSchemes(const QStringList &schemes)
{
    QStringList result;
    QSet<QString> seen;

    for (const QString &schemeValue : schemes) {
        const QString scheme = schemeValue.trimmed().toLower();
        if (scheme.isEmpty() || seen.contains(scheme)) {
            continue;
        }
        result.append(scheme);
        seen.insert(scheme);
    }

    return result;
}

bool isReservedScheme(const QString &scheme)
{
    return scheme == QStringLiteral("file")
        || scheme == QStringLiteral("archive")
        || scheme == QStringLiteral("devices")
        || scheme == QStringLiteral("favorites");
}

void appendLoadError(QStringList &errors, const QString &path, const QString &message)
{
    const QString error = QStringLiteral("%1: %2").arg(path, message);
    errors.append(error);
    qWarning().noquote() << "[ProviderPlugin]" << error;
}

QString qualifiedActionId(const QString &pluginId, const QString &actionId)
{
    return pluginId + QStringLiteral("::") + actionId;
}

QString actionPluginIdFromQualifiedId(const QString &qualifiedId)
{
    const int separator = qualifiedId.indexOf(QStringLiteral("::"));
    return separator > 0 ? qualifiedId.left(separator) : QString{};
}

QString actionIdFromQualifiedId(const QString &qualifiedId)
{
    const int separator = qualifiedId.indexOf(QStringLiteral("::"));
    return separator > 0 ? qualifiedId.mid(separator + 2) : QString{};
}

} // namespace

FileProviderPluginRegistry &FileProviderPluginRegistry::instance()
{
    static FileProviderPluginRegistry registry;
    return registry;
}

void FileProviderPluginRegistry::loadDefaultPluginDirectories()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    loadPluginDirectory(appDir.filePath(QStringLiteral("plugins/providers")));

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    const QString customDir = settings.value(QStringLiteral("customDirectory")).toString();
    settings.endGroup();
    if (!customDir.isEmpty()) {
        loadPluginDirectory(customDir);
    }

    const QString extraPaths = QString::fromLocal8Bit(qgetenv("FM_PROVIDER_PLUGIN_PATH"));
    if (extraPaths.isEmpty()) {
        return;
    }

    const QStringList paths = extraPaths.split(QDir::listSeparator(), Qt::SkipEmptyParts);
    for (const QString &path : paths) {
        loadPluginDirectory(path.trimmed());
    }
}

void FileProviderPluginRegistry::loadPluginDirectory(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return;
    }

    const QDir directory(path);
    if (!directory.exists()) {
        return;
    }

    const QFileInfoList files = directory.entryInfoList(QDir::Files | QDir::Readable, QDir::Name);
    for (const QFileInfo &fileInfo : files) {
        if (!QLibrary::isLibrary(fileInfo.fileName())) {
            continue;
        }
        loadPluginFile(fileInfo.absoluteFilePath());
    }
}

bool FileProviderPluginRegistry::hasProviderForPath(const QString &path) const
{
    const QString scheme = schemeForPath(path);
    if (scheme.isEmpty()) {
        return false;
    }

    std::vector<FileProviderPlugin *> candidates;
    {
        QMutexLocker locker(&m_mutex);
        for (const Entry &entry : m_entries) {
            if (entry.schemes.contains(scheme) && entry.providerPlugin) {
                candidates.push_back(entry.providerPlugin);
            }
        }
    }

    for (FileProviderPlugin *plugin : candidates) {
        if (plugin->canHandle(path)) {
            return true;
        }
    }
    return false;
}

std::unique_ptr<FileProvider> FileProviderPluginRegistry::createProvider(const QString &path) const
{
    const QString scheme = schemeForPath(path);
    if (scheme.isEmpty()) {
        return nullptr;
    }

    std::vector<FileProviderPlugin *> candidates;
    {
        QMutexLocker locker(&m_mutex);
        for (const Entry &entry : m_entries) {
            if (entry.schemes.contains(scheme) && entry.providerPlugin) {
                candidates.push_back(entry.providerPlugin);
            }
        }
    }

    for (FileProviderPlugin *plugin : candidates) {
        if (plugin->canHandle(path)) {
            return plugin->createProvider();
        }
    }
    return nullptr;
}

QString FileProviderPluginRegistry::thumbnailUrlForPath(const QString &path) const
{
    const QString scheme = schemeForPath(path);
    if (scheme.isEmpty()) {
        return {};
    }

    std::vector<FileProviderPlugin *> candidates;
    {
        QMutexLocker locker(&m_mutex);
        for (const Entry &entry : m_entries) {
            if (entry.schemes.contains(scheme) && entry.providerPlugin) {
                candidates.push_back(entry.providerPlugin);
            }
        }
    }

    for (FileProviderPlugin *plugin : candidates) {
        if (plugin->canHandle(path)) {
            return plugin->thumbnailUrlForPath(path);
        }
    }
    return {};
}

ProviderThumbnailResult FileProviderPluginRegistry::thumbnailForPath(const QString &path,
                                                                    const QSize &requestedSize,
                                                                    QString *error) const
{
    std::unique_ptr<FileProvider> provider = createProvider(path);
    if (!provider) {
        if (error) {
            *error = QStringLiteral("No provider for path.");
        }
        return {};
    }
    return provider->thumbnailForPath(path, requestedSize, error);
}

QString FileProviderPluginRegistry::thumbnailCacheIdentity(const QString &path) const
{
    std::unique_ptr<FileProvider> provider = createProvider(path);
    if (!provider) {
        return {};
    }
    return provider->thumbnailCacheIdentity(path);
}

QString FileProviderPluginRegistry::preprocessPath(const QString &path) const
{
    std::vector<FileProviderPlugin *> plugins;
    {
        QMutexLocker locker(&m_mutex);
        for (const Entry &entry : m_entries) {
            if (entry.providerPlugin) {
                plugins.push_back(entry.providerPlugin);
            }
        }
    }

    QString result = path;
    for (FileProviderPlugin *plugin : plugins) {
        result = plugin->preprocessPath(result);
    }
    return result;
}

QList<FileActionDescriptor> FileProviderPluginRegistry::actionsForContext(const FileActionContext &context) const
{
    struct Candidate {
        QString pluginId;
        QString displayName;
        FileActionPlugin *plugin = nullptr;
    };

    QList<Candidate> candidates;
    {
        QMutexLocker locker(&m_mutex);
        for (const Entry &entry : m_entries) {
            if (entry.actionPlugin) {
                candidates.append({entry.pluginId, entry.displayName, entry.actionPlugin});
            }
        }
    }

    QList<FileActionDescriptor> actions;
    for (const Candidate &candidate : candidates) {
        const QList<FileActionDescriptor> pluginActions = candidate.plugin->actionsForContext(context);
        for (FileActionDescriptor action : pluginActions) {
            action.id = action.id.trimmed();
            action.text = action.text.trimmed();
            if (action.id.isEmpty() || action.text.isEmpty()) {
                continue;
            }
            action.id = qualifiedActionId(candidate.pluginId, action.id);
            actions.append(action);
        }
    }

    std::sort(actions.begin(), actions.end(), [](const FileActionDescriptor &lhs, const FileActionDescriptor &rhs) {
        if (lhs.order != rhs.order) {
            return lhs.order < rhs.order;
        }
        return lhs.text.localeAwareCompare(rhs.text) < 0;
    });

    return actions;
}

QVariantMap FileProviderPluginRegistry::triggerAction(const QString &qualifiedActionIdValue,
                                                      const FileActionContext &context) const
{
    const QString pluginId = actionPluginIdFromQualifiedId(qualifiedActionIdValue);
    const QString actionId = actionIdFromQualifiedId(qualifiedActionIdValue);
    if (pluginId.isEmpty() || actionId.isEmpty()) {
        return {
            {QStringLiteral("title"), QStringLiteral("Plugin Action")},
            {QStringLiteral("message"), QStringLiteral("Invalid plugin action id.")},
        };
    }

    FileActionPlugin *plugin = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        for (const Entry &entry : m_entries) {
            if (entry.pluginId == pluginId && entry.actionPlugin) {
                plugin = entry.actionPlugin;
                break;
            }
        }
    }

    if (!plugin) {
        return {
            {QStringLiteral("title"), QStringLiteral("Plugin Action")},
            {QStringLiteral("message"), QStringLiteral("Plugin action is no longer available.")},
        };
    }

    return plugin->triggerAction(actionId, context);
}

QList<ProviderPlaceItem> FileProviderPluginRegistry::providerPlaces() const
{
    std::vector<PlacesProviderPlugin *> candidates;
    {
        QMutexLocker locker(&m_mutex);
        for (const Entry &entry : m_entries) {
            if (entry.placesPlugin) {
                candidates.push_back(entry.placesPlugin);
            }
        }
    }

    QList<ProviderPlaceItem> places;
    for (PlacesProviderPlugin *plugin : candidates) {
        for (ProviderPlaceItem place : plugin->places()) {
            place.name = place.name.trimmed();
            place.path = place.path.trimmed();
            place.icon = place.icon.trimmed();
            place.section = place.section.trimmed().toLower();
            place.driveType = place.driveType.trimmed().toLower();
            place.subtitle = place.subtitle.trimmed();
            if (place.name.isEmpty() || place.path.isEmpty()) {
                continue;
            }
            places.append(place);
        }
    }
    return places;
}

QList<FilePluginInfo> FileProviderPluginRegistry::pluginInfos() const
{
    QMutexLocker locker(&m_mutex);

    QList<FilePluginInfo> result;
    result.reserve(static_cast<qsizetype>(m_entries.size()) + m_unloadedPlugins.size());
    for (const Entry &entry : m_entries) {
        result.append({
            entry.pluginId,
            entry.displayName,
            entry.filePath,
            entry.schemes,
            entry.providerPlugin != nullptr,
            entry.actionPlugin != nullptr,
            entry.placesPlugin != nullptr,
            true,
        });
    }
    result.append(m_unloadedPlugins);
    return result;
}

bool FileProviderPluginRegistry::unloadPlugin(const QString &pluginId)
{
    const QString id = pluginId.trimmed();
    if (id.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        Entry &entry = *it;
        if (entry.pluginId == id) {
            FilePluginInfo unloadedInfo{
                entry.pluginId,
                entry.displayName,
                entry.filePath,
                entry.schemes,
                entry.providerPlugin != nullptr,
                entry.actionPlugin != nullptr,
                entry.placesPlugin != nullptr,
                false,
            };

            for (auto knownIt = m_unloadedPlugins.begin(); knownIt != m_unloadedPlugins.end();) {
                if (knownIt->pluginId == unloadedInfo.pluginId || knownIt->filePath == unloadedInfo.filePath) {
                    knownIt = m_unloadedPlugins.erase(knownIt);
                } else {
                    ++knownIt;
                }
            }
            m_unloadedPlugins.append(unloadedInfo);
            m_loadedFiles.remove(entry.filePath);
            m_entries.erase(it);
            return true;
        }
    }
    return false;
}

QStringList FileProviderPluginRegistry::loadedPluginIds() const
{
    QMutexLocker locker(&m_mutex);

    QStringList pluginIds;
    for (const Entry &entry : m_entries) {
        pluginIds.append(entry.pluginId);
    }
    return pluginIds;
}

QStringList FileProviderPluginRegistry::loadErrors() const
{
    QMutexLocker locker(&m_mutex);
    return m_loadErrors;
}

void FileProviderPluginRegistry::loadPluginFile(const QString &path)
{
    const QString pluginPath = canonicalPluginPath(path);

    {
        QMutexLocker locker(&m_mutex);
        if (m_loadedFiles.contains(pluginPath)) {
            return;
        }
    }

    auto loader = std::make_unique<QPluginLoader>(pluginPath);
    loader->setLoadHints(QLibrary::PreventUnloadHint);

    QObject *instance = loader->instance();
    if (!instance) {
        QMutexLocker locker(&m_mutex);
        appendLoadError(m_loadErrors, pluginPath, loader->errorString());
        return;
    }

    auto *providerPlugin = qobject_cast<FileProviderPlugin *>(instance);
    auto *actionPlugin = qobject_cast<FileActionPlugin *>(instance);
    auto *placesPlugin = qobject_cast<PlacesProviderPlugin *>(instance);
    if (!providerPlugin && !actionPlugin && !placesPlugin) {
        QMutexLocker locker(&m_mutex);
        appendLoadError(m_loadErrors, pluginPath, QStringLiteral("does not implement a supported FM plugin interface"));
        return;
    }

    if (providerPlugin && providerPlugin->apiVersion() != FM_FILE_PROVIDER_PLUGIN_API_VERSION) {
        QMutexLocker locker(&m_mutex);
        appendLoadError(m_loadErrors,
                        pluginPath,
                        QStringLiteral("unsupported provider API version %1").arg(providerPlugin->apiVersion()));
        return;
    }

    if (actionPlugin && actionPlugin->actionApiVersion() != FM_FILE_ACTION_PLUGIN_API_VERSION) {
        QMutexLocker locker(&m_mutex);
        appendLoadError(m_loadErrors,
                        pluginPath,
                        QStringLiteral("unsupported action API version %1").arg(actionPlugin->actionApiVersion()));
        return;
    }

    if (placesPlugin && placesPlugin->placesApiVersion() != FM_PLACES_PROVIDER_PLUGIN_API_VERSION) {
        QMutexLocker locker(&m_mutex);
        appendLoadError(m_loadErrors,
                        pluginPath,
                        QStringLiteral("unsupported places API version %1").arg(placesPlugin->placesApiVersion()));
        return;
    }

    const QString pluginId = providerPlugin
        ? providerPlugin->pluginId().trimmed()
        : (actionPlugin ? actionPlugin->actionPluginId().trimmed() : placesPlugin->placesPluginId().trimmed());
    if (pluginId.isEmpty()) {
        QMutexLocker locker(&m_mutex);
        appendLoadError(m_loadErrors, pluginPath, QStringLiteral("empty plugin id"));
        return;
    }

    if (actionPlugin && actionPlugin->actionPluginId().trimmed() != pluginId) {
        QMutexLocker locker(&m_mutex);
        appendLoadError(m_loadErrors, pluginPath, QStringLiteral("provider and action plugin ids do not match"));
        return;
    }

    if (placesPlugin && placesPlugin->placesPluginId().trimmed() != pluginId) {
        QMutexLocker locker(&m_mutex);
        appendLoadError(m_loadErrors, pluginPath, QStringLiteral("provider and places plugin ids do not match"));
        return;
    }

    const QString displayName = providerPlugin
        ? providerPlugin->displayName().trimmed()
        : (actionPlugin ? actionPlugin->actionDisplayName().trimmed() : placesPlugin->placesDisplayName().trimmed());

    QStringList schemes;
    if (providerPlugin) {
        schemes = normalizedSchemes(providerPlugin->schemes());
        if (schemes.isEmpty()) {
            QMutexLocker locker(&m_mutex);
            appendLoadError(m_loadErrors, pluginPath, QStringLiteral("no provider schemes declared"));
            return;
        }
    }

    for (const QString &scheme : schemes) {
        if (isReservedScheme(scheme)) {
            QMutexLocker locker(&m_mutex);
            appendLoadError(m_loadErrors,
                            pluginPath,
                            QStringLiteral("reserved provider scheme '%1'").arg(scheme));
            return;
        }
    }

    Entry entry;
    entry.loader = std::move(loader);
    entry.providerPlugin = providerPlugin;
    entry.actionPlugin = actionPlugin;
    entry.placesPlugin = placesPlugin;
    entry.pluginId = pluginId;
    entry.displayName = displayName;
    entry.filePath = pluginPath;
    entry.schemes = schemes;

    QMutexLocker locker(&m_mutex);
    if (m_loadedFiles.contains(pluginPath)) {
        return;
    }

    bool hasConflict = false;
    for (const Entry &existingEntry : m_entries) {
        if (existingEntry.pluginId == pluginId) {
            hasConflict = true;
            break;
        }
    }

    if (hasConflict) {
        appendLoadError(m_loadErrors,
                        pluginPath,
                        QStringLiteral("plugin id conflicts with an already loaded plugin"));
        return;
    }

    for (const Entry &existingEntry : m_entries) {
        for (const QString &scheme : schemes) {
            if (existingEntry.schemes.contains(scheme)) {
                hasConflict = true;
                break;
            }
        }
        if (hasConflict) {
            break;
        }
    }

    if (hasConflict) {
        appendLoadError(m_loadErrors,
                        pluginPath,
                        QStringLiteral("provider scheme conflicts with an already loaded plugin"));
        return;
    }

    for (auto it = m_unloadedPlugins.begin(); it != m_unloadedPlugins.end();) {
        if (it->pluginId == pluginId || it->filePath == pluginPath) {
            it = m_unloadedPlugins.erase(it);
        } else {
            ++it;
        }
    }
    m_loadedFiles.insert(pluginPath);
    m_entries.push_back(std::move(entry));
}
