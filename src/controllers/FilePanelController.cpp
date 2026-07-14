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

namespace FilePanelControllerInternal {
QString materializeAdminReadOnlyLaunchFile(const QString &sourcePath, QString *error)
{
#ifdef Q_OS_LINUX
    if (LinuxAdminBroker::activeSessionNonce().isEmpty()) {
        return {};
    }
    QString leaseId;
    const QString directory = CleanupSubsystem::instance().allocateStagingDirectory(
        CleanupArtifactKind::RemotePreview,
        StagingLocationPolicy::defaultCleanupRoot(),
        QUuid::createUuid().toString(QUuid::WithoutBraces),
        &leaseId);
    if (directory.isEmpty() || leaseId.isEmpty()) {
        if (error) *error = QStringLiteral("Cannot create administrator read-only staging folder.");
        return {};
    }
    QString name = QFileInfo(sourcePath).fileName();
    name.replace(QRegularExpression(QStringLiteral(R"([/\\\x00-\x1F])")), QStringLiteral("_"));
    if (name.isEmpty()) name = QStringLiteral("admin-file");
    const QString destination = QDir(directory).filePath(name.left(180));
    LocalFileProvider provider;
    QString copyError;
    if (!provider.copyToLocalFileForPreview(sourcePath, destination, {}, &copyError)) {
        CleanupSubsystem::instance().scheduleDeleteOnFailure(leaseId);
        if (error) *error = copyError;
        return {};
    }
    QFile::setPermissions(destination, QFileDevice::ReadOwner);
    // Keep the active lease while the external consumer or archive browser may
    // still use the file.  Startup cleanup removes leases left by dead runs.
    return destination;
#else
    Q_UNUSED(sourcePath)
    Q_UNUSED(error)
    return {};
#endif
}

bool filePanelNavTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_NAV_TRACE");
    return enabled;
}

void traceFilePanelNav(const char *stage, const QString &path, const QString &detail)
{
    if (!filePanelNavTraceEnabled()) {
        return;
    }

    qInfo().noquote() << "[FM_NAV][panel]" << stage
                      << "path=" << QDir::toNativeSeparators(path)
                      << detail;
}

QString normalizedLoadMoreComparablePath(const QString &path)
{
    QString value = QDir::fromNativeSeparators(path).trimmed();
    while (value.size() > 1 && value.endsWith(QLatin1Char('/'))) {
        value.chop(1);
    }
    return value;
}

bool providerPathSupportsLoadMore(const QString &path)
{
    return path.startsWith(QStringLiteral("instagram://"), Qt::CaseInsensitive)
        || path.startsWith(QStringLiteral("telegram://"), Qt::CaseInsensitive);
}

bool isLoadMorePathForCurrentProviderPath(const QString &currentPath, const QString &targetPath)
{
    const QString current = normalizedLoadMoreComparablePath(currentPath);
    const QString target = normalizedLoadMoreComparablePath(targetPath);
    if (current.isEmpty() || !providerPathSupportsLoadMore(current)) {
        return false;
    }

    constexpr QLatin1StringView suffix("/__load_more__");
    if (!target.endsWith(suffix, Qt::CaseInsensitive)) {
        return false;
    }

    const QString parent = normalizedLoadMoreComparablePath(target.left(target.size() - suffix.size()));
    return parent.compare(current, Qt::CaseInsensitive) == 0;
}

QString launchErrorCodeName(LaunchService::LaunchErrorCode code)
{
    switch (code) {
    case LaunchService::LaunchErrorCode::None:
        return QStringLiteral("none");
    case LaunchService::LaunchErrorCode::NotLocalPath:
        return QStringLiteral("notLocalPath");
    case LaunchService::LaunchErrorCode::FileNotFound:
        return QStringLiteral("pathNotFound");
    case LaunchService::LaunchErrorCode::PermissionDenied:
        return QStringLiteral("accessDenied");
    case LaunchService::LaunchErrorCode::NotExecutable:
        return QStringLiteral("notExecutable");
    case LaunchService::LaunchErrorCode::NoAssociation:
        return QStringLiteral("noAssociation");
    case LaunchService::LaunchErrorCode::UserCancelled:
        return QStringLiteral("userCancelled");
    case LaunchService::LaunchErrorCode::SecurityBlocked:
        return QStringLiteral("securityBlocked");
    case LaunchService::LaunchErrorCode::InvalidExecutable:
        return QStringLiteral("invalidExecutable");
    case LaunchService::LaunchErrorCode::RunnerUnavailable:
        return QStringLiteral("runnerUnavailable");
    case LaunchService::LaunchErrorCode::RunnerStartFailed:
        return QStringLiteral("runnerStartFailed");
    case LaunchService::LaunchErrorCode::DesktopLauncherUntrusted:
        return QStringLiteral("desktopLauncherUntrusted");
    case LaunchService::LaunchErrorCode::WindowsAppRequiresExplicitRunner:
        return QStringLiteral("windowsAppRequiresExplicitRunner");
    case LaunchService::LaunchErrorCode::UnsupportedPlatform:
        return QStringLiteral("unsupportedOperation");
    case LaunchService::LaunchErrorCode::UnknownFailure:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QVariantMap launchErrorInfo(const LaunchService::LaunchResult &result, const QString &path)
{
    QVariantMap map;
    map.insert(QStringLiteral("code"), launchErrorCodeName(result.errorCode));
    map.insert(QStringLiteral("title"), result.title.isEmpty() ? QStringLiteral("Launch failed") : result.title);
    map.insert(QStringLiteral("message"), result.message.isEmpty() ? QStringLiteral("Could not open file.") : result.message);
    map.insert(QStringLiteral("path"), QDir::toNativeSeparators(path));
    map.insert(QStringLiteral("operation"), QStringLiteral("open"));
    map.insert(QStringLiteral("actions"), QStringList{QStringLiteral("copyPath")});
    map.insert(QStringLiteral("showDialog"), result.showDialog);
    if (!result.details.isEmpty()) {
        map.insert(QStringLiteral("details"), result.details);
    }
    return map;
}

QVariantMap launchResultMap(const LaunchService::LaunchResult &result, const QString &path)
{
    QVariantMap map;
    map.insert(QStringLiteral("ok"), result.ok);
    map.insert(QStringLiteral("code"), launchErrorCodeName(result.errorCode));
    map.insert(QStringLiteral("title"), result.title);
    map.insert(QStringLiteral("message"), result.message);
    map.insert(QStringLiteral("details"), result.details);
    map.insert(QStringLiteral("path"), QDir::toNativeSeparators(path));
    return map;
}

OpenWithService &openWithService()
{
#if defined(Q_OS_LINUX)
    static LinuxOpenWithBackend backend;
    static OpenWithService service(&backend);
    return service;
#else
    static OpenWithService service;
    return service;
#endif
}

QVariantMap openWithCandidateMap(const OpenWithCandidate &candidate)
{
    QString kind = QStringLiteral("application");
    if (candidate.kind == OpenWithCandidateKind::Wine) kind = QStringLiteral("wine");
    else if (candidate.kind == OpenWithCandidateKind::Proton) kind = QStringLiteral("proton");
    return {{QStringLiteral("id"), candidate.id},
            {QStringLiteral("name"), candidate.displayName},
            {QStringLiteral("iconName"), candidate.iconName},
            {QStringLiteral("kind"), kind},
            {QStringLiteral("recommended"), candidate.recommended},
            {QStringLiteral("systemDefault"), candidate.systemDefault},
            {QStringLiteral("fmDefault"), candidate.fmDefault},
            {QStringLiteral("supportsMultipleFiles"), candidate.supportsMultipleFiles},
            {QStringLiteral("available"), candidate.available},
            {QStringLiteral("unavailableReason"), candidate.unavailableReason}};
}

QVariantMap openWithErrorInfo(const OpenWithResult &result, const QString &path)
{
    QVariantMap map;
    map.insert(QStringLiteral("code"), launchErrorCodeName(result.errorCode));
    map.insert(QStringLiteral("title"), result.title.isEmpty() ? QStringLiteral("Open With failed") : result.title);
    map.insert(QStringLiteral("message"), result.message.isEmpty() ? QStringLiteral("Could not open file.") : result.message);
    map.insert(QStringLiteral("path"), QDir::toNativeSeparators(path));
    map.insert(QStringLiteral("operation"), QStringLiteral("open"));
    map.insert(QStringLiteral("actions"), QStringList{QStringLiteral("copyPath")});
    map.insert(QStringLiteral("showDialog"), result.showDialog);
    if (!result.details.isEmpty()) map.insert(QStringLiteral("details"), result.details);
    return map;
}

bool samePanelFilesystemPath(const QString &left, const QString &right)
{
    const QString normalizedLeft = QDir::cleanPath(QDir::fromNativeSeparators(left));
    const QString normalizedRight = QDir::cleanPath(QDir::fromNativeSeparators(right));
#ifdef Q_OS_WIN
    return normalizedLeft.compare(normalizedRight, Qt::CaseInsensitive) == 0;
#else
    return normalizedLeft == normalizedRight;
#endif
}

QString uriSchemeForPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return {};
    }

    const QString scheme = trimmed.left(separatorIndex);
    if (!scheme.at(0).isLetter()) {
        return {};
    }
    for (const QChar ch : scheme) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('+') && ch != QLatin1Char('.') && ch != QLatin1Char('-')) {
            return {};
        }
    }
    return scheme.toLower();
}

bool isProviderUriPath(const QString &path)
{
    const QString scheme = uriSchemeForPath(path);
    return !scheme.isEmpty()
        && scheme != QStringLiteral("file")
        && scheme != QStringLiteral("archive")
        && scheme != QStringLiteral("devices")
        && scheme != QStringLiteral("favorites");
}

bool isPortableUriPath(const QString &path)
{
    return uriSchemeForPath(path) == QLatin1String("portable");
}

bool portableFailureIndicatesRemoval(const QString &error)
{
    const QString lower = error.toLower();
    return lower.contains(QStringLiteral("removed"))
        || lower.contains(QStringLiteral("not connected"))
        || lower.contains(QStringLiteral("not functioning"))
        || lower.contains(QStringLiteral("does not exist"))
        || lower.contains(QStringLiteral("cannot find"))
        || lower.contains(QStringLiteral("no longer available"));
}

bool isNonLocalAutocompletePath(const QString &path)
{
    const QString lowerPath = path.trimmed().toLower();
    return ArchiveSupport::isArchivePath(lowerPath)
        || isProviderUriPath(lowerPath)
        || lowerPath.startsWith(QStringLiteral("archive://"))
        || lowerPath.startsWith(QStringLiteral("devices://"))
        || lowerPath.startsWith(QStringLiteral("favorites://"));
}

bool localAutocompleteAllowedFor(const QString &inputPath, const QString &currentPath)
{
    return !isNonLocalAutocompletePath(inputPath)
        && !isNonLocalAutocompletePath(currentPath);
}

QString expandHomeShortcutPath(const QString &path)
{
    QString value = path.trimmed();
    if (value != QLatin1String("~")
        && !value.startsWith(QStringLiteral("~/"))
        && !value.startsWith(QStringLiteral("~\\"))) {
        return value;
    }

    QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (home.isEmpty()) {
        home = QDir::homePath();
    }
    if (home.isEmpty()) {
        return value;
    }

    const QString normalizedHome = QDir::cleanPath(QDir::fromNativeSeparators(home));
    if (value == QLatin1String("~")) {
        return normalizedHome;
    }

    const QString tail = QDir::fromNativeSeparators(value.mid(2));
    QString expanded = QDir::cleanPath(QDir(normalizedHome).filePath(tail));
    if ((value.endsWith(QLatin1Char('/')) || value.endsWith(QLatin1Char('\\')))
        && !expanded.endsWith(QLatin1Char('/'))) {
        expanded += QLatin1Char('/');
    }
    return expanded;
}

bool providerNavigationSuggestionsAllowedFor(const QString &inputPath)
{
    return isProviderUriPath(inputPath);
}

QString activeLinuxAdminSessionNonce()
{
    const QString nonce = LinuxAdminBroker::activeSessionNonce();
    if (nonce.isEmpty()) {
        return {};
    }
    return nonce;
}

LinuxAdminBroker::Result submitLinuxAdminRequest(LinuxAdminBroker::Request request)
{
    request.operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.sessionNonce = activeLinuxAdminSessionNonce();
    if (request.sessionNonce.isEmpty()) {
        return {false, QStringLiteral("session-inactive"), QStringLiteral("Linux administrator mode is not active"), {}};
    }
    LinuxAdminBroker broker;
    return broker.submitBlocking(request);
}

QString uniqueCreationName(FileProvider *provider, const QString &parentPath, const QString &name, bool file)
{
    QString candidateName = name.trimmed();
    if (!provider || candidateName.isEmpty()
        || !provider->pathExists(provider->childPath(parentPath, candidateName))) {
        return candidateName;
    }

    const int dot = file ? candidateName.lastIndexOf(QChar('.')) : -1;
    const QString base = dot > 0 ? candidateName.left(dot) : candidateName;
    const QString ext = dot > 0 ? candidateName.mid(dot) : QString();
    for (int i = 1; i < 1000; ++i) {
        const QString numbered = ext.isEmpty()
            ? QStringLiteral("%1 (%2)").arg(base).arg(i)
            : QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(ext);
        if (!provider->pathExists(provider->childPath(parentPath, numbered))) {
            return numbered;
        }
    }
    return candidateName;
}

QString normalizedVirtualRoot(const QString &path)
{
    QString value = QDir::fromNativeSeparators(path.trimmed()).toLower();
    while (value.endsWith(QLatin1Char('/')) && !value.endsWith(QStringLiteral("://"))) {
        value.chop(1);
    }

    if (value == QLatin1String("devices:")
        || value == QLatin1String("devices:/")
        || value == QLatin1String("devices://")) {
        return QStringLiteral("devices://");
    }

    if (value == QLatin1String("fav")
        || value == QLatin1String("favorites")
        || value == QLatin1String("favorites:")
        || value == QLatin1String("favorites:/")
        || value == QLatin1String("favorites://")) {
        return QStringLiteral("favorites://");
    }

    return {};
}


NavigationResolution resolveNavigationPath(QString path)
{
    path = path.trimmed();
    if (path.isEmpty()) {
        return {NavigationResolution::Type::Invalid, {}, QStringLiteral("Path is empty"), QStringLiteral("empty")};
    }

    if (ArchiveSupport::isArchivePath(path)) {
        QString normalized = ArchiveSupport::normalizeArchivePath(path);
        const QString fileName = ArchiveSupport::archiveFileName(normalized);
        const QString suffix = QFileInfo(fileName).suffix().toLower();
        if (!normalized.endsWith(QStringLiteral("|/")) && ArchiveSupport::isArchiveExtension(suffix)) {
            normalized = ArchiveSupport::archiveRootPathForPath(normalized);
            return {NavigationResolution::Type::OpenPath, normalized, {}, QStringLiteral("archive-root")};
        }
        return {NavigationResolution::Type::OpenPath, normalized, {}, QStringLiteral("archive-path")};
    }

    const QFileInfo info(path);
    if (info.isFile()) {
        const QString suffix = info.suffix().toLower();
        if (IsoSupport::isIsoImageExtension(suffix)) {
            return {NavigationResolution::Type::MountIso, path, {}, QStringLiteral("iso")};
        }
        if (ArchiveSupport::isArchiveExtension(suffix)) {
            return {NavigationResolution::Type::OpenPath,
                    ArchiveSupport::archiveRootPath(path),
                    {},
                    QStringLiteral("archive-file")};
        }
    }

    return {NavigationResolution::Type::OpenPath, path, {}, QStringLiteral("file")};
}

QString fallbackPathForMissing(QString path)
{
    LocalFileProvider provider;
    QString candidate = provider.normalizedPath(path);
    if (candidate.isEmpty()) {
        return {};
    }

    const QString firstParent = provider.parentPath(candidate);
    if (!firstParent.isEmpty() && !samePanelFilesystemPath(firstParent, candidate)) {
        candidate = firstParent;
    }

    while (!candidate.isEmpty()) {
        if (provider.pathExists(candidate) && provider.isDirectory(candidate)) {
            return provider.normalizedPath(candidate);
        }

        const QString parent = provider.parentPath(candidate);
        if (parent.isEmpty() || samePanelFilesystemPath(parent, candidate)) {
            break;
        }
        candidate = parent;
    }

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty() && provider.pathExists(home) && provider.isDirectory(home)) {
        return provider.normalizedPath(home);
    }

    return {};
}

#ifdef Q_OS_WIN
QString extendedWindowsSearchPattern(QString searchDir)
{
    searchDir = QDir::toNativeSeparators(searchDir);
    if (!searchDir.endsWith(QLatin1Char('\\'))) {
        searchDir += QLatin1Char('\\');
    }

    QString pattern = searchDir + QLatin1Char('*');
    if (pattern.startsWith(QStringLiteral("\\\\?\\"))) {
        return pattern;
    }
    if (pattern.startsWith(QStringLiteral("\\\\"))) {
        return QStringLiteral("\\\\?\\UNC\\") + pattern.mid(2);
    }
    return QStringLiteral("\\\\?\\") + pattern;
}
#endif

QVariantMap directorySuggestionEntry(const QString &path, const QString &label, bool isDrive = false)
{
    QVariantMap entry;
    entry.insert(QStringLiteral("path"), path);
    entry.insert(QStringLiteral("label"), label.isEmpty() ? path : label);
    entry.insert(QStringLiteral("isDrive"), isDrive);
    return entry;
}

QString fallbackSuggestionLabel(QString path)
{
    path = QDir::fromNativeSeparators(path);
    while (path.size() > 1 && path.endsWith(QLatin1Char('/'))) {
        if (path.size() == 3 && path.at(1) == QLatin1Char(':')) {
            break;
        }
        path.chop(1);
    }
    const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    return parts.isEmpty() ? path : parts.constLast();
}

void sortSuggestionEntries(QVariantList &entries)
{
    std::sort(entries.begin(), entries.end(), [](const QVariant &left, const QVariant &right) {
        const QString leftLabel = left.toMap().value(QStringLiteral("label")).toString();
        const QString rightLabel = right.toMap().value(QStringLiteral("label")).toString();
        return QString::compare(leftLabel, rightLabel, Qt::CaseInsensitive) < 0;
    });
}

bool appendSuggestionEntry(QVariantList &entries,
                           const QString &path,
                           const QString &label,
                           qsizetype maxSuggestions,
                           bool isDrive = false)
{
    entries.append(directorySuggestionEntry(path, label, isDrive));
    return maxSuggestions > 0 && entries.size() >= maxSuggestions;
}

QString normalizedArchiveScopeSegment(QString segment)
{
    segment = QDir::fromNativeSeparators(segment.trimmed());
    if (segment == QLatin1String("/")) {
        return {};
    }
    if (segment.startsWith(QLatin1Char('/'))) {
        segment.remove(0, 1);
    }
    while (segment.endsWith(QLatin1Char('/'))) {
        segment.chop(1);
    }
    return segment;
}

QString nestedArchiveApprovalTarget(QString path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return {};
    }

    path = ArchiveSupport::normalizeArchivePath(path);
    const QString fileName = ArchiveSupport::archiveFileName(path);
    const QString suffix = QFileInfo(fileName).suffix().toLower();
    if (ArchiveSupport::isArchiveExtension(suffix) && !path.endsWith(QStringLiteral("|/"))) {
        return ArchiveSupport::archiveRootPathForPath(path);
    }
    return path;
}

QString nestedArchiveScopeKeyForPath(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return {};
    }

    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    const QStringList tokens = ArchiveSupport::splitArchiveTokens(normalized);
    if (tokens.size() < 3) {
        return {};
    }

    const int containerTokenCount = tokens.size() - 1;
    QStringList parts;
    parts.reserve(containerTokenCount);
    parts.append(QDir::fromNativeSeparators(QFileInfo(tokens.first()).absoluteFilePath()));
    for (int i = 1; i < containerTokenCount; ++i) {
        parts.append(normalizedArchiveScopeSegment(tokens.at(i)));
    }
    return QStringLiteral("archive://") + parts.join(QLatin1Char('|'));
}

QString outerArchiveSessionKeyForPath(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    return QDir::fromNativeSeparators(QFileInfo(ArchiveSupport::physicalArchivePath(path)).absoluteFilePath());
}

QString nestedArchiveDisplayNameForPath(const QString &path)
{
    const QString target = nestedArchiveApprovalTarget(path);
    if (target.isEmpty()) {
        return ArchiveSupport::archiveFileName(path);
    }
    return ArchiveSupport::archiveFileName(target);
}

QString nestedArchiveEntryPathForTarget(const QString &path)
{
    QString target = nestedArchiveApprovalTarget(path);
    if (target.endsWith(QStringLiteral("|/"))) {
        target.chop(2);
    }
    return target;
}

QString formatNestedArchiveSize(qint64 bytes)
{
    if (bytes < 0) {
        return {};
    }

    constexpr qint64 KB = 1024LL;
    constexpr qint64 MB = 1024LL * KB;
    constexpr qint64 GB = 1024LL * MB;

    auto formatValue = [](double value, const QString &unit) {
        const bool whole = qAbs(value - std::round(value)) < 0.05;
        return QStringLiteral("%1 %2").arg(value, 0, 'f', whole ? 0 : 1).arg(unit);
    };

    if (bytes >= GB) {
        return formatValue(static_cast<double>(bytes) / static_cast<double>(GB), QStringLiteral("GB"));
    }
    if (bytes >= MB) {
        return formatValue(static_cast<double>(bytes) / static_cast<double>(MB), QStringLiteral("MB"));
    }
    const double kb = qMax(1.0, std::ceil(static_cast<double>(bytes) / static_cast<double>(KB)));
    return formatValue(kb, QStringLiteral("KB"));
}

QString nestedArchiveSizeTextForPath(const QString &path)
{
    const QString entryPath = nestedArchiveEntryPathForTarget(path);
    if (entryPath.isEmpty()) {
        return {};
    }

    const auto entry = ArchiveFileProvider::cachedEntryInfo(entryPath);
    if (!entry || entry->size < 0) {
        return {};
    }
    return formatNestedArchiveSize(entry->size);
}

int nestedArchiveDepthForPath(const QString &path)
{
    const QString target = nestedArchiveApprovalTarget(path);
    if (target.isEmpty()) {
        return 0;
    }

    const QStringList tokens = ArchiveSupport::splitArchiveTokens(ArchiveSupport::normalizeArchivePath(target));
    return qMax(0, tokens.size() - 2);
}

QString nestedArchivePreparationStatusForPath(const QString &path)
{
    const int depth = qMax(1, nestedArchiveDepthForPath(path));
    return QStringLiteral("Preparing nested archive 1/%1: %2...")
        .arg(depth)
        .arg(nestedArchiveDisplayNameForPath(path));
}

QString nestedArchivePreparedStatusForPath(const QString &path)
{
    const int depth = nestedArchiveDepthForPath(path);
    if (depth > 1) {
        return QStringLiteral("Nested archive prepared (%1 levels)").arg(depth);
    }
    return QStringLiteral("Nested archive prepared");
}

QString failedNavigationRevealPath(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return path;
    }

    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    const QStringList tokens = ArchiveSupport::splitArchiveTokens(normalized);
    if (tokens.size() == 2 && tokens.last() == QLatin1String("/")) {
        return ArchiveSupport::physicalArchivePath(normalized);
    }
    if (tokens.size() > 2 && tokens.last() == QLatin1String("/")) {
        return QStringLiteral("archive://") + tokens.mid(0, tokens.size() - 1).join(QLatin1Char('|'));
    }
    return normalized;
}

bool navigationFailureIndicatesMissingPath(const QString &error)
{
    const QString lower = error.toLower();
    return lower.contains(QStringLiteral("does not exist"))
        || lower.contains(QStringLiteral("no longer available"))
        || lower.contains(QStringLiteral("not found"));
}

constexpr qsizetype MaxSuggestionScanEntries = 4096;
constexpr int MaxSuggestionScanMs = 120;

bool suggestionsCancelled(const SuggestionCancelCheck &shouldCancel)
{
    return shouldCancel && shouldCancel();
}

#if defined(Q_OS_WIN)
QVariantList nativeDirectorySuggestionEntries(const QString &searchDir,
                                              const QString &prefix,
                                              qsizetype maxSuggestions,
                                              const SuggestionCancelCheck &shouldCancel)
{
    QVariantList suggestions;
    if (suggestionsCancelled(shouldCancel)) {
        return suggestions;
    }

    QString outputBase = QDir::toNativeSeparators(searchDir);
    if (!outputBase.endsWith(QLatin1Char('\\'))) {
        outputBase += QLatin1Char('\\');
    }

    WIN32_FIND_DATAW findData;
    const QString pattern = extendedWindowsSearchPattern(searchDir);
    HANDLE handle = FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()),
                                     FindExInfoBasic,
                                     &findData,
                                     FindExSearchNameMatch,
                                     nullptr,
                                     FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER) {
        handle = FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()),
                                  FindExInfoBasic,
                                  &findData,
                                  FindExSearchNameMatch,
                                  nullptr,
                                  0);
    }
    if (handle == INVALID_HANDLE_VALUE) {
        return suggestions;
    }

    QElapsedTimer scanTimer;
    scanTimer.start();
    qsizetype scannedEntries = 0;
    do {
        if (suggestionsCancelled(shouldCancel)) {
            FindClose(handle);
            return {};
        }
        if (++scannedEntries > MaxSuggestionScanEntries || scanTimer.hasExpired(MaxSuggestionScanMs)) {
            break;
        }

        const QString name = QString::fromWCharArray(findData.cFileName);
        if (name == QLatin1String(".") || name == QLatin1String("..")) {
            continue;
        }
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            continue;
        }
        if ((findData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0 || name.startsWith(QLatin1Char('.'))) {
            continue;
        }
        if (!prefix.isEmpty() && !name.startsWith(prefix, Qt::CaseInsensitive)) {
            continue;
        }
        if (appendSuggestionEntry(suggestions, outputBase + name + QLatin1Char('\\'), name, maxSuggestions)) {
            break;
        }
    } while (FindNextFileW(handle, &findData));

    FindClose(handle);
    sortSuggestionEntries(suggestions);
    return suggestions;
}
#elif defined(Q_OS_LINUX)
QVariantList nativeDirectorySuggestionEntries(const QString &searchDir,
                                              const QString &prefix,
                                              qsizetype maxSuggestions,
                                              const SuggestionCancelCheck &shouldCancel)
{
    QVariantList suggestions;
    if (suggestionsCancelled(shouldCancel)) {
        return suggestions;
    }

    QString outputBase = QDir::fromNativeSeparators(QDir(searchDir).absolutePath());
    if (!outputBase.endsWith(QLatin1Char('/'))) {
        outputBase += QLatin1Char('/');
    }

    const QByteArray nativePath = QFile::encodeName(outputBase);
    DIR *directory = opendir(nativePath.constData());
    if (!directory) {
        return suggestions;
    }

    QElapsedTimer scanTimer;
    scanTimer.start();
    qsizetype scannedEntries = 0;
    errno = 0;
    while (dirent *entry = readdir(directory)) {
        if (suggestionsCancelled(shouldCancel)) {
            closedir(directory);
            return {};
        }
        if (++scannedEntries > MaxSuggestionScanEntries || scanTimer.hasExpired(MaxSuggestionScanMs)) {
            break;
        }

        const QByteArray nameBytes(entry->d_name);
        if (nameBytes == "." || nameBytes == "..") {
            continue;
        }

        const QString name = QString::fromLocal8Bit(nameBytes);
        if (name.startsWith(QLatin1Char('.'))) {
            continue;
        }
        if (!prefix.isEmpty() && !name.startsWith(prefix, Qt::CaseInsensitive)) {
            continue;
        }

        struct stat statBuffer {};
        if (fstatat(dirfd(directory), nameBytes.constData(), &statBuffer, AT_SYMLINK_NOFOLLOW) != 0) {
            continue;
        }
        if (S_ISLNK(statBuffer.st_mode)
            && fstatat(dirfd(directory), nameBytes.constData(), &statBuffer, 0) != 0) {
            continue;
        }
        if (!S_ISDIR(statBuffer.st_mode)) {
            continue;
        }

        if (appendSuggestionEntry(suggestions, outputBase + name + QLatin1Char('/'), name, maxSuggestions)) {
            break;
        }
    }

    closedir(directory);
    sortSuggestionEntries(suggestions);
    return suggestions;
}
#endif

QVariantList directorySuggestionEntriesForInput(const QString &inputPath,
                                                const QString &currentPath,
                                                qsizetype maxSuggestions,
                                                const SuggestionCancelCheck &shouldCancel)
{
    constexpr QLatin1String deviceRoot{"devices://"};

    QVariantList suggestions;
    if (suggestionsCancelled(shouldCancel)) {
        return suggestions;
    }

    QString cleanPath = expandHomeShortcutPath(inputPath);
    if (cleanPath.isEmpty()) {
        return suggestions;
    }

    if (cleanPath.startsWith(deviceRoot, Qt::CaseInsensitive)) {
#ifdef Q_OS_WIN
        for (const QFileInfo &drive : QDir::drives()) {
            if (suggestionsCancelled(shouldCancel)) {
                return {};
            }
            const QString drivePath = QDir::toNativeSeparators(drive.absoluteFilePath());
            if (appendSuggestionEntry(suggestions, drivePath, DriveUtils::rootDisplayName(drivePath), maxSuggestions, true)) {
                break;
            }
        }
#endif
        return suggestions;
    }

    bool isArchive = ArchiveSupport::isArchivePath(cleanPath);
    const bool isProviderUri = isProviderUriPath(cleanPath);
    const bool currentIsArchive = ArchiveSupport::isArchivePath(currentPath);
    QString searchDir;
    QString prefix;

    if (isArchive) {
        if (cleanPath.endsWith(QLatin1Char('|'))) {
            searchDir = cleanPath + QLatin1Char('/');
            prefix = "";
        } else if (cleanPath.endsWith(QLatin1Char('/'))) {
            searchDir = cleanPath;
            prefix = "";
        } else {
            const int lastSlash = cleanPath.lastIndexOf(QLatin1Char('/'));
            const int lastPipe = cleanPath.lastIndexOf(QLatin1Char('|'));
            const int lastSeparator = qMax(lastSlash, lastPipe);
            if (lastSeparator != -1) {
                if (cleanPath.at(lastSeparator) == QLatin1Char('|')) {
                    searchDir = cleanPath.left(lastSeparator + 1) + QLatin1Char('/');
                    prefix = cleanPath.mid(lastSeparator + 1);
                } else {
                    searchDir = cleanPath.left(lastSeparator + 1);
                    prefix = cleanPath.mid(lastSeparator + 1);
                }
            } else {
                searchDir = cleanPath;
                prefix = "";
            }
        }
    } else if (isProviderUri) {
        const QString scheme = uriSchemeForPath(cleanPath);
        const QString rootPath = scheme + QStringLiteral("://");
        const QString normalizedProviderPath = FileProviderFactory::normalizePath(cleanPath);
        const QString providerPath = normalizedProviderPath.isEmpty() ? cleanPath : normalizedProviderPath;
        const bool trailingSeparator = cleanPath.endsWith(QLatin1Char('/')) || cleanPath.endsWith(QLatin1Char('\\'));

        if (providerPath == rootPath || trailingSeparator) {
            searchDir = providerPath;
            prefix = {};
        } else {
            const QString tail = providerPath.mid(rootPath.size());
            const int lastSeparator = tail.lastIndexOf(QLatin1Char('/'));
            if (lastSeparator >= 0) {
                const QString parentTail = tail.left(lastSeparator);
                searchDir = parentTail.isEmpty() ? rootPath : rootPath + parentTail;
                prefix = tail.mid(lastSeparator + 1);
            } else {
                searchDir = rootPath;
                prefix = tail;
            }
        }
    } else if (currentIsArchive
               && !cleanPath.contains(QLatin1Char(':'))
               && !QDir::fromNativeSeparators(cleanPath).startsWith(QLatin1Char('/'))) {
        isArchive = true;
        const QString relativePath = QDir::fromNativeSeparators(cleanPath);
        const int lastSlash = relativePath.lastIndexOf(QLatin1Char('/'));
        const QString relativeParent = lastSlash >= 0 ? relativePath.left(lastSlash) : QString{};
        prefix = lastSlash >= 0 ? relativePath.mid(lastSlash + 1) : relativePath;
        searchDir = currentPath;
        if (!searchDir.endsWith(QLatin1Char('/')) && !searchDir.endsWith(QLatin1Char('|'))) {
            searchDir += QLatin1Char('/');
        }

        const QStringList parentParts = relativeParent.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        for (const QString &part : parentParts) {
            if (suggestionsCancelled(shouldCancel)) {
                return {};
            }
            searchDir = ArchiveSupport::archiveChildPath(searchDir, part);
            if (!searchDir.endsWith(QLatin1Char('/'))) {
                searchDir += QLatin1Char('/');
            }
        }
    } else {
        const QString nativePath = QDir::toNativeSeparators(cleanPath);

        if (nativePath.endsWith(QDir::separator())) {
            searchDir = nativePath;
            prefix = "";
        } else {
            const int lastSeparator = nativePath.lastIndexOf(QDir::separator());
            if (lastSeparator != -1) {
                searchDir = nativePath.left(lastSeparator + 1);
                prefix = nativePath.mid(lastSeparator + 1);
            } else if (nativePath.length() == 2 && nativePath.endsWith(':')) {
                searchDir = nativePath + QDir::separator();
                prefix = "";
            } else if (nativePath.length() == 1 && nativePath[0].isLetter()) {
#ifdef Q_OS_WIN
                for (const QFileInfo &drive : QDir::drives()) {
                    if (suggestionsCancelled(shouldCancel)) {
                        return {};
                    }
                    const QString drivePath = drive.absoluteFilePath();
                    if (drivePath.startsWith(nativePath, Qt::CaseInsensitive)
                            && appendSuggestionEntry(suggestions,
                                                     QDir::toNativeSeparators(drivePath),
                                                     DriveUtils::rootDisplayName(drivePath),
                                                     maxSuggestions,
                                                     true)) {
                        break;
                    }
                }
#endif
                return suggestions;
            } else {
                searchDir = currentPath + QDir::separator();
                prefix = nativePath;
            }
        }
    }

#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    if (!isArchive && !isProviderUri) {
        return nativeDirectorySuggestionEntries(searchDir, prefix, maxSuggestions, shouldCancel);
    }
#endif

    if (suggestionsCancelled(shouldCancel)) {
        return {};
    }

    std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(searchDir);
    if (!provider || searchDir.isEmpty() || !provider->pathExists(searchDir) || !provider->isDirectory(searchDir)) {
        return suggestions;
    }

    const QStringList childPathsList = provider->childPaths(searchDir, false);
    for (const QString &child : childPathsList) {
        if (suggestionsCancelled(shouldCancel)) {
            return {};
        }
        if (provider->isDirectory(child)) {
            const QString name = provider->fileName(child);
            if (name.startsWith(prefix, Qt::CaseInsensitive)) {
                QString path = child;
                if (!isArchive && !isProviderUri) {
                    path = QDir::toNativeSeparators(path);
                    if (!path.endsWith(QDir::separator())) {
                        path += QDir::separator();
                    }
                } else if (!path.endsWith(QLatin1Char('/'))) {
                    path += QLatin1Char('/');
                }
                if (appendSuggestionEntry(suggestions, path, name.isEmpty() ? fallbackSuggestionLabel(path) : name, maxSuggestions)) {
                    break;
                }
            }
        }
    }

    sortSuggestionEntries(suggestions);
    return suggestions;
}

QString normalizedScopePath(const QString &path)
{
    QString value = QDir::fromNativeSeparators(path.trimmed());
    while (value.size() > 1 && value.endsWith(QLatin1Char('/'))) {
        if (value.size() == 3 && value.at(1) == QLatin1Char(':')) {
            break;
        }
        value.chop(1);
    }
    return value;
}

bool sameOrChildPath(const QString &path, const QString &scope)
{
    const QString normalizedPath = normalizedScopePath(path);
    const QString normalizedScope = normalizedScopePath(scope);
    if (normalizedPath.isEmpty() || normalizedScope.isEmpty()) {
        return false;
    }

#ifdef Q_OS_WIN
    constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseInsensitive;
#else
    constexpr Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive;
#endif

    if (normalizedPath.compare(normalizedScope, caseSensitivity) == 0) {
        return true;
    }

    const QString prefix = normalizedScope.endsWith(QLatin1Char('/'))
        ? normalizedScope
        : normalizedScope + QLatin1Char('/');
    return normalizedPath.startsWith(prefix, caseSensitivity);
}

QString categoryFilterSummaryText(DirectoryModel::CategoryFilter filter)
{
    switch (filter) {
    case DirectoryModel::FilterExecutables:
        return QStringLiteral("Executables");
    case DirectoryModel::FilterLibraries:
        return QStringLiteral("Libraries");
    case DirectoryModel::FilterImages:
        return QStringLiteral("Images");
    case DirectoryModel::FilterArchives:
        return QStringLiteral("Archives");
    case DirectoryModel::FilterMedia:
        return QStringLiteral("Media");
    case DirectoryModel::FilterDocuments:
        return QStringLiteral("Documents");
    case DirectoryModel::FilterAll:
        break;
    }
    return {};
}
}


using namespace FilePanelControllerInternal;

FilePanelController::FilePanelController(QObject *parent)
    : QObject(parent)
    , m_fileProvider(std::make_unique<LocalFileProvider>())
{
    connect(&m_directoryModel, &DirectoryModel::currentPathChanged, this, &FilePanelController::currentPathChanged);
    connect(&m_directoryModel, &DirectoryModel::currentPathChanged, this, [this]() {
        ++m_storageInfoRevision;
        emit storageInfoChanged();
    });
    connect(&m_directoryModel, &DirectoryModel::directoryUnavailable,
            this, &FilePanelController::recoverFromMissingPath,
            Qt::QueuedConnection);
    connect(&m_directoryModel, &DirectoryModel::providerStatusMessage,
            this, &FilePanelController::setStatusMessage);
    connect(&m_directoryModel, &DirectoryModel::currentPathChanged, this, &FilePanelController::capabilitiesChanged);
    connect(&m_directoryModel, &DirectoryModel::selectionChanged, this, &FilePanelController::capabilitiesChanged);
    connect(&m_directoryModel, &DirectoryModel::dataChanged, this,
            [this](const QModelIndex &topLeft, const QModelIndex &bottomRight, const QList<int> &roles) {
        if (m_hoveredPath.isEmpty()
                || (!roles.isEmpty() && !roles.contains(DirectoryModel::ThumbnailRevisionRole))) {
            return;
        }
        const int hoveredRow = m_directoryModel.indexOfPath(m_hoveredPath);
        if (hoveredRow >= topLeft.row() && hoveredRow <= bottomRight.row()) {
            emit hoveredPathChanged();
        }
    });
    connect(&m_directoryModel, &DirectoryModel::loadingChanged, this, [this]() {
        if (m_directoryModel.loading()) {
            return;
        }

        ++m_storageInfoRevision;
        emit storageInfoChanged();

        const QString path = currentPath();
        if (nestedArchiveScopeKeyForPath(path).isEmpty()) {
            return;
        }
        if (ArchiveFileProvider::hasCachedContainerForPath(path)) {
            setStatusMessage(nestedArchivePreparedStatusForPath(path));
        } else if (!m_directoryModel.error().isEmpty()) {
            setStatusMessage(m_directoryModel.error());
        }
    });
    connect(&m_directoryModel, &DirectoryModel::sortRoleChanged, this, [this]() {
        if (m_panelSortRole != m_directoryModel.sortRole()) {
            m_panelSortRole = m_directoryModel.sortRole();
            emit panelSortRoleChanged();
        }
    });
    connect(&m_directoryModel, &DirectoryModel::sortOrderChanged, this, [this]() {
        if (m_panelSortOrder != m_directoryModel.sortOrder()) {
            m_panelSortOrder = m_directoryModel.sortOrder();
            emit panelSortOrderChanged();
        }
    });
    m_createdEntryRevealTimer.setSingleShot(true);
    m_createdEntryRevealTimer.setInterval(75);
    connect(&m_createdEntryRevealTimer, &QTimer::timeout, this, [this]() {
        const QString path = m_pendingCreatedEntryRevealPath;
        if (path.isEmpty()) {
            return;
        }
        if (m_directoryModel.indexOfPath(path) < 0) {
            if (++m_createdEntryRevealAttempts <= 80) {
                m_createdEntryRevealTimer.start();
            } else {
                m_pendingCreatedEntryRevealPath.clear();
                m_createdEntryRevealAttempts = 0;
            }
            return;
        }
        m_pendingCreatedEntryRevealPath.clear();
        m_createdEntryRevealAttempts = 0;
        m_directoryModel.selectOnly(m_directoryModel.indexOfPath(path));
        emit createdEntryRevealRequested(path);
    });
}

void FilePanelController::setVolumeMonitor(VolumeMonitor *monitor)
{
    m_volumeMonitor = monitor;
    if (!m_volumeMonitor) {
        return;
    }

    m_directoryModel.refreshMountPointBadges();
    connect(m_volumeMonitor, &VolumeMonitor::volumesChanged, this, [this]() {
        m_directoryModel.refreshMountPointBadges();
    });
}

void FilePanelController::setFavoritesController(FavoritesController *controller)
{
    if (!controller) {
        return;
    }

    m_directoryModel.setPinnedPathSnapshot(controller->pinnedPathSnapshot());
    connect(controller, &FavoritesController::pinnedPathsChanged, this,
            [this, controller](const QStringList &paths) {
        m_directoryModel.updatePinnedPaths(paths, controller->pinnedPathSnapshot());
    });
}

bool FilePanelController::isDeviceRoot() const
{
    return m_isDeviceRoot;
}

bool FilePanelController::isFavoritesRoot() const
{
    return m_isFavoritesRoot;
}

bool FilePanelController::isVirtualRoot() const
{
    return m_isDeviceRoot || m_isFavoritesRoot;
}

DirectoryModel::SortRole FilePanelController::panelSortRole() const
{
    return m_panelSortRole;
}

void FilePanelController::setPanelSortRole(DirectoryModel::SortRole role)
{
    setPanelSortPolicy(int(role), int(m_panelSortOrder));
}

Qt::SortOrder FilePanelController::panelSortOrder() const
{
    return m_panelSortOrder;
}

void FilePanelController::setPanelSortOrder(Qt::SortOrder order)
{
    setPanelSortPolicy(int(m_panelSortRole), int(order));
}

void FilePanelController::setPanelSortPolicy(int role, int order)
{
    const auto normalizedRole = static_cast<DirectoryModel::SortRole>(qBound(0, role, 5));
    const Qt::SortOrder normalizedOrder = order == int(Qt::DescendingOrder)
            ? Qt::DescendingOrder
            : Qt::AscendingOrder;

    const bool roleChanged = m_panelSortRole != normalizedRole;
    const bool orderChanged = m_panelSortOrder != normalizedOrder;
    if (!roleChanged && !orderChanged) {
        if (m_directoryModel.sortRole() != normalizedRole
                || m_directoryModel.sortOrder() != normalizedOrder) {
            m_directoryModel.setSortPolicy(normalizedRole, normalizedOrder);
        }
        return;
    }

    m_panelSortRole = normalizedRole;
    m_panelSortOrder = normalizedOrder;

    if (roleChanged) {
        emit panelSortRoleChanged();
    }
    if (orderChanged) {
        emit panelSortOrderChanged();
    }

    m_directoryModel.setSortPolicy(normalizedRole, normalizedOrder);
}

void FilePanelController::setIsDeviceRoot(bool value)
{
    if (m_isDeviceRoot == value) return;
    m_isDeviceRoot = value;
    emit isDeviceRootChanged();
    emit virtualRootChanged();
}

void FilePanelController::setIsFavoritesRoot(bool value)
{
    if (m_isFavoritesRoot == value) return;
    m_isFavoritesRoot = value;
    emit isFavoritesRootChanged();
    emit virtualRootChanged();
}

DirectoryModel *FilePanelController::directoryModel()
{
    return &m_directoryModel;
}

QString FilePanelController::currentPath() const
{
    if (m_isDeviceRoot) {
        return QString(DEVICE_ROOT);
    }
    if (m_isFavoritesRoot) {
        return QString(FAVORITES_ROOT);
    }
    return m_directoryModel.currentPath();
}

QString FilePanelController::pathKindFor(const QString &path) const
{
    const QString lowerPath = path.toLower();
    if (lowerPath.startsWith(QStringLiteral("archive://"))) {
        return QStringLiteral("archive");
    }
    if (lowerPath.startsWith(QStringLiteral("devices://"))) {
        return QStringLiteral("devices");
    }
    if (lowerPath.startsWith(QStringLiteral("favorites://"))) {
        return QStringLiteral("favorites");
    }
    if (lowerPath.startsWith(QStringLiteral("file://"))) {
        return QStringLiteral("local");
    }
    if (lowerPath.startsWith(QStringLiteral("ftp://"))) {
        return QStringLiteral("ftp");
    }
    if (lowerPath.startsWith(QStringLiteral("gdrive://"))) {
        return QStringLiteral("gdrive");
    }
    if (lowerPath.startsWith(QStringLiteral("mega://"))) {
        return QStringLiteral("mega");
    }
    if (lowerPath.startsWith(QStringLiteral("instagram://"))) {
        return QStringLiteral("instagram");
    }
    if (lowerPath.startsWith(QStringLiteral("telegram://"))) {
        return QStringLiteral("telegram");
    }
    if (lowerPath.indexOf(QStringLiteral("://")) > 0) {
        return QStringLiteral("remote");
    }
    return QStringLiteral("local");
}

QString FilePanelController::fileTypeLabelFor(const QString &suffix, bool isDirectory) const
{
    if (isDirectory) {
        return QStringLiteral("Folder");
    }
    if (suffix.isEmpty()) {
        return QStringLiteral("File");
    }

    const QString s = suffix.toLower();
    if (s == QStringLiteral("png") || s == QStringLiteral("jpg") || s == QStringLiteral("jpeg")
        || s == QStringLiteral("gif") || s == QStringLiteral("webp") || s == QStringLiteral("bmp")
        || s == QStringLiteral("ico") || s == QStringLiteral("svg") || s == QStringLiteral("avif")
        || s == QStringLiteral("heic")) {
        return s.toUpper() + QStringLiteral(" Image");
    }
    if (s == QStringLiteral("pdf")) return QStringLiteral("PDF Document");
    if (s == QStringLiteral("txt")) return QStringLiteral("Text File");
    if (s == QStringLiteral("md")) return QStringLiteral("Markdown");
    if (s == QStringLiteral("json")) return QStringLiteral("JSON");
    if (s == QStringLiteral("xml") || s == QStringLiteral("html") || s == QStringLiteral("htm")) {
        return s.toUpper();
    }
    if (s == QStringLiteral("css")) return QStringLiteral("CSS Stylesheet");
    if (s == QStringLiteral("js") || s == QStringLiteral("ts")) return s.toUpper() + QStringLiteral(" Script");
    if (s == QStringLiteral("cpp") || s == QStringLiteral("c") || s == QStringLiteral("h") || s == QStringLiteral("hpp")) {
        return QStringLiteral("C/C++ Source");
    }
    if (s == QStringLiteral("py")) return QStringLiteral("Python Script");
    if (s == QStringLiteral("rs")) return QStringLiteral("Rust Source");
    if (s == QStringLiteral("go")) return QStringLiteral("Go Source");
    if (s == QStringLiteral("java") || s == QStringLiteral("kt")) {
        return s == QStringLiteral("kt") ? QStringLiteral("Kotlin Source") : QStringLiteral("Java Source");
    }
    if (s == QStringLiteral("mp3") || s == QStringLiteral("flac") || s == QStringLiteral("ogg")
        || s == QStringLiteral("m4a") || s == QStringLiteral("wav") || s == QStringLiteral("wma")) {
        return s.toUpper() + QStringLiteral(" Audio");
    }
    if (s == QStringLiteral("mp4") || s == QStringLiteral("mkv") || s == QStringLiteral("avi")
        || s == QStringLiteral("mov") || s == QStringLiteral("wmv")) {
        return s.toUpper() + QStringLiteral(" Video");
    }
    if (s == QStringLiteral("zip") || s == QStringLiteral("rar") || s == QStringLiteral("7z")
        || s == QStringLiteral("tar") || s == QStringLiteral("gz") || s == QStringLiteral("xz")) {
        return s.toUpper() + QStringLiteral(" Archive");
    }
    if (s == QStringLiteral("exe") || s == QStringLiteral("msi")) {
        return s.toUpper() + QStringLiteral(" Application");
    }
    if (s == QStringLiteral("bat") || s == QStringLiteral("cmd") || s == QStringLiteral("ps1") || s == QStringLiteral("sh")) {
        return QStringLiteral("Script");
    }
    if (s == QStringLiteral("lnk") || s == QStringLiteral("shortcut")) return QStringLiteral("Shortcut");
    if (s == QStringLiteral("iso")) return QStringLiteral("Disk Image");
    if (s == QStringLiteral("ttf") || s == QStringLiteral("otf") || s == QStringLiteral("woff") || s == QStringLiteral("woff2")) {
        return QStringLiteral("Font");
    }
    return s.toUpper() + QStringLiteral(" File");
}

bool FilePanelController::isArchiveFilePath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::isArchiveExtension(
            QFileInfo(ArchiveSupport::archiveFileName(path)).suffix().toLower());
    }
    return ArchiveSupport::isArchiveExtension(QFileInfo(path).suffix().toLower());
}

bool FilePanelController::canGoBack() const
{
    return !m_backStack.isEmpty();
}

bool FilePanelController::canGoForward() const
{
    return !m_forwardStack.isEmpty();
}

int FilePanelController::backStackCount() const
{
    return m_backStack.size();
}

int FilePanelController::forwardStackCount() const
{
    return m_forwardStack.size();
}

QString FilePanelController::hoveredPath() const
{
    return m_hoveredPath;
}

QVariantMap FilePanelController::hoveredFileInfo() const
{
    QVariantMap info;
    if (m_hoveredPath.isEmpty()) {
        return info;
    }

    const int row = m_directoryModel.indexOfPath(m_hoveredPath);
    if (row < 0) {
        return info;
    }

    const QModelIndex modelIndex = m_directoryModel.index(row, 0);
    const QString suffix = m_directoryModel.data(modelIndex, DirectoryModel::SuffixRole).toString();
    const bool isDirectory = m_directoryModel.data(modelIndex, DirectoryModel::IsDirectoryRole).toBool();

    info.insert(QStringLiteral("path"), m_hoveredPath);
    info.insert(QStringLiteral("name"), m_directoryModel.data(modelIndex, DirectoryModel::NameRole).toString());
    info.insert(QStringLiteral("suffix"), suffix);
    info.insert(QStringLiteral("typeLabel"), fileTypeLabelFor(suffix, isDirectory));
    info.insert(QStringLiteral("sizeText"), m_directoryModel.data(modelIndex, DirectoryModel::SizeTextRole).toString());
    info.insert(QStringLiteral("modifiedText"), m_directoryModel.data(modelIndex, DirectoryModel::ModifiedTextRole).toString());
    info.insert(QStringLiteral("mimeType"), m_directoryModel.data(modelIndex, DirectoryModel::MimeTypeRole).toString());
    info.insert(QStringLiteral("isImage"), m_directoryModel.data(modelIndex, DirectoryModel::IsImageRole).toBool());
    info.insert(QStringLiteral("hasThumbnail"), m_directoryModel.data(modelIndex, DirectoryModel::HasThumbnailRole).toBool());
    info.insert(QStringLiteral("thumbnailRevision"), m_directoryModel.data(modelIndex, DirectoryModel::ThumbnailRevisionRole).toInt());
    return info;
}

QString FilePanelController::currentItemPath() const
{
    return m_currentItemPath;
}

QString FilePanelController::statusMessage() const
{
    return m_statusMessage;
}

QVariantMap FilePanelController::lastError() const
{
    return m_lastError;
}

bool FilePanelController::scrolling() const
{
    return m_scrolling;
}

bool FilePanelController::isReadOnlyContainerPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)
        || IsoSupport::isIsoImageExtension(QFileInfo(path).suffix().toLower())) {
        return true;
    }
    if (isProviderUriPath(path)) {
        std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
        return provider && provider->isReadOnlyContainer(path);
    }
    return false;
}

bool FilePanelController::pathCanCopy(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }
    if (isProviderUriPath(path)) {
        std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
        return provider && provider->canCopyPath(path);
    }

#ifdef Q_OS_LINUX
    if (!ArchiveSupport::isArchivePath(path)
            && !LinuxAdminBroker::activeSessionNonce().isEmpty()) {
        return true;
    }
#endif

    const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
    return capabilities.exists
        && (capabilities.isDirectory ? capabilities.access.canBrowse : capabilities.access.canRead);
}

bool FilePanelController::pathCanCreateChildren(const QString &path) const
{
    if (path.isEmpty() || isReadOnlyContainerPath(path)) {
        return false;
    }
    if (isProviderUriPath(path)) {
        std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
        return provider && provider->canCreateChildren(path);
    }
    if (!(m_fileProvider->capabilities() & FileProvider::Create)) {
        return false;
    }

    const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
    return capabilities.exists
        && capabilities.isDirectory
        && capabilities.access.canCreateChildren;
}

bool FilePanelController::pathCanDelete(const QString &path) const
{
    if (path.isEmpty() || isReadOnlyContainerPath(path)) {
        return false;
    }
    if (isProviderUriPath(path)) {
        std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
        return provider && provider->canRemovePath(path);
    }
    if (!(m_fileProvider->capabilities() & FileProvider::Remove)) {
        return false;
    }

    const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
    return capabilities.exists && capabilities.access.canDelete;
}

bool FilePanelController::canCreateInCurrentPath() const
{
    if (isVirtualRoot()) {
        return false;
    }
    return pathCanCreateChildren(currentPath());
}

bool FilePanelController::canCopySelection() const
{
    return canCopyPaths(selectedPaths());
}

bool FilePanelController::canCopyPaths(const QStringList &paths) const
{
    if (isVirtualRoot()) {
        return false;
    }
    if (paths.isEmpty()) {
        return false;
    }
    for (const QString &path : paths) {
        if (!pathCanCopy(path)) {
            return false;
        }
    }
    return true;
}

bool FilePanelController::canRenameSelection() const
{
    if (isVirtualRoot()
        || !pathCanCreateChildren(currentPath())) {
        return false;
    }
    const QStringList paths = selectedPaths();
    if (paths.isEmpty()) {
        return false;
    }
    for (const QString &path : paths) {
        if (!pathCanDelete(path)) {
            return false;
        }
    }
    return true;
}

bool FilePanelController::canDeleteSelection() const
{
    return canDeletePaths(selectedPaths());
}

bool FilePanelController::canDeletePaths(const QStringList &paths) const
{
    if (isVirtualRoot() || isReadOnlyContainerPath(currentPath())) {
        return false;
    }
    if (paths.isEmpty()) {
        return false;
    }
    for (const QString &path : paths) {
        if (!pathCanDelete(path)) {
            return false;
        }
    }
    return true;
}

bool FilePanelController::canDuplicateSelection() const
{
    if (isVirtualRoot() || !pathCanCreateChildren(currentPath())) {
        return false;
    }
    const QStringList paths = selectedPaths();
    if (paths.size() != 1) {
        return false;
    }
    const QString path = paths.constFirst();
    if (path.isEmpty() || ArchiveSupport::isArchivePath(path)) {
        return false;
    }
    const int row = m_directoryModel.indexOfPath(path);
    return row >= 0 && !m_directoryModel.isDirectoryAt(row);
}

bool FilePanelController::canCompressSelection() const
{
    if (isVirtualRoot() || !pathCanCreateChildren(currentPath())) {
        return false;
    }
    const QStringList paths = selectedPaths();
    if (paths.isEmpty()) {
        return false;
    }
    for (const QString &path : paths) {
        if (path.isEmpty() || ArchiveSupport::isArchivePath(path)) {
            return false;
        }
    }
    return true;
}

bool FilePanelController::canPasteIntoCurrentPath() const
{
    if (isVirtualRoot()) {
        return false;
    }
    return pathCanCreateChildren(currentPath());
}

int FilePanelController::storageInfoRevision() const
{
    return m_storageInfoRevision;
}

int FilePanelController::categoryFilter() const
{
    return m_categoryFilter;
}

bool FilePanelController::categoryFilterActive() const
{
    return m_categoryFilter != DirectoryModel::FilterAll;
}

bool FilePanelController::categoryFilterSuspended() const
{
    return categoryFilterActive() && m_directoryModel.categoryFilter() == DirectoryModel::FilterAll;
}

QString FilePanelController::categoryFilterSummary() const
{
    return categoryFilterSummaryText(m_categoryFilter);
}

void FilePanelController::setHoveredPath(const QString &path)
{
    if (m_hoveredPath == path) {
        return;
    }
    m_hoveredPath = path;
    emit hoveredPathChanged();
}

void FilePanelController::setCurrentItemPath(const QString &path)
{
    if (m_currentItemPath == path) {
        return;
    }
    m_currentItemPath = path;
    emit currentItemPathChanged();
}

void FilePanelController::setScrolling(bool scrolling)
{
    if (m_scrolling == scrolling) {
        return;
    }
    m_scrolling = scrolling;
    emit scrollingChanged();
}

bool FilePanelController::navigationPending() const
{
    return m_navigationPending;
}

QString FilePanelController::pendingNavigationPath() const
{
    return m_pendingNavigationPath;
}

void FilePanelController::setNavigationPending(bool pending, const QString &path)
{
    if (m_pendingNavigationPath != path) {
        m_pendingNavigationPath = path;
        emit pendingNavigationPathChanged();
    }
    if (m_navigationPending == pending) {
        return;
    }
    m_navigationPending = pending;
    emit navigationPendingChanged();
}

void FilePanelController::setStatusMessage(const QString &message)
{
    m_statusMessage = message;
    emit statusMessageChanged();
}

void FilePanelController::showStatusMessage(const QString &message)
{
    setStatusMessage(message);
}

void FilePanelController::setLastError(const QVariantMap &error)
{
    if (m_lastError == error) {
        return;
    }
    m_lastError = error;
    emit lastErrorChanged();
}

void FilePanelController::setOperationError(const QString &message, const QString &path, const QString &operation)
{
    setStatusMessage(message);
    setLastError(FileError::classify(message, path, operation));
}

int FilePanelController::viewMode() const
{
    return m_viewMode;
}

void FilePanelController::setViewMode(int mode)
{
    if (m_viewMode == mode) return;
    m_viewMode = mode;
    emit viewModeChanged();
}
