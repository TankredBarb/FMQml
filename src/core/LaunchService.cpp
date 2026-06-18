#include "LaunchService.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QGuiApplication>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QWindow>
#include <QCryptographicHash>
#include <QDateTime>

#include <algorithm>
#include <optional>

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

QString categoryName(LaunchService::LaunchCategory category)
{
    switch (category) {
    case LaunchService::LaunchCategory::Document:
        return QStringLiteral("document");
    case LaunchService::LaunchCategory::NativeExecutableElf:
        return QStringLiteral("nativeExecutableElf");
    case LaunchService::LaunchCategory::NativeExecutableScript:
        return QStringLiteral("nativeExecutableScript");
    case LaunchService::LaunchCategory::NativeExecutableAppImage:
        return QStringLiteral("nativeExecutableAppImage");
    case LaunchService::LaunchCategory::DesktopLauncherTrusted:
        return QStringLiteral("desktopLauncherTrusted");
    case LaunchService::LaunchCategory::DesktopLauncherBlocked:
        return QStringLiteral("desktopLauncherBlocked");
    case LaunchService::LaunchCategory::WindowsApplication:
        return QStringLiteral("windowsApplication");
    case LaunchService::LaunchCategory::NonExecutableScript:
        return QStringLiteral("nonExecutableScript");
    case LaunchService::LaunchCategory::UnknownExecutable:
        return QStringLiteral("unknownExecutable");
    case LaunchService::LaunchCategory::Unsupported:
        return QStringLiteral("unsupported");
    }
    return QStringLiteral("unsupported");
}

LaunchService::LaunchResult failure(LaunchService::LaunchErrorCode errorCode,
                                    const QString &title,
                                    const QString &message,
                                    const QString &details = {},
                                    bool showDialog = false)
{
    return {false, errorCode, title, message, details, showDialog};
}

bool hasLocalLaunchableScheme(const QString &path)
{
    const QString scheme = explicitScheme(path);
    return scheme.isEmpty() || scheme == QLatin1String("file");
}

QByteArray readHeader(const QString &path, qint64 maxSize = 256)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.read(maxSize);
}

bool hasPeSignature(const QByteArray &header)
{
    if (header.size() < 2 || header.at(0) != 'M' || header.at(1) != 'Z') {
        return false;
    }
    if (header.size() < 0x40) {
        return false;
    }

    const auto b = [](char value) { return static_cast<quint32>(static_cast<unsigned char>(value)); };
    const quint32 offset = b(header.at(0x3c))
        | (b(header.at(0x3d)) << 8)
        | (b(header.at(0x3e)) << 16)
        | (b(header.at(0x3f)) << 24);
    if (offset + 4 > static_cast<quint32>(header.size())) {
        return false;
    }
    return header.mid(static_cast<qsizetype>(offset), 4) == QByteArray("PE\0\0", 4);
}

bool desktopEntryTypeApplication(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    bool inDesktopEntry = false;
    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
            continue;
        }
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inDesktopEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (inDesktopEntry && line.startsWith(QLatin1String("Type="))) {
            return line.mid(5).trimmed() == QLatin1String("Application");
        }
    }
    return false;
}

LaunchService::LaunchCategory classifyLocalPath(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile()) {
        return LaunchService::LaunchCategory::Unsupported;
    }

    const QString suffix = info.suffix();
    const QByteArray header = readHeader(info.absoluteFilePath());
    const bool executable = info.isExecutable();

    if (suffix.compare(QStringLiteral("desktop"), Qt::CaseInsensitive) == 0) {
        return executable && desktopEntryTypeApplication(info.absoluteFilePath())
            ? LaunchService::LaunchCategory::DesktopLauncherTrusted
            : LaunchService::LaunchCategory::DesktopLauncherBlocked;
    }

    if (hasPeSignature(header) || suffix.compare(QStringLiteral("exe"), Qt::CaseInsensitive) == 0
            || suffix.compare(QStringLiteral("msi"), Qt::CaseInsensitive) == 0) {
        return LaunchService::LaunchCategory::WindowsApplication;
    }

    if (header.startsWith(QByteArray("\x7f"
                                     "ELF", 4))) {
        return executable ? LaunchService::LaunchCategory::NativeExecutableElf
                          : LaunchService::LaunchCategory::Unsupported;
    }

    if (header.startsWith("#!")) {
        return executable ? LaunchService::LaunchCategory::NativeExecutableScript
                          : LaunchService::LaunchCategory::NonExecutableScript;
    }

    if (suffix.compare(QStringLiteral("AppImage"), Qt::CaseInsensitive) == 0) {
        return executable ? LaunchService::LaunchCategory::NativeExecutableAppImage
                          : LaunchService::LaunchCategory::Unsupported;
    }

    return LaunchService::LaunchCategory::Document;
}

LaunchService::LaunchResult validateLocalFilePath(const QString &path)
{
    if (!hasLocalLaunchableScheme(path)) {
        return failure(LaunchService::LaunchErrorCode::NotLocalPath,
                       QStringLiteral("Cannot open non-local file"),
                       QStringLiteral("This location does not support direct file launch."));
    }

    const QString localPath = localPathFromInput(path);
    if (localPath.isEmpty() || !QFileInfo::exists(localPath)) {
        return failure(LaunchService::LaunchErrorCode::FileNotFound,
                       QStringLiteral("File not found"),
                       QStringLiteral("The selected file is no longer available."));
    }
    return {true, LaunchService::LaunchErrorCode::None};
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

#if defined(Q_OS_LINUX)
struct ProtonRuntime {
    QString id;
    QString name;
    QString source;
    QString steamRoot;
    QString protonDir;
    QString protonExecutable;
};

LaunchService::LaunchResult openDocumentWithDesktop(const QString &path)
{
    const bool ok = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    if (ok) {
        return {true, LaunchService::LaunchErrorCode::None};
    }
    return failure(LaunchService::LaunchErrorCode::NoAssociation,
                   QStringLiteral("Could not open file"),
                   QStringLiteral("No default application could open this file."),
                   {},
                   true);
}

LaunchService::LaunchResult startDetachedProgram(const QString &program,
                                                 const QStringList &arguments,
                                                 const QString &workingDirectory,
                                                 const QString &title,
                                                 const QString &message)
{
    qint64 pid = 0;
    if (QProcess::startDetached(program, arguments, workingDirectory, &pid)) {
        return {true, LaunchService::LaunchErrorCode::None};
    }
    return failure(LaunchService::LaunchErrorCode::RunnerStartFailed,
                   title,
                   message,
                   QStringLiteral("Program: %1\nWorking directory: %2").arg(program, workingDirectory),
                   true);
}

QString protonRuntimeIdForPath(const QString &protonExecutable)
{
    return QString::fromLatin1(QCryptographicHash::hash(protonExecutable.toUtf8(), QCryptographicHash::Sha256)
                                   .toHex()
                                   .left(24));
}

QString protonRuntimeNameForDir(const QString &protonDir)
{
    const QString name = QFileInfo(protonDir).fileName().trimmed();
    return name.isEmpty() ? QStringLiteral("Steam Proton") : name;
}

QString normalizedExistingDirectory(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists() || !info.isDir()) {
        return {};
    }
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}

void appendUniqueDirectory(QStringList &directories, const QString &path)
{
    const QString normalized = normalizedExistingDirectory(path);
    if (!normalized.isEmpty() && !directories.contains(normalized)) {
        directories.append(normalized);
    }
}

QStringList steamLibraryPathsFromVdf(const QString &vdfPath)
{
    QFile file(vdfPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    QStringList paths;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (!line.startsWith(QLatin1String("\"path\""))) {
            continue;
        }

        const qsizetype firstQuote = line.indexOf(QLatin1Char('"'), 6);
        if (firstQuote < 0) {
            continue;
        }
        const qsizetype secondQuote = line.indexOf(QLatin1Char('"'), firstQuote + 1);
        if (secondQuote <= firstQuote) {
            continue;
        }

        const QString path = line.mid(firstQuote + 1, secondQuote - firstQuote - 1);
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

QStringList steamRootCandidates()
{
    const QString home = QDir::homePath();
    QStringList roots;
    appendUniqueDirectory(roots, QDir(home).filePath(QStringLiteral(".steam/root")));
    appendUniqueDirectory(roots, QDir(home).filePath(QStringLiteral(".steam/steam")));
    appendUniqueDirectory(roots, QDir(home).filePath(QStringLiteral(".local/share/Steam")));
    appendUniqueDirectory(roots, QDir(home).filePath(QStringLiteral(".var/app/com.valvesoftware.Steam/.local/share/Steam")));
    return roots;
}

QStringList steamLibraryCandidates(const QStringList &steamRoots)
{
    QStringList libraries;
    for (const QString &root : steamRoots) {
        appendUniqueDirectory(libraries, root);
        for (const QString &library : steamLibraryPathsFromVdf(QDir(root).filePath(QStringLiteral("steamapps/libraryfolders.vdf")))) {
            appendUniqueDirectory(libraries, library);
        }
        for (const QString &library : steamLibraryPathsFromVdf(QDir(root).filePath(QStringLiteral("config/libraryfolders.vdf")))) {
            appendUniqueDirectory(libraries, library);
        }
    }
    return libraries;
}

QList<ProtonRuntime> discoverProtonRuntimes()
{
    const QStringList steamRoots = steamRootCandidates();
    const QStringList libraries = steamLibraryCandidates(steamRoots);
    QList<ProtonRuntime> runtimes;

    auto appendRuntime = [&runtimes](const QString &steamRoot, const QString &protonDir, const QString &source) {
        const QString normalizedDir = normalizedExistingDirectory(protonDir);
        if (normalizedDir.isEmpty()) {
            return;
        }
        const QString proton = QDir(normalizedDir).filePath(QStringLiteral("proton"));
        if (!QFileInfo(proton).isExecutable()) {
            return;
        }
        for (const ProtonRuntime &runtime : std::as_const(runtimes)) {
            if (runtime.protonDir == normalizedDir) {
                return;
            }
        }
        runtimes.append({protonRuntimeIdForPath(proton),
                         protonRuntimeNameForDir(normalizedDir),
                         source,
                         steamRoot,
                         normalizedDir,
                         proton});
    };

    for (const QString &library : libraries) {
        const QString commonPath = QDir(library).filePath(QStringLiteral("steamapps/common"));
        const QDir common(commonPath);
        if (!common.exists()) {
            continue;
        }
        const QFileInfoList entries = common.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            if (entry.fileName().startsWith(QStringLiteral("Proton"), Qt::CaseInsensitive)) {
                appendRuntime(library, entry.absoluteFilePath(), QStringLiteral("steam-common"));
            }
        }
    }

    for (const QString &root : steamRoots) {
        const QDir compatibilityTools(QDir(root).filePath(QStringLiteral("compatibilitytools.d")));
        if (!compatibilityTools.exists()) {
            continue;
        }
        const QFileInfoList entries = compatibilityTools.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QFileInfo &entry : entries) {
            appendRuntime(root, entry.absoluteFilePath(), QStringLiteral("compatibilitytools"));
        }
    }

    std::sort(runtimes.begin(), runtimes.end(), [](const ProtonRuntime &left, const ProtonRuntime &right) {
        const QFileInfo leftInfo(left.protonDir);
        const QFileInfo rightInfo(right.protonDir);
        if (leftInfo.lastModified() != rightInfo.lastModified()) {
            return leftInfo.lastModified() > rightInfo.lastModified();
        }
        return left.protonDir > right.protonDir;
    });
    return runtimes;
}

std::optional<ProtonRuntime> protonRuntimeById(const QList<ProtonRuntime> &runtimes, const QString &runtimeId)
{
    const QString trimmed = runtimeId.trimmed();
    if (trimmed.isEmpty() || trimmed == QLatin1String("auto")) {
        return runtimes.isEmpty() ? std::optional<ProtonRuntime>{} : runtimes.first();
    }
    for (const ProtonRuntime &runtime : runtimes) {
        if (runtime.id == trimmed) {
            return runtime;
        }
    }
    return runtimes.isEmpty() ? std::optional<ProtonRuntime>{} : runtimes.first();
}

bool fileExists(const QString &path)
{
    return QFileInfo::exists(path);
}

bool vkBasaltAvailable()
{
    if (!QStandardPaths::findExecutable(QStringLiteral("vkbasalt")).isEmpty()) {
        return true;
    }

    const QString home = QDir::homePath();
    const QStringList manifests = {
        QStringLiteral("/usr/share/vulkan/implicit_layer.d/vkBasalt.json"),
        QStringLiteral("/usr/share/vulkan/explicit_layer.d/vkBasalt.json"),
        QDir(home).filePath(QStringLiteral(".local/share/vulkan/implicit_layer.d/vkBasalt.json")),
        QDir(home).filePath(QStringLiteral(".local/share/vulkan/explicit_layer.d/vkBasalt.json")),
    };
    for (const QString &manifest : manifests) {
        if (fileExists(manifest)) {
            return true;
        }
    }
    return false;
}

QVariantMap protonRuntimeMap(const ProtonRuntime &runtime)
{
    QVariantMap map;
    map.insert(QStringLiteral("id"), runtime.id);
    map.insert(QStringLiteral("name"), runtime.name);
    map.insert(QStringLiteral("source"), runtime.source);
    map.insert(QStringLiteral("steamRoot"), QDir::toNativeSeparators(runtime.steamRoot));
    map.insert(QStringLiteral("path"), QDir::toNativeSeparators(runtime.protonDir));
    map.insert(QStringLiteral("executable"), QDir::toNativeSeparators(runtime.protonExecutable));
    return map;
}

QString savedProtonRuntimeId()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("launch"));
    return settings.value(QStringLiteral("protonRuntimeId"), QStringLiteral("auto")).toString();
}

bool savedProtonVkBasaltEnabled()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("launch"));
    return settings.value(QStringLiteral("protonVkBasaltEnabled"), false).toBool();
}

bool savedProtonCaptureLog()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("launch"));
    return settings.value(QStringLiteral("protonCaptureLog"), false).toBool();
}

bool savedProtonClearXModifiers()
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("launch"));
    return settings.value(QStringLiteral("protonClearXModifiers"), false).toBool();
}

QString protonLogDirectory()
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if (!basePath.isEmpty()) {
            basePath = QDir(basePath).filePath(QStringLiteral("FMQml"));
        }
    }
    if (basePath.isEmpty()) {
        basePath = QDir::tempPath();
    }
    const QString path = QDir(basePath).filePath(QStringLiteral("proton/logs"));
    QDir().mkpath(path);
    return path;
}

QString sanitizedProtonLogTokenForTarget(const QString &targetPath)
{
    QString name = QFileInfo(targetPath).completeBaseName().trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("windows-app");
    }

    QString sanitized;
    sanitized.reserve(name.size());
    for (const QChar ch : name) {
        sanitized.append(ch.isLetterOrNumber() || ch == QLatin1Char('.') || ch == QLatin1Char('_') || ch == QLatin1Char('-')
                             ? ch
                             : QLatin1Char('_'));
    }
    while (sanitized.contains(QStringLiteral("__"))) {
        sanitized.replace(QStringLiteral("__"), QStringLiteral("_"));
    }
    sanitized = sanitized.left(48).trimmed();
    if (sanitized.isEmpty()) {
        sanitized = QStringLiteral("windows-app");
    }

    return QStringLiteral("%1-%2").arg(sanitized, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss")));
}

QString protonLogFilePathForToken(const QString &logToken)
{
    return QDir(protonLogDirectory()).filePath(QStringLiteral("%1.log").arg(logToken));
}

QString protonCompatDataPathForTarget(const QString &targetPath)
{
    QString basePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (basePath.isEmpty()) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if (!basePath.isEmpty()) {
            basePath = QDir(basePath).filePath(QStringLiteral("FMQml"));
        }
    }
    if (basePath.isEmpty()) {
        basePath = QDir::tempPath();
    }

    const QByteArray digest = QCryptographicHash::hash(QFileInfo(targetPath).absoluteFilePath().toUtf8(),
                                                       QCryptographicHash::Sha256)
                                  .toHex()
                                  .left(24);
    const QString compatDataPath = QDir(basePath).filePath(QStringLiteral("proton/compatdata/%1").arg(QString::fromLatin1(digest)));
    QDir().mkpath(compatDataPath);
    return compatDataPath;
}

LaunchService::LaunchResult startDetachedWithEnvironment(const QString &program,
                                                         const QStringList &arguments,
                                                         const QString &workingDirectory,
                                                         const QProcessEnvironment &environment,
                                                         const QString &title,
                                                         const QString &message)
{
    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.setProcessEnvironment(environment);

    qint64 pid = 0;
    if (process.startDetached(&pid)) {
        return {true, LaunchService::LaunchErrorCode::None};
    }
    return failure(LaunchService::LaunchErrorCode::RunnerStartFailed,
                   title,
                   message,
                   QStringLiteral("Program: %1\nWorking directory: %2").arg(program, workingDirectory),
                   true);
}

LaunchService::LaunchResult startDetachedWithEnvironmentAndLog(const QString &program,
                                                               const QStringList &arguments,
                                                               const QString &workingDirectory,
                                                               const QProcessEnvironment &environment,
                                                               const QString &logPath,
                                                               const QString &title,
                                                               const QString &message)
{
    QFile logFile(logPath);
    if (!logFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return failure(LaunchService::LaunchErrorCode::RunnerStartFailed,
                       QStringLiteral("Could not create Proton log"),
                       QStringLiteral("FMQml could not create the Proton log file."),
                       QDir::toNativeSeparators(logPath),
                       true);
    }

    logFile.write("FMQml Proton launch log\n");
    logFile.write(("Started: " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n").toUtf8());
    logFile.write(("Program: " + QDir::toNativeSeparators(program) + "\n").toUtf8());
    logFile.write(("Working directory: " + QDir::toNativeSeparators(workingDirectory) + "\n").toUtf8());
    logFile.write(("Arguments: " + arguments.join(QLatin1Char(' ')) + "\n\n").toUtf8());
    logFile.close();

    QProcess process;
    process.setProgram(program);
    process.setArguments(arguments);
    process.setWorkingDirectory(workingDirectory);
    process.setProcessEnvironment(environment);
    process.setStandardOutputFile(logPath, QIODevice::Append);
    process.setStandardErrorFile(logPath, QIODevice::Append);

    qint64 pid = 0;
    if (process.startDetached(&pid)) {
        LaunchService::LaunchResult result{true, LaunchService::LaunchErrorCode::None};
        result.details = QDir::toNativeSeparators(logPath);
        return result;
    }
    return failure(LaunchService::LaunchErrorCode::RunnerStartFailed,
                   title,
                   message,
                   QStringLiteral("Program: %1\nWorking directory: %2\nLog: %3")
                       .arg(program, workingDirectory, logPath),
                   true);
}

LaunchService::LaunchResult openPathWithLinuxPolicy(const QString &path)
{
    const QFileInfo info(path);
    const QString absolutePath = info.absoluteFilePath();
    const QString workingDirectory = info.absolutePath();
    const LaunchService::LaunchCategory category = classifyLocalPath(absolutePath);

    switch (category) {
    case LaunchService::LaunchCategory::Document:
    case LaunchService::LaunchCategory::DesktopLauncherTrusted:
        return openDocumentWithDesktop(absolutePath);
    case LaunchService::LaunchCategory::NativeExecutableElf:
    case LaunchService::LaunchCategory::NativeExecutableScript:
    case LaunchService::LaunchCategory::NativeExecutableAppImage:
        return startDetachedProgram(absolutePath,
                                    {},
                                    workingDirectory,
                                    QStringLiteral("Could not start executable"),
                                    QStringLiteral("Linux could not start this executable file."));
    case LaunchService::LaunchCategory::UnknownExecutable:
        return openDocumentWithDesktop(absolutePath);
    case LaunchService::LaunchCategory::WindowsApplication:
        return failure(LaunchService::LaunchErrorCode::WindowsAppRequiresExplicitRunner,
                       QStringLiteral("Windows application"),
                       QStringLiteral("This is a Windows application. Use Open with Wine or Open with Steam Proton from the context menu."),
                       {},
                       true);
    case LaunchService::LaunchCategory::NonExecutableScript:
        return failure(LaunchService::LaunchErrorCode::NotExecutable,
                       QStringLiteral("Script is not executable"),
                       QStringLiteral("Mark this script as executable, then try opening it again."),
                       {},
                       true);
    case LaunchService::LaunchCategory::DesktopLauncherBlocked:
        return failure(LaunchService::LaunchErrorCode::DesktopLauncherUntrusted,
                       QStringLiteral("Desktop launcher is not trusted"),
                       QStringLiteral("Mark this desktop launcher as executable before opening it."),
                       {},
                       true);
    case LaunchService::LaunchCategory::Unsupported:
        return failure(LaunchService::LaunchErrorCode::UnsupportedPlatform,
                       QStringLiteral("Could not open file"),
                       QStringLiteral("This file type is not supported by the Linux launch path yet."),
                       {},
                       true);
    }
    return failure(LaunchService::LaunchErrorCode::UnknownFailure,
                   QStringLiteral("Could not open file"),
                   QStringLiteral("Linux could not open this file."),
                   {},
                   true);
}
#endif

} // namespace

namespace LaunchService {

LaunchResult openPath(const QString &path)
{
    const LaunchResult validation = validateLocalFilePath(path);
    if (!validation.ok) {
        return validation;
    }
    const QString localPath = localPathFromInput(path);

#ifdef Q_OS_WIN
    return openPathWithWindowsShell(localPath);
#elif defined(Q_OS_LINUX)
    return openPathWithLinuxPolicy(localPath);
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

LaunchResult openWithWine(const QString &path)
{
    const LaunchResult validation = validateLocalFilePath(path);
    if (!validation.ok) {
        return validation;
    }

#if defined(Q_OS_LINUX)
    const QString localPath = localPathFromInput(path);
    const QFileInfo info(localPath);
    if (classifyLocalPath(info.absoluteFilePath()) != LaunchCategory::WindowsApplication) {
        return failure(LaunchErrorCode::UnsupportedPlatform,
                       QStringLiteral("Wine launch is not available"),
                       QStringLiteral("Open with Wine is only available for Windows applications."));
    }

    const QString wine = QStandardPaths::findExecutable(QStringLiteral("wine"));
    if (wine.isEmpty()) {
        return failure(LaunchErrorCode::RunnerUnavailable,
                       QStringLiteral("Wine is not installed"),
                       QStringLiteral("Install Wine and try Open with Wine again."),
                       {},
                       true);
    }

    return startDetachedProgram(wine,
                                {info.absoluteFilePath()},
                                info.absolutePath(),
                                QStringLiteral("Could not start Wine"),
                                QStringLiteral("Could not start Wine for this file."));
#else
    return failure(LaunchErrorCode::UnsupportedPlatform,
                   QStringLiteral("Wine launch is not available"),
                   QStringLiteral("Open with Wine is only available on Linux."));
#endif
}

LaunchResult openWithSteamProton(const QString &path)
{
#if defined(Q_OS_LINUX)
    return openWithSteamProton(path,
                               savedProtonRuntimeId(),
                               savedProtonVkBasaltEnabled(),
                               savedProtonCaptureLog(),
                               savedProtonClearXModifiers());
#else
    return openWithSteamProton(path, QString(), false, false, false);
#endif
}

LaunchResult openWithSteamProton(const QString &path,
                                 const QString &runtimeId,
                                 bool enableVkBasalt,
                                 bool captureLog,
                                 bool clearXModifiers)
{
    const LaunchResult validation = validateLocalFilePath(path);
    if (!validation.ok) {
        return validation;
    }

#if defined(Q_OS_LINUX)
    const QString localPath = localPathFromInput(path);
    const QFileInfo info(localPath);
    if (classifyLocalPath(info.absoluteFilePath()) != LaunchCategory::WindowsApplication) {
        return failure(LaunchErrorCode::UnsupportedPlatform,
                       QStringLiteral("Steam Proton launch is not available"),
                       QStringLiteral("Open with Steam Proton is only available for Windows applications."));
    }

    const QList<ProtonRuntime> runtimes = discoverProtonRuntimes();
    const std::optional<ProtonRuntime> maybeRuntime = protonRuntimeById(runtimes, runtimeId);
    if (!maybeRuntime.has_value()) {
        return failure(LaunchErrorCode::RunnerUnavailable,
                       QStringLiteral("Steam Proton is not available"),
                       QStringLiteral("Install Steam and a Proton compatibility tool, then try Open with Steam Proton again."),
                       QStringLiteral("Checked Steam roots: %1").arg(steamRootCandidates().join(QStringLiteral(", "))),
                       true);
    }

    const ProtonRuntime runtime = maybeRuntime.value();
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH"), runtime.steamRoot);
    environment.insert(QStringLiteral("STEAM_COMPAT_DATA_PATH"), protonCompatDataPathForTarget(info.absoluteFilePath()));
    if (enableVkBasalt && vkBasaltAvailable()) {
        environment.insert(QStringLiteral("ENABLE_VKBASALT"), QStringLiteral("1"));
    }
    if (clearXModifiers) {
        environment.insert(QStringLiteral("XMODIFIERS"), QString());
    }
    if (captureLog) {
        const QString logToken = sanitizedProtonLogTokenForTarget(info.absoluteFilePath());
        environment.insert(QStringLiteral("PROTON_LOG"), QStringLiteral("1"));
        environment.insert(QStringLiteral("PROTON_LOG_DIR"), protonLogDirectory());

        return startDetachedWithEnvironmentAndLog(runtime.protonExecutable,
                                                  {QStringLiteral("run"), info.absoluteFilePath()},
                                                  info.absolutePath(),
                                                  environment,
                                                  protonLogFilePathForToken(logToken),
                                                  QStringLiteral("Could not start Steam Proton"),
                                                  QStringLiteral("Could not start Steam Proton for this file."));
    }

    return startDetachedWithEnvironment(runtime.protonExecutable,
                                        {QStringLiteral("run"), info.absoluteFilePath()},
                                        info.absolutePath(),
                                        environment,
                                        QStringLiteral("Could not start Steam Proton"),
                                        QStringLiteral("Could not start Steam Proton for this file."));
#else
    return failure(LaunchErrorCode::UnsupportedPlatform,
                   QStringLiteral("Steam Proton launch is not available"),
                   QStringLiteral("Open with Steam Proton is only available on Linux."));
#endif
}

QVariantMap steamProtonLaunchOptions(const QString &path)
{
    QVariantMap options;
    const LaunchResult validation = validateLocalFilePath(path);
    if (!validation.ok) {
        options.insert(QStringLiteral("available"), false);
        options.insert(QStringLiteral("errorTitle"), validation.title);
        options.insert(QStringLiteral("errorMessage"), validation.message);
        return options;
    }

#if defined(Q_OS_LINUX)
    const QString localPath = localPathFromInput(path);
    const QFileInfo info(localPath);
    if (classifyLocalPath(info.absoluteFilePath()) != LaunchCategory::WindowsApplication) {
        options.insert(QStringLiteral("available"), false);
        options.insert(QStringLiteral("errorTitle"), QStringLiteral("Steam Proton launch is not available"));
        options.insert(QStringLiteral("errorMessage"), QStringLiteral("Open with Steam Proton is only available for Windows applications."));
        return options;
    }

    const QList<ProtonRuntime> runtimes = discoverProtonRuntimes();
    QVariantList runtimeMaps;
    for (const ProtonRuntime &runtime : runtimes) {
        runtimeMaps.append(protonRuntimeMap(runtime));
    }

    const QString selectedId = savedProtonRuntimeId();
    const bool basaltAvailable = vkBasaltAvailable();
    options.insert(QStringLiteral("available"), !runtimes.isEmpty());
    options.insert(QStringLiteral("targetPath"), QDir::toNativeSeparators(info.absoluteFilePath()));
    options.insert(QStringLiteral("targetName"), info.fileName());
    options.insert(QStringLiteral("runtimes"), runtimeMaps);
    options.insert(QStringLiteral("selectedRuntimeId"), selectedId.isEmpty() ? QStringLiteral("auto") : selectedId);
    options.insert(QStringLiteral("vkBasaltAvailable"), basaltAvailable);
    options.insert(QStringLiteral("vkBasaltEnabled"), basaltAvailable && savedProtonVkBasaltEnabled());
    options.insert(QStringLiteral("vkBasaltMessage"), basaltAvailable
                   ? QStringLiteral("vkBasalt is available for this launch.")
                   : QStringLiteral("vkBasalt was not found on this system."));
    options.insert(QStringLiteral("captureLog"), savedProtonCaptureLog());
    options.insert(QStringLiteral("clearXModifiers"), savedProtonClearXModifiers());
    options.insert(QStringLiteral("logDirectory"), QDir::toNativeSeparators(protonLogDirectory()));
    options.insert(QStringLiteral("logFile"), QStringLiteral("%1-YYYYMMDD-HHMMSS.log").arg(QFileInfo(info.absoluteFilePath()).completeBaseName()));
    if (runtimes.isEmpty()) {
        options.insert(QStringLiteral("errorTitle"), QStringLiteral("Steam Proton is not available"));
        options.insert(QStringLiteral("errorMessage"), QStringLiteral("Install Steam and a Proton compatibility tool, then try Open with Steam Proton again."));
    }
    return options;
#else
    options.insert(QStringLiteral("available"), false);
    options.insert(QStringLiteral("errorTitle"), QStringLiteral("Steam Proton launch is not available"));
    options.insert(QStringLiteral("errorMessage"), QStringLiteral("Open with Steam Proton is only available on Linux."));
    return options;
#endif
}

void saveSteamProtonLaunchSettings(const QString &runtimeId,
                                   bool enableVkBasalt,
                                   bool captureLog,
                                   bool clearXModifiers)
{
    QSettings settings;
    settings.beginGroup(QStringLiteral("launch"));
    settings.setValue(QStringLiteral("protonRuntimeId"), runtimeId.trimmed().isEmpty() ? QStringLiteral("auto") : runtimeId.trimmed());
    settings.setValue(QStringLiteral("protonVkBasaltEnabled"), enableVkBasalt);
    settings.setValue(QStringLiteral("protonCaptureLog"), captureLog);
    settings.setValue(QStringLiteral("protonClearXModifiers"), clearXModifiers);
}

LaunchCapabilities launchCapabilities(const QString &path)
{
    LaunchCapabilities capabilities;
    if (!hasLocalLaunchableScheme(path)) {
        capabilities.openBlockedReason = QStringLiteral("This location does not support direct file launch.");
        return capabilities;
    }

    const QString localPath = localPathFromInput(path);
    const QFileInfo info(localPath);
    capabilities.isLocal = true;
    if (!info.exists() || !info.isFile()) {
        capabilities.openBlockedReason = QStringLiteral("The selected file is no longer available.");
        return capabilities;
    }

    capabilities.canOpen = true;
#if defined(Q_OS_LINUX)
    capabilities.category = classifyLocalPath(info.absoluteFilePath());
    capabilities.isWindowsApplication = capabilities.category == LaunchCategory::WindowsApplication;
    capabilities.canOpenWithWine = capabilities.isWindowsApplication;
    capabilities.canOpenWithSteamProton = capabilities.isWindowsApplication;
    switch (capabilities.category) {
    case LaunchCategory::WindowsApplication:
        capabilities.canOpen = false;
        capabilities.openBlockedReason = QStringLiteral("Use Open with Wine or Open with Steam Proton from the context menu.");
        break;
    case LaunchCategory::NonExecutableScript:
        capabilities.canOpen = false;
        capabilities.openBlockedReason = QStringLiteral("Mark this script as executable, then try opening it again.");
        break;
    case LaunchCategory::DesktopLauncherBlocked:
        capabilities.canOpen = false;
        capabilities.openBlockedReason = QStringLiteral("Mark this desktop launcher as executable before opening it.");
        break;
    case LaunchCategory::Unsupported:
        capabilities.canOpen = false;
        capabilities.openBlockedReason = QStringLiteral("This file type is not supported by the Linux launch path yet.");
        break;
    default:
        break;
    }
#else
    capabilities.category = LaunchCategory::Document;
#endif
    return capabilities;
}

QVariantMap launchCapabilitiesMap(const QString &path)
{
    const LaunchCapabilities capabilities = launchCapabilities(path);
    QVariantMap map;
    map.insert(QStringLiteral("canOpen"), capabilities.canOpen);
    map.insert(QStringLiteral("canOpenWithWine"), capabilities.canOpenWithWine);
    map.insert(QStringLiteral("canOpenWithSteamProton"), capabilities.canOpenWithSteamProton);
    map.insert(QStringLiteral("isLocal"), capabilities.isLocal);
    map.insert(QStringLiteral("isWindowsApplication"), capabilities.isWindowsApplication);
    map.insert(QStringLiteral("openBlockedReason"), capabilities.openBlockedReason);
    map.insert(QStringLiteral("category"), categoryName(capabilities.category));
    return map;
}

} // namespace LaunchService
