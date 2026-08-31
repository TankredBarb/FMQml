#include "HistoryManager.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

HistoryAction action(const QString &id)
{
    return {HistoryAction::Type::Rename,
            {QStringLiteral("source-%1").arg(id)},
            QStringLiteral("destination-%1").arg(id),
            {QStringLiteral("original-%1").arg(id)}};
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    HistoryManager history;
    if (history.canUndo() || history.canRedo() || history.undoCount() != 0 || history.redoCount() != 0) {
        return fail(QStringLiteral("history should start empty"));
    }

    history.recordAction(action(QStringLiteral("first")));
    history.recordAction(action(QStringLiteral("second")));
    if (!history.canUndo() || history.canRedo() || history.undoCount() != 2) {
        return fail(QStringLiteral("recordAction did not update the undo stack"));
    }

    const HistoryAction undone = history.takeUndo();
    if (undone.destination != QStringLiteral("destination-second")
        || history.undoCount() != 1 || history.redoCount() != 1 || !history.canRedo()) {
        return fail(QStringLiteral("takeUndo did not preserve LIFO ordering"));
    }

    const HistoryAction redone = history.takeRedo();
    if (redone.destination != QStringLiteral("destination-second")
        || history.undoCount() != 2 || history.redoCount() != 0) {
        return fail(QStringLiteral("takeRedo did not restore the action"));
    }

    history.takeUndo();
    history.recordAction(action(QStringLiteral("replacement")));
    if (history.canRedo() || history.redoCount() != 0
        || history.takeUndo().destination != QStringLiteral("destination-replacement")) {
        return fail(QStringLiteral("a new action should clear the redo branch"));
    }

    history.clear();
    for (int i = 0; i < 55; ++i) {
        history.recordAction(action(QString::number(i)));
    }
    if (history.undoCount() != 50) {
        return fail(QStringLiteral("history should retain at most 50 actions"));
    }
    for (int i = 54; i >= 5; --i) {
        if (history.takeUndo().destination != QStringLiteral("destination-%1").arg(i)) {
            return fail(QStringLiteral("history limit discarded or reordered the wrong action"));
        }
    }
    if (history.canUndo() || history.undoCount() != 0 || history.redoCount() != 50) {
        return fail(QStringLiteral("bounded undo traversal ended in an invalid state"));
    }

    history.clear();
    if (history.canUndo() || history.canRedo() || history.undoCount() != 0 || history.redoCount() != 0) {
        return fail(QStringLiteral("clear did not reset both history stacks"));
    }

    return 0;
}
