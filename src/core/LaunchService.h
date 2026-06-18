#pragma once

#include <QVariantMap>
#include <QString>

namespace LaunchService {

enum class LaunchErrorCode {
    None,
    NotLocalPath,
    FileNotFound,
    PermissionDenied,
    NotExecutable,
    NoAssociation,
    UserCancelled,
    SecurityBlocked,
    InvalidExecutable,
    RunnerUnavailable,
    RunnerStartFailed,
    DesktopLauncherUntrusted,
    WindowsAppRequiresExplicitRunner,
    UnsupportedPlatform,
    UnknownFailure
};

enum class LaunchCategory {
    Unsupported,
    Document,
    NativeExecutableElf,
    NativeExecutableScript,
    NativeExecutableAppImage,
    DesktopLauncherTrusted,
    DesktopLauncherBlocked,
    WindowsApplication,
    NonExecutableScript,
    UnknownExecutable
};

struct LaunchResult {
    bool ok = false;
    LaunchErrorCode errorCode = LaunchErrorCode::None;
    QString title;
    QString message;
    QString details;
    bool showDialog = false;
};

struct LaunchCapabilities {
    bool canOpen = false;
    bool canOpenWithWine = false;
    bool canOpenWithSteamProton = false;
    bool isLocal = false;
    bool isWindowsApplication = false;
    QString openBlockedReason;
    LaunchCategory category = LaunchCategory::Unsupported;
};

LaunchResult openPath(const QString &path);
LaunchResult openWithWine(const QString &path);
LaunchResult openWithSteamProton(const QString &path);
LaunchResult openWithSteamProton(const QString &path,
                                 const QString &runtimeId,
                                 bool enableVkBasalt,
                                 bool captureLog,
                                 bool clearXModifiers);
LaunchCapabilities launchCapabilities(const QString &path);
QVariantMap launchCapabilitiesMap(const QString &path);
QVariantMap steamProtonLaunchOptions(const QString &path);
void saveSteamProtonLaunchSettings(const QString &runtimeId,
                                   bool enableVkBasalt,
                                   bool captureLog,
                                   bool clearXModifiers);

} // namespace LaunchService
