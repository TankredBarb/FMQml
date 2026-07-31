#include "FileEntryPresentationResolver.h"
#include "FileTypeIconResolver.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

namespace {
bool expect(bool condition, const QString &message)
{
    if (!condition) QTextStream(stderr) << "FAILED: " << message << '\n';
    return condition;
}
}

int main(int argc, char **argv)
{
    QStandardPaths::setTestModeEnabled(true);
    QSettings::setPath(QSettings::NativeFormat, QSettings::UserScope,
                       QDir::tempPath() + QStringLiteral("/fmqml-file-entry-presentation-test"));
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("FMQmlTest"));
    QCoreApplication::setApplicationName(QStringLiteral("FileEntryPresentationResolverTest"));
    bool ok = true;

    {
        QSettings settings;
        settings.beginGroup(QStringLiteral("appearance"));
        settings.remove(QStringLiteral("iconOverridesRules"));
        settings.endGroup();
    }
    FileTypeIconResolver iconOverrides;
    ok &= expect(iconOverrides.addOrUpdateIconOverride(QStringLiteral("zip"), QStringLiteral("bundled"), QStringLiteral("archive")),
                 QStringLiteral("Could not add ZIP override"));
    ok &= expect(iconOverrides.addOrUpdateIconOverride(QStringLiteral(".fb2.zip"), QStringLiteral("bundled"), QStringLiteral("fb2")),
                 QStringLiteral("Could not add compound FB2 override"));
    ok &= expect(!iconOverrides.addOrUpdateIconOverride(QStringLiteral("fb2.zip"), QStringLiteral("bundled"), QStringLiteral("fb2.zip")),
                 QStringLiteral("Invalid bundled icon name was accepted"));
    ok &= expect(!iconOverrides.addOrUpdateIconOverride(QStringLiteral("bad/suffix"), QStringLiteral("bundled"), QStringLiteral("document")),
                 QStringLiteral("Invalid suffix was accepted"));
    ok &= expect(iconOverrides.nativeIconOverrideForPathHint(QStringLiteral("/tmp/book.FB2.ZIP"), false).endsWith(QStringLiteral("fb2.svg")),
                 QStringLiteral("Compound suffix did not take precedence"));
    ok &= expect(iconOverrides.nativeIconOverrideForPathHint(QStringLiteral("/tmp/archive.zip"), false).endsWith(QStringLiteral("archive.svg")),
                 QStringLiteral("Ordinary ZIP did not keep its own override"));
    FileTypeIconResolver reloadedOverrides;
    {
        QSettings settings;
        settings.sync();
        settings.beginGroup(QStringLiteral("appearance"));
        const bool rulesWereSaved = !settings.value(QStringLiteral("iconOverridesRules")).toByteArray().isEmpty();
        settings.endGroup();
        ok &= expect(rulesWereSaved,
                     QStringLiteral("Icon overrides were not written to QSettings"));
    }
    ok &= expect(reloadedOverrides.nativeIconOverrideForPathHint(QStringLiteral("/tmp/book.fb2.zip"), false).endsWith(QStringLiteral("fb2.svg")),
                 QStringLiteral("Icon overrides were not persisted"));
    iconOverrides.clearIconOverrides();

    ok &= expect(FileEntryPresentationResolver::breadcrumbIconNameForPath(QStringLiteral("gdrive://shared-with-me"))
                     == QStringLiteral("gdrive-badge-shared"),
                 QStringLiteral("GDrive shared breadcrumb icon changed"));
    ok &= expect(FileEntryPresentationResolver::breadcrumbIconNameForPath(QStringLiteral("mega:///folder"))
                     == QStringLiteral("mega"),
                 QStringLiteral("Mega breadcrumb branding changed"));
    ok &= expect(FileEntryPresentationResolver::breadcrumbIconNameForPath(QStringLiteral("telegram://chat/42"))
                     == QStringLiteral("telegram-badge-chat"),
                 QStringLiteral("Telegram chat breadcrumb icon changed"));
    ok &= expect(FileEntryPresentationResolver::previewIconNameForPath(QStringLiteral("gdrive://"))
                     == QStringLiteral("gdrive"),
                 QStringLiteral("GDrive preview branding changed"));
    ok &= expect(FileEntryPresentationResolver::previewIconNameForPath(QStringLiteral("gdrive://item/42")).isEmpty(),
                 QStringLiteral("Ordinary GDrive items should keep their file-type preview icon"));
    ok &= expect(FileEntryPresentationResolver::previewIconSource(
                     QStringLiteral("selection://"), false, {}, {}, true)
                     == QStringLiteral("qrc:/qt/qml/FM/qml/assets/icons/grid.svg"),
                 QStringLiteral("Selection preview icon changed"));
    ok &= expect(FileEntryPresentationResolver::previewIconSource(
                     QStringLiteral("mega://item/photo.jpg"), false, QStringLiteral("jpg"),
                     QStringLiteral("image/jpeg"), false).endsWith(QStringLiteral("image.svg")),
                 QStringLiteral("Remote suffix fallback changed"));
    ok &= expect(FileEntryPresentationResolver::previewIconSource(
                     QStringLiteral("/tmp/book.epub"), false, QStringLiteral("epub"),
                     QStringLiteral("application/epub+zip"), false).endsWith(QStringLiteral("epub.svg")),
                 QStringLiteral("EPUB fallback icon changed"));

    FileEntry ordinaryFolder;
    ordinaryFolder.path = QStringLiteral("gdrive://item/folder-id");
    ordinaryFolder.iconName = QStringLiteral("folder");
    ok &= expect(FileEntryPresentationResolver::menuIconName(ordinaryFolder).isEmpty(),
                 QStringLiteral("Ordinary provider folder should use the generic folder icon"));

    FileEntry cloudDrive;
    cloudDrive.path = QStringLiteral("mega:///Cloud Drive");
    cloudDrive.iconName = QStringLiteral("folder");
    ok &= expect(FileEntryPresentationResolver::menuIconName(cloudDrive) == QStringLiteral("mega"),
                 QStringLiteral("Mega Cloud Drive menu branding changed"));

    FileEntry chat;
    chat.iconName = QStringLiteral("telegram-badge-chat");
    chat.hasThumbnail = true;
    ok &= expect(FileEntryPresentationResolver::menuUsesAvatar(chat),
                 QStringLiteral("Telegram chat avatar eligibility changed"));
    chat.hasThumbnail = false;
    ok &= expect(!FileEntryPresentationResolver::menuUsesAvatar(chat),
                 QStringLiteral("Telegram chat without a thumbnail should use fallback"));

    return ok ? 0 : 1;
}
