#include "TerminalLauncher.h"

#include "CleanupSubsystem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryFile>
#include <QTimer>
#include <QTextStream>
#include <QThread>

namespace {

    bool launch(const QString &program, const QStringList &arguments, const QString &workingDirectory)
    {
        if (QStandardPaths::findExecutable(program).isEmpty()) {
            return false;
        }
        return QProcess::startDetached(program, arguments, workingDirectory);
    }

    QString shellQuote(const QString &value)
    {
        QString quoted = value;
        quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
        return QStringLiteral("'%1'").arg(quoted);
    }

    bool runDbus(const QString &dbus, const QStringList &arguments, QString *output = nullptr)
    {
        QProcess process;
        process.start(dbus, arguments);
        if (!process.waitForFinished(2000) || process.exitStatus() != QProcess::NormalExit
            || process.exitCode() != 0) {
            return false;
            }

            if (output) {
                *output = QString::fromLocal8Bit(process.readAllStandardOutput()).trimmed();
            }
            return true;
    }

    bool yakuakeServiceAvailable(const QString &dbus)
    {
        QString services;
        return runDbus(dbus, {}, &services) && services.contains(QStringLiteral("org.kde.yakuake"));
    }

    bool waitForYakuakeService(const QString &dbus)
    {
        for (int attempt = 0; attempt < 10; ++attempt) {
            if (yakuakeServiceAvailable(dbus)) {
                return true;
            }
            QThread::msleep(250);
        }
        return false;
    }

    bool launchYakuake(const QString &workingDirectory)
    {
        QString dbus;
        for (const QString &candidate : {QStringLiteral("qdbus6"), QStringLiteral("qdbus"), QStringLiteral("qdbus-qt5")}) {
            if (!QStandardPaths::findExecutable(candidate).isEmpty()) {
                dbus = candidate;
                break;
            }
        }
        if (dbus.isEmpty()) {
            return false;
        }

        if (!yakuakeServiceAvailable(dbus)) {
            if (!launch(QStringLiteral("yakuake"), {}, workingDirectory) || !waitForYakuakeService(dbus)) {
                return false;
            }
        }

        const QString cleanupRoot = StagingLocationPolicy::defaultCleanupRoot();
        if (cleanupRoot.isEmpty()) {
            return false;
        }
        const QString sessionRoot = QDir(cleanupRoot).filePath(QStringLiteral("yakuake-session"));
        if (!QDir().mkpath(sessionRoot)) {
            return false;
        }

        QTemporaryFile sessionFile(QDir(sessionRoot).filePath(QStringLiteral("fmqml-yakuake-session-XXXXXX")));
        sessionFile.setAutoRemove(false);
        if (!sessionFile.open()) {
            return false;
        }

        const QString sessionFileName = sessionFile.fileName();
        QString sessionLeaseId;
        CleanupSubsystem::instance().registerArtifact(
            CleanupArtifactKind::YakuakeSession,
            sessionFileName,
            sessionRoot,
            false,
            &sessionLeaseId);
        QTextStream stream(&sessionFile);
        stream << QStringLiteral("clear\n");
        stream << QStringLiteral("rm -f ") << shellQuote(sessionFileName) << QStringLiteral(" 2>/dev/null\n");
        stream << QStringLiteral("cd -- ") << shellQuote(workingDirectory) << QStringLiteral("\n");
        stream.flush();
        sessionFile.close();

        // Yakuake's runCommand API targets the newly-added session. Feeding it a
        // short source file mirrors the yakuake-session script and is more reliable
        // across user shells than trying to inline an escaped cd command.
        if (!runDbus(dbus, {QStringLiteral("org.kde.yakuake"), QStringLiteral("/yakuake/sessions"),
            QStringLiteral("addSession")})) {
            if (!sessionLeaseId.isEmpty()) {
                CleanupSubsystem::instance().scheduleDeleteOnFailure(sessionLeaseId);
            } else {
                QFile::remove(sessionFileName);
            }
            return false;
            }

            if (!runDbus(dbus, {QStringLiteral("org.kde.yakuake"), QStringLiteral("/yakuake/sessions"),
                QStringLiteral("runCommand"),
                         QStringLiteral(" source %1").arg(shellQuote(sessionFileName))})) {
                if (!sessionLeaseId.isEmpty()) {
                    CleanupSubsystem::instance().scheduleDeleteOnFailure(sessionLeaseId);
                } else {
                    QFile::remove(sessionFileName);
                }
                return false;
                         }

                         QString sessionIds;
                         if (runDbus(dbus, {QStringLiteral("org.kde.yakuake"), QStringLiteral("/yakuake/sessions"),
                             QStringLiteral("sessionIdList")}, &sessionIds)) {
                             const QStringList ids = sessionIds.split(QLatin1Char(','), Qt::SkipEmptyParts);
                             if (!ids.isEmpty()) {
                                 const QString sessionId = ids.constLast().trimmed();
                                 runDbus(dbus, {QStringLiteral("org.kde.yakuake"), QStringLiteral("/yakuake/tabs"),
                                     QStringLiteral("setTabTitle"), sessionId, QFileInfo(workingDirectory).fileName()});
                             }
                             }

                             if (!sessionLeaseId.isEmpty()) {
                                 QTimer::singleShot(60000, &CleanupSubsystem::instance(), [sessionLeaseId]() {
                                     CleanupSubsystem::instance().scheduleDelete(sessionLeaseId);
                                 });
                             }

                             runDbus(dbus, {QStringLiteral("org.kde.yakuake"), QStringLiteral("/yakuake/window"),
                                 QStringLiteral("toggleWindowState")});
                             return true;
    }

} // namespace

namespace TerminalLauncher {

    bool openTerminalAt(const QString &folder)
    {
        const QFileInfo info(folder);
        if (!info.exists() || !info.isDir()) {
            return false;
        }

        const QString workingDirectory = QDir::cleanPath(info.absoluteFilePath());

        #if defined(Q_OS_WIN)
        const QString nativePath = QDir::toNativeSeparators(workingDirectory);
        return launch(QStringLiteral("wt.exe"),
                      {QStringLiteral("-d"), nativePath, QStringLiteral("powershell.exe"),
                          QStringLiteral("-NoExit"), QStringLiteral("-Command"),
                      QStringLiteral("Set-Location '%1'").arg(nativePath)},
                      workingDirectory);
        #elif defined(Q_OS_MACOS)
        return launch(QStringLiteral("open"),
                      {QStringLiteral("-a"), QStringLiteral("Terminal"), workingDirectory},
                      workingDirectory);
        #else
        // Prefer the freedesktop portal helper when available, then try Yakuake's
        // D-Bus API using the same source-file approach as yakuake-session. After
        // that, fall back to desktop-specific terminal CLIs. KDE's Konsole, for
        // example, does not understand x-terminal-emulator's --working-directory
        // argument, but it does support --workdir.
        return launch(QStringLiteral("xdg-terminal-exec"), {}, workingDirectory)
        || launchYakuake(workingDirectory)
        || launch(QStringLiteral("konsole"), {QStringLiteral("--workdir"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("kgx"), {QStringLiteral("--working-directory"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("gnome-terminal"), {QStringLiteral("--working-directory"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("xfce4-terminal"), {QStringLiteral("--working-directory"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("mate-terminal"), {QStringLiteral("--working-directory"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("tilix"), {QStringLiteral("--working-directory"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("kitty"), {QStringLiteral("--directory"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("alacritty"), {QStringLiteral("--working-directory"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("wezterm"), {QStringLiteral("start"), QStringLiteral("--cwd"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("foot"), {QStringLiteral("--working-directory"), workingDirectory}, workingDirectory)
        || launch(QStringLiteral("x-terminal-emulator"), {}, workingDirectory)
        || launch(QStringLiteral("xterm"), {}, workingDirectory);
        #endif
    }

} // namespace TerminalLauncher
