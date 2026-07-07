#pragma once

#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVariant>
#include <QtPlugin>

inline constexpr int FM_FILE_ACTION_PLUGIN_API_VERSION = 1;

struct FileActionContext {
    QString scope;
    QString currentPath;
    QString targetPath;
    QString destinationPath;
    QStringList selectedPaths;
    bool targetIsDirectory = false;
    QVariantMap parameters;
};

struct FileActionDescriptor {
    QString id;
    QString text;
    QString iconSource;
    bool enabled = true;
    bool asynchronous = false;
    int order = 0;
};

class FileActionPlugin
{
public:
    virtual ~FileActionPlugin() = default;

    virtual int actionApiVersion() const = 0;
    virtual QString actionPluginId() const = 0;
    virtual QString actionDisplayName() const = 0;
    virtual QList<FileActionDescriptor> actionsForContext(const FileActionContext &context) const = 0;
    virtual QVariantMap triggerAction(const QString &actionId, const FileActionContext &context) = 0;
};

#define FM_FILE_ACTION_PLUGIN_IID "FM.FileActionPlugin/1.0"

Q_DECLARE_INTERFACE(FileActionPlugin, FM_FILE_ACTION_PLUGIN_IID)
