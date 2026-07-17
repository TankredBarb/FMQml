#include "FilePanelInteractionPolicy.h"

#include <QCoreApplication>

#include <iostream>

namespace {

bool fail(const char *message)
{
    std::cerr << message << '\n';
    return false;
}

struct FakeAsyncModel {
    QString directory = QStringLiteral("/target");
    QStringList paths;
    quint64 navigationGeneration = 4;

    bool contains(const QString &path) const
    {
        return paths.contains(path);
    }
};

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    const QStringList paths{
        QStringLiteral("/root/a"),
        QStringLiteral("/root/b"),
        QStringLiteral("/root/c"),
        QStringLiteral("/root/d")
    };

    ok &= FilePanelInteractionPolicy::nearestSurvivor(
              paths, {QStringLiteral("/root/b")}, 1) == QStringLiteral("/root/c")
        || fail("middle delete did not choose the next survivor");
    ok &= FilePanelInteractionPolicy::nearestSurvivor(
              paths, {QStringLiteral("/root/d")}, 3) == QStringLiteral("/root/c")
        || fail("last delete did not choose the previous survivor");
    ok &= FilePanelInteractionPolicy::nearestSurvivor(
              paths, paths, 0).isEmpty()
        || fail("delete-all still produced a survivor");
    ok &= FilePanelInteractionPolicy::nearestSurvivor(
              paths, {QStringLiteral("/root/b"), QStringLiteral("/root/d")}, 1)
            == QStringLiteral("/root/c")
        || fail("non-adjacent delete chose a removed or previous item");
    ok &= FilePanelInteractionPolicy::currentAfterRemoval(
              paths, {QStringLiteral("/root/b")}, QStringLiteral("/root/a"), 1)
            == QStringLiteral("/root/a")
        || fail("surviving current item was not preserved");

    FakeAsyncModel model;
    const QString resultPath = QStringLiteral("/target/copied.txt");
    FilePanelInteractionPolicy::AttentionGuard guard{
        QStringLiteral("/target"), model.directory,
        4, model.navigationGeneration,
        9, 9,
        model.contains(resultPath)
    };
    ok &= !FilePanelInteractionPolicy::canApplyAttention(guard)
        || fail("attention applied before delayed model convergence");

    model.paths.append(resultPath);
    guard.modelConverged = model.contains(resultPath);
    ok &= FilePanelInteractionPolicy::canApplyAttention(guard)
        || fail("attention did not apply after model convergence");

    guard.currentInteractionRevision = 10;
    ok &= !FilePanelInteractionPolicy::canApplyAttention(guard)
        || fail("stale attention overrode newer user interaction");

    guard.currentInteractionRevision = 9;
    model.navigationGeneration = 5;
    guard.currentNavigationGeneration = model.navigationGeneration;
    ok &= !FilePanelInteractionPolicy::canApplyAttention(guard)
        || fail("stale attention crossed a navigation generation");

    return ok ? 0 : 1;
}
