#include "PathSemantics.h"

#include <QDebug>

namespace {
int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}
}

int main()
{
    struct ClassificationCase {
        QString path;
        PathKind kind;
        QString scheme;
    };
    const QList<ClassificationCase> cases = {
        {{}, PathKind::Empty, {}},
        {QStringLiteral("/tmp/folder with # and %"), PathKind::Local, {}},
        {QStringLiteral("C:\\Users\\Example"), PathKind::Local, {}},
        {QStringLiteral("\\\\server\\share\\folder"), PathKind::Local, {}},
        {QStringLiteral("file:///tmp/example"), PathKind::FileUrl, QStringLiteral("file")},
        {QStringLiteral("ARCHIVE:///tmp/example.zip!/folder"), PathKind::Archive, QStringLiteral("archive")},
        {QStringLiteral("devices://"), PathKind::DevicesRoot, QStringLiteral("devices")},
        {QStringLiteral("favorites://"), PathKind::FavoritesRoot, QStringLiteral("favorites")},
        {QStringLiteral("gdrive://my-drive"), PathKind::Provider, QStringLiteral("gdrive")},
        {QStringLiteral("MEGA://root"), PathKind::Provider, QStringLiteral("mega")},
        {QStringLiteral("telegram://chats"), PathKind::Provider, QStringLiteral("telegram")},
        {QStringLiteral("instagram://stories"), PathKind::Provider, QStringLiteral("instagram")},
        {QStringLiteral("portable://device/id"), PathKind::Provider, QStringLiteral("portable")},
        {QStringLiteral("unknown+scheme://value"), PathKind::Provider, QStringLiteral("unknown+scheme")},
        {QStringLiteral("1invalid://value"), PathKind::Unknown, {}}
    };
    for (const ClassificationCase &test : cases) {
        const PathDescriptor actual = PathSemantics::describe(test.path);
        if (actual.kind != test.kind || actual.scheme != test.scheme) {
            return fail(QStringLiteral("classification failed for '%1'").arg(test.path));
        }
    }

    const QString localPath = QStringLiteral("/tmp/folder with # and %/file.txt");
    const QString fileUrl = PathSemantics::fileUrlFromLocalPath(localPath);
    if (!fileUrl.contains(QStringLiteral("%23")) || !fileUrl.contains(QStringLiteral("%25"))
        || PathSemantics::localPathFromFileUrl(fileUrl) != localPath) {
        return fail(QStringLiteral("file URL round trip did not preserve reserved characters"));
    }
    if (!PathSemantics::isProviderPath(QStringLiteral("gdrive://my-drive"))
        || PathSemantics::isProviderPath(QStringLiteral("archive:///tmp/a.zip"))
        || !PathSemantics::hasExplicitNonLocalScheme(QStringLiteral("archive:///tmp/a.zip"))
        || PathSemantics::hasExplicitNonLocalScheme(QStringLiteral("file:///tmp/a"))) {
        return fail(QStringLiteral("provider or non-local scheme classification is inconsistent"));
    }
    if (PathSemantics::compatibilityKindName(PathSemantics::describe(QStringLiteral("portable://device"))) != QLatin1String("remote")
        || PathSemantics::compatibilityKindName(PathSemantics::describe(QStringLiteral("gdrive://my-drive"))) != QLatin1String("gdrive")) {
        return fail(QStringLiteral("legacy path-kind mapping changed"));
    }
    return 0;
}
