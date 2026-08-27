#include "FileSearchController.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>

#include <cstdio>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("match\n") == 6;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!expect(directory.isValid()
                    && writeFile(directory.filePath(QStringLiteral("z-match.txt")))
                    && writeFile(directory.filePath(QStringLiteral("a-match.txt")))
                    && writeFile(directory.filePath(QStringLiteral("m-match.txt"))),
                "Controller test fixtures must be created")) return 1;

    FileSearchController controller;
    controller.setSortMode(FileSearchModel::NameSort);
    controller.setHoldResultUpdates(true);
    controller.search(directory.path(), QStringLiteral("match"));

    QElapsedTimer timeout;
    timeout.start();
    while (controller.state() != FileSearchController::State::Finished && timeout.elapsed() < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(1);
    }
    if (!expect(controller.state() == FileSearchController::State::Finished,
                "Held search must finish")) return 1;
    if (!expect(controller.resultsModel()->count() == 0,
                "Held result batches must not mutate the visible model")) return 1;

    controller.setHoldResultUpdates(false);
    if (!expect(controller.resultsModel()->count() == 3,
                "Releasing hold must publish every pending result")) return 1;
    if (!expect(controller.resultsModel()->pathAt(0).endsWith(QStringLiteral("a-match.txt"))
                    && controller.resultsModel()->pathAt(1).endsWith(QStringLiteral("m-match.txt"))
                    && controller.resultsModel()->pathAt(2).endsWith(QStringLiteral("z-match.txt")),
                "Pending results must be included in the final full-list sort")) return 1;
    return 0;
}
