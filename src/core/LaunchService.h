#pragma once

#include <QString>

namespace LaunchService {

enum class LaunchErrorCode {
    None,
    NotLocalPath,
    FileNotFound,
    PermissionDenied,
    NoAssociation,
    UserCancelled,
    SecurityBlocked,
    InvalidExecutable,
    UnsupportedPlatform,
    UnknownFailure
};

struct LaunchResult {
    bool ok = false;
    LaunchErrorCode errorCode = LaunchErrorCode::None;
    QString title;
    QString message;
    QString details;
    bool showDialog = false;
};

LaunchResult openPath(const QString &path);

} // namespace LaunchService
