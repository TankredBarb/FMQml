#pragma once

#include <memory>

#include <QObject>
#include <QString>
#include <QStringList>

#include "FileActionPlugin.h"
#include "FileProviderPlugin.h"

class InstagramFileProviderPlugin final : public QObject, public FileProviderPlugin, public FileActionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FM_FILE_PROVIDER_PLUGIN_IID)
    Q_INTERFACES(FileProviderPlugin FileActionPlugin)

public:
    int apiVersion() const override;
    QString pluginId() const override;
    QString displayName() const override;
    QStringList schemes() const override;
    bool canHandle(const QString &path) const override;
    std::unique_ptr<FileProvider> createProvider() override;
    QString preprocessPath(const QString &path) const override;
    QString thumbnailUrlForPath(const QString &path) const override;

    int actionApiVersion() const override;
    QString actionPluginId() const override;
    QString actionDisplayName() const override;
    QList<FileActionDescriptor> actionsForContext(const FileActionContext &context) const override;
    QVariantMap triggerAction(const QString &actionId, const FileActionContext &context) override;
};
