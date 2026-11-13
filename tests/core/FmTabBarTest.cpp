#include "ui/framework/FmTabBarVisual.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QTemporaryDir>
#include <QTest>
#include <cstdio>

namespace {
int fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}
bool activate(QObject *tabs, int index, bool focus = false)
{
    return QMetaObject::invokeMethod(tabs, "activateTab", Q_ARG(QVariant, index), Q_ARG(QVariant, focus));
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    qmlRegisterType<FmTabBarVisual>("TabTestVisuals", 1, 0, "FmTabBarVisual");
    QTemporaryDir directory;
    QDir dir(directory.path());
    if (!directory.isValid() || !dir.mkpath("qml/components/framework") || !dir.mkpath("qml/style"))
        return fail("Cannot create tab fixture directory");
    auto write = [&](const QString &path, const QByteArray &data) {
        QFile file(dir.filePath(path));
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    };
    QFile source(QStringLiteral(FM_TAB_SOURCE_DIR "/qml/components/framework/FmTabBar.qml"));
    if (!source.open(QIODevice::ReadOnly)) return fail("Cannot read FmTabBar");
    auto component = source.readAll();
    component.replace("import FM\n", "import TabTestVisuals\n");
    if (!write("qml/components/framework/FmTabBar.qml", component)
        || !write("qml/style/qmldir", "singleton Theme 1.0 Theme.qml\n")
        || !write("qml/style/Theme.qml", R"(
            pragma Singleton
            import QtQuick
            QtObject {
                property color accent: "cyan"
                property color panelSurface: "#102030"
                property color panelBorder: "#405060"
                property color textPrimary: "white"
                property color textSecondary: "gray"
                property string fontFamily: "Sans Serif"
                property int fontSizeCaption: 11
                function withAlpha(c,a) { return Qt.rgba(c.r,c.g,c.b,a) }
            }
        )") || !write("Fixture.qml", R"(
            import QtQuick
            import QtQuick.Controls
            import "qml/components/framework"
            Window {
                width: 600; height: 220; visible: true
                property bool hasDetails: true
                QtObject { id: theme; property bool isDark: true }
                property alias themeController: theme
                Popup {
                    id: dialog; objectName: "dialog"
                    width: 580; height: 90
                    property int currentTab: 0
                    function visibleTabs() {
                        return hasDetails ? [{text:"General",value:0},{text:"Details",value:1},{text:"Access",value:2}]
                                          : [{text:"General",value:0},{text:"Access",value:2}]
                    }
                    function visibleTabIndex(value) {
                        const tabs=visibleTabs()
                        for (let i=0;i<tabs.length;i++) if(tabs[i].value===value) return i
                        return 0
                    }
                    onVisibleChanged: if(visible) currentTab=0
                    contentItem: FmTabBar {
                        objectName: "controlled"
                        model: dialog.visibleTabs()
                        currentIndex: dialog.visibleTabIndex(dialog.currentTab)
                        onActivated: (index,value) => dialog.currentTab=value
                    }
                }
                FmTabBar { objectName:"standalone"; x:10; y:130; width:580; model:["One","Two"] }
                Component.onCompleted: dialog.open()
            }
        )")) return fail("Cannot write tab fixture");
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlEngine::warnings, [](const QList<QQmlError> &errors) {
        for (const auto &error : errors) std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
    });
    engine.load(QUrl::fromLocalFile(dir.filePath("Fixture.qml")));
    if (engine.rootObjects().isEmpty()) return fail("Cannot create tab fixture");
    QTest::qWait(30);
    auto *window = engine.rootObjects().first();
    auto *dialog = window->findChild<QObject *>("dialog");
    auto *tabs = window->findChild<QObject *>("controlled");
    auto *standalone = window->findChild<QObject *>("standalone");
    if (!dialog || !tabs || !standalone) return fail("Missing tab controls");
    for (int cycle = 0; cycle < 3; ++cycle) {
        if (!activate(tabs, 1) || dialog->property("currentTab").toInt() != 1
            || tabs->property("currentIndex").toInt() != 1) return fail("Cannot select Details");
        QTest::qWait(300);
        QMetaObject::invokeMethod(dialog, "close");
        QTest::qWait(20);
        QMetaObject::invokeMethod(dialog, "open");
        QTest::qWait(30);
        auto *visual = tabs->property("background").value<FmTabBarVisual *>();
        auto *active = tabs->property("activeTab").value<QQuickItem *>();
        if (!visual || !active || qAbs(visual->highlightX()
                - active->x() - tabs->property("leftPadding").toDouble()) > 0.5
                || qAbs(visual->highlightWidth() - active->width()) > 0.5)
            return fail("Reopening must immediately place the highlight on General");
        if (dialog->property("currentTab").toInt() != 0 || tabs->property("currentIndex").toInt() != 0)
            return fail("Reopening must reset both content and selector to General");
    }
    if (!activate(tabs, 2, true) || dialog->property("currentTab").toInt() != 2) return fail("Cannot select Access");
    window->setProperty("hasDetails", false);
    QTest::qWait(30);
    if (dialog->property("currentTab").toInt() != 2 || tabs->property("currentIndex").toInt() != 1)
        return fail("Removing Details must remap Access without losing the binding");
    if (!activate(standalone, 1) || standalone->property("currentIndex").toInt() != 1)
        return fail("Standalone tab selection must still work");
    std::puts("Tab bindings survive activation, reopening, and model changes");
    return 0;
}
