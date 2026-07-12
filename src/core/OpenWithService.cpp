#include "OpenWithService.h"

#include <QFileInfo>
#include <QMimeDatabase>
#include <QSettings>
#include <QUrl>

#include <algorithm>

namespace {

QString openWithLocalPathFromInput(const QString &path)
{
    const QUrl url(path.trimmed());
    return url.scheme() == QLatin1String("file") ? url.toLocalFile() : path.trimmed();
}

QString categoryKey(LaunchService::LaunchCategory category)
{
    switch (category) {
    case LaunchService::LaunchCategory::NativeExecutableElf:
        return QStringLiteral("native-executable-elf");
    case LaunchService::LaunchCategory::NativeExecutableScript:
        return QStringLiteral("native-executable-script");
    case LaunchService::LaunchCategory::NativeExecutableAppImage:
        return QStringLiteral("native-executable-appimage");
    case LaunchService::LaunchCategory::DesktopLauncherTrusted:
    case LaunchService::LaunchCategory::DesktopLauncherBlocked:
        return QStringLiteral("desktop-launcher");
    case LaunchService::LaunchCategory::WindowsApplication:
        return QStringLiteral("windows-application");
    case LaunchService::LaunchCategory::NonExecutableScript:
        return QStringLiteral("non-executable-script");
    case LaunchService::LaunchCategory::UnknownExecutable:
        return QStringLiteral("unknown-executable");
    case LaunchService::LaunchCategory::Document:
    case LaunchService::LaunchCategory::Unsupported:
        return {};
    }
    return {};
}

QString contentTypeKey(const QFileInfo &info, const QString &mimeType, LaunchService::LaunchCategory category)
{
    const QString categoryPart = categoryKey(category);
    if (!categoryPart.isEmpty()) {
        return QStringLiteral("category/%1").arg(categoryPart);
    }
    if (!mimeType.isEmpty() && mimeType != QLatin1String("application/octet-stream")) {
        return QStringLiteral("mime/%1").arg(mimeType.toLower());
    }
    if (!info.suffix().isEmpty()) {
        return QStringLiteral("suffix/%1").arg(info.suffix().toLower());
    }
    return QStringLiteral("unknown");
}

OpenWithResult openWithFailure(LaunchService::LaunchErrorCode errorCode,
                               const QString &title,
                               const QString &message,
                               bool showDialog = false)
{
    return {false, errorCode, title, message, {}, showDialog};
}

QString preferenceGroup(const QString &contentTypeKey)
{
    return QStringLiteral("openWith/preferences/v1/%1").arg(QString::fromLatin1(contentTypeKey.toUtf8().toHex()));
}

} // namespace

OpenWithService::OpenWithService(const OpenWithBackend *backend)
    : m_backend(backend)
{
}

OpenWithTarget OpenWithService::targetInfo(const QString &path) const
{
    OpenWithTarget target;
    target.path = openWithLocalPathFromInput(path);

    const LaunchService::LaunchCapabilities capabilities = LaunchService::launchCapabilities(path);
    target.category = capabilities.category;
    target.isLocal = capabilities.isLocal;
    target.isLaunchable = capabilities.isLocal && QFileInfo::exists(target.path);
    target.blockedReason = capabilities.openBlockedReason;

    const QFileInfo info(target.path);
    target.displayName = info.fileName();
    if (!info.exists() || !info.isFile()) {
        return target;
    }

    const QMimeType mime = QMimeDatabase().mimeTypeForFile(info, QMimeDatabase::MatchContent);
    target.mimeType = mime.name();
    target.suffix = info.suffix().toLower();
    target.contentTypeKey = contentTypeKey(info, target.mimeType, target.category);
    return target;
}

QList<OpenWithCandidate> OpenWithService::candidatesForPath(const QString &path) const
{
    const OpenWithTarget target = targetInfo(path);
    if (!target.isLocal || target.contentTypeKey.isEmpty() || !m_backend) {
        return {};
    }

    QList<OpenWithCandidate> candidates = m_backend->enumerateCandidates(target);
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FMQml"), QStringLiteral("OpenWith"));
    const QString preferencePath = preferenceGroup(target.contentTypeKey);
    const QString preferredId = settings.value(preferencePath + QStringLiteral("/candidateId")).toString();
    for (OpenWithCandidate &candidate : candidates) {
        candidate.fmDefault = candidate.available && candidate.id == preferredId;
    }
    return candidates;
}

std::optional<OpenWithCandidate> OpenWithService::effectiveCandidate(const QString &path) const
{
    const QList<OpenWithCandidate> candidates = candidatesForPath(path);
    for (const OpenWithCandidate &candidate : candidates) {
        if (candidate.fmDefault) {
            return candidate;
        }
    }
    for (const OpenWithCandidate &candidate : candidates) {
        if (candidate.available && candidate.systemDefault) {
            return candidate;
        }
    }
    return std::nullopt;
}

bool OpenWithService::setPreferredCandidate(const QString &path, const QString &candidateId) const
{
    const OpenWithTarget target = targetInfo(path);
    if (!target.isLocal || target.contentTypeKey.isEmpty()) {
        return false;
    }

    const QString normalizedCandidateId = candidateId.trimmed();
    if (normalizedCandidateId.isEmpty()) {
        return false;
    }

    QString displayName;
    for (const OpenWithCandidate &candidate : candidatesForPath(path)) {
        if (candidate.available && candidate.id == normalizedCandidateId) {
            displayName = candidate.displayName;
            break;
        }
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FMQml"), QStringLiteral("OpenWith"));
    const QString preferencePath = preferenceGroup(target.contentTypeKey);
    settings.setValue(preferencePath + QStringLiteral("/candidateId"), normalizedCandidateId);
    settings.setValue(preferencePath + QStringLiteral("/displayName"), displayName);
    settings.setValue(preferencePath + QStringLiteral("/schemaVersion"), 1);
    settings.sync();
    return settings.status() == QSettings::NoError
        && settings.value(preferencePath + QStringLiteral("/candidateId")).toString() == normalizedCandidateId;
}

void OpenWithService::clearPreferredCandidate(const QString &path) const
{
    const OpenWithTarget target = targetInfo(path);
    if (!target.isLocal || target.contentTypeKey.isEmpty()) {
        return;
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FMQml"), QStringLiteral("OpenWith"));
    settings.remove(preferenceGroup(target.contentTypeKey));
    settings.sync();
}

OpenWithResult OpenWithService::openWith(const QString &path, const QString &candidateId) const
{
    return openWithMany({path}, candidateId);
}

OpenWithResult OpenWithService::openWithMany(const QStringList &paths, const QString &candidateId) const
{
    if (!m_backend) {
        return openWithFailure(LaunchService::LaunchErrorCode::UnsupportedPlatform,
                               QStringLiteral("Open With is not available"),
                               QStringLiteral("This platform does not have an Open With backend yet."),
                               true);
    }

    QList<OpenWithTarget> targets;
    for (const QString &path : paths) {
        const OpenWithTarget target = targetInfo(path);
        if (!target.isLocal || target.contentTypeKey.isEmpty()) {
            return openWithFailure(LaunchService::LaunchErrorCode::NotLocalPath,
                                   QStringLiteral("Cannot open non-local file"),
                                   target.blockedReason.isEmpty()
                                       ? QStringLiteral("This location does not support direct file launch.")
                                       : target.blockedReason);
        }
        targets.append(target);
    }
    if (targets.isEmpty()) {
        return openWithFailure(LaunchService::LaunchErrorCode::FileNotFound,
                               QStringLiteral("No files selected"),
                               QStringLiteral("Select a local file to open."));
    }

    const QList<OpenWithCandidate> candidates = m_backend->enumerateCandidates(targets.first());
    const auto candidate = std::find_if(candidates.cbegin(), candidates.cend(), [&candidateId, &targets](const OpenWithCandidate &item) {
        return item.available && item.id == candidateId && (targets.size() == 1 || item.supportsMultipleFiles);
    });
    if (candidate == candidates.cend()) {
        return openWithFailure(LaunchService::LaunchErrorCode::NoAssociation,
                               QStringLiteral("Application is not available"),
                               QStringLiteral("The selected application is no longer available."),
                               true);
    }
    return m_backend->launch(targets, *candidate);
}
