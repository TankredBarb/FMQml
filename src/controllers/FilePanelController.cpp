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
#include "../core/LinuxAdminBroker.h"
#include "../core/LocalFileProvider.h"
#include "../core/MetadataExtractor.h"
#include "../core/TerminalLauncher.h"
#include "../core/WallpaperSetter.h"
#include "../core/DriveUtils.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileError.h"
#include "../core/VolumeMonitor.h"

namespace {
bool filePanelNavTraceEnabled()
{
    static const bool enabled = qEnvironmentVariableIsSet("FM_NAV_TRACE");
    return enabled;
}

void traceFilePanelNav(const char *stage, const QString &path = {}, const QString &detail = {})
{
    if (!filePanelNavTraceEnabled()) {
        return;
    }

    qInfo().noquote() << "[FM_NAV][panel]" << stage
                      << "path=" << QDir::toNativeSeparators(path)
                      << detail;
}

QString archiveExtractionBaseName(const QString &fileName)
{
    const QString lower = fileName.toLower();
    const QStringList compoundSuffixes = {
        QStringLiteral(".tar.gz"),
        QStringLiteral(".tgz"),
        QStringLiteral(".tar.xz"),
        QStringLiteral(".txz"),
        QStringLiteral(".tar.bz2"),
        QStringLiteral(".tbz"),
        QStringLiteral(".tbz2"),
        QStringLiteral(".tar.zst"),
        QStringLiteral(".tzst"),
    };
    for (const QString &suffix : compoundSuffixes) {
        if (lower.endsWith(suffix) && fileName.size() > suffix.size()) {
            return fileName.left(fileName.size() - suffix.size());
        }
    }

    const QString baseName = QFileInfo(fileName).completeBaseName();
    return baseName.isEmpty() ? fileName : baseName;
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

struct NavigationResolution {
    enum class Type {
        OpenPath,
        MountIso,
        Invalid
    };

    Type type = Type::Invalid;
    QString path;
    QString error;
    QString traceType;
};

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

using SuggestionCancelCheck = std::function<bool()>;
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

QStringList suggestionPaths(const QVariantList &entries)
{
    QStringList paths;
    paths.reserve(entries.size());
    for (const QVariant &entry : entries) {
        const QString path = entry.toMap().value(QStringLiteral("path")).toString();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

QVariantList directorySuggestionEntriesForInput(const QString &inputPath,
                                                const QString &currentPath,
                                                qsizetype maxSuggestions,
                                                const SuggestionCancelCheck &shouldCancel = {})
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

QStringList directorySuggestionsForInput(const QString &inputPath,
                                         const QString &currentPath,
                                         qsizetype maxSuggestions,
                                         const SuggestionCancelCheck &shouldCancel = {})
{
    return suggestionPaths(directorySuggestionEntriesForInput(inputPath, currentPath, maxSuggestions, shouldCancel));
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
            emit detailsSortRoleChanged();
        }
    });
    connect(&m_directoryModel, &DirectoryModel::sortOrderChanged, this, [this]() {
        if (m_panelSortOrder != m_directoryModel.sortOrder()) {
            m_panelSortOrder = m_directoryModel.sortOrder();
            emit panelSortOrderChanged();
            emit detailsSortOrderChanged();
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

DirectoryModel::SortRole FilePanelController::detailsSortRole() const
{
    return panelSortRole();
}

void FilePanelController::setDetailsSortRole(DirectoryModel::SortRole role)
{
    setPanelSortRole(role);
}

DirectoryModel::SortRole FilePanelController::panelSortRole() const
{
    return m_panelSortRole;
}

void FilePanelController::setPanelSortRole(DirectoryModel::SortRole role)
{
    setPanelSortPolicy(int(role), int(m_panelSortOrder));
}

Qt::SortOrder FilePanelController::detailsSortOrder() const
{
    return panelSortOrder();
}

void FilePanelController::setDetailsSortOrder(Qt::SortOrder order)
{
    setPanelSortOrder(order);
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
        emit detailsSortRoleChanged();
    }
    if (orderChanged) {
        emit panelSortOrderChanged();
        emit detailsSortOrderChanged();
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

bool FilePanelController::isIsoImageFilePath(const QString &path) const
{
    return IsoSupport::isIsoImageExtension(QFileInfo(path).suffix().toLower());
}

QString FilePanelController::archiveExtractionFolderNameForPath(const QString &path) const
{
    if (!isArchiveFilePath(path)) {
        return {};
    }

    const QString fileName = fileNameForPath(path);
    if (fileName.isEmpty()) {
        return {};
    }

    return archiveExtractionBaseName(fileName);
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

bool FilePanelController::openPath(const QString &path)
{
    const QString current = currentPath();
    const QString preprocessed = FileProviderFactory::preprocessPath(expandHomeShortcutPath(path));
    const QString normalized = FileProviderFactory::normalizePath(preprocessed.isEmpty() ? path : preprocessed);
    const bool loadMoreForCurrentPath = current.startsWith(QStringLiteral("instagram://"), Qt::CaseInsensitive)
        && normalized == current + QStringLiteral("/__load_more__");
    return requestOpenPath(path, true, loadMoreForCurrentPath);
}

bool FilePanelController::openPathPreservingScroll(const QString &path)
{
    return requestOpenPath(path, true, true);
}

bool FilePanelController::openStartupRestoredFolder(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()
        || !normalizedVirtualRoot(trimmedPath).isEmpty()
        || ArchiveSupport::isArchivePath(trimmedPath)
        || ArchiveSupport::isArchiveFilePath(trimmedPath)
        || IsoSupport::isIsoImagePath(trimmedPath)) {
        return requestOpenPath(path, true);
    }

    const std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(trimmedPath);
    if (!provider || !provider->pathExists(trimmedPath) || !provider->isDirectory(trimmedPath)) {
        return requestOpenPath(path, true);
    }

    ++m_navigationRequestId;
    setNavigationPending(false);
    return openPathInternal(trimmedPath, true);
}

bool FilePanelController::requestOpenPath(const QString &path, bool addToHistory, bool preserveScroll)
{
    cancelDirectorySuggestions();
    QElapsedTimer totalTimer;
    totalTimer.start();
    if (filePanelNavTraceEnabled()) {
        traceFilePanelNav("openPath-begin", path,
                          QStringLiteral("current=%1").arg(QDir::toNativeSeparators(currentPath())));
    }

    if (path.isEmpty()) {
        ++m_navigationRequestId;
        setNavigationPending(false);
        traceFilePanelNav("openPath-end", path, QStringLiteral("result=false reason=empty elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }

    const QString preprocessedPath = FileProviderFactory::preprocessPath(expandHomeShortcutPath(path));
    if (preprocessedPath.isEmpty()) {
        ++m_navigationRequestId;
        setNavigationPending(false);
        traceFilePanelNav("openPath-end", path, QStringLiteral("result=false reason=blank elapsedMs=%1").arg(totalTimer.elapsed()));
        return false;
    }
    const QString virtualRoot = normalizedVirtualRoot(preprocessedPath);
    if (!virtualRoot.isEmpty()) {
        ++m_navigationRequestId;
        setNavigationPending(false);
        const bool result = openPathInternal(virtualRoot, addToHistory, preserveScroll);
        traceFilePanelNav("openPath-end", virtualRoot,
                          QStringLiteral("result=%1 type=virtual elapsedMs=%2").arg(result).arg(totalTimer.elapsed()));
        return result;
    }

    const QString approvalTarget = nestedArchiveApprovalTarget(preprocessedPath);
    const QString approvalScope = nestedArchiveScopeKeyForPath(approvalTarget);
    if (!approvalScope.isEmpty()
        && !m_approvedNestedArchiveScopeKeys.contains(approvalScope)
        && !ArchiveFileProvider::hasCachedContainerForPath(approvalTarget)) {
        ++m_navigationRequestId;
        setNavigationPending(false);
        emit nestedArchiveOpenRequested(approvalTarget,
                                        nestedArchiveDisplayNameForPath(approvalTarget),
                                        nestedArchiveSizeTextForPath(approvalTarget));
        traceFilePanelNav("openPath-end", approvalTarget,
                          QStringLiteral("result=true reason=nested-approval elapsedMs=%1").arg(totalTimer.elapsed()));
        return true;
    }
    if (!approvalScope.isEmpty()) {
        m_approvedNestedArchiveScopeKeys.insert(approvalScope);
        if (!ArchiveFileProvider::hasCachedContainerForPath(approvalTarget)) {
            setStatusMessage(nestedArchivePreparationStatusForPath(approvalTarget));
        }
    }

    const int requestId = ++m_navigationRequestId;
    setNavigationPending(true, preprocessedPath);
    QPointer<FilePanelController> self(this);
    (void)QtConcurrent::run([self, preprocessedPath, requestId, addToHistory, preserveScroll]() {
        QElapsedTimer resolverTimer;
        resolverTimer.start();
        const NavigationResolution resolution = resolveNavigationPath(preprocessedPath);
        traceFilePanelNav("openPath-resolver-finished", preprocessedPath,
                          QStringLiteral("requestId=%1 type=%2 elapsedMs=%3")
                              .arg(requestId)
                              .arg(resolution.traceType)
                              .arg(resolverTimer.elapsed()));
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(),
                                  [self, preprocessedPath, requestId, addToHistory, preserveScroll, resolution]() {
            if (!self || requestId != self->m_navigationRequestId) {
                return;
            }

            switch (resolution.type) {
            case NavigationResolution::Type::Invalid:
                self->setNavigationPending(false);
                self->setOperationError(resolution.error.isEmpty()
                                             ? QStringLiteral("Path is invalid, unavailable, or not a folder.")
                                             : resolution.error,
                                         preprocessedPath,
                                         QStringLiteral("open"));
                emit self->pathNavigationFailed(preprocessedPath);
                return;
            case NavigationResolution::Type::MountIso:
                self->setNavigationPending(false);
                emit self->isoMountRequested(resolution.path);
                return;
            case NavigationResolution::Type::OpenPath:
                break;
            }

            const bool result = self->openPathInternal(resolution.path, addToHistory, preserveScroll);
            self->setNavigationPending(false);
            traceFilePanelNav("openPath-end", resolution.path,
                              QStringLiteral("result=%1 type=%2 requestId=%3")
                                  .arg(result)
                                  .arg(resolution.traceType)
                                  .arg(requestId));
        }, Qt::QueuedConnection);
    });

    traceFilePanelNav("openPath-end", preprocessedPath,
                      QStringLiteral("result=true reason=queued requestId=%1 elapsedMs=%2")
                          .arg(requestId)
                          .arg(totalTimer.elapsed()));
    return true;
}

bool FilePanelController::canOpenPath(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    const QString trimmedPath = path.trimmed();
    if (trimmedPath.isEmpty()) {
        return false;
    }
    if (!normalizedVirtualRoot(trimmedPath).isEmpty()) {
        return true;
    }

    if (ArchiveSupport::isArchivePath(trimmedPath)) {
        return true;
    }

    return true;
}

bool FilePanelController::openSearchResult(const QString &path, bool isDirectory)
{
    if (isVirtualRoot() || path.trimmed().isEmpty()) {
        return false;
    }
    if (isDirectory) {
        return openPath(path);
    }

    const QString parentPath = parentPathForPath(path);
    if (parentPath.isEmpty()) {
        return false;
    }
    scheduleCreatedEntryReveal(path);
    return openPath(parentPath);
}

bool FilePanelController::openNestedArchivePath(const QString &path)
{
    if (isVirtualRoot() || !ArchiveSupport::isArchivePath(path)) {
        return false;
    }

    const QString targetPath = nestedArchiveApprovalTarget(path);
    const QString approvalScope = nestedArchiveScopeKeyForPath(targetPath);
    if (approvalScope.isEmpty()) {
        return false;
    }

    const QString archiveName = ArchiveSupport::archiveFileName(targetPath);
    const QString archiveSuffix = QFileInfo(archiveName).suffix().toLower();
    if (!ArchiveSupport::isArchiveExtension(archiveSuffix)) {
        return false;
    }

    m_approvedNestedArchiveScopeKeys.insert(approvalScope);
    setStatusMessage(nestedArchivePreparationStatusForPath(targetPath));
    return requestOpenPath(targetPath, true);
}

void FilePanelController::submitArchivePassword(const QString &path, const QString &password)
{
    const QString trimmedPath = path.trimmed();
    if (!ArchiveSupport::isArchivePath(trimmedPath)) {
        return;
    }

    ArchiveFileProvider::setPasswordForPath(trimmedPath, password);
    const QString approvalScope = nestedArchiveScopeKeyForPath(trimmedPath);
    if (!approvalScope.isEmpty()) {
        m_approvedNestedArchiveScopeKeys.insert(approvalScope);
    }
    setStatusMessage(QStringLiteral("Opening archive..."));
    requestOpenPath(trimmedPath, false);
}

void FilePanelController::cancelArchivePassword(const QString &path)
{
    const QString trimmedPath = path.trimmed();
    if (!ArchiveSupport::isArchivePath(trimmedPath)) {
        return;
    }

    ArchiveFileProvider::clearPasswordForPath(trimmedPath);
    setOperationError(QStringLiteral("Archive password required"), trimmedPath, QStringLiteral("open"));
    emit pathNavigationFailed(trimmedPath);
}

void FilePanelController::cancelCurrentLoad()
{
    if (!m_directoryModel.loading()) {
        if (m_navigationPending) {
            const QString cancelledPath = m_pendingNavigationPath;
            const QString cancelledScope = nestedArchiveScopeKeyForPath(cancelledPath);
            if (!cancelledScope.isEmpty()) {
                m_approvedNestedArchiveScopeKeys.remove(cancelledScope);
                ArchiveFileProvider::invalidateCacheForPath(cancelledPath);
            }
            ++m_navigationRequestId;
            setStatusMessage(QStringLiteral("Archive preparation was cancelled"));
            setNavigationPending(false);
        }
        return;
    }

    const QString cancelledPath = currentPath();
    const QString cancelledScope = nestedArchiveScopeKeyForPath(cancelledPath);
    if (!cancelledScope.isEmpty()) {
        m_approvedNestedArchiveScopeKeys.remove(cancelledScope);
        ArchiveFileProvider::invalidateCacheForPath(cancelledPath);
    }

    m_directoryModel.cancelLoading();
    setStatusMessage(QStringLiteral("Archive preparation was cancelled"));
    setNavigationPending(false);

    if (!m_backStack.isEmpty()) {
        const QString previous = m_backStack.takeLast();
        requestOpenPath(previous, false, true);
        emit historyChanged();
    }
}

void FilePanelController::openRow(int row)
{
    if (isVirtualRoot()) return;
    if (m_directoryModel.isShortcutAt(row)
        && m_directoryModel.shortcutTargetIsDirectoryAt(row)
        && (!m_directoryModel.shortcutOpenPathAt(row).isEmpty()
            || !m_directoryModel.shortcutTargetPathAt(row).isEmpty())) {
        const QString openPathForShortcut = m_directoryModel.shortcutOpenPathAt(row);
        openPath(openPathForShortcut.isEmpty() ? m_directoryModel.shortcutTargetPathAt(row) : openPathForShortcut);
        return;
    }
    if (!m_directoryModel.isDirectoryAt(row)) {
        return;
    }
    openPath(m_directoryModel.pathAt(row));
}

void FilePanelController::openItem(int row)
{
    if (isVirtualRoot()) return;
    const bool shortcut = m_directoryModel.isShortcutAt(row);
    const QString itemPath = m_directoryModel.pathAt(row);
    QString shortcutTargetPath = shortcut ? m_directoryModel.shortcutOpenPathAt(row) : QString{};
    if (shortcutTargetPath.isEmpty() && shortcut) {
        shortcutTargetPath = m_directoryModel.shortcutTargetPathAt(row);
    }
    const bool shortcutTargetIsDirectory = shortcut && m_directoryModel.shortcutTargetIsDirectoryAt(row);
    const QString path = shortcutTargetIsDirectory
        ? shortcutTargetPath
        : itemPath;
    if (!path.isEmpty()) {
        if (shortcutTargetIsDirectory || (!shortcut && m_directoryModel.isDirectoryAt(row))) {
            openPath(path);
            return;
        }

        const QString suffix = QFileInfo(path).suffix().toLower();
        if (IsoSupport::isIsoImageExtension(suffix)) {
            emit isoMountRequested(path);
            return;
        }

        if (ArchiveSupport::isArchivePath(path)) {
            const QString archiveSuffix = QFileInfo(ArchiveSupport::archiveFileName(path)).suffix().toLower();
            if (ArchiveSupport::isArchiveExtension(archiveSuffix)) {
                const QString targetPath = nestedArchiveApprovalTarget(path);
                const QString approvalScope = nestedArchiveScopeKeyForPath(targetPath);
                if (!approvalScope.isEmpty()
                    && (m_approvedNestedArchiveScopeKeys.contains(approvalScope)
                        || ArchiveFileProvider::hasCachedContainerForPath(targetPath))) {
                    m_approvedNestedArchiveScopeKeys.insert(approvalScope);
                    openPath(targetPath);
                    return;
                }
                emit nestedArchiveOpenRequested(targetPath,
                                                nestedArchiveDisplayNameForPath(targetPath),
                                                nestedArchiveSizeTextForPath(targetPath));
                return;
            }
        }

        if (ArchiveSupport::isArchiveExtension(suffix)) {
            openPath(path);
            return;
        }

        if (isProviderUriPath(path)) {
            setStatusMessage(QStringLiteral("This provider does not support direct file launch."));
            return;
        }

        const LaunchService::LaunchResult launchResult = LaunchService::openPath(path);
        if (!launchResult.ok) {
            setStatusMessage(launchResult.message.isEmpty()
                                 ? QStringLiteral("Could not open file.")
                                 : launchResult.message);
            setLastError(launchErrorInfo(launchResult, path));
        }
    }
}

QVariantMap FilePanelController::launchCapabilitiesForPath(const QString &path) const
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    return LaunchService::launchCapabilitiesMap(path);
}

void FilePanelController::openPathWithWine(const QString &path)
{
    if (isVirtualRoot() || path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        return;
    }

    const LaunchService::LaunchResult result = LaunchService::openWithWine(path);
    if (!result.ok) {
        setStatusMessage(result.message.isEmpty() ? QStringLiteral("Could not open file with Wine.") : result.message);
        setLastError(launchErrorInfo(result, path));
    } else {
        setLastError({});
    }
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

void FilePanelController::goBack()
{
    if (m_backStack.isEmpty()) {
        return;
    }

    const QString previous = m_backStack.takeLast();
    if (!currentPath().isEmpty()) {
        m_forwardStack.append(currentPath());
    }
    requestOpenPath(previous, false, true);
    emit historyChanged();
}

void FilePanelController::goForward()
{
    if (m_forwardStack.isEmpty()) {
        return;
    }

    const QString next = m_forwardStack.takeLast();
    if (!currentPath().isEmpty()) {
        m_backStack.append(currentPath());
    }
    requestOpenPath(next, false);
    emit historyChanged();
}

void FilePanelController::goUp()
{
    if (isVirtualRoot()) {
        return; // Already at the top
    }
    const QString cp = currentPath();
    const QString parent = parentPathForPath(cp);
    // If parent == current, we are at the drive root — go to devices://
    if (parent.isEmpty() || parent == cp) {
        if (isProviderUriPath(cp)) {
            return;
        }
        openPath(QString(DEVICE_ROOT));
    } else {
        requestOpenPath(parent, true, true);
    }
}

bool FilePanelController::rename(int row, const QString &newName)
{
    if (isVirtualRoot()) {
        return false;
    }
    const QString oldPath = m_directoryModel.pathAt(row);
    if (oldPath.isEmpty()) {
        return false;
    }
    if (!(m_fileProvider->capabilities() & FileProvider::Rename)
        || !pathCanCreateChildren(currentPath())
        || !pathCanDelete(oldPath)) {
        setOperationError(QStringLiteral("You do not have permission to rename this item here."),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    return renamePath(oldPath, newName);
}

bool FilePanelController::renamePath(const QString &oldPath, const QString &newName)
{
    if (isVirtualRoot()) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(oldPath)) {
        setOperationError(QStringLiteral("Archive contents are read-only"),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }
    if (oldPath.isEmpty()) {
        return false;
    }
    if (!(m_fileProvider->capabilities() & FileProvider::Rename)
        || !pathCanCreateChildren(m_fileProvider->parentPath(oldPath))
        || !pathCanDelete(oldPath)) {
        setOperationError(QStringLiteral("You do not have permission to rename this item here."),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    if (m_fileProvider->renamePath(oldPath, newName)) {
        setLastError({});
        const QString trimmedName = newName.trimmed();
        const QString newPath = m_fileProvider->childPath(m_fileProvider->parentPath(oldPath), trimmedName);
        FileAccessResolver::invalidate(oldPath);
        FileAccessResolver::invalidate(newPath);
        FileAccessResolver::invalidate(m_fileProvider->parentPath(oldPath));
        if (!m_directoryModel.renamePath(oldPath, newPath)) {
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
        }
        emit entryRenamed(oldPath, newPath);
        emit contentsChanged(m_fileProvider->parentPath(oldPath));
        return true;
    }

    const QString renameMessage = m_fileProvider->lastErrorString().isEmpty()
        ? QStringLiteral("Cannot rename %1").arg(QDir::toNativeSeparators(oldPath))
        : m_fileProvider->lastErrorString();
    setOperationError(renameMessage,
                      oldPath,
                      QStringLiteral("rename"));
    return false;
}

bool FilePanelController::renameAsAdministrator(int row, const QString &newName)
{
#ifdef Q_OS_LINUX
    if (isVirtualRoot()) {
        return false;
    }

    const QString oldPath = m_directoryModel.pathAt(row);
    const QString trimmedName = newName.trimmed();
    if (oldPath.isEmpty() || trimmedName.isEmpty()) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(oldPath) || isProviderUriPath(oldPath)) {
        setOperationError(QStringLiteral("Administrator rename is available for local items only"),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }
    if (trimmedName.contains(QLatin1Char('/')) || trimmedName.contains(QLatin1Char('\\'))) {
        setOperationError(QStringLiteral("The new name is invalid"),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    const QString parentPath = m_fileProvider->parentPath(oldPath);
    const QString newPath = m_fileProvider->childPath(parentPath, trimmedName);
    if (samePanelFilesystemPath(oldPath, newPath)) {
        return true;
    }
    if (m_fileProvider->pathExists(newPath)) {
        setOperationError(QStringLiteral("Cannot rename %1: an item with the same name already exists")
                              .arg(QDir::toNativeSeparators(oldPath)),
                          oldPath,
                          QStringLiteral("rename"));
        return false;
    }

    LinuxAdminBroker::Request request;
    request.operation = LinuxAdminBroker::Operation::RenamePath;
    request.sourcePath = oldPath;
    request.destinationPath = newPath;
    const LinuxAdminBroker::Result result = submitLinuxAdminRequest(request);
    if (!result.success) {
        setOperationError(result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage,
                          result.failedPath.isEmpty() ? oldPath : result.failedPath,
                          QStringLiteral("rename"));
        return false;
    }

    setLastError({});
    FileAccessResolver::invalidate(oldPath);
    FileAccessResolver::invalidate(newPath);
    FileAccessResolver::invalidate(parentPath);
    if (!m_directoryModel.renamePath(oldPath, newPath)) {
        refresh();
    } else {
        m_directoryModel.noteLocalMutation();
    }
    emit entryRenamed(oldPath, newPath);
    emit administratorOperationSucceeded();
    emit contentsChanged(parentPath);
    return true;
#else
    Q_UNUSED(row)
    Q_UNUSED(newName)
    return false;
#endif
}

QVariantList FilePanelController::previewBatchRename(const QStringList &paths, const QVariantList &rules)
{
    QList<BatchRenameEngine::RenamePreview> previews = m_renameEngine.generatePreview(paths, rules);
    QVariantList result;
    for (const auto &p : previews) {
        QVariantMap map;
        map["oldPath"] = p.oldPath;
        map["oldName"] = p.oldName;
        map["newName"] = p.newName;
        map["newPath"] = p.newPath;
        map["hasConflict"] = p.hasConflict;
        map["error"] = p.error;
        result.append(map);
    }
    return result;
}

QVariantList FilePanelController::applyBatchRename(const QStringList &paths, const QVariantList &rules)
{
    if (isVirtualRoot()
        || !(m_fileProvider->capabilities() & FileProvider::Rename)
        || !pathCanCreateChildren(currentPath())) {
        QVariantList results;
        for (const QString &path : paths) {
            const QString oldName = fileNameForPath(path);
            QVariantMap map;
            map["oldPath"] = path;
            map["oldName"] = oldName;
            map["newName"] = oldName;
            map["newPath"] = path;
            map["success"] = false;
            map["error"] = QStringLiteral("Cannot rename items in this location");
            results.append(map);
        }
        setOperationError(QStringLiteral("You do not have permission to rename items here."),
                          currentPath(),
                          QStringLiteral("rename"));
        return results;
    }

    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path)) {
            setStatusMessage(QStringLiteral("Archive contents are read-only"));
            return {};
        }
        if (!pathCanDelete(path)) {
            QVariantList results;
            QList<BatchRenameEngine::RenamePreview> previews = m_renameEngine.generatePreview(paths, rules);
            for (const auto &p : previews) {
                QVariantMap map;
                map["oldPath"] = p.oldPath;
                map["oldName"] = p.oldName;
                map["newName"] = p.newName;
                map["newPath"] = p.newPath;
                map["success"] = false;
                map["error"] = p.oldPath == path
                    ? QStringLiteral("Permission denied")
                    : QStringLiteral("Cancelled due to permission failure");
                results.append(map);
            }
            setOperationError(QStringLiteral("You do not have permission to rename one or more selected items."),
                              path,
                              QStringLiteral("rename"));
            return results;
        }
    }

    QList<BatchRenameEngine::RenamePreview> previews = m_renameEngine.generatePreview(paths, rules);
    QVariantList results;
    
    // Check conflicts first
    bool hasAnyConflict = false;
    for (const auto &p : previews) {
        if (p.hasConflict) {
            hasAnyConflict = true;
            break;
        }
    }
    
    if (hasAnyConflict) {
        for (const auto &p : previews) {
            QVariantMap map;
            map["oldPath"] = p.oldPath;
            map["oldName"] = p.oldName;
            map["newName"] = p.newName;
            map["newPath"] = p.newPath;
            map["success"] = false;
            map["error"] = p.hasConflict ? p.error : QStringLiteral("Cancelled due to other conflicts");
            results.append(map);
        }
        return results;
    }

    bool allSuccess = true;
    for (const auto &p : previews) {
        QVariantMap map;
        map["oldPath"] = p.oldPath;
        map["oldName"] = p.oldName;
        map["newName"] = p.newName;
        map["newPath"] = p.newPath;

        if (p.newName == p.oldName) {
            map["success"] = true;
            map["error"] = QString();
        } else {
            if (m_fileProvider->renamePath(p.oldPath, p.newName)) {
                FileAccessResolver::invalidate(p.oldPath);
                FileAccessResolver::invalidate(p.newPath);
                FileAccessResolver::invalidate(m_fileProvider->parentPath(p.oldPath));
                if (!m_directoryModel.renamePath(p.oldPath, p.newPath)) {
                    // refresh at the end
                }
                emit entryRenamed(p.oldPath, p.newPath);
                map["success"] = true;
                map["error"] = QString();
            } else {
                allSuccess = false;
                map["success"] = false;
                map["error"] = QStringLiteral("Rename failed (system error)");
            }
        }
        results.append(map);
    }
    
    if (!allSuccess) {
        setStatusMessage(QStringLiteral("Some files could not be renamed"));
    }
    
    refresh();
    return results;
}

bool FilePanelController::createFolder(const QString &name)
{
    if (isVirtualRoot()) {
        return false;
    }
    if (!canCreateInCurrentPath()) {
        setOperationError(QStringLiteral("You do not have permission to create items in this location."),
                          currentPath(),
                          QStringLiteral("createFolder"));
        return false;
    }
    QString path;
    if (m_fileProvider->createFolder(currentPath(), name, &path)) {
        setLastError({});
        FileAccessResolver::invalidate(currentPath());
        FileAccessResolver::invalidate(path);
        const bool inserted = m_directoryModel.insertPath(path);
        if (!inserted) {
            scheduleCreatedEntryReveal(path);
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
            scheduleCreatedEntryReveal(path);
        }
        setStatusMessage(QStringLiteral("\"%1\" created").arg(m_fileProvider->fileName(path)));
        emit entryCreated(path);
        emit contentsChanged(currentPath());
        return true;
    }
    const QString folderMessage = m_fileProvider->lastErrorString().isEmpty()
        ? QStringLiteral("Cannot create folder in %1").arg(QDir::toNativeSeparators(currentPath()))
        : m_fileProvider->lastErrorString();
    setOperationError(folderMessage,
                      currentPath(),
                      QStringLiteral("createFolder"));
    return false;
}

bool FilePanelController::createFolderAsAdministrator(const QString &name)
{
#ifdef Q_OS_LINUX
    if (isVirtualRoot()) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(currentPath()) || isProviderUriPath(currentPath())) {
        setOperationError(QStringLiteral("Administrator folder creation is available for local folders only"),
                          currentPath(),
                          QStringLiteral("createFolder"));
        return false;
    }

    const QString folderName = uniqueCreationName(m_fileProvider.get(), currentPath(), name, false);
    if (folderName.isEmpty()) {
        return false;
    }

    const QString path = m_fileProvider->childPath(currentPath(), folderName);
    LinuxAdminBroker::Request request;
    request.operation = LinuxAdminBroker::Operation::MakeDirectory;
    request.destinationPath = path;
    const LinuxAdminBroker::Result result = submitLinuxAdminRequest(request);
    if (!result.success) {
        setOperationError(result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage,
                          result.failedPath.isEmpty() ? path : result.failedPath,
                          QStringLiteral("createFolder"));
        return false;
    }

    setLastError({});
    FileAccessResolver::invalidate(currentPath());
    FileAccessResolver::invalidate(path);
    const bool inserted = m_directoryModel.insertPath(path);
    if (!inserted) {
        scheduleCreatedEntryReveal(path);
        refresh();
    } else {
        m_directoryModel.noteLocalMutation();
        scheduleCreatedEntryReveal(path);
    }
    setStatusMessage(QStringLiteral("\"%1\" created").arg(m_fileProvider->fileName(path)));
    emit entryCreated(path);
    emit administratorOperationSucceeded();
    emit contentsChanged(currentPath());
    return true;
#else
    Q_UNUSED(name)
    return false;
#endif
}

bool FilePanelController::createFile(const QString &name)
{
    if (isVirtualRoot()) {
        return false;
    }
    if (!canCreateInCurrentPath()) {
        setOperationError(QStringLiteral("You do not have permission to create items in this location."),
                          currentPath(),
                          QStringLiteral("createFile"));
        return false;
    }
    QString path;
    if (m_fileProvider->createFile(currentPath(), name, &path)) {
        setLastError({});
        FileAccessResolver::invalidate(currentPath());
        FileAccessResolver::invalidate(path);
        const bool inserted = m_directoryModel.insertPath(path);
        if (!inserted) {
            scheduleCreatedEntryReveal(path);
            refresh();
        } else {
            m_directoryModel.noteLocalMutation();
            scheduleCreatedEntryReveal(path);
        }
        setStatusMessage(QStringLiteral("\"%1\" created").arg(m_fileProvider->fileName(path)));
        emit entryCreated(path);
        emit contentsChanged(currentPath());
        return true;
    }
    const QString fileMessage = m_fileProvider->lastErrorString().isEmpty()
        ? QStringLiteral("Cannot create file in %1").arg(QDir::toNativeSeparators(currentPath()))
        : m_fileProvider->lastErrorString();
    setOperationError(fileMessage,
                      currentPath(),
                      QStringLiteral("createFile"));
    return false;
}

bool FilePanelController::createFileAsAdministrator(const QString &name)
{
#ifdef Q_OS_LINUX
    if (isVirtualRoot()) {
        return false;
    }
    if (ArchiveSupport::isArchivePath(currentPath()) || isProviderUriPath(currentPath())) {
        setOperationError(QStringLiteral("Administrator file creation is available for local folders only"),
                          currentPath(),
                          QStringLiteral("createFile"));
        return false;
    }

    QString fileName = uniqueCreationName(m_fileProvider.get(), currentPath(), name, true);
    if (fileName.isEmpty()) {
        return false;
    }

    const QString path = m_fileProvider->childPath(currentPath(), fileName);
    LinuxAdminBroker::Request request;
    request.operation = LinuxAdminBroker::Operation::CreateFile;
    request.destinationPath = path;
    const LinuxAdminBroker::Result result = submitLinuxAdminRequest(request);
    if (!result.success) {
        setOperationError(result.errorMessage.isEmpty() ? result.errorCode : result.errorMessage,
                          result.failedPath.isEmpty() ? path : result.failedPath,
                          QStringLiteral("createFile"));
        return false;
    }

    setLastError({});
    FileAccessResolver::invalidate(currentPath());
    FileAccessResolver::invalidate(path);
    const bool inserted = m_directoryModel.insertPath(path);
    if (!inserted) {
        scheduleCreatedEntryReveal(path);
        refresh();
    } else {
        m_directoryModel.noteLocalMutation();
        scheduleCreatedEntryReveal(path);
    }
    setStatusMessage(QStringLiteral("\"%1\" created").arg(m_fileProvider->fileName(path)));
    emit entryCreated(path);
    emit administratorOperationSucceeded();
    emit contentsChanged(currentPath());
    return true;
#else
    Q_UNUSED(name)
    return false;
#endif
}

void FilePanelController::scheduleCreatedEntryReveal(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }
    m_pendingCreatedEntryRevealPath = path;
    m_createdEntryRevealAttempts = 0;
    m_createdEntryRevealTimer.start();
}

QString FilePanelController::fileNameForPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveFileName(path);
    }
    if (isProviderUriPath(path)) {
        if (std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path)) {
            return provider->fileName(path);
        }
    }
    return m_fileProvider->fileName(path);
}

QString FilePanelController::parentPathForPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveParentPath(path);
    }
    if (isProviderUriPath(path)) {
        if (std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path)) {
            return provider->parentPath(path);
        }
        return {};
    }
    return m_fileProvider->parentPath(path);
}

QString FilePanelController::childPathForCurrent(const QString &name) const
{
    if (ArchiveSupport::isArchivePath(currentPath())) {
        return ArchiveSupport::archiveChildPath(currentPath(), name);
    }
    if (isProviderUriPath(currentPath())) {
        if (std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(currentPath())) {
            return provider->childPath(currentPath(), name);
        }
        return {};
    }
    return m_fileProvider->childPath(currentPath(), name);
}

QString FilePanelController::childPathForPath(const QString &parentPath, const QString &name) const
{
    if (ArchiveSupport::isArchivePath(parentPath)) {
        return ArchiveSupport::archiveChildPath(parentPath, name);
    }
    if (isProviderUriPath(parentPath)) {
        if (std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(parentPath)) {
            return provider->childPath(parentPath, name);
        }
        return {};
    }
    return m_fileProvider->childPath(parentPath, name);
}

QStringList FilePanelController::breadcrumbPathsForPath(const QString &path) const
{
    QStringList result;
    if (path.isEmpty() || path == QString(DEVICE_ROOT) || path == QString(FAVORITES_ROOT)) {
        return result;
    }

    if (ArchiveSupport::isArchivePath(path)) {
        const QStringList tokens = ArchiveSupport::splitArchiveTokens(path);
        if (tokens.isEmpty()) {
            return result;
        }

        const QString physicalPath = QDir::fromNativeSeparators(tokens.first().trimmed());
        if (physicalPath.isEmpty()) {
            return result;
        }

        // Get breadcrumbs for the containing local folder
        const QString parentDir = QDir::fromNativeSeparators(QFileInfo(physicalPath).absoluteDir().absolutePath());
        result = breadcrumbPathsForPath(parentDir);

        // Append the outer archive root path
        result.append(ArchiveSupport::archiveRootPath(physicalPath));

        const int n = tokens.size();
        // Append intermediate nested archives if any
        for (int i = 1; i < n - 1; ++i) {
            QStringList subTokens = tokens.mid(0, i + 1);
            result.append(QStringLiteral("archive://") + subTokens.join(QLatin1Char('|')) + QStringLiteral("|/"));
        }

        // Append paths inside the innermost archive
        QString browse = QDir::fromNativeSeparators(tokens.last().trimmed());
        if (browse != QLatin1String("/") && !browse.isEmpty()) {
            if (browse.startsWith(QLatin1Char('/'))) {
                browse.remove(0, 1);
            }
            if (browse.endsWith(QLatin1Char('/'))) {
                browse.chop(1);
            }
            if (!browse.isEmpty()) {
                const QString innerArchiveRoot = QStringLiteral("archive://") + tokens.mid(0, n - 1).join(QLatin1Char('|')) + QStringLiteral("|/");
                const QStringList browseParts = browse.split(QLatin1Char('/'), Qt::SkipEmptyParts);
                QString rel;
                for (const QString &part : browseParts) {
                    if (!rel.isEmpty()) {
                        rel += QLatin1Char('/');
                    }
                    rel += part;
                    result.append(innerArchiveRoot + rel);
                }
            }
        }
        return result;
    }

    if (isProviderUriPath(path)) {
        std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
        const QString normalized = provider ? provider->normalizedPath(path) : FileProviderFactory::normalizePath(path);
        const QString providerPath = normalized.isEmpty() ? path.trimmed() : normalized;
        const QString scheme = provider ? provider->scheme() : uriSchemeForPath(providerPath);
        if (scheme.isEmpty()) {
            return result;
        }

        const QString schemeRoot = scheme + QStringLiteral("://");
        const QString normalizedRoot = provider ? provider->normalizedPath(schemeRoot) : QString{};
        const QString rootPath = normalizedRoot.isEmpty() ? schemeRoot : normalizedRoot;
        if (provider && !providerPath.isEmpty()) {
            QStringList chain;
            QSet<QString> seen;
            QString current = providerPath;
            for (int depth = 0; depth < 64 && !current.isEmpty(); ++depth) {
                const QString normalizedCurrent = provider->normalizedPath(current);
                current = normalizedCurrent.isEmpty() ? current.trimmed() : normalizedCurrent;
                if (current.isEmpty() || seen.contains(current)) {
                    break;
                }
                seen.insert(current);
                chain.prepend(current);
                if (current == rootPath) {
                    break;
                }
                const QString parent = provider->parentPath(current);
                if (parent.isEmpty() || parent == current) {
                    break;
                }
                current = parent;
            }
            if (!chain.isEmpty() && chain.constFirst() != rootPath) {
                chain.prepend(rootPath);
            }
            if (!chain.isEmpty()) {
                return chain;
            }
        }

        result.append(rootPath);
        if (providerPath != rootPath) {
            result.append(providerPath);
        }
        return result;
    }

    const QString normalized = QDir::fromNativeSeparators(path);
    const QStringList parts = normalized.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (parts.isEmpty()) {
        return result;
    }

    QString current;
    int startIndex = 0;
    if (normalized.size() >= 2 && normalized.at(1) == QLatin1Char(':')) {
        current = parts.first() + QStringLiteral("/");
        result.append(current);
        startIndex = 1;
    } else if (normalized.startsWith(QLatin1Char('/'))) {
        current = QStringLiteral("/");
    }

    for (int i = startIndex; i < parts.size(); ++i) {
        const QString part = parts.at(i);
        if (part.isEmpty()) {
            continue;
        }
        if (!current.isEmpty() && !current.endsWith(QLatin1Char('/'))) {
            current += QLatin1Char('/');
        }
        current += part;
        result.append(current);
    }

    return result;
}

QVariantList FilePanelController::breadcrumbEntriesForPath(const QString &path) const
{
    QVariantList result;
    const QString lowerPath = path.toLower();
    if (lowerPath.startsWith(QStringLiteral("instagram://"))) {
        const QString normalized = FileProviderFactory::normalizePath(path).trimmed();
        const QString tail = normalized.mid(QStringLiteral("instagram://").size());
        const QStringList parts = tail.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            const QString kind = parts.at(0).toLower();
            const QString id = parts.at(1);
            QVariantMap entry;
            entry[QStringLiteral("name")] = kind == QLatin1String("user")
                ? id
                : QStringLiteral("%1 %2").arg(kind, id);
            entry[QStringLiteral("path")] = QStringLiteral("instagram://%1/%2").arg(kind, id);
            entry[QStringLiteral("pathKind")] = QStringLiteral("instagram");
            entry[QStringLiteral("isDrive")] = false;
            entry[QStringLiteral("isArchive")] = false;
            result.append(entry);
            return result;
        }
    }

    const QStringList paths = breadcrumbPathsForPath(path);
    auto appendEntry = [this, &result](const QString &name, const QString &entryPath, bool isDrive = false) {
        QVariantMap entry;
        entry[QStringLiteral("name")] = name;
        entry[QStringLiteral("path")] = entryPath;
        entry[QStringLiteral("pathKind")] = pathKindFor(entryPath);
        entry[QStringLiteral("isDrive")] = isDrive;
        entry[QStringLiteral("isArchive")] = ArchiveSupport::isArchivePath(entryPath)
            && entryPath.endsWith(QStringLiteral("|/"));
        result.append(entry);
    };

    for (int i = 0; i < paths.size(); ++i) {
        const QString &entryPath = paths.at(i);
        const bool isDrive = !ArchiveSupport::isArchivePath(entryPath)
                            && entryPath.size() >= 2
                            && entryPath.at(1) == QLatin1Char(':')
                            && entryPath.endsWith(QLatin1Char('/'));
        QString name;
        if (ArchiveSupport::isArchivePath(entryPath)) {
            if (i == 0) {
                name = ArchiveSupport::physicalArchivePath(entryPath);
            } else if (entryPath.endsWith(QStringLiteral("|/"))) {
                name = ArchiveSupport::archiveFileName(entryPath);
            } else {
                name = fileNameForPath(entryPath);
            }
        } else if (isProviderUriPath(entryPath) && uriSchemeForPath(entryPath) == QLatin1String("ftp")
                   && entryPath == QLatin1String("ftp://")) {
            name = QStringLiteral("FTP");
        } else {
            name = isDrive ? DriveUtils::rootDisplayName(entryPath) : fileNameForPath(entryPath);
        }
        appendEntry(name.isEmpty() ? entryPath : name, entryPath, isDrive);
    }
    return result;
}

void FilePanelController::showProperties(int row)
{
    if (isVirtualRoot()) return;
    QStringList selected = m_directoryModel.selectedPaths();
    if (selected.isEmpty()) {
        // Fallback: use the path at the given row
        const QString path = m_directoryModel.pathAt(row);
        if (!path.isEmpty()) {
            selected = { path };
        }
    }
    if (!selected.isEmpty()) {
        emit revealProperties(selected);
    }
}

void FilePanelController::fetchMetadataAsync(const QString &path)
{
    if (isVirtualRoot()) return;
    // Run extraction on a worker thread; marshal result back to GUI thread via signal.
    QThreadPool::globalInstance()->start([this, path]() {
        const QVariantList props = MetadataExtractor::extract(path);
        // Convert the label/value list into a flat map for efficient QML access
        QVariantMap meta;
        for (const QVariant &v : props) {
            const QVariantMap pair = v.toMap();
            const QString label = pair.value(QStringLiteral("label")).toString();
            const QString value = pair.value(QStringLiteral("value")).toString();
            // Normalize keys to camelCase for QML
            if (label == QLatin1String("Dimensions")) {
                meta[QStringLiteral("dimensions")] = value;
                meta[QStringLiteral("resolution")] = value;
            }
            if (label == QLatin1String("Duration"))    meta[QStringLiteral("duration")]   = value;
            if (label == QLatin1String("Artist"))      meta[QStringLiteral("artist")]     = value;
            if (label == QLatin1String("Album"))       meta[QStringLiteral("album")]      = value;
            if (label == QLatin1String("Bitrate"))     meta[QStringLiteral("bitrate")]    = value;
        }
        // Always emit even if empty so delegate knows loading is done
        QMetaObject::invokeMethod(this, [this, path, meta]() {
            emit metadataReady(path, meta);
        }, Qt::QueuedConnection);
    });
}

void FilePanelController::refresh()
{
    clearError();
    if (isVirtualRoot()) {
        emit contentsChanged(currentPath());
        return;
    }
    m_directoryModel.refresh();
    emit contentsChanged(currentPath());
}

void FilePanelController::clearError()
{
    setStatusMessage({});
    setLastError({});
    m_directoryModel.clearError();
}

void FilePanelController::setCategoryFilter(int filter)
{
    if (filter < DirectoryModel::FilterAll || filter > DirectoryModel::FilterDocuments) {
        filter = DirectoryModel::FilterAll;
    }

    const auto category = static_cast<DirectoryModel::CategoryFilter>(filter);
    if (category == DirectoryModel::FilterAll) {
        clearCategoryFilterScope();
        return;
    }

    const bool stateChanged = m_categoryFilter != category
        || m_categoryFilterScopePath != filterScopeForPath(currentPath())
        || m_categoryFilterContext != filterContextForPath(currentPath());
    m_categoryFilter = category;
    m_categoryFilterScopePath = filterScopeForPath(currentPath());
    m_categoryFilterContext = filterContextForPath(currentPath());
    updateCategoryFilterForPath(currentPath());
    if (stateChanged) {
        emit categoryFilterStateChanged();
    }
}

QString FilePanelController::filterScopeForPath(const QString &path) const
{
    if (path.isEmpty()) {
        return {};
    }
    if (ArchiveSupport::isArchivePath(path)) {
        return normalizedScopePath(ArchiveSupport::normalizeArchivePath(path));
    }
    return normalizedScopePath(m_fileProvider->normalizedPath(path));
}

QString FilePanelController::comparisonPathForFilterScope(const QString &path) const
{
    if (path.isEmpty()) {
        return {};
    }
    if (ArchiveSupport::isArchivePath(m_categoryFilterScopePath)) {
        return ArchiveSupport::isArchivePath(path)
            ? normalizedScopePath(ArchiveSupport::normalizeArchivePath(path))
            : normalizedScopePath(m_fileProvider->normalizedPath(path));
    }
    if (ArchiveSupport::isArchivePath(path)) {
        return normalizedScopePath(ArchiveSupport::physicalArchivePath(path));
    }
    return normalizedScopePath(m_fileProvider->normalizedPath(path));
}

QString FilePanelController::filterContextForPath(const QString &path) const
{
    const QString trimmed = path.trimmed();
    if (ArchiveSupport::isArchivePath(trimmed)) {
        return QStringLiteral("archive");
    }
    if (normalizedVirtualRoot(trimmed) == DEVICE_ROOT) {
        return QStringLiteral("devices");
    }
    if (normalizedVirtualRoot(trimmed) == FAVORITES_ROOT) {
        return QStringLiteral("favorites");
    }

    const int schemeIndex = trimmed.indexOf(QStringLiteral("://"));
    if (schemeIndex > 0) {
        return trimmed.left(schemeIndex).toLower();
    }
    return QStringLiteral("filesystem");
}

bool FilePanelController::isPathInsideCategoryFilterScope(const QString &path) const
{
    if (m_categoryFilterScopePath.isEmpty()) {
        return true;
    }
    return sameOrChildPath(comparisonPathForFilterScope(path), m_categoryFilterScopePath);
}

void FilePanelController::clearCategoryFilterScope()
{
    const bool stateChanged = categoryFilterActive() || categoryFilterSuspended();
    m_categoryFilter = DirectoryModel::FilterAll;
    m_categoryFilterScopePath.clear();
    m_categoryFilterContext.clear();
    m_directoryModel.setCategoryFilter(DirectoryModel::FilterAll);
    if (stateChanged) {
        emit categoryFilterStateChanged();
    }
}

void FilePanelController::updateCategoryFilterForPath(const QString &path)
{
    if (m_categoryFilter == DirectoryModel::FilterAll) {
        m_categoryFilterScopePath.clear();
        m_categoryFilterContext.clear();
        m_directoryModel.setCategoryFilter(DirectoryModel::FilterAll);
        return;
    }

    if (!isPathInsideCategoryFilterScope(path)) {
        clearCategoryFilterScope();
        return;
    }

    const DirectoryModel::CategoryFilter displayFilter =
        filterContextForPath(path) == m_categoryFilterContext
            ? m_categoryFilter
            : DirectoryModel::FilterAll;
    const bool wasSuspended = categoryFilterSuspended();
    m_directoryModel.setCategoryFilter(displayFilter);
    if (wasSuspended != categoryFilterSuspended()) {
        emit categoryFilterStateChanged();
    }
}

QStringList FilePanelController::selectedPaths() const
{
    return m_directoryModel.selectedPaths();
}

QVariantList FilePanelController::selectedItems() const
{
    QVariantList items;
    const QStringList paths = m_directoryModel.selectedPaths();
    items.reserve(paths.size());
    for (const QString &path : paths) {
        QString name;
        const int row = m_directoryModel.indexOfPath(path);
        if (row >= 0) {
            name = m_directoryModel.data(m_directoryModel.index(row, 0), DirectoryModel::NameRole).toString();
        }
        if (name.isEmpty()) {
            name = fileNameForPath(path);
        }

        QVariantMap item;
        item.insert(QStringLiteral("path"), path);
        item.insert(QStringLiteral("name"), name);
        items.append(item);
    }
    return items;
}

QVariantMap FilePanelController::storageInfoForPath(const QString &rootPath) const
{
    if (isProviderUriPath(rootPath)) {
        std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(rootPath);
        return provider ? provider->storageInfo(rootPath) : QVariantMap{};
    }

    const QStorageInfo storage(rootPath);
    if (!storage.isValid() || !storage.isReady()) {
        return {};
    }
    const qint64 total = storage.bytesTotal();
    const qint64 free  = storage.bytesFree();
    const qint64 used  = total - free;
    const double pct   = total > 0 ? static_cast<double>(used) / static_cast<double>(total) : 0.0;
    return {
        {QStringLiteral("total"),      total},
        {QStringLiteral("free"),       free},
        {QStringLiteral("used"),       used},
        {QStringLiteral("percent"),    pct},
        {QStringLiteral("totalStr"),   DriveUtils::formatSize(total)},
        {QStringLiteral("freeStr"),    DriveUtils::formatSize(free)},
        {QStringLiteral("fs"),         QString::fromLatin1(storage.fileSystemType())},
        {QStringLiteral("isCritical"), total > 0 && (static_cast<double>(free) / static_cast<double>(total)) < 0.10},
    };
}

void FilePanelController::ejectDrive(const QString &rootPath)
{
#ifdef Q_OS_WIN
    // Run eject asynchronously so we don't block the GUI thread
    const QString path = rootPath;
    QThreadPool::globalInstance()->start([this, path]() {
        // Build volume path like "\\.\C:"
        QString vol = path;
        if (vol.endsWith('/') || vol.endsWith('\\')) vol.chop(1);
        const QString devPath = QStringLiteral("\\\\.\\%1").arg(vol);
        const std::wstring wdev = devPath.toStdWString();

        HANDLE hDevice = ::CreateFileW(
            wdev.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);

        bool ok = false;
        if (hDevice != INVALID_HANDLE_VALUE) {
            DWORD bytesReturned = 0;
            ok = ::DeviceIoControl(
                hDevice,
                IOCTL_STORAGE_EJECT_MEDIA,
                nullptr, 0,
                nullptr, 0,
                &bytesReturned,
                nullptr) != 0;
            ::CloseHandle(hDevice);
        }

        QMetaObject::invokeMethod(this, [this, path, ok]() {
            emit ejectFinished(path, ok);
        }, Qt::QueuedConnection);
    });
#else
    Q_UNUSED(rootPath)
    emit ejectFinished(rootPath, false);
#endif
}

void FilePanelController::handleDeviceRemoved(const QString &rootPath, const QString &displayName)
{
    Q_UNUSED(rootPath)

    cancelDirectorySuggestions();
    ++m_navigationRequestId;
    setNavigationPending(false);
    setCurrentItemPath({});
    m_directoryModel.clearSelection();
    m_directoryModel.suppressNextWatchRestart();
    if (m_directoryModel.loading()) {
        m_directoryModel.cancelLoading();
    }

    openPathInternal(QString(DEVICE_ROOT), false);
    const QString label = displayName.trimmed();
    setStatusMessage(label.isEmpty()
                         ? QStringLiteral("Device was removed")
                         : QStringLiteral("%1 was removed").arg(label));
}

void FilePanelController::syncStateFrom(FilePanelController *other)
{
    if (!other || other == this) {
        return;
    }

    const QString sourcePath = other->currentPath();
    if (!sourcePath.isEmpty() && sourcePath != currentPath()) {
        openPath(sourcePath);
    }

    setViewMode(other->viewMode());
    setPanelSortPolicy(int(other->panelSortRole()), int(other->panelSortOrder()));

    DirectoryModel *sourceModel = other->directoryModel();
    DirectoryModel *targetModel = directoryModel();
    if (!sourceModel || !targetModel) {
        return;
    }

    targetModel->setShowHidden(sourceModel->showHidden());
    targetModel->setMixFilesAndFolders(sourceModel->mixFilesAndFolders());
    targetModel->setSearchText(sourceModel->searchText());
}

bool FilePanelController::openPathInternal(const QString &path, bool addToHistory, bool preserveScroll)
{
    QElapsedTimer totalTimer;
    totalTimer.start();
    if (filePanelNavTraceEnabled()) {
        traceFilePanelNav("openPathInternal-begin", path,
                          QStringLiteral("addToHistory=%1 preserveScroll=%2")
                              .arg(addToHistory)
                              .arg(preserveScroll));
    }

    const bool targetIsDeviceRoot = (path == DEVICE_ROOT);
    const bool targetIsFavoritesRoot = (path == FAVORITES_ROOT);
    const bool wasVirtualRoot = isVirtualRoot();

    QString newPath;
    if (targetIsDeviceRoot) {
        newPath = DEVICE_ROOT;
    } else if (targetIsFavoritesRoot) {
        newPath = FAVORITES_ROOT;
    } else if (ArchiveSupport::isArchivePath(path)) {
        newPath = ArchiveSupport::normalizeArchivePath(path);
    } else {
        newPath = FileProviderFactory::normalizePath(path);
    }

    const QString oldPath = currentPath();
    const QString oldOuterArchiveSession = outerArchiveSessionKeyForPath(oldPath);
    const QString uiNavigationPath = preserveScroll
            && oldPath.startsWith(QStringLiteral("instagram://"), Qt::CaseInsensitive)
            && !oldPath.isEmpty()
            && newPath == oldPath + QStringLiteral("/__load_more__")
        ? oldPath
        : newPath;
    traceFilePanelNav("openPathInternal-normalized", newPath,
                      QStringLiteral("old=%1 ui=%2 preserveScroll=%3 elapsedMs=%4")
                          .arg(QDir::toNativeSeparators(oldPath))
                          .arg(QDir::toNativeSeparators(uiNavigationPath))
                          .arg(preserveScroll)
                          .arg(totalTimer.elapsed()));

    const bool sameLocalPath = !newPath.isEmpty()
        && !targetIsDeviceRoot
        && !targetIsFavoritesRoot
        && !ArchiveSupport::isArchivePath(newPath)
        && !ArchiveSupport::isArchivePath(oldPath)
        && samePanelFilesystemPath(newPath, oldPath);
    if (!newPath.isEmpty() && (newPath == oldPath || sameLocalPath)) {
        emit pathNavigated(newPath);
        traceFilePanelNav("openPathInternal-end", newPath,
                          QStringLiteral("result=true reason=same elapsedMs=%1").arg(totalTimer.elapsed()));
        return true;
    }

    traceFilePanelNav("openPathInternal-before-pathAboutToChange", newPath,
                      QStringLiteral("from=%1 elapsedMs=%2")
                          .arg(QDir::toNativeSeparators(oldPath))
                          .arg(totalTimer.elapsed()));
    emit pathAboutToChange(oldPath, uiNavigationPath, preserveScroll);
    setCurrentItemPath({});
    traceFilePanelNav("openPathInternal-after-pathAboutToChange", newPath,
                      QStringLiteral("elapsedMs=%1").arg(totalTimer.elapsed()));

    if (targetIsDeviceRoot || targetIsFavoritesRoot) {
        m_directoryModel.setSearchText({});
        clearCategoryFilterScope();
        if (!oldOuterArchiveSession.isEmpty()) {
            m_approvedNestedArchiveScopeKeys.clear();
            ArchiveFileProvider::invalidateCacheForPath(oldPath);
        }
        setStatusMessage({});
        setLastError({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        setIsDeviceRoot(targetIsDeviceRoot);
        setIsFavoritesRoot(targetIsFavoritesRoot);
        m_directoryModel.clear();
        emit pathNavigated(newPath);
        traceFilePanelNav("openPathInternal-before-currentPathChanged", newPath,
                          QStringLiteral("type=virtual elapsedMs=%1").arg(totalTimer.elapsed()));
        emit currentPathChanged();
        traceFilePanelNav("openPathInternal-after-currentPathChanged", newPath,
                          QStringLiteral("type=virtual elapsedMs=%1").arg(totalTimer.elapsed()));
        emit capabilitiesChanged();
        emit historyChanged();
        traceFilePanelNav("openPathInternal-end", newPath,
                          QStringLiteral("result=true type=virtual elapsedMs=%1").arg(totalTimer.elapsed()));
        return true;
    }

    QElapsedTimer modelTimer;
    modelTimer.start();
    const bool modelOpened = m_directoryModel.openPath(newPath);
    traceFilePanelNav("openPathInternal-directoryModel.openPath", newPath,
                      QStringLiteral("result=%1 elapsedMs=%2 totalMs=%3")
                          .arg(modelOpened)
                          .arg(modelTimer.elapsed())
                          .arg(totalTimer.elapsed()));

    if (modelOpened) {
        updateCategoryFilterForPath(newPath);
        const QString newOuterArchiveSession = outerArchiveSessionKeyForPath(newPath);
        if (!oldOuterArchiveSession.isEmpty() && oldOuterArchiveSession != newOuterArchiveSession) {
            m_approvedNestedArchiveScopeKeys.clear();
            ArchiveFileProvider::invalidateCacheForPath(oldPath);
        }
        m_directoryModel.setSearchText({});
        const bool keepNestedPreparationStatus = !nestedArchiveScopeKeyForPath(newPath).isEmpty()
            && !ArchiveFileProvider::hasCachedContainerForPath(newPath);
        if (!keepNestedPreparationStatus) {
            setStatusMessage({});
        }
        setLastError({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        setIsDeviceRoot(false);
        setIsFavoritesRoot(false);
        emit pathNavigated(uiNavigationPath);
        if (wasVirtualRoot) {
            traceFilePanelNav("openPathInternal-before-currentPathChanged", newPath,
                              QStringLiteral("type=from-virtual elapsedMs=%1").arg(totalTimer.elapsed()));
            emit currentPathChanged();
            traceFilePanelNav("openPathInternal-after-currentPathChanged", newPath,
                              QStringLiteral("type=from-virtual elapsedMs=%1").arg(totalTimer.elapsed()));
            emit capabilitiesChanged();
        }
        emit historyChanged();
        traceFilePanelNav("openPathInternal-end", newPath,
                          QStringLiteral("result=true elapsedMs=%1").arg(totalTimer.elapsed()));
        return true;
    }

    emit pathNavigationFailed(newPath);
    const QString failedScope = nestedArchiveScopeKeyForPath(newPath);
    if (!failedScope.isEmpty()) {
        m_approvedNestedArchiveScopeKeys.remove(failedScope);
    }
    traceFilePanelNav("openPathInternal-end", newPath,
                      QStringLiteral("result=false elapsedMs=%1").arg(totalTimer.elapsed()));
    return false;
}

void FilePanelController::pushHistory(const QString &path)
{
    m_backStack.append(path);
    constexpr qsizetype maxHistory = 64;
    while (m_backStack.size() > maxHistory) {
        m_backStack.removeFirst();
    }
}

bool FilePanelController::removeLastHistoryEntryIfPath(const QString &path)
{
    const QString normalizedPath = m_fileProvider->normalizedPath(path);
    if (normalizedPath.isEmpty() || m_backStack.isEmpty()) {
        return false;
    }

    const QString lastHistoryPath = m_fileProvider->normalizedPath(m_backStack.constLast());
    if (lastHistoryPath.isEmpty() || !samePanelFilesystemPath(lastHistoryPath, normalizedPath)) {
        return false;
    }

    m_backStack.removeLast();
    emit historyChanged();
    return true;
}

void FilePanelController::recoverFromMissingPath(const QString &path, const QString &error)
{
    const QString revealPath = failedNavigationRevealPath(path);
    if (!revealPath.isEmpty()) {
        scheduleCreatedEntryReveal(revealPath);
    }

    if (ArchiveSupport::isArchivePath(path)) {
        if (ArchiveFileProvider::errorNeedsPassword(error)) {
            ArchiveFileProvider::clearPasswordForPath(path);
            emit archivePasswordRequested(path,
                                          nestedArchiveDisplayNameForPath(path),
                                          error.isEmpty()
                                              ? QStringLiteral("Archive password required")
                                              : error);
            return;
        }
        setOperationError(error.isEmpty()
                              ? QStringLiteral("Cannot open archive.")
                              : error,
                          path,
                          QStringLiteral("open"));
        emit pathNavigationFailed(path);
        return;
    }

    if (isPortableUriPath(path)) {
        if (portableFailureIndicatesRemoval(error)) {
            handleDeviceRemoved({}, {});
            return;
        }
        setOperationError(error.isEmpty()
                              ? QStringLiteral("Cannot open portable device.")
                              : error,
                          path,
                          QStringLiteral("open"));
        emit pathNavigationFailed(path);
        return;
    }

    const QString normalizedCurrent = m_fileProvider->normalizedPath(currentPath());
    const QString normalizedMissing = m_fileProvider->normalizedPath(path);
    if (normalizedMissing.isEmpty()) {
        return;
    }

    if (m_volumeMonitor) {
        const QString unavailableRoot = m_volumeMonitor->unavailableRootForPath(path);
        if (!unavailableRoot.isEmpty()) {
            handleDeviceRemoved(unavailableRoot, m_volumeMonitor->displayNameForRoot(unavailableRoot));
            return;
        }
    }

    if (!normalizedCurrent.isEmpty() && !samePanelFilesystemPath(normalizedCurrent, normalizedMissing)) {
        removeLastHistoryEntryIfPath(normalizedCurrent);
        if (navigationFailureIndicatesMissingPath(error)) {
            m_directoryModel.refresh();
        }
        setOperationError(error.isEmpty()
                              ? QStringLiteral("Cannot open folder.")
                              : error,
                          path,
                          QStringLiteral("open"));
        emit pathNavigationFailed(path);
        return;
    }

    const int requestId = ++m_navigationRequestId;
    setNavigationPending(true, normalizedMissing);
    QPointer<FilePanelController> self(this);
    (void)QtConcurrent::run([self, normalizedMissing, requestId]() {
        const QString fallback = fallbackPathForMissing(normalizedMissing);
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, normalizedMissing, fallback, requestId]() {
            if (!self || requestId != self->m_navigationRequestId) {
                return;
            }

            self->setNavigationPending(false);
            const QString currentNow = self->m_fileProvider->normalizedPath(self->currentPath());
            if (!currentNow.isEmpty() && !samePanelFilesystemPath(currentNow, normalizedMissing)) {
                return;
            }

            if (fallback.isEmpty() || samePanelFilesystemPath(fallback, currentNow)) {
                self->setStatusMessage(QStringLiteral("Folder is no longer available"));
                return;
            }

            self->removeLastHistoryEntryIfPath(fallback);
            self->m_directoryModel.suppressNextWatchRestart();
            if (!self->openPathInternal(fallback, false)) {
                self->setStatusMessage(QStringLiteral("Folder is no longer available"));
                return;
            }

            self->setStatusMessage(QStringLiteral("Folder was removed externally. Moved up to %1")
                                   .arg(self->m_fileProvider->fileName(fallback).isEmpty()
                                            ? fallback
                                            : self->m_fileProvider->fileName(fallback)));
        }, Qt::QueuedConnection);
    });
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

QStringList FilePanelController::getDirectorySuggestions(const QString &inputPath) const
{
    Q_UNUSED(inputPath);
    return {};
}

void FilePanelController::requestDirectorySuggestions(const QString &inputPath, int requestId, int maxSuggestions) const
{
    const QString basePath = currentPath();
    const qsizetype boundedMax = maxSuggestions <= 0 ? 0 : qBound(1, maxSuggestions, 512);
    const int generation = ++m_directorySuggestionGeneration;
    QPointer<FilePanelController> self(const_cast<FilePanelController *>(this));
    traceFilePanelNav("suggestions-request", inputPath,
                      QStringLiteral("requestId=%1 base=%2 max=%3")
                          .arg(requestId)
                          .arg(QDir::toNativeSeparators(basePath))
                          .arg(boundedMax));

    if (!localAutocompleteAllowedFor(inputPath, basePath)) {
        QMetaObject::invokeMethod(self.data(), [self, requestId, generation]() {
            if (!self || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation) {
                return;
            }
            emit self->directorySuggestionsReady(requestId, {});
        }, Qt::QueuedConnection);
        return;
    }

    (void)QtConcurrent::run([self, inputPath, requestId, basePath, boundedMax, generation]() {
        const auto shouldCancel = [self, generation]() {
            return !self
                || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation;
        };

        QElapsedTimer timer;
        timer.start();
        const QStringList suggestions = directorySuggestionsForInput(inputPath, basePath, boundedMax, shouldCancel);
        if (shouldCancel()) {
            return;
        }
        traceFilePanelNav("suggestions-worker-finished", inputPath,
                          QStringLiteral("requestId=%1 count=%2 elapsedMs=%3")
                              .arg(requestId)
                              .arg(suggestions.size())
                              .arg(timer.elapsed()));
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, requestId, generation, suggestions]() {
            if (!self || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation) {
                return;
            }
            emit self->directorySuggestionsReady(requestId, suggestions);
        }, Qt::QueuedConnection);
    });
}

void FilePanelController::requestDirectorySuggestionEntries(const QString &inputPath, int requestId, int maxSuggestions) const
{
    const QString basePath = currentPath();
    const qsizetype boundedMax = maxSuggestions <= 0 ? 0 : qBound(1, maxSuggestions, 512);
    const int generation = ++m_directorySuggestionGeneration;
    QPointer<FilePanelController> self(const_cast<FilePanelController *>(this));
    traceFilePanelNav("suggestion-entries-request", inputPath,
                      QStringLiteral("requestId=%1 base=%2 max=%3")
                          .arg(requestId)
                          .arg(QDir::toNativeSeparators(basePath))
                          .arg(boundedMax));

    if (!localAutocompleteAllowedFor(inputPath, basePath)
        && !providerNavigationSuggestionsAllowedFor(inputPath)) {
        QMetaObject::invokeMethod(self.data(), [self, requestId, generation]() {
            if (!self || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation) {
                return;
            }
            emit self->directorySuggestionEntriesReady(requestId, {});
        }, Qt::QueuedConnection);
        return;
    }

    (void)QtConcurrent::run([self, inputPath, requestId, basePath, boundedMax, generation]() {
        const auto shouldCancel = [self, generation]() {
            return !self
                || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation;
        };

        QElapsedTimer timer;
        timer.start();
        const QVariantList suggestions = directorySuggestionEntriesForInput(inputPath, basePath, boundedMax, shouldCancel);
        if (shouldCancel()) {
            return;
        }
        traceFilePanelNav("suggestion-entries-worker-finished", inputPath,
                          QStringLiteral("requestId=%1 count=%2 elapsedMs=%3")
                              .arg(requestId)
                              .arg(suggestions.size())
                              .arg(timer.elapsed()));
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, requestId, generation, suggestions]() {
            if (!self || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation) {
                return;
            }
            emit self->directorySuggestionEntriesReady(requestId, suggestions);
        }, Qt::QueuedConnection);
    });
}

void FilePanelController::cancelDirectorySuggestions() const
{
    ++m_directorySuggestionGeneration;
}
