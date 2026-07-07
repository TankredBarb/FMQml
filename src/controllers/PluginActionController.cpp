#include "PluginActionController.h"

#include "../core/FileProviderPluginRegistry.h"

#include <QDir>
#include <QSettings>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QUrl>
#include <QVariant>
#include <QtConcurrent>

namespace {
QStringList stringListFromVariant(const QVariant &value)
{
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }

    QStringList result;
    const QVariantList list = value.toList();
    for (const QVariant &item : list) {
        const QString text = item.toString();
        if (!text.isEmpty()) {
            result.append(text);
        }
    }
    return result;
}
QVariantMap normalizeActionResult(QVariantMap result, const QString &actionId = {})
{
    if (!actionId.isEmpty() && !result.contains(QStringLiteral("actionId"))) {
        result.insert(QStringLiteral("actionId"), actionId);
    }
    if (!result.contains(QStringLiteral("title"))) {
        result.insert(QStringLiteral("title"), QStringLiteral("Plugin Action"));
    }
    if (!result.contains(QStringLiteral("message"))) {
        result.insert(QStringLiteral("message"), QStringLiteral("Action completed."));
    }
    return result;
}
}

PluginActionController::PluginActionController(QObject *parent)
    : QObject(parent)
{
}

QVariantList PluginActionController::actionsForContext(const QVariantMap &context) const
{
    const QList<FileActionDescriptor> actions = FileProviderPluginRegistry::instance().actionsForContext(contextFromMap(context));

    QVariantList result;
    result.reserve(actions.size());
    for (const FileActionDescriptor &action : actions) {
        result.append(QVariantMap{
            {QStringLiteral("id"), action.id},
            {QStringLiteral("text"), action.text},
            {QStringLiteral("iconSource"), action.iconSource},
            {QStringLiteral("enabled"), action.enabled},
            {QStringLiteral("asynchronous"), action.asynchronous},
        });
    }
    return result;
}

QVariantMap PluginActionController::triggerAction(const QString &actionId, const QVariantMap &context)
{
    QVariantMap result = normalizeActionResult(
        FileProviderPluginRegistry::instance().triggerAction(actionId, contextFromMap(context)),
        actionId);
    if (result.value(QStringLiteral("refreshPlaces")).toBool()) {
        emit placesRefreshRequested();
    }
    return result;
}

QVariantMap PluginActionController::triggerActionAsync(const QString &actionId, const QVariantMap &context)
{
    const FileActionContext actionContext = contextFromMap(context);
    auto *watcher = new QFutureWatcher<QVariantMap>(this);
    connect(watcher, &QFutureWatcher<QVariantMap>::finished, this, [this, watcher, actionId]() {
        QVariantMap result = normalizeActionResult(watcher->result(), actionId);
        watcher->deleteLater();
        if (result.value(QStringLiteral("refreshPlaces")).toBool()) {
            emit placesRefreshRequested();
        }
        emit actionFinished(result);
    });
    watcher->setFuture(QtConcurrent::run([actionId, actionContext]() {
        return FileProviderPluginRegistry::instance().triggerAction(actionId, actionContext);
    }));
    return normalizeActionResult({
        {QStringLiteral("ok"), true},
        {QStringLiteral("actionId"), actionId},
        {QStringLiteral("statusOnly"), true},
        {QStringLiteral("message"), QStringLiteral("Plugin action started.")},
    }, actionId);
}

QVariantList PluginActionController::plugins() const
{
    const QList<FilePluginInfo> infos = FileProviderPluginRegistry::instance().pluginInfos();

    QVariantList result;
    result.reserve(infos.size());
    for (const FilePluginInfo &info : infos) {
        QStringList capabilities;
        if (info.hasProvider) {
            capabilities.append(QStringLiteral("Provider"));
        }
        if (info.hasActions) {
            capabilities.append(QStringLiteral("Actions"));
        }
        if (info.hasPlaces) {
            capabilities.append(QStringLiteral("Places"));
        }

        result.append(QVariantMap{
            {QStringLiteral("pluginId"), info.pluginId},
            {QStringLiteral("displayName"), info.displayName.isEmpty() ? info.pluginId : info.displayName},
            {QStringLiteral("filePath"), QDir::toNativeSeparators(info.filePath)},
            {QStringLiteral("schemes"), info.schemes},
            {QStringLiteral("schemesText"), info.schemes.join(QStringLiteral(", "))},
            {QStringLiteral("capabilities"), capabilities},
            {QStringLiteral("capabilitiesText"), capabilities.join(QStringLiteral(", "))},
            {QStringLiteral("loaded"), info.loaded},
        });
    }
    return result;
}

QVariantMap PluginActionController::loadPluginFile(const QString &fileUrl)
{
    const QString path = localPathFromUrl(fileUrl);
    if (path.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("No plugin file selected.")}};
    }

    FileProviderPluginRegistry::instance().loadPluginFile(path);
    emit pluginsChanged();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("message"), QStringLiteral("Plugin load requested.")}};
}

QVariantMap PluginActionController::loadPluginDirectory(const QString &folderUrl)
{
    const QString path = localPathFromUrl(folderUrl);
    if (path.isEmpty()) {
        return {{QStringLiteral("ok"), false}, {QStringLiteral("message"), QStringLiteral("No plugin folder selected.")}};
    }

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    settings.setValue(QStringLiteral("customDirectory"), path);
    settings.endGroup();

    FileProviderPluginRegistry::instance().loadPluginDirectory(path);
    emit pluginsChanged();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("message"), QStringLiteral("Plugin folder scanned.")}};
}

QVariantMap PluginActionController::rescanDefaultPluginDirectories()
{
    FileProviderPluginRegistry::instance().loadDefaultPluginDirectories();
    emit pluginsChanged();
    return {{QStringLiteral("ok"), true}, {QStringLiteral("message"), QStringLiteral("Default plugin directories scanned.")}};
}

QVariantMap PluginActionController::unloadPlugin(const QString &pluginId)
{
    const bool changed = FileProviderPluginRegistry::instance().unloadPlugin(pluginId);
    if (changed) {
        emit pluginsChanged();
    }
    return {
        {QStringLiteral("ok"), changed},
        {QStringLiteral("message"),
         changed
             ? QStringLiteral("Plugin unloaded from this session.")
             : QStringLiteral("Plugin was not found.")},
    };
}

QStringList PluginActionController::loadErrors() const
{
    return FileProviderPluginRegistry::instance().loadErrors();
}

FileActionContext PluginActionController::contextFromMap(const QVariantMap &map)
{
    FileActionContext context;
    context.scope = map.value(QStringLiteral("scope")).toString();
    context.currentPath = map.value(QStringLiteral("currentPath")).toString();
    context.targetPath = map.value(QStringLiteral("targetPath")).toString();
    context.destinationPath = map.value(QStringLiteral("destinationPath")).toString();
    context.selectedPaths = stringListFromVariant(map.value(QStringLiteral("selectedPaths")));
    context.targetIsDirectory = map.value(QStringLiteral("targetIsDirectory")).toBool();
    const QVariant parameters = map.value(QStringLiteral("parameters"));
    if (parameters.canConvert<QVariantMap>()) {
        context.parameters = parameters.toMap();
    }
    return context;
}

QString PluginActionController::localPathFromUrl(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QUrl url(trimmed);
    if (url.isLocalFile()) {
        return QDir::fromNativeSeparators(url.toLocalFile());
    }
    return QDir::fromNativeSeparators(trimmed);
}
