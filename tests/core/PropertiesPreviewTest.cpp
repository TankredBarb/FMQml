#include "controllers/PropertiesPreviewController.h"
#include "preview/PropertiesImageProvider.h"

#include <QElapsedTimer>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <atomic>
#include <cstdio>
#include <functional>
#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {
int fail(const char *message)
{
    std::fprintf(stderr, "%s\n", message);
    return 1;
}
QString token(const PropertiesPreviewController &controller)
{
    return controller.request().value("id").toString();
}
bool waitFor(const std::function<bool()> &condition)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < 2500) QTest::qWait(5);
    return condition();
}
}

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    PropertiesPreviewController controller;
    controller.setPath("/media/picture.png");
    if (!controller.request().isEmpty() || controller.available()) return fail("Inactive preview must not request images");
    controller.setActive(true);
    if (token(controller).isEmpty() || controller.available()) return fail("Availability must await a decoded thumbnail");
    const auto first = token(controller);
    controller.complete(first, true, 80, 60);
    if (!controller.available()) return fail("Real images must expose Preview");
    controller.complete(first, true, 1, 1);
    if (controller.available()) return fail("Full-size missing-thumbnail sentinel must withdraw Preview");
    controller.complete(first, true, 80, 60);
    controller.complete(first, false, 0, 0);
    if (controller.available()) return fail("Full-size decode failures must withdraw Preview");
    controller.setPath("/media/cover.mp3");
    const auto audio = token(controller);
    controller.complete(audio, true, 128, 128);
    controller.complete(first, false, 0, 0);
    if (!controller.available()) return fail("Stale requests must not change the current preview");
    controller.setActive(false);
    controller.complete(audio, true, 128, 128);
    if (controller.available() || !controller.request().isEmpty()) return fail("Closed dialogs must ignore late results");
    controller.setActive(true);
    if (token(controller) == audio) return fail("Reopening must start a new generation");
    controller.complete(audio, true, 128, 128);
    if (controller.available()) return fail("Previous opening must not publish into a new session");
    controller.setPath("https://example.test/image.png");
    if (!controller.request().isEmpty()) return fail("Remote files must not be materialized");

    QTemporaryDir fixtures;
    if (!fixtures.isValid()) return fail("Cannot create fixtures");
    QImage picture(80, 60, QImage::Format_ARGB32);
    picture.fill(Qt::blue);
    for (const auto &name : {"first.png", "slow.png", "slow-close.png", "next.png", "image.bin", "image"}) {
        if (!picture.save(fixtures.filePath(name), "PNG")) return fail("Cannot write image fixture");
    }
    QImage pixel(1, 1, QImage::Format_ARGB32);
    pixel.fill(Qt::transparent);
    if (!pixel.save(fixtures.filePath("pixel.png"))) return fail("Cannot write one-pixel fixture");
    for (const auto &name : {"no-cover.mp3", "unstable.mp3", "readme.txt", "program.exe", "font.ttf"}) {
        QFile file(fixtures.filePath(name));
        if (!file.open(QIODevice::WriteOnly)) return fail("Cannot write non-image fixture");
        file.write("fixture");
    }

    std::atomic_bool blockedGui{false}, slowStarted{false}, cancellationObserved{false};
    std::atomic_int calls{0}, largeCalls{0};
    const auto guiThread = QThread::currentThread();
    qmlRegisterType<PropertiesPreviewController>("PreviewTest", 1, 0, "PropertiesPreviewController");
    QQmlApplicationEngine engine;
    int warnings = 0;
    QObject::connect(&engine, &QQmlEngine::warnings, [&](const QList<QQmlError> &errors) {
        warnings += errors.size();
        for (const auto &error : errors) std::fprintf(stderr, "%s\n", qPrintable(error.toString()));
    });
    engine.addImageProvider("propertiespreview", new PropertiesImageProvider([&](const VisualThumbnailRequest &request) {
        if (QThread::currentThread() == guiThread) blockedGui = true;
        ++calls;
        if (request.size.width() > 128) ++largeCalls;
        if (request.path.contains("slow")) {
            slowStarted = true;
            // Deliberately simulate a raster decoder that cannot stop mid-read.
            QThread::msleep(1000);
            if (request.cancelled()) cancellationObserved = true;
        }
        if (request.cancelled() || request.path.endsWith("no-cover.mp3")) return QImage{};
        if (request.path.endsWith("unstable.mp3"))
            return request.size.width() > 128 ? QImage{} : picture;
        if (request.path.endsWith("pixel.png")) return QImage(request.path);
        QImageReader reader(request.path);
        QSize scaled = reader.size();
        if (scaled.isValid()) {
            scaled.scale(request.size, Qt::KeepAspectRatio);
            reader.setScaledSize(scaled);
        }
        return reader.read();
    }));
    engine.rootContext()->setContextProperty("fixturePath", fixtures.path());
    QByteArray qml = R"(
        import QtQuick
        import PreviewTest
        import "file:SOURCE/qml/components/properties"
        Item {
            property bool showLarge: false
            PropertiesPreviewController { id: preview; objectName: "controller"; path: fixturePath + "/first.png"; active: true }
            PropertiesPreviewProbe { controller: preview }
            Repeater {
                model: showLarge && preview.available ? [preview.request] : []
                Image {
                    required property var modelData
                    source: modelData.source; sourceSize: Qt.size(1024,1024); asynchronous: true; cache: false
                    onStatusChanged: if(status===Image.Ready || status===Image.Error)
                        preview.complete(modelData.id,status===Image.Ready,implicitWidth,implicitHeight)
                }
            }
        }
    )";
    qml.replace("SOURCE", FM_PROPERTIES_PREVIEW_SOURCE_DIR);
    engine.loadData(qml);
    if (engine.rootObjects().isEmpty()) return fail("Cannot instantiate async preview probe");
    auto *root = engine.rootObjects().first();
    auto *preview = root->findChild<PropertiesPreviewController *>("controller");
    if (!preview || preview->available()) return fail("Probe must start asynchronously");
    if (!waitFor([&] { return preview->available(); })) return fail("Real thumbnails must publish asynchronously");
    for (const auto &name : {"image.bin", "image", "pixel.png"}) {
        preview->setPath(fixtures.filePath(name));
        if (!waitFor([&] { return preview->available(); })) return fail("Content-detected and one-pixel images must work");
    }
#ifdef Q_OS_UNIX
    const QString fifo = fixtures.filePath("pipe.png");
    if (::mkfifo(QFile::encodeName(fifo).constData(), 0600) != 0) return fail("Cannot create FIFO fixture");
    preview->setPath(fifo);
    QTest::qWait(50);
    if (preview->available()) return fail("Special files must not expose Preview");
#endif
    const int beforeUnsupported = calls;
    for (const auto &name : {"readme.txt", "program.exe", "font.ttf"}) {
        preview->setPath(fixtures.filePath(name));
        QTest::qWait(50);
        if (preview->available()) return fail("Non-media must not expose Preview");
    }
    if (calls != beforeUnsupported) return fail("Non-media must not reach the decoder");
    preview->setPath(fixtures.filePath("no-cover.mp3"));
    QTest::qWait(100);
    if (preview->available()) return fail("Missing covers must not expose Preview");
    preview->setPath(fixtures.filePath("unstable.mp3"));
    if (!waitFor([&] { return preview->available(); })) return fail("Small cover did not load");
    if (largeCalls != 0) return fail("Large previews must remain lazy");
    root->setProperty("showLarge", true);
    if (!waitFor([&] { return largeCalls > 0 && !preview->available(); })) return fail("Failed large cover must withdraw Preview");
    root->setProperty("showLarge", false);

    preview->setPath(fixtures.filePath("slow.png"));
    if (!waitFor([&] { return slowStarted.load(); })) return fail("Slow request did not start");
    const auto slow = token(*preview);
    QElapsedTimer replacementTimer;
    replacementTimer.start();
    preview->setPath(fixtures.filePath("next.png"));
    if (!waitFor([&] { return preview->available(); }) || replacementTimer.elapsed() >= 700)
        return fail("Cancelled slow decoder must not serialize its replacement");
    std::printf("Replacement loaded in %lld ms while the cancelled decoder was still running\n",
                static_cast<long long>(replacementTimer.elapsed()));
    preview->complete(slow, false, 0, 0);
    if (!preview->available()) return fail("Cancelled generation changed the replacement");
    if (!waitFor([&] { return cancellationObserved.load(); })) return fail("Decoder did not observe cancellation");
    slowStarted = false;
    cancellationObserved = false;
    preview->setPath(fixtures.filePath("slow-close.png"));
    if (!waitFor([&] { return slowStarted.load(); })) return fail("Close-during-load request did not start");
    const auto closingToken = token(*preview);
    replacementTimer.restart();
    preview->setActive(false);
    if (replacementTimer.elapsed() >= 700) return fail("Closing must not wait for decoding");
    if (preview->available() || !preview->request().isEmpty()) return fail("Closing must remove the request");
    preview->setActive(true);
    if (token(*preview) == closingToken) return fail("Reopening must use a new generation");
    if (!waitFor([&] { return preview->available(); })) return fail("Reopening the same in-flight file must work");
    if (!cancellationObserved) return fail("Closing must cancel the old response");
    preview->complete(closingToken, false, 0, 0);
    if (!preview->available()) return fail("Closed session must not affect the reopened session");
    preview->setActive(false);
    if (warnings) return fail("Unexpected QML warnings");
    if (blockedGui) return fail("MIME detection and decoding must stay off the GUI thread");
    std::puts("Properties Preview content detection, large-result validation, and async cancellation passed");
    return 0;
}
