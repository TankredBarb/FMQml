#include "FilePanelController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

#if defined(Q_OS_LINUX)
#  include <QDBusConnection>
#  include <QDBusInterface>
#  include <QDBusMessage>
#  include <QDBusObjectPath>
#  include <QDBusUnixFileDescriptor>
#elif defined(Q_OS_WIN)
#  include <windows.h>
#  include <shlobj.h>
#endif

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <functional>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <winioctl.h>
#endif

#ifdef Q_OS_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include "../core/ArchiveSupport.h"
#include "../core/ArchiveFileProvider.h"
#include "../core/FileAccessResolver.h"
#include "../core/IsoSupport.h"
#include "../core/LaunchService.h"
#include "../core/OpenWithService.h"
#include "../core/LinuxAdminBroker.h"
#include "../core/LocalFileProvider.h"
#include "../core/MetadataExtractor.h"
#include "../core/TerminalLauncher.h"
#include "../core/WallpaperSetter.h"
#include "../core/DriveUtils.h"
#include "../core/CleanupSubsystem.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileError.h"
#include "../core/VolumeMonitor.h"
#include "FavoritesController.h"
#include "../platform/openwith/LinuxOpenWithBackend.h"

#include "FilePanelControllerInternal.h"

using namespace FilePanelControllerInternal;

namespace {

bool showSystemApplicationChooser(const QString &path,
                                  QObject *responseReceiver,
                                  QString *errorMessage)
{
    const QFileInfo file(path);
    if (!file.exists() || !file.isFile()) {
        if (errorMessage) *errorMessage = QStringLiteral("The selected file is no longer available.");
        return false;
    }

#if defined(Q_OS_LINUX)
    qInfo().noquote() << "[OpenWithChooser] Request for" << file.absoluteFilePath();
    qInfo() << "[OpenWithChooser] Session bus connected:"
            << QDBusConnection::sessionBus().isConnected();
    QDBusInterface portal(QStringLiteral("org.freedesktop.portal.Desktop"),
                          QStringLiteral("/org/freedesktop/portal/desktop"),
                          QStringLiteral("org.freedesktop.portal.OpenURI"),
                          QDBusConnection::sessionBus());
    qInfo() << "[OpenWithChooser] Portal interface valid:" << portal.isValid()
            << "last error:" << portal.lastError().name() << portal.lastError().message();
    if (!portal.isValid()) {
        if (errorMessage) *errorMessage = QStringLiteral("The system application chooser is not available.");
        return false;
    }

    QFile sourceFile(file.absoluteFilePath());
    if (!sourceFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[OpenWithChooser] Could not open file for portal:"
                   << sourceFile.errorString();
        if (errorMessage) *errorMessage = sourceFile.errorString();
        return false;
    }

    QVariantMap options;
    options.insert(QStringLiteral("ask"), true);
    const QDBusUnixFileDescriptor descriptor(sourceFile.handle());
    qInfo().noquote() << "[OpenWithChooser] Calling OpenFile with ask=true for"
                      << file.absoluteFilePath();
    const QDBusMessage reply = portal.call(QStringLiteral("OpenFile"),
                                           QString(),
                                           QVariant::fromValue(descriptor),
                                           options);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        qWarning() << "[OpenWithChooser] OpenFile failed:"
                   << reply.errorName() << reply.errorMessage();
        if (errorMessage) *errorMessage = reply.errorMessage();
        return false;
    }

    const QString requestPath = reply.arguments().constFirst().value<QDBusObjectPath>().path();
    qInfo().noquote() << "[OpenWithChooser] Portal request created:" << requestPath;
    const bool responseConnected = QDBusConnection::sessionBus().connect(
        QStringLiteral("org.freedesktop.portal.Desktop"),
        requestPath,
        QStringLiteral("org.freedesktop.portal.Request"),
        QStringLiteral("Response"),
        responseReceiver,
        SLOT(onSystemApplicationChooserResponse(uint,QVariantMap)));
    qInfo() << "[OpenWithChooser] Response signal connected:" << responseConnected;
    return true;
#elif defined(Q_OS_WIN)
    const std::wstring nativePath = QDir::toNativeSeparators(file.absoluteFilePath()).toStdWString();
    OPENASINFO info{};
    info.pcszFile = nativePath.c_str();
    info.oaifInFlags = OAIF_EXEC | OAIF_HIDE_REGISTRATION;
    if (FAILED(SHOpenWithDialog(nullptr, &info))) {
        if (errorMessage) *errorMessage = QStringLiteral("Windows could not show the application chooser.");
        return false;
    }
    return true;
#else
    Q_UNUSED(path)
    if (errorMessage) *errorMessage = QStringLiteral("The system application chooser is not supported on this platform.");
    return false;
#endif
}

} // namespace

QVariantMap FilePanelController::launchCapabilitiesForPath(const QString &path) const
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    return LaunchService::launchCapabilitiesMap(path);
}

bool FilePanelController::openWithAvailableForPath(const QString &path) const
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        return false;
    }
    const OpenWithTarget target = openWithService().targetInfo(path);
    return target.isLocal && !target.contentTypeKey.isEmpty();
}

QVariantList FilePanelController::openWithCandidatesForPath(const QString &path) const
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    QVariantList result;
    for (const OpenWithCandidate &candidate : openWithService().candidatesForPath(path)) {
        result.append(openWithCandidateMap(candidate));
    }
    return result;
}

bool FilePanelController::openWithAvailableForPaths(const QStringList &paths) const
{
    if (paths.isEmpty()) return false;
    QString contentTypeKey;
    for (const QString &path : paths) {
        if (!openWithAvailableForPath(path)) return false;
        const OpenWithTarget target = openWithService().targetInfo(path);
        if (contentTypeKey.isEmpty()) contentTypeKey = target.contentTypeKey;
        else if (contentTypeKey != target.contentTypeKey) return false;
    }
    return true;
}

QVariantList FilePanelController::openWithCandidatesForPaths(const QStringList &paths) const
{
    return openWithAvailableForPaths(paths) ? openWithCandidatesForPath(paths.first()) : QVariantList{};
}

void FilePanelController::openPathsWithApplication(const QStringList &paths, const QString &candidateId)
{
    if (!openWithAvailableForPaths(paths)) return;
    const OpenWithResult result = openWithService().openWithMany(paths, candidateId);
    if (!result.ok) {
        setStatusMessage(result.message.isEmpty() ? QStringLiteral("Could not open files.") : result.message);
        setLastError(openWithErrorInfo(result, paths.first()));
    } else {
        setLastError({});
    }
}

void FilePanelController::openPathWithSystemApplicationChooser(const QString &path)
{
    qInfo().noquote() << "[OpenWithChooser] Controller invoked for" << path;
    if (!openWithAvailableForPath(path)) return;

    QString errorMessage;
    if (!showSystemApplicationChooser(path, this, &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty()
                             ? QStringLiteral("Could not show the application chooser.")
                             : errorMessage);
    }
}

void FilePanelController::onSystemApplicationChooserResponse(uint response, const QVariantMap &results)
{
    qInfo() << "[OpenWithChooser] Portal response:" << response << "results:" << results;
}

bool FilePanelController::setOpenWithPreferredCandidate(const QString &path, const QString &candidateId)
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        return false;
    }
    return openWithService().setPreferredCandidate(path, candidateId);
}

void FilePanelController::clearOpenWithPreferredCandidate(const QString &path)
{
    openWithService().clearPreferredCandidate(path);
}

void FilePanelController::openPathWithSteamProton(const QString &path)
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        return;
    }

    const LaunchService::LaunchResult result = LaunchService::openWithSteamProton(path);
    if (!result.ok) {
        setStatusMessage(result.message.isEmpty() ? QStringLiteral("Could not open file with Steam Proton.") : result.message);
        setLastError(launchErrorInfo(result, path));
    } else {
        setLastError({});
    }
}

QVariantMap FilePanelController::steamProtonLaunchOptionsForPath(const QString &path) const
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        QVariantMap options;
        options.insert(QStringLiteral("available"), false);
        options.insert(QStringLiteral("errorTitle"), QStringLiteral("Steam Proton launch is not available"));
        options.insert(QStringLiteral("errorMessage"), QStringLiteral("This location does not support direct file launch."));
        return options;
    }
    return LaunchService::steamProtonLaunchOptions(path);
}

QVariantMap FilePanelController::launchPathWithSteamProton(const QString &path,
                                                           const QString &runtimeId,
                                                           bool enableVkBasalt,
                                                           bool captureLog,
                                                           bool clearXModifiers)
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        QVariantMap result;
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("title"), QStringLiteral("Steam Proton launch is not available"));
        result.insert(QStringLiteral("message"), QStringLiteral("This location does not support direct file launch."));
        return result;
    }

    LaunchService::saveSteamProtonLaunchSettings(runtimeId, enableVkBasalt, captureLog, clearXModifiers);
    const LaunchService::LaunchResult result = LaunchService::openWithSteamProton(path,
                                                                                  runtimeId,
                                                                                  enableVkBasalt,
                                                                                  captureLog,
                                                                                  clearXModifiers);
    if (!result.ok) {
        setStatusMessage(result.message.isEmpty() ? QStringLiteral("Could not open file with Steam Proton.") : result.message);
        setLastError(launchErrorInfo(result, path));
    } else {
        setLastError({});
    }
    return launchResultMap(result, path);
}

void FilePanelController::revealInFileManager(int row)
{
    if (isVirtualRoot()) return;
    const QString path = m_directoryModel.pathAt(row);
    if (path.isEmpty()) {
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(
        ArchiveSupport::isArchivePath(path) ? ArchiveSupport::physicalArchivePath(path) : path);

#if defined(Q_OS_WIN)
    const QString arg = QStringLiteral("/select,\"%1\"").arg(nativePath);
    QProcess::startDetached(QStringLiteral("explorer.exe"), {arg});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), path});
#else
    const QString parent = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::archiveParentPath(path)
        : m_fileProvider->parentPath(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(parent));
#endif
}

void FilePanelController::openInTerminal()
{
    if (isVirtualRoot()) return;
    const QString path = ArchiveSupport::isArchivePath(currentPath())
        ? ArchiveSupport::physicalArchivePath(currentPath())
        : currentPath();
    TerminalLauncher::openTerminalAt(path);
}

bool FilePanelController::canSetWallpaperPath(const QString &path) const
{
    if (isProviderUriPath(path)) {
        return false;
    }
    return WallpaperSetter::canSetWallpaperForPath(path);
}

void FilePanelController::setAsWallpaper(int row)
{
    if (isVirtualRoot()) {
        return;
    }

    const QString path = m_directoryModel.pathAt(row);
    setPathAsWallpaper(path);
}

void FilePanelController::setPathAsWallpaper(const QString &path)
{
    if (isVirtualRoot()) {
        return;
    }

    if (path.isEmpty() || isProviderUriPath(path)) {
        setStatusMessage(QStringLiteral("Wallpaper can only be set from a local image file."));
        return;
    }

    QString errorMessage;
    if (!WallpaperSetter::setWallpaper(path, &errorMessage)) {
        setStatusMessage(errorMessage.isEmpty()
            ? QStringLiteral("Failed to set wallpaper.")
            : errorMessage);
        return;
    }

    setStatusMessage(QStringLiteral("Wallpaper updated"));
}
