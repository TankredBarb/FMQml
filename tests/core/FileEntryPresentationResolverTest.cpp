#include "FileEntryPresentationResolver.h"

#include <QCoreApplication>
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
    QCoreApplication app(argc, argv);
    bool ok = true;

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
