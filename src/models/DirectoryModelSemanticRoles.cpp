#include "DirectoryModelSemanticRoles.h"
QVariant directoryModelSemanticRoleValue(const FileEntry &entry, int role)
{
    switch (role) {
    case DirectoryModel::SpecialActionRole: return static_cast<int>(entry.specialAction);
    case DirectoryModel::OverlayIconNameRole: return entry.overlayIconName;
    case DirectoryModel::IconRecolorAllowedRole: return entry.iconRecolorAllowed;
    default: return {};
    }
}
