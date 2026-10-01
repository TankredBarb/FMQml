#include "PanelResizeController.h"

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQuickView>
#include <QTest>
#include <cstdio>
#include <cmath>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    qmlRegisterType<PanelResizeController>("ResizeTest", 1, 0, "PanelResizeController");
    QQuickView window;
    QQmlComponent component(window.engine());
    const QUrl url("file:///panel-resize-test.qml");
    component.setData(R"(
        import QtQuick
        import QtQuick.Controls
        import ResizeTest
        Item {
            id: root; width: 640; height: 300
            property bool coalescing: true
            property int updates: 0
            SplitView {
                id: split; objectName: "split"; anchors.fill: parent
                Rectangle {
                    objectName: "left"; SplitView.preferredWidth: 320
                    SplitView.minimumWidth: 100
                    onWidthChanged: root.updates++
                }
                Rectangle { SplitView.minimumWidth: 100; SplitView.fillWidth: true }
                handle: Rectangle { implicitWidth: 12; objectName: "handle" }
            }
            PanelResizeController {
                objectName: "controller"; enabled: root.coalescing && split.resizing
            }
        }
    )", url);
    auto *root = qobject_cast<QQuickItem *>(component.create());
    if (!root) {
        for (const auto &error : component.errors()) std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
        return 1;
    }
    window.setContent(url, &component, root);
    window.show();
    QTest::qWait(100);
    auto *left = root->findChild<QQuickItem *>("left");
    auto *split = root->findChild<QQuickItem *>("split");
    auto *handle = root->findChild<QQuickItem *>("handle");
    if (!left || !split || !handle) return 1;
    auto position = [&] { return handle->mapToScene({6, 150}).toPoint(); };
    auto move = [&](const QPoint &point) {
        QMouseEvent event(QEvent::MouseMove, point, window.mapToGlobal(point),
                          Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&window, &event);
    };
    auto fail = [](const char *message) { std::fprintf(stderr, "%s\n", message); return 1; };

    QPoint start = position();
    double width = left->width();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    if (!split->property("resizing").toBool()) return fail("handle did not grab the mouse");
    root->setProperty("updates", 0);
    for (int offset = 1; offset <= 100; ++offset) move(start + QPoint(offset, 0));
    if (left->width() != width || root->property("updates").toInt() != 0)
        return fail("burst changed geometry before the frame");
    QMetaObject::invokeMethod(&window, "afterAnimating");
    if (std::abs(left->width() - width - 100) > 1 || root->property("updates").toInt() != 1)
        return fail("frame did not apply exactly the latest coordinate once");
    for (int offset = 101; offset <= 110; ++offset) move(start + QPoint(offset, 0));
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, start + QPoint(120, 0));
    if (std::abs(left->width() - width - 120) > 1 || split->property("resizing").toBool())
        return fail("release lost its final coordinate or kept resize active");
    const double finalWidth = left->width();
    QMetaObject::invokeMethod(&window, "afterAnimating");
    if (left->width() != finalWidth) return fail("stale move applied after release");

    start = position(); width = left->width();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    move(start + QPoint(-40, 0));
    root->setProperty("coalescing", false);
    QMetaObject::invokeMethod(&window, "afterAnimating");
    if (left->width() != width) return fail("disabling retained a pending move");
    move(start + QPoint(-20, 0));
    if (std::abs(left->width() - width + 20) > 1) return fail("disabled controller swallowed mouse input");
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, start + QPoint(-20, 0));
    if (split->property("resizing").toBool()) return fail("second drag did not end");

    root->setProperty("coalescing", true);
    start = position();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    move(QPoint(1, start.y()));
    QMetaObject::invokeMethod(&window, "afterAnimating");
    if (left->width() < 100 || std::abs(left->width() - 100) > 1)
        return fail("coalescing bypassed the native minimum width");
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, QPoint(1, start.y()));

    root->setProperty("coalescing", true);
    start = position(); width = left->width();
    QTest::mousePress(&window, Qt::LeftButton, Qt::NoModifier, start);
    move(start + QPoint(-30, 0));
    QEvent deactivate(QEvent::WindowDeactivate);
    QCoreApplication::sendEvent(&window, &deactivate);
    QMetaObject::invokeMethod(&window, "afterAnimating");
    if (left->width() != width) return fail("deactivation retained a pending move");
    QTest::mouseRelease(&window, Qt::LeftButton, Qt::NoModifier, start);
    return 0;
}
