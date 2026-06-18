#include "LaunchService.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QTimer>
#include <QUrl>
#include <QWindow>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#ifndef ERROR_NO_ASSOCIATION
#define ERROR_NO_ASSOCIATION 1155L
#endif
#endif

namespace {

QString explicitScheme(const QString &path)
{
    const QString value = path.trimmed();
    const qsizetype index = value.indexOf(QStringLiteral("://"));
    if (index <= 0) {
        return {};
    }

    const QString scheme = value.left(index).toLower();
    const QChar first = scheme.at(0);
    if (!first.isLetter()) {
        return {};
    }
    for (const QChar ch : scheme) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('+') && ch != QLatin1Char('.') && ch != QLatin1Char('-')) {
            return {};
        }
    }
    return scheme;
}

QString localPathFromInput(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (explicitScheme(trimmed) == QLatin1String("file")) {
        return QUrl(trimmed).toLocalFile();
    }
    return trimmed;
}

LaunchService::LaunchResult failure(LaunchService::LaunchErrorCode errorCode,
                                    const QString &title,
                                    const QString &message,
                                    const QString &details = {},
                                    bool showDialog = false)
{
    return {false, errorCode, title, message, details, showDialog};
}

#ifdef Q_OS_WIN
struct ParentWindowInfo {
    QPointer<QWindow> window;
    HWND hwnd = nullptr;
};

ParentWindowInfo parentWindowInfo()
{
    QWindow *window = QGuiApplication::focusWindow();
    if (!window) {
        const QWindowList windows = QGuiApplication::topLevelWindows();
        for (QWindow *candidate : windows) {
            if (candidate && candidate->isVisible()) {
                window = candidate;
                break;
            }
        }
    }
    return {window, window ? reinterpret_cast<HWND>(window->winId()) : nullptr};
}

void reactivateParentWindow(const ParentWindowInfo &parent)
{
    if (!parent.window && !parent.hwnd) {
        return;
    }

    const QPointer<QWindow> window = parent.window;
    const HWND hwnd = parent.hwnd;
    QTimer::singleShot(0, [window, hwnd]() {
        if (hwnd && IsWindow(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }
        if (window) {
            window->raise();
            window->requestActivate();
        }
    });
}

LaunchService::LaunchErrorCode windowsErrorCode(unsigned long error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return LaunchService::LaunchErrorCode::FileNotFound;
    case ERROR_ACCESS_DENIED:
        return LaunchService::LaunchErrorCode::PermissionDenied;
    case ERROR_CANCELLED:
        return LaunchService::LaunchErrorCode::UserCancelled;
    case ERROR_NO_ASSOCIATION:
        return LaunchService::LaunchErrorCode::NoAssociation;
    case ERROR_ELEVATION_REQUIRED:
        return LaunchService::LaunchErrorCode::SecurityBlocked;
    case ERROR_BAD_EXE_FORMAT:
        return LaunchService::LaunchErrorCode::InvalidExecutable;
    default:
        return LaunchService::LaunchErrorCode::UnknownFailure;
    }
}

QString windowsErrorTitle(LaunchService::LaunchErrorCode errorCode)
{
    switch (errorCode) {
    case LaunchService::LaunchErrorCode::FileNotFound:
        return QStringLiteral("File not found");
    case LaunchService::LaunchErrorCode::PermissionDenied:
        return QStringLiteral("Launch was denied");
    case LaunchService::LaunchErrorCode::UserCancelled:
        return QStringLiteral("Launch cancelled");
    case LaunchService::LaunchErrorCode::NoAssociation:
        return QStringLiteral("No app is associated with this file");
    case LaunchService::LaunchErrorCode::SecurityBlocked:
        return QStringLiteral("Windows blocked the launch");
    case LaunchService::LaunchErrorCode::InvalidExecutable:
        return QStringLiteral("Invalid executable");
    default:
        return QStringLiteral("Could not open file");
    }
}

QString windowsErrorMessage(LaunchService::LaunchErrorCode errorCode, const QString &path)
{
    switch (errorCode) {
    case LaunchService::LaunchErrorCode::FileNotFound:
        return QStringLiteral("The file is no longer available: %1").arg(QDir::toNativeSeparators(path));
    case LaunchService::LaunchErrorCode::PermissionDenied:
        return QStringLiteral("Windows denied access while opening: %1").arg(QDir::toNativeSeparators(path));
    case LaunchService::LaunchErrorCode::UserCancelled:
        return QStringLiteral("The launch was cancelled.");
    case LaunchService::LaunchErrorCode::NoAssociation:
        return QStringLiteral("Choose a default app in Windows Settings, then try opening this file again.");
    case LaunchService::LaunchErrorCode::SecurityBlocked:
        return QStringLiteral("Windows security policy blocked this launch.");
    case LaunchService::LaunchErrorCode::InvalidExecutable:
        return QStringLiteral("Windows could not run this executable file.");
    default:
        return QStringLiteral("Windows could not open: %1").arg(QDir::toNativeSeparators(path));
    }
}

LaunchService::LaunchResult openPathWithWindowsShell(const QString &path)
{
    const QFileInfo fileInfo(path);
    const QString nativePath = QDir::toNativeSeparators(fileInfo.absoluteFilePath());
    const QString nativeDirectory = QDir::toNativeSeparators(fileInfo.absolutePath());
    const std::wstring file = nativePath.toStdWString();
    const std::wstring directory = nativeDirectory.toStdWString();
    const ParentWindowInfo parent = parentWindowInfo();

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.hwnd = parent.hwnd;
    info.lpVerb = L"open";
    info.lpFile = file.c_str();
    info.lpDirectory = directory.empty() ? nullptr : directory.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&info)) {
        reactivateParentWindow(parent);
        return {true, LaunchService::LaunchErrorCode::None};
    }

    const unsigned long rawError = GetLastError();
    const LaunchService::LaunchErrorCode errorCode = windowsErrorCode(rawError);
    reactivateParentWindow(parent);
    return failure(errorCode,
                   windowsErrorTitle(errorCode),
                   windowsErrorMessage(errorCode, path),
                   QStringLiteral("ShellExecuteExW failed with error %1.").arg(rawError),
                   errorCode != LaunchService::LaunchErrorCode::UserCancelled);
}
#endif

} // namespace

namespace LaunchService {

LaunchResult openPath(const QString &path)
{
    const QString scheme = explicitScheme(path);
    if (!scheme.isEmpty() && scheme != QLatin1String("file")) {
        return failure(LaunchErrorCode::NotLocalPath,
                       QStringLiteral("Cannot open non-local file"),
                       QStringLiteral("This location does not support direct file launch."));
    }

    const QString localPath = localPathFromInput(path);
    if (localPath.isEmpty()) {
        return failure(LaunchErrorCode::FileNotFound,
                       QStringLiteral("File not found"),
                       QStringLiteral("The selected file is no longer available."));
    }

#ifdef Q_OS_WIN
    return openPathWithWindowsShell(localPath);
#else
    const bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(localPath));
    if (ok) {
        return {true, LaunchErrorCode::None};
    }
    return failure(LaunchErrorCode::UnsupportedPlatform,
                   QStringLiteral("Could not open file"),
                   QStringLiteral("This platform launch path is not implemented yet."));
#endif
}

} // namespace LaunchService
