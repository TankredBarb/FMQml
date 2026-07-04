#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>
#include <QtPlugin>

#include "FileProvider.h"

inline constexpr int FM_FILE_PROVIDER_PLUGIN_API_VERSION = 1;

class FileProviderPlugin
{
public:
    virtual ~FileProviderPlugin() = default;

    virtual int apiVersion() const = 0;
    virtual QString pluginId() const = 0;
    virtual QString displayName() const = 0;
    virtual QStringList schemes() const = 0;
    virtual bool canHandle(const QString &path) const = 0;
    virtual std::unique_ptr<FileProvider> createProvider() = 0;
    virtual QString preprocessPath(const QString &path) const { return path; }
    virtual QString thumbnailUrlForPath(const QString &path) const
    {
        Q_UNUSED(path)
        return {};
    }
};

#define FM_FILE_PROVIDER_PLUGIN_IID "FM.FileProviderPlugin/1.0"

Q_DECLARE_INTERFACE(FileProviderPlugin, FM_FILE_PROVIDER_PLUGIN_IID)
