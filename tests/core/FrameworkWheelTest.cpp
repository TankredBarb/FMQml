#include "ui/framework/FmDocumentViewVisual.h"
#include "ui/framework/FmScrollBarVisual.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickView>
#include <QStyleHints>
#include <QTemporaryDir>
#include <QTimer>
#include <QWheelEvent>
#include <cmath>
#include <cstdio>

namespace {
void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    qmlRegisterType<FmScrollBarVisual>("WheelTestVisuals", 1, 0, "FmScrollBarVisual");
    qmlRegisterType<FmDocumentViewVisual>("WheelTestVisuals", 1, 0, "FmDocumentViewVisual");
    QTemporaryDir directory;
    QDir dir(directory.path());
    if (!directory.isValid() || !dir.mkpath("qml/components/framework") || !dir.mkpath("qml/style")) return 1;
    // Isolate input/geometry from the application's theme service.
    auto write = [&](const QString &path, const QByteArray &data) {
        QFile file(dir.filePath(path));
        return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
    };
    for (const auto &name : {"FmScrollBar.qml", "FmDocumentView.qml"}) {
        QFile source(QStringLiteral(FM_WHEEL_SOURCE_DIR "/qml/components/framework/") + name);
        if (!source.open(QIODevice::ReadOnly)) return 1;
        QByteArray data = source.readAll();
        data.replace("import FM\n", "import WheelTestVisuals\n");
        if (!write(QStringLiteral("qml/components/framework/") + name, data)) return 1;
    }
    if (!write("qml/style/qmldir", "singleton Theme 1.0 Theme.qml\n")
        || !write("qml/style/Theme.qml", R"(
            pragma Singleton
            import QtQuick
            QtObject {
                property color panelSurface: "white"
                property color panelSurfaceStrong: "white"
                property color panelBorder: "gray"
                property color bg: "white"
                property color textPrimary: "black"
                property color textSecondary: "gray"
                property color accent: "blue"
                function withAlpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }
                function opaque(c) { return c }
                function mixColors(a, b, t) { return a }
                function readableOn(a, b) { return b }
            }
        )")) return 1;

    quint64 timestamp = 10000;
    bool success = true;
    for (const auto &kind : {"bar", "horizontal", "panel", "document"}) {
        QQuickView window;
        window.rootContext()->setContextProperty("themeController", QVariantMap{{"isDark", false}});
        QQmlComponent component(window.engine());
        const QUrl url = QUrl::fromLocalFile(dir.filePath("WheelTest.qml"));
        QByteArray fixture = R"(
            import QtQuick
            import QtQuick.Controls
            import "qml/components/framework"
            import "file:SOURCE/qml/components/filepanel"
            Item {
                width: 640; height: 360
                Flickable {
                    id: viewport; objectName: "viewport"; anchors.fill: parent
                    contentWidth: 4000; contentHeight: 4000
                    FilePanelWheelHandler { id: wheel; objectName: "wheel"; view: viewport; enabled: PANEL }
                    ScrollBar.vertical: FmScrollBar {
                        objectName: "bar"; wheelTarget: viewport; wheelHandler: PANEL ? wheel : null
                    }
                }
            }
        )";
        fixture.replace("SOURCE", FM_WHEEL_SOURCE_DIR);
        fixture.replace("PANEL", QByteArray(kind) == "panel" ? "true" : "false");
        if (QByteArray(kind) == "horizontal") fixture.replace("ScrollBar.vertical:", "ScrollBar.horizontal:");
        if (QByteArray(kind) == "document") {
            fixture = R"(
                import QtQuick
                import "qml/components/framework"
                FmDocumentView { width: 640; height: 360; text: ("long line\n").repeat(500) }
            )";
        }
        component.setData(fixture, url);
        auto *root = qobject_cast<QQuickItem *>(component.create());
        if (!root) {
            for (const auto &error : component.errors()) std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
            return 1;
        }
        window.setContent(url, &component, root);
        window.resize(640, 360);
        window.show();
        wait(50);
        QQuickItem *viewport = nullptr;
        for (auto *item : root->findChildren<QQuickItem *>()) {
            if (item->inherits("QQuickFlickable")) { viewport = item; break; }
        }
        if (!viewport) return 1;
        const bool horizontal = QByteArray(kind) == "horizontal";
        const char *axis = horizontal ? "contentX" : "contentY";
        auto *bar = root->findChild<QQuickItem *>("bar");
        const QPointF at = bar ? bar->mapToScene({bar->width() / 2, bar->height() / 2}) : QPointF(150, 150);
        for (int format = 0; format < 4; ++format) {
            viewport->setProperty(axis, 180.0);
            // Vertical wheel rotation also scrolls a horizontal scrollbar.
            const QPoint pixels = format == 0 ? QPoint() : QPoint(0, -15);
            const QPoint angles = format == 3 ? QPoint() : QPoint(0, -120);
            QWheelEvent event(at, window.mapToGlobal(at.toPoint()), pixels, angles,
                              Qt::NoButton, Qt::NoModifier, format == 2 ? Qt::ScrollUpdate : Qt::NoScrollPhase,
                              false, Qt::MouseEventSynthesizedBySystem);
            event.setTimestamp(timestamp += 12);
            QCoreApplication::sendEvent(&window, &event);
            wait(400);
            const double expected = format >= 2 ? 6.0 : QByteArray(kind) == "panel"
                ? app.styleHints()->wheelScrollLines() * 20.0 : 48.0;
            const double distance = viewport->property(axis).toDouble() - 180.0;
            std::printf("%s format=%d distance=%.2f expected=%.2f\n", kind, format, distance, expected);
            if (std::abs(distance - expected) > .5) success = false;
        }
    }
    return success ? 0 : 1;
}
