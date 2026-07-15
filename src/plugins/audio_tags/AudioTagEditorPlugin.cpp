#include "AudioTagEditorPlugin.h"

#include "AudioTagEditorBackend.h"
#include "AudioTagEditorSession.h"

#include <QFileInfo>
#include <QQmlEngine>
#include <QSet>
#include <QUrl>

namespace {
constexpr auto PluginId = "fm.audio-tag-editor";
constexpr auto EditTagsAction = "editAudioTags";

bool isProviderPath(const QString &path)
{
    return path.contains(QStringLiteral("://"));
}

bool isSupportedAudioPath(const QString &path)
{
    if (path.trimmed().isEmpty() || isProviderPath(path)) {
        return false;
    }

    static const QSet<QString> supportedSuffixes{
        QStringLiteral("mp3"),
        QStringLiteral("flac"),
        QStringLiteral("m4a"),
        QStringLiteral("m4b"),
        QStringLiteral("mp4"),
        QStringLiteral("ogg"),
        QStringLiteral("oga"),
    };

    const QFileInfo info(path);
    return info.exists()
        && info.isFile()
        && info.isReadable()
        && supportedSuffixes.contains(info.suffix().toLower());
}

QStringList actionPathsForContext(const FileActionContext &context)
{
    QStringList paths = context.selectedPaths;
    if (paths.isEmpty() && !context.targetPath.trimmed().isEmpty()) {
        paths.append(context.targetPath);
    }
    paths.removeDuplicates();
    return paths;
}
}

AudioTagEditorPlugin::AudioTagEditorPlugin()
{
    qmlRegisterType<AudioTagEditorBackend>("FMAudioTags", 1, 0, "AudioTagEditorBackend");
    qmlRegisterType<AudioTagEditorSession>("FMAudioTags", 1, 0, "AudioTagEditorSession");
}

int AudioTagEditorPlugin::actionApiVersion() const
{
    return FM_FILE_ACTION_PLUGIN_API_VERSION;
}

QString AudioTagEditorPlugin::actionPluginId() const
{
    return QString::fromLatin1(PluginId);
}

QString AudioTagEditorPlugin::actionDisplayName() const
{
    return QStringLiteral("Audio Tag Editor");
}

QList<FileActionDescriptor> AudioTagEditorPlugin::actionsForContext(const FileActionContext &context) const
{
    if (context.scope != QLatin1String("item")) {
        return {};
    }

    const QStringList paths = actionPathsForContext(context);
    if (paths.isEmpty()) {
        return {};
    }

    for (const QString &path : paths) {
        if (!isSupportedAudioPath(path)) {
            return {};
        }
    }

    FileActionDescriptor action;
    action.id = QString::fromLatin1(EditTagsAction);
    action.text = paths.size() == 1 ? QStringLiteral("Edit audio tags") : QStringLiteral("Edit audio tags for selection");
    action.iconSource = QStringLiteral("qrc:/qt/qml/FM/qml/assets/icons/music.svg");
    action.order = 140;
    return {action};
}

QVariantMap AudioTagEditorPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    if (actionId != QLatin1String(EditTagsAction)) {
        return {
            {QStringLiteral("ok"), false},
            {QStringLiteral("title"), QStringLiteral("Audio Tag Editor")},
            {QStringLiteral("message"), QStringLiteral("Unknown audio tag editor action.")},
        };
    }

    const QStringList paths = actionPathsForContext(context);
    return {
        {QStringLiteral("ok"), true},
        {QStringLiteral("resultType"), QStringLiteral("pluginUi")},
        {QStringLiteral("title"), QStringLiteral("Edit Audio Tags")},
        {QStringLiteral("subtitle"), QStringLiteral("Audio tag editor plugin")},
        {QStringLiteral("iconSource"), QStringLiteral("qrc:/qt/qml/FM/qml/assets/icons/music.svg")},
        {QStringLiteral("pluginId"), QString::fromLatin1(PluginId)},
        {QStringLiteral("componentUrl"), QStringLiteral("qrc:/audio_tags/AudioTagEditor.qml")},
        {QStringLiteral("context"), QVariantMap{
            {QStringLiteral("selectedPaths"), paths},
            {QStringLiteral("currentPath"), context.currentPath},
        }},
    };
}
