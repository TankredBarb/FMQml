#include "LaunchService.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

#include <sys/stat.h>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(contents);
    return file.flush();
}

bool chmodPath(const QString &path, mode_t mode)
{
    return ::chmod(QFile::encodeName(path).constData(), mode) == 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QTemporaryDir tempDir;
    if (!tempDir.isValid()) {
        return fail(QStringLiteral("failed to create temp dir"));
    }

    const QString exePath = tempDir.filePath(QStringLiteral("game.exe"));
    if (!writeFile(exePath, QByteArray("MZ") + QByteArray(80, '\0'))) {
        return fail(QStringLiteral("failed to write PE fixture"));
    }

    const QVariantMap exeCaps = LaunchService::launchCapabilitiesMap(exePath);
    if (exeCaps.value(QStringLiteral("category")).toString() != QLatin1String("windowsApplication")
            || !exeCaps.value(QStringLiteral("canOpenWithWine")).toBool()
            || !exeCaps.value(QStringLiteral("canOpenWithSteamProton")).toBool()) {
        return fail(QStringLiteral("Windows application capabilities were not exposed"));
    }

    const LaunchService::LaunchResult normalOpen = LaunchService::openPath(exePath);
    if (normalOpen.ok
            || normalOpen.errorCode != LaunchService::LaunchErrorCode::WindowsAppRequiresExplicitRunner) {
        return fail(QStringLiteral("normal Open should block Windows applications on Linux"));
    }

    const QString blockedScriptPath = tempDir.filePath(QStringLiteral("blocked.sh"));
    if (!writeFile(blockedScriptPath, "#!/bin/sh\nexit 0\n") || !chmodPath(blockedScriptPath, 0640)) {
        return fail(QStringLiteral("failed to create non-executable script fixture"));
    }

    const QVariantMap blockedScriptCaps = LaunchService::launchCapabilitiesMap(blockedScriptPath);
    if (blockedScriptCaps.value(QStringLiteral("category")).toString() != QLatin1String("nonExecutableScript")) {
        return fail(QStringLiteral("non-executable shebang script was not classified"));
    }

    const LaunchService::LaunchResult blockedScriptOpen = LaunchService::openPath(blockedScriptPath);
    if (blockedScriptOpen.ok || blockedScriptOpen.errorCode != LaunchService::LaunchErrorCode::NotExecutable) {
        return fail(QStringLiteral("non-executable script should not launch"));
    }

    const QString scriptPath = tempDir.filePath(QStringLiteral("ok.sh"));
    if (!writeFile(scriptPath, "#!/bin/sh\nexit 0\n") || !chmodPath(scriptPath, 0750)) {
        return fail(QStringLiteral("failed to create executable script fixture"));
    }

    const QVariantMap scriptCaps = LaunchService::launchCapabilitiesMap(scriptPath);
    if (scriptCaps.value(QStringLiteral("category")).toString() != QLatin1String("nativeExecutableScript")
            || scriptCaps.value(QStringLiteral("isWindowsApplication")).toBool()) {
        return fail(QStringLiteral("executable shebang script was not classified as native"));
    }

    const QString executableDocumentPath = tempDir.filePath(QStringLiteral("notes.txt"));
    if (!writeFile(executableDocumentPath, "plain text\n") || !chmodPath(executableDocumentPath, 0755)) {
        return fail(QStringLiteral("failed to create executable document fixture"));
    }

    const QVariantMap executableDocumentCaps = LaunchService::launchCapabilitiesMap(executableDocumentPath);
    if (executableDocumentCaps.value(QStringLiteral("category")).toString() != QLatin1String("document")
            || executableDocumentCaps.value(QStringLiteral("isWindowsApplication")).toBool()
            || executableDocumentCaps.value(QStringLiteral("canOpenWithWine")).toBool()) {
        return fail(QStringLiteral("executable-bit document should stay a document"));
    }

    return 0;
}
