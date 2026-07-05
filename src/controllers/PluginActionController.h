#pragma once

#include <QObject>
#include <QVariantList>
#include <QVariantMap>

#include "../core/FileActionPlugin.h"

class PluginActionController final : public QObject {
    Q_OBJECT

public:
    explicit PluginActionController(QObject *parent = nullptr);

    Q_INVOKABLE QVariantList actionsForContext(const QVariantMap &context) const;
    Q_INVOKABLE QVariantMap triggerAction(const QString &actionId, const QVariantMap &context);
    Q_INVOKABLE QVariantList plugins() const;
    Q_INVOKABLE QVariantMap loadPluginFile(const QString &fileUrl);
    Q_INVOKABLE QVariantMap loadPluginDirectory(const QString &folderUrl);
    Q_INVOKABLE QVariantMap rescanDefaultPluginDirectories();
    Q_INVOKABLE QVariantMap unloadPlugin(const QString &pluginId);
    Q_INVOKABLE QStringList loadErrors() const;

signals:
    void pluginsChanged();
    void placesRefreshRequested();

private:
    static FileActionContext contextFromMap(const QVariantMap &map);
    static QString localPathFromUrl(const QString &value);
};
