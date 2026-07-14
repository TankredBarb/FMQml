#include "InstagramInternal.h"

#include <QCoreApplication>
#include <QDebug>

namespace InstagramAuth {
QByteArray sessionCookieHeader()
{
    return {};
}
}

using namespace InstagramProviderInternal;

namespace {
int fail(const QString &message)
{
    qCritical().noquote() << message;
    return 1;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const ParsedPath post = parseInstagramPath(QStringLiteral(" INSTAGRAM://POST/DaQrtl7HLQ3/ "));
    if (!post.valid || post.kind != QStringLiteral("post")
        || post.normalized != QStringLiteral("instagram://post/DaQrtl7HLQ3")) {
        return fail(QStringLiteral("Instagram post normalization changed"));
    }
    const ParsedPath stories = parseInstagramPath(QStringLiteral("instagram://user/nasa/stories/story.jpg"));
    if (!stories.valid || !stories.stories || stories.storyItemName != QStringLiteral("story.jpg")) {
        return fail(QStringLiteral("Instagram stories path classification changed"));
    }
    if (parentInstagramPath(QStringLiteral("instagram://user/nasa/stories"))
        != QStringLiteral("instagram://user/nasa")) {
        return fail(QStringLiteral("Instagram Stories must navigate back to the profile root"));
    }
    if (instagramUrlToPath(QStringLiteral("https://www.instagram.com/reel/DaVp3AgscMA/"))
            != QStringLiteral("instagram://reel/DaVp3AgscMA")
        || instagramUrlToPath(QStringLiteral("https://www.instagram.com/nasa/"))
            != QStringLiteral("instagram://user/nasa")) {
        return fail(QStringLiteral("Instagram URL preprocessing changed"));
    }
    if (parseInstagramPath(QStringLiteral("instagram://unknown/value")).valid
        || !instagramUrlToPath(QStringLiteral("https://example.com/nasa/")).isEmpty()) {
        return fail(QStringLiteral("Invalid Instagram routes must remain rejected"));
    }

    InstagramPost profile;
    profile.rootPath = QStringLiteral("instagram://user/nasa");
    profile.hasNextPage = true;
    InstagramMediaItem media;
    media.name = QStringLiteral("post.jpg");
    media.path = profile.rootPath + QStringLiteral("/post.jpg");
    profile.items.append(media);
    InstagramPost storiesPost;
    InstagramMediaItem story;
    story.name = QStringLiteral("story.jpg");
    story.path = profile.rootPath + QStringLiteral("/stories/story.jpg");
    storiesPost.items.append(story);
    const QList<FileEntry> entries = entriesFromProfile(profile, storiesPost);
    if (entries.size() != 3
        || entries.at(1).path != QStringLiteral("instagram://user/nasa/stories")
        || entries.at(2).path != QStringLiteral("instagram://user/nasa/__load_more__")) {
        return fail(QStringLiteral("Profile pagination must preserve the stories entry"));
    }
    return 0;
}
