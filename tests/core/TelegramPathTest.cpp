#include "TelegramPath.h"

#include <QCoreApplication>
#include <QTextStream>

using namespace TelegramProviderInternal;

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << "FAILED: " << message << '\n';
    return 1;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (!isTelegramSchemePath(QStringLiteral("telegram:///"))) {
        return fail(QStringLiteral("telegram:/// should be identified as Telegram path"));
    }
    if (!isTelegramSchemePath(QStringLiteral("TELEGRAM://saved"))) {
        return fail(QStringLiteral("Telegram scheme should be case-insensitive"));
    }
    if (isTelegramSchemePath(QStringLiteral("gdrive://my-drive"))) {
        return fail(QStringLiteral("gdrive:// should not be identified as Telegram path"));
    }

    ParsedTelegramPath parsed = parseTelegramPath(QStringLiteral("telegram:///"));
    if (!parsed.valid || parsed.kind != TelegramPathKind::Root || parsed.normalized != QStringLiteral("telegram:///")) {
        return fail(QStringLiteral("Root path should parse and normalize"));
    }

    parsed = parseTelegramPath(QStringLiteral("telegram://\\saved\\"));
    if (!parsed.valid || parsed.kind != TelegramPathKind::Saved || parsed.normalized != QStringLiteral("telegram://saved")) {
        return fail(QStringLiteral("Saved Messages path should normalize slashes"));
    }

    parsed = parseTelegramPath(QStringLiteral("telegram://saved/__load_more__"));
    if (!parsed.valid || parsed.kind != TelegramPathKind::Saved || !parsed.loadMore) {
        return fail(QStringLiteral("Saved Messages load-more path should parse"));
    }

    parsed = parseTelegramPath(QStringLiteral("telegram://chats"));
    if (!parsed.valid || parsed.kind != TelegramPathKind::Chats) {
        return fail(QStringLiteral("Chats root should parse"));
    }

    parsed = parseTelegramPath(QStringLiteral("telegram://downloads"));
    if (!parsed.valid || parsed.kind != TelegramPathKind::Downloads) {
        return fail(QStringLiteral("Downloads root should parse"));
    }

    parsed = parseTelegramPath(QStringLiteral("telegram://chat/-1001234567890"));
    if (!parsed.valid || parsed.kind != TelegramPathKind::Chat || parsed.id != QStringLiteral("-1001234567890")) {
        return fail(QStringLiteral("Chat id path should parse"));
    }

    parsed = parseTelegramPath(QStringLiteral("telegram://chat/-1001234567890/__load_more__"));
    if (!parsed.valid || parsed.kind != TelegramPathKind::Chat || !parsed.loadMore) {
        return fail(QStringLiteral("Chat load-more path should parse"));
    }

    parsed = parseTelegramPath(QStringLiteral("telegram://channel/news_channel/file.txt"));
    if (!parsed.valid
        || parsed.kind != TelegramPathKind::Channel
        || parsed.id != QStringLiteral("news_channel")
        || parsed.itemName != QStringLiteral("file.txt")) {
        return fail(QStringLiteral("Channel child path should parse"));
    }

    parsed = parseTelegramPath(QStringLiteral("telegram://status/telegram-provider-status.txt"));
    if (!parsed.valid
        || parsed.kind != TelegramPathKind::Status
        || parsed.itemName != QStringLiteral("telegram-provider-status.txt")) {
        return fail(QStringLiteral("Status child path should parse"));
    }

    if (!parseTelegramPath(QStringLiteral("telegram://chat")).normalized.isEmpty()) {
        return fail(QStringLiteral("Chat path without id should be invalid"));
    }
    if (parseTelegramPath(QStringLiteral("telegram://chat/bad id")).valid) {
        return fail(QStringLiteral("Chat id with spaces should be invalid"));
    }
    if (parseTelegramPath(QStringLiteral("telegram://unknown")).valid) {
        return fail(QStringLiteral("Unknown Telegram root should be invalid"));
    }
    if (parseTelegramPath(QStringLiteral("telegram://downloads/../file.txt")).valid) {
        return fail(QStringLiteral("Downloads path should reject parent traversal"));
    }

    if (parentTelegramPath(QStringLiteral("telegram://saved")) != QStringLiteral("telegram:///")) {
        return fail(QStringLiteral("Saved parent should be Telegram root"));
    }
    if (parentTelegramPath(QStringLiteral("telegram://saved/__load_more__")) != QStringLiteral("telegram://saved")) {
        return fail(QStringLiteral("Saved load-more parent should be Saved Messages"));
    }
    if (parentTelegramPath(QStringLiteral("telegram://chat/-1001234567890")) != QStringLiteral("telegram://chats")) {
        return fail(QStringLiteral("Chat root parent should be Chats and Channels"));
    }
    if (parentTelegramPath(QStringLiteral("telegram://chat/-1001234567890/__load_more__")) != QStringLiteral("telegram://chat/-1001234567890")) {
        return fail(QStringLiteral("Chat load-more parent should be chat root"));
    }
    if (parentTelegramPath(QStringLiteral("telegram://channel/news_channel")) != QStringLiteral("telegram://chats")) {
        return fail(QStringLiteral("Channel root parent should be Chats and Channels"));
    }
    if (parentTelegramPath(QStringLiteral("telegram://channel/news_channel/file.txt")) != QStringLiteral("telegram://channel/news_channel")) {
        return fail(QStringLiteral("Channel child parent should preserve channel path"));
    }
    if (!parentTelegramPath(QStringLiteral("telegram:///")).isEmpty()) {
        return fail(QStringLiteral("Root parent should be empty"));
    }

    if (childTelegramPath(QStringLiteral("telegram:///"), QStringLiteral("saved")) != QStringLiteral("telegram://saved")) {
        return fail(QStringLiteral("Root child path should use telegram:// prefix"));
    }
    if (childTelegramPath(QStringLiteral("telegram://channel/news_channel"), QStringLiteral("Folder/File.txt"))
        != QStringLiteral("telegram://channel/news_channel/Folder/File.txt")) {
        return fail(QStringLiteral("Nested child path should append clean name"));
    }

    if (fileNameForTelegramPath(QStringLiteral("telegram:///")) != QStringLiteral("Telegram")) {
        return fail(QStringLiteral("Root filename should be Telegram"));
    }
    if (fileNameForTelegramPath(QStringLiteral("telegram://chat/-1001234567890")) != QStringLiteral("Chat -1001234567890")) {
        return fail(QStringLiteral("Chat fallback filename should be readable"));
    }
    if (fileNameForTelegramPath(QStringLiteral("telegram://channel/news_channel")) != QStringLiteral("Channel news_channel")) {
        return fail(QStringLiteral("Channel fallback filename should be readable"));
    }
    if (fileNameForTelegramPath(QStringLiteral("telegram://channel/news_channel/file.txt")) != QStringLiteral("file.txt")) {
        return fail(QStringLiteral("Child filename should use final path segment"));
    }

    if (telegramPathFromUserInput(QStringLiteral("@news_channel")) != QStringLiteral("telegram://channel/news_channel")) {
        return fail(QStringLiteral("@username should convert to channel path"));
    }
    if (telegramPathFromUserInput(QStringLiteral("https://t.me/news_channel/12")) != QStringLiteral("telegram://channel/news_channel")) {
        return fail(QStringLiteral("t.me public link should convert to channel path"));
    }
    if (telegramPathFromUserInput(QStringLiteral("https://t.me/c/123456789/12")) != QStringLiteral("telegram://chat/-100123456789")) {
        return fail(QStringLiteral("t.me private channel link should convert to internal chat path"));
    }
    if (telegramPathFromUserInput(QStringLiteral("-100123456789")) != QStringLiteral("telegram://chat/-100123456789")) {
        return fail(QStringLiteral("Numeric chat id should convert to chat path"));
    }
    if (!telegramPathFromUserInput(QStringLiteral("https://t.me/+invite")).isEmpty()) {
        return fail(QStringLiteral("Invite links should not convert without a resolvable chat id"));
    }

    QTextStream(stdout) << "All TelegramPath unit tests passed successfully!\n";
    return 0;
}
