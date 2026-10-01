#include <QGuiApplication>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickView>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QEventLoop>
#include <QTimer>
#include <QStyleHints>
#include <cmath>
#include <cstdio>

namespace {
void wait(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

int fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQuickView window;
    QQmlComponent component(window.engine());
    const QUrl url = QUrl::fromLocalFile(QStringLiteral(FM_GRID_WHEEL_QML_DIR "/WheelTest.qml"));
    QByteArray fixture = R"(
        import QtQuick
        Item {
            width: 640; height: 360
            property int clicks: 0
            GridView {
                id: grid; objectName: "grid"; anchors.fill: parent
                model: 1000; cellWidth: 100; cellHeight: 120
                pixelAligned: false; boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                delegate: Rectangle {
                    width: 100; height: 120
                    MouseArea {
                        anchors.fill: parent
                        scrollGestureEnabled: false
                        onWheel: (event) => { event.accepted = false }
                        onClicked: grid.parent.clicks++
                    }
                }
                FilePanelWheelHandler {
                    objectName: "wheel"; parent: grid; anchors.fill: parent
                    z: 9; view: grid
                }
            }
        }
    )";
    if (app.arguments().contains(QStringLiteral("--detailed"))) {
        fixture.replace("GridView {", "ListView {");
        fixture.replace("model: 1000; cellWidth: 100; cellHeight: 120", "model: 1000");
        fixture.replace("width: 100; height: 120", "width: 640; height: 32");
    }
    if (app.arguments().contains(QStringLiteral("--brief"))) {
        fixture.replace("cellWidth: 100; cellHeight: 120", "cellWidth: 320; cellHeight: 32");
        fixture.replace("width: 100; height: 120", "width: 320; height: 32");
    }
    component.setData(fixture, url);
    auto *root = qobject_cast<QQuickItem *>(component.create());
    if (!root) {
        for (const auto &error : component.errors()) std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
        return fail("could not load real grid wheel component");
    }
    window.setContent(url, &component, root);
    window.resize(640, 360);
    window.show();
    wait(100);
    auto *grid = root->findChild<QQuickItem *>("grid");
    auto *handler = root->findChild<QQuickItem *>("wheel");
    if (!grid || !handler) return fail("grid wheel fixture incomplete");
    if (app.arguments().contains(QStringLiteral("--native-baseline")))
        handler->setProperty("enabled", false);
    quint64 timestamp = 10000;
    const double step = app.styleHints()->wheelScrollLines() * 20;
    auto position = [&] { return grid->mapToScene(QPointF(150, 150)); };
    auto wheel = [&](int delta, bool sameTimestamp = false) {
        const auto point = position();
        QWheelEvent event(point, window.mapToGlobal(point.toPoint()), {}, QPoint(0, -delta),
                          Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        if (!sameTimestamp) timestamp += 12;
        event.setTimestamp(timestamp);
        QCoreApplication::sendEvent(&window, &event);
    };
    auto y = [&] { return grid->property("contentY").toDouble(); };
    auto reset = [&](double value) {
        QMetaObject::invokeMethod(handler, "cancel");
        QMetaObject::invokeMethod(grid, "cancelFlick");
        grid->setProperty("contentY", value);
        wait(50);
    };
    auto near = [](double a, double b) { return std::abs(a - b) < 0.5; };

    reset(180);
    wheel(120); wait(400);
    if (!near(y(), 180 + step)) return fail("single detent step changed");
    reset(180);
    for (int i = 0; i < 8; ++i) { wheel(15); wait(12); }
    wait(400);
    if (!near(y(), 180 + step)) return fail("fragmented detent lost pending distance");
    reset(180);
    for (int i = 0; i < 10; ++i) { wheel(120); wait(100); }
    wait(400);
    if (!near(y(), 180 + 10 * step)) return fail("repeated detents lost pending distance");
    reset(180);
    wheel(120); wheel(120, true); wait(400);
    if (!near(y(), 180 + 2 * step)) return fail("equal timestamps lost a detent");

    reset(180);
    for (int i = 0; i < 8; ++i) { wheel(120); wait(12); }
    wheel(-120);
    const double reverseStart = y();
    wait(40);
    if (y() >= reverseStart) return fail("wheel reversal retained forward backlog");
    wait(400);
    if (!near(y(), reverseStart - step)) return fail("reversal did not restart from current position");

    const double maximum = grid->property("contentHeight").toDouble() - grid->height();
    reset(maximum - 10); wheel(120); wheel(120); wait(400);
    if (!near(y(), maximum)) return fail("wheel exceeded bottom bound");
    reset(10); wheel(-120); wheel(-120); wait(400);
    if (!near(y(), 0)) return fail("wheel exceeded top bound");

    grid->setProperty("bottomMargin", 32);
    reset(maximum - 10); wheel(120); wheel(120); wait(400);
    if (!near(y(), maximum + 32)) return fail("wheel could not reach content with bottom margin");
    grid->setProperty("topMargin", 16);
    reset(10); wheel(-120); wheel(-120); wait(400);
    if (!near(y(), -16)) return fail("wheel did not respect top margin");
    grid->setProperty("topMargin", 0);
    grid->setProperty("bottomMargin", 0);

    reset(180); wheel(120); wait(30);
    handler->setProperty("enabled", false);
    const double cancelled = y();
    wait(400);
    if (!near(y(), cancelled) || handler->property("active").toBool())
        return fail("disabled handler retained its animation");
    handler->setProperty("enabled", true);
    reset(180);
    wheel(120); wait(30);
    QMetaObject::invokeMethod(grid, "flick", Q_ARG(qreal, 0), Q_ARG(qreal, -400));
    wait(40);
    if (handler->property("active").toBool()) return fail("native flick did not cancel wheel animation");
    reset(180);
    wheel(120); wait(30);
    const auto gesturePoint = position();
    QWheelEvent begin(gesturePoint, window.mapToGlobal(gesturePoint.toPoint()), {}, {},
                      Qt::NoButton, Qt::NoModifier, Qt::ScrollBegin, false,
                      Qt::MouseEventSynthesizedBySystem);
    begin.setAccepted(false);
    begin.setTimestamp(timestamp += 12);
    QCoreApplication::sendEvent(&window, &begin);
    if (handler->property("active").toBool()) return fail("gesture start retained pending wheel motion");
    QWheelEvent gesture(gesturePoint, window.mapToGlobal(gesturePoint.toPoint()), QPoint(0, -40), QPoint(0, -120),
                        Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false,
                        Qt::MouseEventSynthesizedBySystem);
    gesture.setAccepted(false);
    gesture.setTimestamp(timestamp += 12);
    QCoreApplication::sendEvent(&window, &gesture);
    wait(12);
    gesture.setTimestamp(timestamp += 12);
    gesture.setAccepted(false);
    QCoreApplication::sendEvent(&window, &gesture);
    wait(40);
    if (handler->property("active").toBool() || !grid->property("moving").toBool())
        return fail("pixel gesture did not retain native Flickable handling");
    QWheelEvent end(gesturePoint, window.mapToGlobal(gesturePoint.toPoint()), {}, {},
                    Qt::NoButton, Qt::NoModifier, Qt::ScrollEnd, false,
                    Qt::MouseEventSynthesizedBySystem);
    end.setAccepted(false);
    end.setTimestamp(timestamp += 12);
    QCoreApplication::sendEvent(&window, &end);
    reset(180);

    const auto point = position();
    QMouseEvent press(QEvent::MouseButtonPress, point, window.mapToGlobal(point.toPoint()),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease, point, window.mapToGlobal(point.toPoint()),
                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&window, &press);
    QCoreApplication::sendEvent(&window, &release);
    if (root->property("clicks").toInt() != 1) return fail("wheel surface intercepted delegate click");
    std::puts("grid wheel distance, reversal, bounds, cancellation and click checks passed");
    return 0;
}
