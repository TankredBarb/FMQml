#pragma once

#include <memory>
#include <vector>

#include <QMutex>
#include <QPluginLoader>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "FileProvider.h"
#include "FileActionPlugin.h"
#include "FileProviderPlugin.h"
#include "PlacesProviderPlugin.h"

struct FilePluginInfo {
    QString pluginId;
    QString displayName;
    QString filePath;
    QStringList schemes;
    bool hasProvider = false;
    bool hasActions = false;
    bool hasPlaces = false;
    bool loaded = true;
};

class FileProviderPluginRegistry final
{
public:
    static FileProviderPluginRegistry &instance();

    void loadDefaultPluginDirectories();
    void loadPluginDirectory(const QString &path);
    void loadPluginFile(const QString &path);

    bool hasProviderForPath(const QString &path) const;
    std::unique_ptr<FileProvider> createProvider(const QString &path) const;
    QString preprocessPath(const QString &path) const;
    QList<FileActionDescriptor> actionsForContext(const FileActionContext &context) const;
    QVariantMap triggerAction(const QString &qualifiedActionId, const FileActionContext &context) const;
    QList<ProviderPlaceItem> providerPlaces() const;
    QList<FilePluginInfo> pluginInfos() const;
    bool unloadPlugin(const QString &pluginId);

    QStringList loadedPluginIds() const;
    QStringList loadErrors() const;

private:
    FileProviderPluginRegistry() = default;

    FileProviderPluginRegistry(const FileProviderPluginRegistry &) = delete;
    FileProviderPluginRegistry &operator=(const FileProviderPluginRegistry &) = delete;

    struct Entry {
        std::unique_ptr<QPluginLoader> loader;
        FileProviderPlugin *providerPlugin = nullptr;
        FileActionPlugin *actionPlugin = nullptr;
        PlacesProviderPlugin *placesPlugin = nullptr;
        QString pluginId;
        QString displayName;
        QString filePath;
        QStringList schemes;
    };

    mutable QMutex m_mutex;
    std::vector<Entry> m_entries;
    QList<FilePluginInfo> m_unloadedPlugins;
    QSet<QString> m_loadedFiles;
    QStringList m_loadErrors;
};
