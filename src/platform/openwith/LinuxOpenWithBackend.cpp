#include "LinuxOpenWithBackend.h"

#include "../../core/LaunchService.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProcess>
#include <QRegularExpression>
#include <QSettings>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>
#include <optional>

namespace {

OpenWithResult linuxFailure(LaunchService::LaunchErrorCode code, const QString &title, const QString &message)
{
    return {false, code, title, message, {}, true};
}

#if defined(Q_OS_LINUX)
struct DesktopEntry {
    QString id;
    QString path;
    QString name;
    QString icon;
    QString exec;
    QStringList mimeTypes;
    bool terminal = false;
};

QStringList xdgDataDirectories()
{
    QStringList directories;
    const QString home = qEnvironmentVariable("XDG_DATA_HOME");
    directories.append(home.isEmpty() ? QDir::home().filePath(QStringLiteral(".local/share")) : home);
    const QString system = qEnvironmentVariable("XDG_DATA_DIRS", QStringLiteral("/usr/local/share:/usr/share"));
    directories.append(system.split(QLatin1Char(':'), Qt::SkipEmptyParts));
    directories.removeDuplicates();
    return directories;
}

QStringList xdgConfigDirectories()
{
    QStringList directories;
    const QString home = qEnvironmentVariable("XDG_CONFIG_HOME");
    directories.append(home.isEmpty() ? QDir::home().filePath(QStringLiteral(".config")) : home);
    directories.append(qEnvironmentVariable("XDG_CONFIG_DIRS", QStringLiteral("/etc/xdg")).split(QLatin1Char(':'), Qt::SkipEmptyParts));
    directories.removeDuplicates();
    return directories;
}

QString desktopId(const QString &applicationsRoot, const QString &path)
{
    QString id = QDir(applicationsRoot).relativeFilePath(path);
    id.replace(QLatin1Char('/'), QLatin1Char('-'));
    return id;
}

bool desktopVisible(const QSettings &settings)
{
    if (settings.value(QStringLiteral("Hidden"), false).toBool()) {
        return false;
    }
    const QStringList desktops = qEnvironmentVariable("XDG_CURRENT_DESKTOP").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    const auto matchesDesktop = [&desktops](const QStringList &list) {
        return std::any_of(list.cbegin(), list.cend(), [&desktops](const QString &item) {
            return std::any_of(desktops.cbegin(), desktops.cend(), [&item](const QString &desktop) {
                return item.compare(desktop, Qt::CaseInsensitive) == 0;
            });
        });
    };
    const QStringList onlyShowIn = settings.value(QStringLiteral("OnlyShowIn")).toString().split(QLatin1Char(';'), Qt::SkipEmptyParts);
    if (!onlyShowIn.isEmpty() && !matchesDesktop(onlyShowIn)) {
        return false;
    }
    const QStringList notShowIn = settings.value(QStringLiteral("NotShowIn")).toString().split(QLatin1Char(';'), Qt::SkipEmptyParts);
    return !matchesDesktop(notShowIn);
}

QString desktopEntryValue(const QString &path, const QString &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    bool inDesktopEntry = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inDesktopEntry = line == QLatin1String("[Desktop Entry]");
            continue;
        }
        if (inDesktopEntry && line.startsWith(key + QLatin1Char('='))) {
            return line.mid(key.size() + 1);
        }
    }
    return {};
}

QString mimeAppsValue(const QString &path, const QString &group, const QString &key)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    bool inGroup = false;
    while (!file.atEnd()) {
        const QString line = QString::fromUtf8(file.readLine()).trimmed();
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            inGroup = line == QStringLiteral("[%1]").arg(group);
            continue;
        }
        if (inGroup && line.startsWith(key + QLatin1Char('='))) {
            return line.mid(key.size() + 1);
        }
    }
    return {};
}

QSet<QString> supportedMimeTypes(const QString &mimeType)
{
    QSet<QString> types{mimeType};
    const QMimeType type = QMimeDatabase().mimeTypeForName(mimeType);
    for (const QString &parent : type.parentMimeTypes()) {
        types.insert(parent);
    }
    if (mimeType == QLatin1String("image/svg+xml") || mimeType == QLatin1String("application/xml")
        || mimeType == QLatin1String("text/xml")) {
        types.insert(QStringLiteral("text/plain"));
    }
    return types;
}

QStringList mimeAppsAssociatedIds(const QSet<QString> &mimeTypes)
{
    QStringList ids;
    const auto appendFromFile = [&ids, &mimeTypes](const QString &path) {
        for (const QString &mimeType : mimeTypes) {
            const QStringList values = mimeAppsValue(path, QStringLiteral("Added Associations"), mimeType)
                                           .split(QLatin1Char(';'), Qt::SkipEmptyParts);
            for (const QString &id : values) {
                if (!ids.contains(id)) ids.append(id);
            }
        }
    };
    for (const QString &directory : xdgConfigDirectories()) {
        appendFromFile(QDir(directory).filePath(QStringLiteral("mimeapps.list")));
        appendFromFile(QDir(directory).filePath(QStringLiteral("kde-mimeapps.list")));
    }
    for (const QString &dataDirectory : xdgDataDirectories()) {
        appendFromFile(QDir(dataDirectory).filePath(QStringLiteral("applications/mimeapps.list")));
        appendFromFile(QDir(dataDirectory).filePath(QStringLiteral("applications/kde-mimeapps.list")));
    }
    return ids;
}

std::optional<DesktopEntry> readDesktopEntry(const QString &applicationsRoot, const QString &path)
{
    QSettings settings(path, QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("Desktop Entry"));
    if (settings.value(QStringLiteral("Type")).toString() != QLatin1String("Application") || !desktopVisible(settings)) {
        return std::nullopt;
    }

    const QString exec = settings.value(QStringLiteral("Exec")).toString().trimmed();
    if (exec.isEmpty()) {
        return std::nullopt;
    }
    const QString tryExec = settings.value(QStringLiteral("TryExec")).toString().trimmed();
    if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty()) {
        return std::nullopt;
    }

    DesktopEntry entry;
    entry.id = desktopId(applicationsRoot, path);
    entry.path = path;
    entry.name = settings.value(QStringLiteral("Name")).toString().trimmed();
    entry.icon = settings.value(QStringLiteral("Icon")).toString().trimmed();
    entry.exec = exec;
    entry.mimeTypes = desktopEntryValue(path, QStringLiteral("MimeType")).split(QLatin1Char(';'), Qt::SkipEmptyParts);
    entry.terminal = settings.value(QStringLiteral("Terminal"), false).toBool();
    return entry;
}

QList<DesktopEntry> allDesktopEntries()
{
    QList<DesktopEntry> entries;
    QSet<QString> seenIds;
    for (const QString &dataDirectory : xdgDataDirectories()) {
        const QString applicationsRoot = QDir(dataDirectory).filePath(QStringLiteral("applications"));
        const QDir root(applicationsRoot);
        if (!root.exists()) {
            continue;
        }
        QDirIterator it(applicationsRoot, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const std::optional<DesktopEntry> entry = readDesktopEntry(applicationsRoot, path);
            if (entry.has_value() && !seenIds.contains(entry->id)) {
                seenIds.insert(entry->id);
                entries.append(*entry);
            }
        }
    }
    return entries;
}

QString defaultDesktopId(const QString &mimeType)
{
    for (const QString &directory : xdgConfigDirectories()) {
        for (const QString &name : {QStringLiteral("mimeapps.list"), QStringLiteral("kde-mimeapps.list")}) {
            QSettings settings(QDir(directory).filePath(name), QSettings::IniFormat);
            settings.beginGroup(QStringLiteral("Default Applications"));
            const QString id = settings.value(mimeType).toString().section(QLatin1Char(';'), 0, 0).trimmed();
            if (!id.isEmpty()) {
                return id;
            }
        }
    }
    for (const QString &dataDirectory : xdgDataDirectories()) {
        for (const QString &name : {QStringLiteral("mimeapps.list"), QStringLiteral("kde-mimeapps.list")}) {
            QSettings settings(QDir(dataDirectory).filePath(QStringLiteral("applications/%1").arg(name)), QSettings::IniFormat);
            settings.beginGroup(QStringLiteral("Default Applications"));
            const QString id = settings.value(mimeType).toString().section(QLatin1Char(';'), 0, 0).trimmed();
            if (!id.isEmpty()) {
                return id;
            }
        }
    }
    return {};
}

std::optional<DesktopEntry> desktopEntryById(const QString &id)
{
    for (const QString &dataDirectory : xdgDataDirectories()) {
        const QString root = QDir(dataDirectory).filePath(QStringLiteral("applications"));
        const QString directPath = QDir(root).filePath(id);
        if (QFileInfo(directPath).isFile()) {
            return readDesktopEntry(root, directPath);
        }
        QDirIterator it(root, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            if (desktopId(root, path) == id) {
                return readDesktopEntry(root, path);
            }
        }
    }
    return std::nullopt;
}

QStringList expandExec(const DesktopEntry &entry, const QList<OpenWithTarget> &targets)
{
    QStringList result;
    const QStringList files = [&targets] {
        QStringList values;
        for (const OpenWithTarget &target : targets) values.append(target.path);
        return values;
    }();
    const QStringList urls = [&targets] {
        QStringList values;
        for (const OpenWithTarget &target : targets) values.append(QUrl::fromLocalFile(target.path).toString());
        return values;
    }();

    for (QString token : QProcess::splitCommand(entry.exec)) {
        if (token == QLatin1String("%F")) { result.append(files); continue; }
        if (token == QLatin1String("%U")) { result.append(urls); continue; }
        if (token == QLatin1String("%i")) {
            if (!entry.icon.isEmpty()) result.append({QStringLiteral("--icon"), entry.icon});
            continue;
        }
        token.replace(QStringLiteral("%f"), files.first());
        token.replace(QStringLiteral("%u"), urls.first());
        token.replace(QStringLiteral("%c"), entry.name);
        token.replace(QStringLiteral("%k"), entry.path);
        token.replace(QStringLiteral("%%"), QStringLiteral("%"));
        token.remove(QRegularExpression(QStringLiteral("%[A-Za-z]")));
        if (!token.isEmpty()) result.append(token);
    }
    return result;
}

QString applicationKey(const DesktopEntry &entry)
{
    const QStringList command = QProcess::splitCommand(entry.exec);
    if (command.isEmpty()) {
        return entry.id;
    }
    const QString program = command.first();
    const QString executable = QStandardPaths::findExecutable(program);
    const QFileInfo info(executable.isEmpty() ? program : executable);
    const QString canonical = info.canonicalFilePath();
    return canonical.isEmpty() ? info.absoluteFilePath() : canonical;
}
#endif

} // namespace

QList<OpenWithCandidate> LinuxOpenWithBackend::enumerateCandidates(const OpenWithTarget &target) const
{
#if defined(Q_OS_LINUX)
    if (!target.isLocal || target.mimeType.isEmpty()) {
        return {};
    }
    const QSet<QString> supportedTypes = supportedMimeTypes(target.mimeType);
    const QString defaultId = defaultDesktopId(target.mimeType);
    const QStringList associationIds = mimeAppsAssociatedIds(supportedTypes);
    QList<OpenWithCandidate> candidates;
    QHash<QString, int> candidateIndexByApplication;
    for (const DesktopEntry &entry : allDesktopEntries()) {
        if (entry.terminal) {
            continue;
        }
        const bool supportsMimeType = std::any_of(entry.mimeTypes.cbegin(), entry.mimeTypes.cend(), [&supportedTypes](const QString &mimeType) {
            return supportedTypes.contains(mimeType);
        });
        if (!supportsMimeType && !associationIds.contains(entry.id)) {
            continue;
        }
        OpenWithCandidate candidate;
        candidate.id = entry.id;
        candidate.displayName = entry.name.isEmpty() ? entry.id : entry.name;
        candidate.iconName = entry.icon;
        candidate.recommended = entry.id == defaultId;
        candidate.systemDefault = entry.id == defaultId;
        candidate.supportsMultipleFiles = entry.exec.contains(QStringLiteral("%F")) || entry.exec.contains(QStringLiteral("%U"));
        const QString key = applicationKey(entry);
        const auto existing = candidateIndexByApplication.constFind(key);
        if (existing == candidateIndexByApplication.cend()) {
            candidateIndexByApplication.insert(key, candidates.size());
            candidates.append(candidate);
        } else if (candidate.systemDefault) {
            candidates[*existing] = candidate;
        }
    }
    if (target.category == LaunchService::LaunchCategory::WindowsApplication) {
        OpenWithCandidate wine;
        wine.id = QStringLiteral("runner:wine");
        wine.displayName = QStringLiteral("Wine");
        wine.iconName = QStringLiteral("wine");
        wine.kind = OpenWithCandidateKind::Wine;
        wine.available = !QStandardPaths::findExecutable(QStringLiteral("wine")).isEmpty();
        wine.unavailableReason = wine.available ? QString() : QStringLiteral("Install Wine to run this Windows application.");
        candidates.append(wine);

        OpenWithCandidate proton;
        proton.id = QStringLiteral("runner:proton");
        proton.displayName = QStringLiteral("Steam Proton");
        proton.iconName = QStringLiteral("steam");
        proton.kind = OpenWithCandidateKind::Proton;
        const QVariantMap options = LaunchService::steamProtonLaunchOptions(target.path);
        proton.available = options.value(QStringLiteral("available")).toBool();
        proton.unavailableReason = proton.available ? QString() : options.value(QStringLiteral("errorMessage")).toString();
        candidates.append(proton);
    }
    std::sort(candidates.begin(), candidates.end(), [](const OpenWithCandidate &left, const OpenWithCandidate &right) {
        if (left.systemDefault != right.systemDefault) return left.systemDefault;
        return left.displayName.localeAwareCompare(right.displayName) < 0;
    });
    return candidates;
#else
    Q_UNUSED(target)
    return {};
#endif
}

OpenWithResult LinuxOpenWithBackend::launch(const QList<OpenWithTarget> &targets, const OpenWithCandidate &candidate) const
{
#if defined(Q_OS_LINUX)
    if (candidate.kind == OpenWithCandidateKind::Wine) {
        const LaunchService::LaunchResult result = LaunchService::openWithWine(targets.first().path);
        return {result.ok, result.errorCode, result.title, result.message, result.details, result.showDialog};
    }
    if (candidate.kind == OpenWithCandidateKind::Proton) {
        const LaunchService::LaunchResult result = LaunchService::openWithSteamProton(targets.first().path);
        return {result.ok, result.errorCode, result.title, result.message, result.details, result.showDialog};
    }
    const std::optional<DesktopEntry> entry = desktopEntryById(candidate.id);
    if (!entry.has_value()) return linuxFailure(LaunchService::LaunchErrorCode::NoAssociation, QStringLiteral("Application is not available"), QStringLiteral("The selected application is no longer installed."));
    if (entry->terminal) return linuxFailure(LaunchService::LaunchErrorCode::UnsupportedPlatform, QStringLiteral("Terminal application is not supported"), QStringLiteral("Launching terminal desktop applications is not implemented yet."));
    const QStringList command = expandExec(*entry, targets);
    if (command.isEmpty()) return linuxFailure(LaunchService::LaunchErrorCode::InvalidExecutable, QStringLiteral("Invalid application launcher"), QStringLiteral("The selected application's desktop entry has no executable command."));
    if (!QProcess::startDetached(command.first(), command.sliced(1), QFileInfo(targets.first().path).absolutePath())) {
        return linuxFailure(LaunchService::LaunchErrorCode::RunnerStartFailed, QStringLiteral("Could not start application"), QStringLiteral("The selected application could not be started."));
    }
    OpenWithResult result;
    result.ok = true;
    return result;
#else
    Q_UNUSED(targets)
    Q_UNUSED(candidate)
    return linuxFailure(LaunchService::LaunchErrorCode::UnsupportedPlatform, QStringLiteral("Open With is not available"), QStringLiteral("This platform does not have a Linux Open With backend."));
#endif
}
