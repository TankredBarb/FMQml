#include "InstagramInternal.h"
#include "InstagramAuth.h"
#include <algorithm>
#include <limits>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSet>
#include <QTimeZone>
#include <QUrlQuery>
namespace InstagramProviderInternal {
QHash<QString, QString> &userCache()
{
    static QHash<QString, QString> cache;
    return cache;
}

QString fetchInstagramUserId(const QString &username)
{
    const QString lowerUser = username.toLower();
    {
        QMutexLocker locker(&cacheMutex());
        const auto it = userCache().constFind(lowerUser);
        if (it != userCache().cend() && !it.value().isEmpty()) {
            return it.value();
        }
    }

    const QUrl searchUrl(QStringLiteral("https://www.instagram.com/web/search/topsearch/?query=%1").arg(lowerUser));
    QString contentType;
    QString error;
    const QByteArray searchBytes = httpGetBytes(searchUrl, &contentType, &error);
    if (!searchBytes.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(searchBytes);
        const QJsonArray users = doc.object().value(QStringLiteral("users")).toArray();
        for (const QJsonValue &val : users) {
            const QJsonObject user = val.toObject().value(QStringLiteral("user")).toObject();
            const QString foundUsername = user.value(QStringLiteral("username")).toString();
            if (foundUsername.compare(lowerUser, Qt::CaseInsensitive) == 0) {
                QString pk = user.value(QStringLiteral("pk")).toVariant().toString();
                if (pk.isEmpty()) {
                    pk = user.value(QStringLiteral("pk_id")).toVariant().toString();
                }
                if (pk.isEmpty()) {
                    pk = user.value(QStringLiteral("id")).toVariant().toString();
                }
                if (!pk.isEmpty()) {
                    traceInstagram(QStringLiteral("Resolved user_id via topsearch"));
                    QMutexLocker locker(&cacheMutex());
                    userCache().insert(lowerUser, pk);
                    return pk;
                }
            }
        }
    }

    const QUrl infoUrl(QStringLiteral("https://www.instagram.com/api/v1/users/web_profile_info/?username=%1").arg(lowerUser));
    const QByteArray infoBytes = httpGetBytes(infoUrl, &contentType, &error);
    if (!infoBytes.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(infoBytes);
        const QString pk = doc.object()
                               .value(QStringLiteral("data"))
                               .toObject()
                               .value(QStringLiteral("user"))
                               .toObject()
                               .value(QStringLiteral("id"))
                               .toString();
        if (!pk.isEmpty()) {
            traceInstagram(QStringLiteral("Resolved user_id via web_profile_info"));
            QMutexLocker locker(&cacheMutex());
            userCache().insert(lowerUser, pk);
            return pk;
        }
    }

    return {};
}

InstagramPost fetchProfileFeedPage(const ParsedPath &path,
                                    const QString &after,
                                    const InstagramPost *basePost = nullptr)
{
    traceInstagram(QStringLiteral("Feed profile after=%1 baseItems=%2")
                       .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            QString::number(basePost ? basePost->items.size() : 0)));

    InstagramPost post;
    if (basePost) {
        post = *basePost;
    } else {
        post.rootPath = path.rootPath;
        post.kind = path.kind;
        post.shortcode = path.shortcode;
        post.sourceUrl = QStringLiteral("https://www.instagram.com/%1/").arg(path.shortcode);
        post.title = path.shortcode;
        post.fetchedAt = QDateTime::currentDateTimeUtc();
    }

    const QString userId = fetchInstagramUserId(path.shortcode);
    if (userId.isEmpty()) {
        post.error = QStringLiteral("Could not resolve Instagram user ID for profile '%1'.").arg(path.shortcode);
        return post;
    }

    QUrl url(QStringLiteral("https://www.instagram.com/api/v1/feed/user/%1/").arg(userId));
    if (!after.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("max_id"), after);
        url.setQuery(query);
    }

    QString contentType;
    QString error;
    const QByteArray bytes = httpGetBytes(url, &contentType, &error);
    if (bytes.isEmpty()) {
        post.error = error.isEmpty() ? QStringLiteral("Instagram feed returned an empty response") : error;
        return post;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        post.error = QStringLiteral("Instagram feed JSON parse error: %1").arg(parseError.errorString());
        return post;
    }

    const QJsonObject obj = doc.object();
    const QJsonArray items = obj.value(QStringLiteral("items")).toArray();
    if (items.isEmpty() && !post.items.isEmpty()) {
        post.hasNextPage = false;
        post.nextCursor.clear();
        return post;
    }

    QSet<QString> seenUrls;
    for (const InstagramMediaItem &item : post.items) {
        seenUrls.insert(item.url);
    }
    qsizetype index = post.items.size() + 1;

    for (const QJsonValue &itemVal : items) {
        appendJsonMediaNode(post, itemVal.toObject(), seenUrls, index);
    }

    post.fetchedAt = QDateTime::currentDateTimeUtc();
    const bool moreAvailable = obj.value(QStringLiteral("more_available")).toBool(false);
    const QString nextMaxId = obj.value(QStringLiteral("next_max_id")).toString();
    post.hasNextPage = moreAvailable && !nextMaxId.isEmpty();
    post.nextCursor = nextMaxId;

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram feed did not expose media for this profile.");
    } else {
        post.error.clear();
    }
    return post;
}

void appendStoryItemsFromArray(InstagramPost &post,
                               const QJsonArray &items,
                               QSet<QString> &seenUrls,
                               qsizetype &index)
{
    for (const QJsonValue &value : items) {
        appendJsonMediaNode(post, value.toObject(), seenUrls, index);
    }
}

InstagramPost parseInstagramStories(const ParsedPath &path, const QByteArray &json)
{
    InstagramPost post;
    post.rootPath = path.storiesRootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = QStringLiteral("https://www.instagram.com/stories/%1/").arg(path.shortcode);
    post.title = QStringLiteral("Stories");
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        post.error = QStringLiteral("Instagram stories JSON parse error: %1").arg(parseError.errorString());
        return post;
    }

    const QJsonObject root = document.object();
    QSet<QString> seenUrls;
    qsizetype index = 1;

    appendStoryItemsFromArray(post,
                              root.value(QStringLiteral("reel")).toObject().value(QStringLiteral("items")).toArray(),
                              seenUrls,
                              index);
    appendStoryItemsFromArray(post, root.value(QStringLiteral("items")).toArray(), seenUrls, index);

    const QJsonArray reelsMedia = root.value(QStringLiteral("reels_media")).toArray();
    for (const QJsonValue &value : reelsMedia) {
        appendStoryItemsFromArray(post, value.toObject().value(QStringLiteral("items")).toArray(), seenUrls, index);
    }

    const QJsonObject reels = root.value(QStringLiteral("reels")).toObject();
    for (auto it = reels.constBegin(); it != reels.constEnd(); ++it) {
        appendStoryItemsFromArray(post, it.value().toObject().value(QStringLiteral("items")).toArray(), seenUrls, index);
    }

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram stories are unavailable or empty for this profile.");
    } else {
        post.error.clear();
    }
    return post;
}

InstagramPost fetchStoriesForProfile(const ParsedPath &path)
{
    {
        QMutexLocker locker(&cacheMutex());
        const auto it = storyCache().constFind(path.storiesRootPath);
        if (it != storyCache().cend()
            && it->fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) < 120) {
            return it.value();
        }
    }

    InstagramPost post;
    post.rootPath = path.storiesRootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = QStringLiteral("https://www.instagram.com/stories/%1/").arg(path.shortcode);
    post.title = QStringLiteral("Stories");
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    if (InstagramAuth::sessionCookieHeader().isEmpty()) {
        post.error = QStringLiteral("Instagram stories require a logged-in session.");
        return post;
    }

    const QString userId = fetchInstagramUserId(path.shortcode);
    if (userId.isEmpty()) {
        post.error = QStringLiteral("Could not resolve Instagram user ID for stories '%1'.").arg(path.shortcode);
        return post;
    }

    QString contentType;
    QString error;
    const QUrl url(QStringLiteral("https://www.instagram.com/api/v1/feed/user/%1/story/").arg(userId));
    QByteArray bytes = httpGetBytes(url,
                                    {},
                                    &contentType,
                                    &error,
                                    true,
                                    QByteArray(InstagramMobileUserAgent));
    if (bytes.isEmpty()) {
        post.error = error.isEmpty() ? QStringLiteral("Instagram stories returned an empty response") : error;
    } else {
        post = parseInstagramStories(path, bytes);
    }
    if (!post.error.isEmpty()) {
        QUrl fallbackUrl(QStringLiteral("https://www.instagram.com/api/v1/feed/reels_media/"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("reel_ids"), userId);
        fallbackUrl.setQuery(query);
        error.clear();
        contentType.clear();
        bytes = httpGetBytes(fallbackUrl,
                             {},
                             &contentType,
                             &error,
                             true,
                             QByteArray(InstagramMobileUserAgent));
        if (!bytes.isEmpty()) {
            InstagramPost fallbackPost = parseInstagramStories(path, bytes);
            if (fallbackPost.error.isEmpty()) {
                post = fallbackPost;
            }
        }
    }

    traceInstagram(QStringLiteral("Stories profile items=%1 error=%2")
                       .arg(QString::number(post.items.size()),
                            post.error.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));

    QMutexLocker locker(&cacheMutex());
    storyCache().insert(path.storiesRootPath, post);
    return post;
}

void appendJsonTimeline(InstagramPost &post,
                        const QJsonObject &user,
                        const QString &timelineKey,
                        QSet<QString> &seenUrls,
                        qsizetype &index)
{
    const QJsonArray edges = user.value(timelineKey)
                                 .toObject()
                                 .value(QStringLiteral("edges"))
                                 .toArray();
    const qsizetype itemsBefore = post.items.size();
    for (const QJsonValue &edgeValue : edges) {
        appendJsonMediaNode(post, edgeValue.toObject().value(QStringLiteral("node")).toObject(), seenUrls, index);
    }
    traceInstagram(QStringLiteral("Profile timeline=%1 posts=%2 media=%3 totalMedia=%4")
                       .arg(timelineKey,
                            QString::number(edges.size()),
                            QString::number(post.items.size() - itemsBefore),
                            QString::number(post.items.size())));

    if (timelineKey == QLatin1String("edge_owner_to_timeline_media")) {
        const QJsonObject pageInfo = user.value(timelineKey)
                                         .toObject()
                                         .value(QStringLiteral("page_info"))
                                         .toObject();
        post.hasNextPage = pageInfo.value(QStringLiteral("has_next_page")).toBool(false);
        post.nextCursor = pageInfo.value(QStringLiteral("end_cursor")).toString();
    }
}

void appendJsonConnection(InstagramPost &post,
                          const QJsonObject &connection,
                          QSet<QString> &seenUrls,
                          qsizetype &index)
{
    const QJsonArray edges = connection.value(QStringLiteral("edges")).toArray();
    for (const QJsonValue &edgeValue : edges) {
        appendJsonMediaNode(post, edgeValue.toObject().value(QStringLiteral("node")).toObject(), seenUrls, index);
    }

    const QJsonObject pageInfo = connection.value(QStringLiteral("page_info")).toObject();
    post.hasNextPage = pageInfo.value(QStringLiteral("has_next_page")).toBool(false);
    post.nextCursor = pageInfo.value(QStringLiteral("end_cursor")).toString();
}

QByteArray profileGraphqlVariablesJson(const QString &username, const QString &after)
{
    QJsonObject variables;
    variables.insert(QStringLiteral("after"), after.isEmpty() ? QJsonValue(QJsonValue::Null) : QJsonValue(after));
    variables.insert(QStringLiteral("before"), QJsonValue::Null);
    variables.insert(QStringLiteral("data"), QJsonValue::Null);
    variables.insert(QStringLiteral("first"), InstagramProfileBatchSize);
    variables.insert(QStringLiteral("last"), QJsonValue::Null);
    variables.insert(QStringLiteral("username"), username);
    variables.insert(QStringLiteral("__relay_internal__pv__PolarisImmersiveFeedChainingEnabledrelayprovider"), false);
    variables.insert(QStringLiteral("__relay_internal__pv__PolarisAIGMMediaWebLabelEnabledrelayprovider"), false);
    variables.insert(QStringLiteral("__relay_internal__pv__PolarisAIGMAccountLabelEnabledrelayprovider"), false);
    variables.insert(QStringLiteral("__relay_internal__pv__PolarisReelsRecoDebugOverlayEnabledrelayprovider"), false);
    return QJsonDocument(variables).toJson(QJsonDocument::Compact);
}

QUrl profileGraphqlUrl(const QString &username, const QString &after)
{
    QUrl url(QStringLiteral("https://www.instagram.com/graphql/query/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("doc_id"), QString::fromLatin1(InstagramProfilePostsConnectionDocId));
    query.addQueryItem(QStringLiteral("variables"), QString::fromUtf8(profileGraphqlVariablesJson(username, after)));
    url.setQuery(query);
    return url;
}

QString firstRegexCapture(const QString &text, const QString &pattern)
{
    const QRegularExpressionMatch match = QRegularExpression(pattern).match(text);
    return match.hasMatch() ? match.captured(1) : QString{};
}

InstagramBootTokens fetchInstagramBootTokens(const QString &username)
{
    InstagramBootTokens tokens;
    QString contentType;
    QString error;
    const QUrl url(QStringLiteral("https://www.instagram.com/%1/").arg(username));
    const QByteArray htmlBytes = httpGetBytes(url, &contentType, &error);
    if (htmlBytes.isEmpty()) {
        traceInstagram(QStringLiteral("GraphQL boot page unavailable error=%1").arg(error));
        return tokens;
    }

    const QString html = QString::fromUtf8(htmlBytes);
    tokens.lsd = firstRegexCapture(html, QStringLiteral(R"ig(\["LSD",\[\],\{"token":"([^"]+)")ig"));
    tokens.dtsg = firstRegexCapture(html, QStringLiteral(R"ig(\["DTSGInitialData",\[\],\{"token":"([^"]+)")ig"));
    tokens.actorId = firstRegexCapture(html, QStringLiteral(R"ig("actorID":"([^"]+)")ig"));
    const QByteArray dsUserId = InstagramAuth::cookieValue(InstagramAuth::sessionCookieHeader(), "ds_user_id");
    traceInstagram(QStringLiteral("GraphQL boot tokens lsd=%1 dtsg=%2 actor=%3 actorMatchesCookieUser=%4")
                       .arg(tokens.lsd.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            tokens.dtsg.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            tokens.actorId.isEmpty() ? QStringLiteral("no")
                                : tokens.actorId == QLatin1String("0") ? QStringLiteral("zero")
                                : QStringLiteral("yes"),
                            !dsUserId.isEmpty() && tokens.actorId.toUtf8() == dsUserId
                                ? QStringLiteral("yes") : QStringLiteral("no")));
    return tokens;
}

QByteArray profileGraphqlPostBody(const QString &username,
                                  const QString &after,
                                  const InstagramBootTokens &tokens)
{
    QUrlQuery form;
    if (!tokens.actorId.isEmpty()) {
        form.addQueryItem(QStringLiteral("av"), tokens.actorId);
        form.addQueryItem(QStringLiteral("__user"), tokens.actorId);
    }
    form.addQueryItem(QStringLiteral("__a"), QStringLiteral("1"));
    form.addQueryItem(QStringLiteral("__req"), QStringLiteral("1"));
    if (!tokens.dtsg.isEmpty()) {
        form.addQueryItem(QStringLiteral("fb_dtsg"), tokens.dtsg);
    }
    if (!tokens.lsd.isEmpty()) {
        form.addQueryItem(QStringLiteral("lsd"), tokens.lsd);
    }
    form.addQueryItem(QStringLiteral("fb_api_req_friendly_name"),
                      QStringLiteral("PolarisProfilePostsTabContentQuery_connection"));
    form.addQueryItem(QStringLiteral("fb_api_caller_class"), QStringLiteral("RelayModern"));
    form.addQueryItem(QStringLiteral("server_timestamps"), QStringLiteral("true"));
    form.addQueryItem(QStringLiteral("doc_id"), QString::fromLatin1(InstagramProfilePostsConnectionDocId));
    form.addQueryItem(QStringLiteral("variables"), QString::fromUtf8(profileGraphqlVariablesJson(username, after)));
    return form.toString(QUrl::FullyEncoded).toUtf8();
}

QStringList jsonObjectKeys(const QJsonObject &object)
{
    QStringList keys;
    keys.reserve(object.size());
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        keys.append(it.key());
    }
    return keys;
}

int graphqlErrorCount(const QJsonObject &root)
{
    return root.value(QStringLiteral("errors")).toArray().size();
}

void traceGraphqlEnvelopeError(const QJsonObject &root)
{
    if (!instagramTraceEnabled() || !root.contains(QStringLiteral("error"))) {
        return;
    }

    const QJsonValue payloadValue = root.value(QStringLiteral("payload"));
    const QString payloadKeys = payloadValue.isObject()
        ? jsonObjectKeys(payloadValue.toObject()).join(QLatin1Char(','))
        : QString{};
    traceInstagram(QStringLiteral("GraphQL envelope error=%1 payloadType=%2 payloadKeys=%3")
                       .arg(root.value(QStringLiteral("error")).isUndefined() ? QStringLiteral("no") : QStringLiteral("yes"),
                            payloadValue.isObject() ? QStringLiteral("object")
                                : payloadValue.isArray() ? QStringLiteral("array")
                                : payloadValue.isString() ? QStringLiteral("string")
                                : payloadValue.isNull() ? QStringLiteral("null")
                                : QStringLiteral("other"),
                            payloadKeys));
}

QByteArray normalizedGraphqlJson(QByteArray json)
{
    json = json.trimmed();
    static constexpr QByteArrayView prefix("for (;;);");
    if (json.startsWith(prefix)) {
        json.remove(0, prefix.size());
    }
    return json.trimmed();
}

InstagramPost parseInstagramProfileGraphql(const ParsedPath &path,
                                           const QByteArray &json,
                                           const InstagramPost *basePost = nullptr)
{
    InstagramPost post;
    if (basePost) {
        post = *basePost;
    } else {
        post.rootPath = path.rootPath;
        post.kind = path.kind;
        post.shortcode = path.shortcode;
        post.sourceUrl = QStringLiteral("https://www.instagram.com/%1/").arg(path.shortcode);
        post.title = path.shortcode;
        post.fetchedAt = QDateTime::currentDateTimeUtc();
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(normalizedGraphqlJson(json), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        traceInstagram(QStringLiteral("GraphQL parse error offset=%1 error=%2")
                           .arg(parseError.offset)
                           .arg(parseError.errorString()));
    }
    const QJsonObject connection = document.object()
                                        .value(QStringLiteral("data"))
                                        .toObject()
                                        .value(QStringLiteral("xdt_api__v1__feed__user_timeline_graphql_connection"))
                                        .toObject();
    if (connection.isEmpty()) {
        const QJsonObject root = document.object();
        traceGraphqlEnvelopeError(root);
        traceInstagram(QStringLiteral("GraphQL missing connection rootKeys=%1 dataKeys=%2 errorCount=%3")
                           .arg(jsonObjectKeys(root).join(QLatin1Char(',')),
                                jsonObjectKeys(root.value(QStringLiteral("data")).toObject()).join(QLatin1Char(',')),
                                QString::number(graphqlErrorCount(root))));
        post.error = QStringLiteral("Instagram GraphQL profile data is unavailable.");
        return post;
    }

    QSet<QString> seenUrls;
    for (const InstagramMediaItem &item : post.items) {
        seenUrls.insert(item.url);
    }
    qsizetype index = post.items.size() + 1;
    appendJsonConnection(post, connection, seenUrls, index);
    post.fetchedAt = QDateTime::currentDateTimeUtc();
    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram GraphQL did not expose public media for this profile.");
    } else {
        post.error.clear();
    }
    return post;
}

InstagramPost fetchProfileGraphqlPage(const ParsedPath &path, const QString &after, const InstagramPost *basePost = nullptr)
{
    traceInstagram(QStringLiteral("GraphQL profile after=%1 baseItems=%2")
                       .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes"),
                            QString::number(basePost ? basePost->items.size() : 0)));
    QString contentType;
    QString error;
    const QByteArray bytes = httpGetBytes(profileGraphqlUrl(path.shortcode, after), &contentType, &error);
    if (bytes.isEmpty()) {
        InstagramPost post = basePost ? *basePost : InstagramPost{};
        post.rootPath = path.rootPath;
        post.kind = path.kind;
        post.shortcode = path.shortcode;
        post.sourceUrl = QStringLiteral("https://www.instagram.com/%1/").arg(path.shortcode);
        post.fetchedAt = QDateTime::currentDateTimeUtc();
        post.error = error.isEmpty() ? QStringLiteral("Instagram GraphQL returned an empty response") : error;
        return post;
    }
    InstagramPost post = parseInstagramProfileGraphql(path, bytes, basePost);
    if (!post.error.isEmpty() && !InstagramAuth::sessionCookieHeader().isEmpty()) {
        const InstagramBootTokens tokens = fetchInstagramBootTokens(path.shortcode);
        if (!tokens.lsd.isEmpty() || !tokens.dtsg.isEmpty() || !tokens.actorId.isEmpty()) {
            const QString referer = QStringLiteral("https://www.instagram.com/%1/").arg(path.shortcode);
            const QByteArray body = profileGraphqlPostBody(path.shortcode, after, tokens);
            error.clear();
            contentType.clear();
            const QByteArray postBytes = httpPostFormBytes(QUrl(QStringLiteral("https://www.instagram.com/api/graphql/")),
                                                           body,
                                                           referer,
                                                           tokens.lsd.toUtf8(),
                                                           &contentType,
                                                           &error);
            if (!postBytes.isEmpty()) {
                InstagramPost postResult = parseInstagramProfileGraphql(path, postBytes, basePost);
                if (postResult.error.isEmpty()) {
                    traceInstagram(QStringLiteral("GraphQL POST succeeded after=%1")
                                       .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
                    return postResult;
                }
                const QByteArray dsUserId = InstagramAuth::cookieValue(InstagramAuth::sessionCookieHeader(), "ds_user_id");
                if (!dsUserId.isEmpty() && tokens.actorId.toUtf8() != dsUserId) {
                    InstagramBootTokens cookieActorTokens = tokens;
                    cookieActorTokens.actorId = QString::fromLatin1(dsUserId);
                    traceInstagram(QStringLiteral("GraphQL POST retry with cookie user actor after=%1")
                                       .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
                    error.clear();
                    contentType.clear();
                    const QByteArray cookieActorBody = profileGraphqlPostBody(path.shortcode, after, cookieActorTokens);
                    const QByteArray cookieActorBytes = httpPostFormBytes(QUrl(QStringLiteral("https://www.instagram.com/api/graphql/")),
                                                                          cookieActorBody,
                                                                          referer,
                                                                          tokens.lsd.toUtf8(),
                                                                          &contentType,
                                                                          &error);
                    if (!cookieActorBytes.isEmpty()) {
                        postResult = parseInstagramProfileGraphql(path, cookieActorBytes, basePost);
                        if (postResult.error.isEmpty()) {
                            traceInstagram(QStringLiteral("GraphQL POST cookie actor succeeded after=%1")
                                               .arg(after.isEmpty() ? QStringLiteral("no") : QStringLiteral("yes")));
                            return postResult;
                        }
                    }
                }
                post = postResult;
            } else if (!error.isEmpty()) {
                post.error = error;
            }
        }
    }
    if (!post.error.isEmpty()) {
        post.error = QStringLiteral("%1 Set FM_INSTAGRAM_COOKIE or FM_INSTAGRAM_COOKIE_FILE with a valid browser session cookie if this profile requires it.")
                         .arg(post.error);
    }
    return post;
}

InstagramPost parseInstagramProfile(const ParsedPath &path, const QByteArray &json)
{
    InstagramPost post;
    post.rootPath = path.rootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = sourceUrlFor(path);
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    const QJsonDocument document = QJsonDocument::fromJson(json);
    const QJsonObject user = document.object()
                                 .value(QStringLiteral("data"))
                                 .toObject()
                                 .value(QStringLiteral("user"))
                                 .toObject();
    if (user.isEmpty()) {
        post.error = QStringLiteral("Instagram profile data is unavailable. It may require login or the response format changed.");
        return post;
    }

    post.title = user.value(QStringLiteral("full_name")).toString();
    if (post.title.isEmpty()) {
        post.title = path.shortcode;
    }

    QSet<QString> seenUrls;
    qsizetype index = 1;
    appendJsonTimeline(post, user, QStringLiteral("edge_owner_to_timeline_media"), seenUrls, index);
    appendJsonTimeline(post, user, QStringLiteral("edge_felix_video_timeline"), seenUrls, index);

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram did not expose public media for this profile. It may require login or the profile is private.");
    }
    return post;
}

InstagramPost parseInstagramMediaInfo(const ParsedPath &path, const QByteArray &json)
{
    InstagramPost post;
    post.rootPath = path.rootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = sourceUrlFor(path);
    post.title = QStringLiteral("Instagram %1 %2").arg(path.kind, path.shortcode);
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        post.error = QStringLiteral("Instagram media info JSON parse error: %1").arg(parseError.errorString());
        return post;
    }

    const QJsonObject root = document.object();
    const QJsonArray items = root.value(QStringLiteral("items")).toArray();
    QSet<QString> seenUrls;
    qsizetype index = 1;
    for (const QJsonValue &value : items) {
        const QJsonObject item = value.toObject();
        if (!jsonNodeMatchesShortcode(item, path.shortcode)) {
            continue;
        }
        appendJsonMediaNode(post, item, seenUrls, index);
    }

    if (post.items.isEmpty()) {
        post.error = QStringLiteral("Instagram media info did not expose media for this link.");
    } else {
        post.error.clear();
    }
    return post;
}

InstagramPost fetchMediaInfoPost(const ParsedPath &path)
{
    InstagramPost post;
    post.rootPath = path.rootPath;
    post.kind = path.kind;
    post.shortcode = path.shortcode;
    post.sourceUrl = sourceUrlFor(path);
    post.fetchedAt = QDateTime::currentDateTimeUtc();

    QString contentType;
    QString error;
    const QUrl url(QStringLiteral("https://www.instagram.com/api/v1/media/shortcode/%1/info/")
                       .arg(path.shortcode));
    QByteArray bytes = httpGetBytes(url, &contentType, &error);
    if (bytes.isEmpty()) {
        const QString mediaId = mediaIdFromShortcode(path.shortcode);
        if (!mediaId.isEmpty()) {
            traceInstagram(QStringLiteral("Media shortcode info unavailable, retry media_id fallback"));
            const QUrl mediaIdUrl(QStringLiteral("https://www.instagram.com/api/v1/media/%1/info/")
                                      .arg(mediaId));
            bytes = httpGetBytes(mediaIdUrl,
                                 {},
                                 &contentType,
                                 &error,
                                 true,
                                 QByteArray(InstagramMobileUserAgent));
        }
    }
    if (bytes.isEmpty()) {
        post.error = error.isEmpty() ? QStringLiteral("Instagram media info returned an empty response") : error;
        return post;
    }
    return parseInstagramMediaInfo(path, bytes);
}


} // namespace InstagramProviderInternal
