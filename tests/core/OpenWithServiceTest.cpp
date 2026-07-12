#include "OpenWithService.h"
#include "LinuxOpenWithBackend.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QTemporaryDir>
#include <QTextStream>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

class TestBackend final : public OpenWithBackend {
public:
    QList<OpenWithCandidate> enumerateCandidates(const OpenWithTarget &) const override
    {
        OpenWithCandidate systemEditor;
        systemEditor.id = QStringLiteral("system-editor");
        systemEditor.displayName = QStringLiteral("System Editor");
        systemEditor.systemDefault = true;
        OpenWithCandidate alternateEditor;
        alternateEditor.id = QStringLiteral("alternate-editor");
        alternateEditor.displayName = QStringLiteral("Alternate Editor");
        return {systemEditor, alternateEditor};
    }

    OpenWithResult launch(const QList<OpenWithTarget> &, const OpenWithCandidate &) const override
    {
        OpenWithResult result;
        result.ok = true;
        return result;
    }
};

bool writeFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write("test\n") > 0;
}

bool writeTextFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

} // namespace

int main(int argc, char **argv)
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) {
        return fail(QStringLiteral("failed to create settings directory"));
    }
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());

    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("FMQmlTestOrg"));
    QCoreApplication::setApplicationName(QStringLiteral("OpenWithServiceTest"));
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FMQml"), QStringLiteral("OpenWith"));
        settings.clear();
        settings.sync();
    }

    QTemporaryDir tempDir;
    const QString path = tempDir.filePath(QStringLiteral("notes.txt"));
    if (!tempDir.isValid() || !writeFile(path)) {
        return fail(QStringLiteral("failed to create fixture"));
    }

    TestBackend backend;
    OpenWithService service(&backend);
    const OpenWithTarget target = service.targetInfo(path);
    if (!target.isLocal || target.contentTypeKey.isEmpty() || target.category != LaunchService::LaunchCategory::Document) {
        return fail(QStringLiteral("local document target was not classified"));
    }
    const QList<OpenWithCandidate> candidates = service.candidatesForPath(path);
    if (candidates.size() != 2 || candidates.at(1).id != QLatin1String("alternate-editor") || !candidates.at(1).available) {
        return fail(QStringLiteral("test backend candidates were not available: %1").arg(candidates.size()));
    }
    const OpenWithTarget beforePreferenceTarget = service.targetInfo(path);
    if (!beforePreferenceTarget.isLocal || beforePreferenceTarget.contentTypeKey.isEmpty()) {
        return fail(QStringLiteral("target became unavailable before preference"));
    }
    const QString alternateEditorId = candidates.at(1).id;
    if (!service.setPreferredCandidate(path, alternateEditorId)) {
        return fail(QStringLiteral("could not store candidate preference"));
    }
    const OpenWithTarget preferenceTarget = service.targetInfo(path);
    if (preferenceTarget.contentTypeKey != target.contentTypeKey) {
        return fail(QStringLiteral("content type key changed: %1 -> %2").arg(target.contentTypeKey, preferenceTarget.contentTypeKey));
    }
    {
        QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FMQml"), QStringLiteral("OpenWith"));
        const QString preferencePath = QStringLiteral("openWith/preferences/v1/%1/candidateId")
                                         .arg(QString::fromLatin1(target.contentTypeKey.toUtf8().toHex()));
        if (settings.value(preferencePath).toString() != QLatin1String("alternate-editor")) {
            return fail(QStringLiteral("candidate preference was not persisted"));
        }
    }
    const auto preferred = service.effectiveCandidate(path);
    if (!preferred || preferred->id != QLatin1String("alternate-editor") || !preferred->fmDefault) {
        return fail(QStringLiteral("stored preference was not resolved: %1").arg(preferred ? preferred->id : QStringLiteral("none")));
    }

    service.clearPreferredCandidate(path);
    const auto systemDefault = service.effectiveCandidate(path);
    if (!systemDefault || systemDefault->id != QLatin1String("system-editor")) {
        return fail(QStringLiteral("system default was not restored"));
    }

    QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FMQml"), QStringLiteral("OpenWith"));
    settings.setValue(QStringLiteral("openWith/preferences/v1/%1/candidateId")
                          .arg(QString::fromLatin1(target.contentTypeKey.toUtf8().toHex())),
                      QStringLiteral("removed-editor"));
    settings.sync();
    const auto staleFallback = service.effectiveCandidate(path);
    if (!staleFallback || staleFallback->id != QLatin1String("system-editor")) {
        return fail(QStringLiteral("stale preference did not fall back to system default"));
    }

    const OpenWithTarget provider = service.targetInfo(QStringLiteral("telegram://chats/1"));
    if (provider.isLocal || provider.isLaunchable || !provider.contentTypeKey.isEmpty()) {
        return fail(QStringLiteral("provider path must remain blocked"));
    }

    QTemporaryDir xdgDir;
    if (!xdgDir.isValid() || !QDir().mkpath(xdgDir.filePath(QStringLiteral("data/applications")))
        || !QDir().mkpath(xdgDir.filePath(QStringLiteral("config")))) {
        return fail(QStringLiteral("failed to create XDG fixture"));
    }
    const QString desktopPath = xdgDir.filePath(QStringLiteral("data/applications/test-editor.desktop"));
    if (!writeTextFile(desktopPath,
                       "[Desktop Entry]\nType=Application\nName=Test Editor\nExec=/bin/true %F\n"
                       "MimeType=application/x-test-fixture;text/plain;\nIcon=accessories-text-editor\n")
        || !writeTextFile(xdgDir.filePath(QStringLiteral("config/mimeapps.list")),
                          "[Default Applications]\ntext/plain=test-editor.desktop;\n")) {
        return fail(QStringLiteral("failed to write XDG fixture"));
    }
    qputenv("XDG_DATA_HOME", xdgDir.filePath(QStringLiteral("data")).toUtf8());
    qputenv("XDG_DATA_DIRS", QByteArray());
    qputenv("XDG_CONFIG_HOME", xdgDir.filePath(QStringLiteral("config")).toUtf8());

    LinuxOpenWithBackend linuxBackend;
    OpenWithService linuxService(&linuxBackend);
    const QList<OpenWithCandidate> linuxCandidates = linuxService.candidatesForPath(path);
    if (linuxCandidates.size() != 1 || linuxCandidates.first().id != QLatin1String("test-editor.desktop")
        || !linuxCandidates.first().systemDefault || !linuxCandidates.first().supportsMultipleFiles) {
        return fail(QStringLiteral("Linux MIME desktop application was not discovered"));
    }
    const OpenWithResult linuxLaunch = linuxService.openWith(path, QStringLiteral("test-editor.desktop"));
    if (!linuxLaunch.ok) {
        return fail(QStringLiteral("Linux desktop application did not launch"));
    }

    QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("FMQml"), QStringLiteral("OpenWith")).clear();
    return 0;
}
