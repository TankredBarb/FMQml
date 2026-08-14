#include "preview/text/TextPreviewController.h"
#include "preview/text/TextPreviewReader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QTemporaryDir>

#include <functional>
#include <memory>
#include <cstdio>

namespace {
int fail(const QString &message)
{
    std::fprintf(stderr, "%s\n", qPrintable(message));
    return 1;
}

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(content) == content.size();
}

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = 3000)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    return condition();
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) {
        return fail(QStringLiteral("could not create temporary directory"));
    }

    const QString firstPath = directory.filePath(QStringLiteral("first.txt"));
    const QString secondPath = directory.filePath(QStringLiteral("second.py"));
    const QByteArray firstContent(
        "one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine\nten\neleven\ntwelve\n");
    const QByteArray secondContent("print('latest')\n");
    if (!writeFile(firstPath, firstContent) || !writeFile(secondPath, secondContent)) {
        return fail(QStringLiteral("could not create controller fixtures"));
    }

    TextPreviewController controller;
    TextPreview::ReadOptions options;
    options.fullDocumentLimit = 12;
    options.windowBytes = 14;
    controller.setReadOptions(options);

    controller.loadLocalFile(firstPath);
    if (!waitUntil([&controller]() { return !controller.loading(); })) {
        return fail(QStringLiteral("initial window did not finish"));
    }
    const TextPreview::Snapshot firstPage = controller.snapshot();
    if (firstPage.state != TextPreview::State::Ready
        || firstPage.mode != TextPreview::Mode::Windowed
        || firstPage.byteOffset != 0 || firstPage.firstLine != 1
        || !firstPage.hasNextPage) {
        return fail(QStringLiteral("initial controller snapshot is inconsistent"));
    }

    controller.requestNextPage();
    if (!waitUntil([&controller, firstPage]() {
            return !controller.loading() && controller.snapshot().byteOffset > firstPage.byteOffset;
        })) {
        return fail(QStringLiteral("next page did not finish"));
    }
    const TextPreview::Snapshot secondPage = controller.snapshot();
    if (!secondPage.hasPreviousPage
        || secondPage.byteOffset != firstPage.byteOffset + firstPage.byteLength
        || secondPage.firstLine != firstPage.firstLine + firstPage.lineAdvance) {
        return fail(QStringLiteral("next page lost byte or line continuity"));
    }

    controller.requestPreviousPage();
    if (controller.snapshot().byteOffset != firstPage.byteOffset
        || controller.snapshot().content != firstPage.content) {
        return fail(QStringLiteral("previous page did not use the cached snapshot"));
    }

    int forwardPages = 1;
    while (controller.snapshot().hasNextPage && forwardPages < 10) {
        const qint64 oldOffset = controller.snapshot().byteOffset;
        controller.requestNextPage();
        if (!waitUntil([&controller, oldOffset]() {
                return !controller.loading() && controller.snapshot().byteOffset > oldOffset;
            })) {
            return fail(QStringLiteral("could not advance through all text pages"));
        }
        ++forwardPages;
    }
    if (forwardPages < 4) {
        return fail(QStringLiteral("controller fixture did not exercise cache eviction"));
    }
    while (controller.snapshot().byteOffset > 0) {
        const qint64 oldOffset = controller.snapshot().byteOffset;
        controller.requestPreviousPage();
        if (!waitUntil([&controller, oldOffset]() {
                return !controller.loading() && controller.snapshot().byteOffset < oldOffset;
            })) {
            return fail(QStringLiteral("evicted previous page could not be re-read"));
        }
    }
    if (controller.snapshot().content != firstPage.content) {
        return fail(QStringLiteral("returning to the first page changed its content"));
    }

    controller.loadLocalFile(firstPath);
    controller.loadLocalFile(secondPath);
    if (!waitUntil([&controller, &secondPath]() {
            return !controller.loading() && controller.path() == secondPath
                && controller.snapshot().state == TextPreview::State::Ready;
        })) {
        return fail(QStringLiteral("replacement document did not finish"));
    }
    if (!controller.snapshot().content.contains(QStringLiteral("latest"))
        || controller.snapshot().content.contains(QStringLiteral("three"))
        || !controller.snapshot().classification.languageId.isEmpty()) {
        return fail(QStringLiteral("stale document content was published"));
    }

    const auto sourceBytes = std::make_shared<const QByteArray>(firstContent);
    controller.loadSource(QStringLiteral("archive://fixture/first.txt"),
        [sourceBytes](qint64 byteOffset, qint64 firstLine,
                      TextPreview::Encoding encoding,
                      const TextPreview::ReadOptions &readOptions) {
            return TextPreview::readBytesPage(
                sourceBytes->mid(byteOffset, readOptions.windowBytes + 4),
                sourceBytes->size(), QStringLiteral("first.txt"),
                QStringLiteral("text/plain"), byteOffset, firstLine,
                encoding, readOptions);
        },
        [sourceBytes]() { return TextPreview::countBytesLines(*sourceBytes); });
    if (!waitUntil([&controller]() { return !controller.loading(); })
        || controller.snapshot().state != TextPreview::State::Ready
        || controller.path() != QLatin1String("archive://fixture/first.txt")
        || !controller.snapshot().hasNextPage
        || !waitUntil([&controller]() { return controller.totalLineCount() == 12; })) {
        return fail(QStringLiteral("bounded range source did not load"));
    }
    const qint64 sourceFirstOffset = controller.snapshot().byteOffset;
    controller.requestNextPage();
    if (!waitUntil([&controller, sourceFirstOffset]() {
            return !controller.loading()
                && controller.snapshot().byteOffset > sourceFirstOffset;
        })) {
        return fail(QStringLiteral("bounded range source did not page"));
    }

    controller.cancel();
    if (controller.loading() || !controller.path().isEmpty()
        || controller.snapshot().state != TextPreview::State::Empty) {
        return fail(QStringLiteral("cancel did not clear the session"));
    }

    return 0;
}
