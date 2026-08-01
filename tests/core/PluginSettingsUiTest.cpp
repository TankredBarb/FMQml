#include "PluginSettingsUi.h"

#include <QDebug>
#include <QObject>

namespace {

class MockSettingsUi final : public QObject, public PluginSettingsUi
{
    Q_OBJECT
    Q_INTERFACES(PluginSettingsUi)

public:
    int apiVersion = FM_PLUGIN_SETTINGS_UI_API_VERSION;
    QString id = QStringLiteral("fm.test");
    QString title = QStringLiteral("Test");
    QString url = QStringLiteral("qrc:/test/Settings.qml");
    int order = 100;

    int settingsUiApiVersion() const override { return apiVersion; }
    QString settingsUiPluginId() const override { return id; }
    QString settingsUiTitle() const override { return title; }
    QString settingsUiComponentUrl() const override { return url; }
    int settingsUiOrder() const override { return order; }
};

int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}

} // namespace

int main()
{
    MockSettingsUi mock;
    QObject *object = &mock;
    PluginSettingsUi *capability = qobject_cast<PluginSettingsUi *>(object);
    if (!capability) {
        return fail(QStringLiteral("PluginSettingsUi capability was not detected"));
    }

    PluginSettingsUiDescriptor descriptor;
    QString error;
    if (!pluginSettingsUiDescriptor(capability, QStringLiteral("fm.test"), &descriptor, &error)
        || descriptor.pluginId != QStringLiteral("fm.test")
        || descriptor.title != QStringLiteral("Test")
        || descriptor.componentUrl != QStringLiteral("qrc:/test/Settings.qml")
        || descriptor.order != 100) {
        return fail(QStringLiteral("valid settings UI descriptor was rejected: %1").arg(error));
    }

    mock.apiVersion = FM_PLUGIN_SETTINGS_UI_API_VERSION + 1;
    if (pluginSettingsUiDescriptor(capability, QStringLiteral("fm.test"), nullptr, &error)
        || !error.contains(QStringLiteral("API version"))) {
        return fail(QStringLiteral("invalid settings UI API version was accepted"));
    }
    mock.apiVersion = FM_PLUGIN_SETTINGS_UI_API_VERSION;

    mock.id = QStringLiteral("fm.other");
    if (pluginSettingsUiDescriptor(capability, QStringLiteral("fm.test"), nullptr, &error)
        || !error.contains(QStringLiteral("ids do not match"))) {
        return fail(QStringLiteral("mismatched settings UI plugin id was accepted"));
    }
    mock.id = QStringLiteral("fm.test");

    mock.url = QStringLiteral("file:///tmp/Settings.qml");
    if (pluginSettingsUiDescriptor(capability, QStringLiteral("fm.test"), nullptr, &error)
        || !error.contains(QStringLiteral("qrc:/"))) {
        return fail(QStringLiteral("non-resource settings UI URL was accepted"));
    }

    QList<PluginSettingsUiDescriptor> descriptors{
        {QStringLiteral("fm.z"), {}, QStringLiteral("qrc:/z.qml"), 200},
        {QStringLiteral("fm.b"), {}, QStringLiteral("qrc:/b.qml"), 100},
        {QStringLiteral("fm.a"), {}, QStringLiteral("qrc:/a.qml"), 100},
    };
    sortPluginSettingsUiDescriptors(&descriptors);
    if (descriptors.at(0).pluginId != QStringLiteral("fm.a")
        || descriptors.at(1).pluginId != QStringLiteral("fm.b")
        || descriptors.at(2).pluginId != QStringLiteral("fm.z")) {
        return fail(QStringLiteral("settings UI descriptor order is not deterministic"));
    }

    return 0;
}

#include "PluginSettingsUiTest.moc"
