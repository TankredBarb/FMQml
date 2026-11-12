#include "ui/framework/FmMenuItemVisual.h"
#include "ui/framework/FmMenuVisual.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlPropertyMap>
#include <QQuickItem>
#include <QTemporaryDir>
#include <QTest>
#include <cstdio>

namespace {
class PropertyMap : public QQmlPropertyMap
{
public:
    PropertyMap() : QQmlPropertyMap(this, nullptr) {}
};

int fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}

QQuickItem *background(QObject *popup)
{
    return popup->property("background").value<QQuickItem *>();
}

QObject *findVisual(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems()) {
        if (auto *found = findVisual(child, name))
            return found;
    }
    return nullptr;
}

QObject *backdropTexture(QObject *surface)
{
    for (auto *child : surface->findChildren<QObject *>()) {
        if (child->inherits("QQuickShaderEffectSource"))
            return child;
    }
    return nullptr;
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    qmlRegisterType<FmMenuVisual>("MenuTestVisuals", 1, 0, "FmMenuVisual");
    qmlRegisterType<FmMenuItemVisual>("MenuTestVisuals", 1, 0, "FmMenuItemVisual");
    QTemporaryDir directory;
    QDir dir(directory.path());
    if (!directory.isValid() || !dir.mkpath("qml/components/framework")
        || !dir.mkpath("qml/components/common") || !dir.mkpath("qml/style"))
        return fail("Cannot create menu fixture directory");
    const auto write = [&](const QString &path, const QByteArray &data) {
        QFile file(dir.filePath(path));
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    };
    for (const auto &path : {"framework/FmMenu.qml", "framework/FmMenuBackground.qml",
                             "framework/FmMenuItem.qml", "framework/FmMenuToggleItem.qml",
                             "common/TranslucentSurface.qml", "common/AmbientPanelBackground.qml"}) {
        QFile source(QStringLiteral(FM_MENU_SOURCE_DIR "/qml/components/") + path);
        if (!source.open(QIODevice::ReadOnly))
            return fail("Cannot read production menu component");
        QByteArray data = source.readAll();
        data.replace("import FM\n", "import MenuTestVisuals\n");
        if (!write(QStringLiteral("qml/components/") + path, data))
            return fail("Cannot copy production menu component");
    }
    // Text/icon presentation is independent of surface ownership and capture.
    if (!write("qml/components/framework/FmMenuItemContent.qml", R"(
        import QtQuick
        Item { property var control; property bool showSubMenuArrow: false }
    )") || !write("qml/style/qmldir", "singleton Theme 1.0 Theme.qml\n")
        || !write("qml/style/Theme.qml", R"(
        pragma Singleton
        import QtQuick
        QtObject {
            property color menuSurface: "#263442"
            property color menuBorder: "#71879b"
            property color accent: "#5bb9ed"
            property color shadow: "black"
            property color glassShadow: "black"
            property color panelSurface: "#263442"
            property color panelBorder: "#71879b"
            property color chromeGradientStart: "#263442"
            property color chromeGradientMid: "#263442"
            property color textSecondary: "gray"
            property color danger: "red"
            property color menuItemHover: "#456789"
            property color menuItemPressed: "#56789a"
            property int panelRadius: 9
            property bool useGradientColors: false
            function innerRadius(radius, border) { return Math.max(0, radius - border) }
        }
    )") || !write("Fixture.qml", R"(
        import QtQuick
        import QtQuick.Controls
        import MenuTestVisuals 1.0
        import "qml/components/framework"
        Window {
            width: 720; height: 480; visible: true
            readonly property Item menuBackdropSource: base
            Rectangle { id: base; anchors.fill: parent; color: "blue" }
            Popup {
                id: dialog; objectName: "dialog"
                x: 20; y: 20; width: 600; height: 400
                contentItem: Item {}
                background: Rectangle { color: "red" }
            }
            FmMenu {
                id: menu; objectName: "menu"
                parent: dialog.contentItem
                backdropSource: dialog.contentItem.parent
                FmMenuItem { objectName: "menuItem"; text: "Open" }
                FmMenuToggleItem { objectName: "toggleItem"; text: "Toggle" }
            }
            Popup {
                id: combo; objectName: "combo"
                x: 400; y: 40; width: 200; height: 100
                contentItem: ListView {
                    model: 1
                    delegate: FmMenuItem { objectName: "comboItem"; text: "Opaque combo" }
                }
                background: FmMenuVisual {}
            }
            FmMenu {
                id: baseMenu; objectName: "baseMenu"
                FmMenuItem { text: "Main menu" }
            }
            Component.onCompleted: { dialog.open(); menu.open(); combo.open() }
        }
    )"))
        return fail("Cannot write menu fixture");

    PropertyMap settings;
    settings.insert("menusTransparency", true);
    settings.insert("commandPaletteTransparencyStrength", 60);
    settings.insert("surfaceBlur", true);
    settings.insert("surfaceBlurStrength", 72);
    PropertyMap theme;
    theme.insert("isDark", true);
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, [](const QList<QQmlError> &errors) {
        for (const auto &error : errors)
            std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
    });
    engine.rootContext()->setContextProperty("appSettings", &settings);
    engine.rootContext()->setContextProperty("themeController", &theme);
    engine.load(QUrl::fromLocalFile(dir.filePath("Fixture.qml")));
    if (engine.rootObjects().isEmpty())
        return fail("Cannot instantiate menu fixture");
    QTest::qWait(250);
    auto *window = engine.rootObjects().first();
    auto *menu = window->findChild<QObject *>("menu");
    auto *dialog = window->findChild<QObject *>("dialog");
    auto *item = window->findChild<QObject *>("menuItem");
    auto *toggle = window->findChild<QObject *>("toggleItem");
    auto *combo = window->findChild<QObject *>("combo");
    auto *comboItem = combo ? findVisual(combo->property("contentItem").value<QQuickItem *>(),
                                         QStringLiteral("comboItem")) : nullptr;
    if (!menu || !dialog || !item || !toggle || !comboItem) {
        std::fprintf(stderr, "menu=%p dialog=%p item=%p toggle=%p comboItem=%p\n",
                     static_cast<void *>(menu), static_cast<void *>(dialog), static_cast<void *>(item),
                     static_cast<void *>(toggle), static_cast<void *>(comboItem));
        return fail("Missing fixture controls");
    }
    auto *surface = background(menu);
    auto *texture = backdropTexture(surface);
    auto *dialogContent = dialog->property("contentItem").value<QQuickItem *>();
    if (!texture || surface->property("backdropSource").value<QQuickItem *>() != dialogContent->parentItem()
        || texture->property("sourceItem").value<QQuickItem *>() != dialogContent->parentItem())
        return fail("Menu blur must capture the dialog surface, not the base window");
    if (!texture->property("live").toBool())
        return fail("Visible menu capture must follow content updates");
    if (!background(item)->property("translucent").toBool()
        || !background(toggle)->property("translucent").toBool())
        return fail("Both menu item types must inherit the menu surface transparency");
    if (background(comboItem)->property("translucent").toBool())
        return fail("Opaque combo items must keep their opaque selection style");

    settings.insert("menusTransparency", false);
    QTest::qWait(30);
    if (background(item)->property("translucent").toBool()
        || background(toggle)->property("translucent").toBool() || texture->property("live").toBool())
        return fail("Disabling transparency must restore opaque items and stop live capture");
    settings.insert("menusTransparency", true);
    QMetaObject::invokeMethod(menu, "close");
    QTest::qWait(150);
    if (texture->property("live").toBool() || texture->property("sourceItem").value<QQuickItem *>())
        return fail("Closed menus must release their live backdrop source");
    auto *baseMenu = window->findChild<QObject *>("baseMenu");
    QMetaObject::invokeMethod(baseMenu, "open");
    QTest::qWait(250);
    auto *baseSource = window->property("menuBackdropSource").value<QQuickItem *>();
    if (!baseMenu || background(baseMenu)->property("backdropSource").value<QQuickItem *>() != baseSource)
        return fail("Main menus must use the window backdrop by default");
    std::puts("Menu surface integration passed");
    return 0;
}
