#pragma once

#include <QObject>
#include <QString>

#include "FileActionPlugin.h"

class AudioTagEditorPlugin final : public QObject, public FileActionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FM_FILE_ACTION_PLUGIN_IID)
    Q_INTERFACES(FileActionPlugin)

public:
    AudioTagEditorPlugin();

    int actionApiVersion() const override;
    QString actionPluginId() const override;
    QString actionDisplayName() const override;
    QList<FileActionDescriptor> actionsForContext(const FileActionContext &context) const override;
    QVariantMap triggerAction(const QString &actionId, const FileActionContext &context) override;
};
