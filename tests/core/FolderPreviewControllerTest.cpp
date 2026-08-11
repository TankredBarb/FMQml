#include "FolderPreviewController.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

namespace {
bool waitForState(FolderPreviewController &controller, const QString &state)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000) {
        QCoreApplication::processEvents();
        if (controller.snapshot().value(QStringLiteral("state")).toString() == state) return true;
        QThread::msleep(1);
    }
    return false;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir root;
    if (!root.isValid()) return 1;

    for (int index = 0; index < 20; ++index) {
        QFile file(root.filePath(QStringLiteral("file-%1.txt").arg(index, 2, 10, QLatin1Char('0'))));
        if (!file.open(QIODevice::WriteOnly)) return 2;
    }
    QFile hidden(root.filePath(QStringLiteral(".hidden.txt")));
    if (!hidden.open(QIODevice::WriteOnly)) return 3;
    hidden.close();
    if (!QDir(root.path()).mkdir(QStringLiteral("aaa-folder"))) return 4;

    FolderPreviewController controller;
    controller.request(root.path(), false, 6);
    if (!waitForState(controller, QStringLiteral("ready"))) return 5;
    const QVariantMap bounded = controller.snapshot();
    if (bounded.value(QStringLiteral("entries")).toList().size() != 6
        || !bounded.value(QStringLiteral("hasMore")).toBool()
        || bounded.value(QStringLiteral("displayedCount")).toInt() != 6) return 6;

    controller.requestWithSort(root.path(), false, 6, 0, int(Qt::DescendingOrder), false);
    if (!waitForState(controller, QStringLiteral("ready"))) return 7;
    QVariantList sortedEntries = controller.snapshot().value(QStringLiteral("entries")).toList();
    if (sortedEntries.isEmpty()
        || sortedEntries.first().toMap().value(QStringLiteral("name")).toString() != QLatin1String("aaa-folder")) return 8;

    controller.requestWithSort(root.path(), false, 6, 0, int(Qt::DescendingOrder), true);
    if (!waitForState(controller, QStringLiteral("ready"))) return 9;
    sortedEntries = controller.snapshot().value(QStringLiteral("entries")).toList();
    if (sortedEntries.isEmpty()
        || sortedEntries.first().toMap().value(QStringLiteral("name")).toString() != QLatin1String("file-19.txt")) return 10;

    QTemporaryDir empty;
    if (!empty.isValid()) return 11;
    controller.request(root.path(), true, 9);
    controller.request(empty.path(), false, 9);
    if (!waitForState(controller, QStringLiteral("empty"))) return 12;
    if (controller.snapshot().value(QStringLiteral("path")).toString() != empty.path()) return 13;

    controller.request(QStringLiteral("telegram://chats/1"), false, 9);
    if (controller.snapshot().value(QStringLiteral("state")).toString() != QLatin1String("unavailable")) return 14;
    return 0;
}
