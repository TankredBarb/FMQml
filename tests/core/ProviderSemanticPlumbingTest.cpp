#include "models/DirectoryModelSemanticRoles.h"
#include "controllers/FilePanelLoadMorePolicy.h"
#include <QCoreApplication>
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    FileEntry entry;
    entry.specialAction = FileEntrySpecialAction::LoadMore;
    entry.overlayIconName = QStringLiteral("provider-badge");
    entry.iconRecolorAllowed = false;
    if (directoryModelSemanticRoleValue(entry, DirectoryModel::SpecialActionRole).toInt() != 1) return 1;
    if (directoryModelSemanticRoleValue(entry, DirectoryModel::OverlayIconNameRole).toString() != QStringLiteral("provider-badge")) return 2;
    if (directoryModelSemanticRoleValue(entry, DirectoryModel::IconRecolorAllowedRole).toBool()) return 3;
    int calls = 0;
    int dispatchedRow = -1;
    const auto dispatch = [&](int row) { ++calls; dispatchedRow = row; };
    if (!dispatchLoadMoreRequest(4, false, dispatch) || calls != 1 || dispatchedRow != 4) return 4;
    if (dispatchLoadMoreRequest(4, true, dispatch) || calls != 1) return 5;
    if (dispatchLoadMoreRequest(-1, false, dispatch) || calls != 1) return 6;
    return 0;
}
