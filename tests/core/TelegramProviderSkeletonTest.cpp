#include "TelegramFileProvider.h"

#include "FileProvider.h"

#include <QCoreApplication>
#include <QIODevice>
#include <QList>
#include <QTextStream>

#include <memory>

using namespace TelegramProviderInternal;

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << "FAILED: " << message << '\n';
    return 1;
}

struct ScanResult {
    bool finished = false;
    bool success = false;
    int generation = 0;
    QString path;
    QString error;
    QList<FileEntry> entries;
};

ScanResult scan(FileProvider &provider, const QString &path)
{
    ScanResult result;
    QObject::connect(&provider, &FileProvider::batchReady, [&result](const QList<FileEntry> &entries, int generation) {
        result.generation = generation;
        result.entries.append(entries);
    });
    QObject::connect(&provider,
                     &FileProvider::finished,
                     [&result](const QString &finishedPath, bool success, int generation, const QString &error) {
                         result.finished = true;
                         result.success = success;
                         result.generation = generation;
                         result.path = finishedPath;
                         result.error = error;
                     });
    provider.scan(path);
    return result;
}

bool hasEntry(const QList<FileEntry> &entries, const QString &path)
{
    for (const FileEntry &entry : entries) {
        if (entry.path == path) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const std::unique_ptr<FileProvider> provider = createTelegramFileProvider();
    if (!provider) {
        return fail(QStringLiteral("Provider factory returned null"));
    }
    if (provider->scheme() != QStringLiteral("telegram")) {
        return fail(QStringLiteral("Provider scheme should be telegram"));
    }
    if (!provider->canHandle(QStringLiteral("telegram:///")) || provider->canHandle(QStringLiteral("mega:///"))) {
        return fail(QStringLiteral("Provider canHandle result is incorrect"));
    }
    if (provider->capabilities()
            != (FileProvider::Browse | FileProvider::ReadMetadata | FileProvider::Create | FileProvider::Transfer)) {
        return fail(QStringLiteral("Skeleton provider should expose browse/read metadata/create/transfer"));
    }

    ScanResult root = scan(*provider, QStringLiteral("telegram:///"));
    if (!root.finished || !root.success || root.path != QStringLiteral("telegram:///")) {
        return fail(QStringLiteral("Root scan should finish successfully"));
    }
    if (!hasEntry(root.entries, QStringLiteral("telegram://saved"))
        || !hasEntry(root.entries, QStringLiteral("telegram://chats"))
        || !hasEntry(root.entries, QStringLiteral("telegram://downloads"))) {
        return fail(QStringLiteral("Root scan should expose expected virtual folders"));
    }
    if (hasEntry(root.entries, QStringLiteral("telegram://status"))) {
        return fail(QStringLiteral("Status diagnostics should not be shown as a root folder"));
    }

    const std::optional<FileEntry> savedLoadMore = provider->entryInfo(QStringLiteral("telegram://saved/__load_more__"));
    if (!savedLoadMore || !savedLoadMore->isDirectory || savedLoadMore->path != QStringLiteral("telegram://saved/__load_more__")) {
        return fail(QStringLiteral("Saved Messages load-more entry should be a virtual folder"));
    }
    if (savedLoadMore->specialAction != FileEntrySpecialAction::LoadMore
        || savedLoadMore->overlayIconName != QStringLiteral("telegram-badge-load-more")
        || savedLoadMore->iconRecolorAllowed) {
        return fail(QStringLiteral("Telegram load-more semantic presentation metadata is incomplete"));
    }

    const std::optional<FileEntry> chatLoadMore = provider->entryInfo(QStringLiteral("telegram://chat/-1001234567890/__load_more__"));
    if (!chatLoadMore || !chatLoadMore->isDirectory || chatLoadMore->path != QStringLiteral("telegram://chat/-1001234567890/__load_more__")) {
        return fail(QStringLiteral("Chat load-more entry should be a virtual folder"));
    }

    ScanResult status = scan(*provider, QStringLiteral("telegram://status"));
    if (!status.finished || !status.success || !hasEntry(status.entries, QStringLiteral("telegram://status/telegram-provider-status.txt"))) {
        return fail(QStringLiteral("Status scan should expose diagnostic file"));
    }

    const std::unique_ptr<QIODevice> statusDevice = provider->openRead(QStringLiteral("telegram://status/telegram-provider-status.txt"));
    if (!statusDevice || !statusDevice->isOpen() || statusDevice->readAll().isEmpty()) {
        return fail(QStringLiteral("Status diagnostic file should be readable"));
    }

    ScanResult saved = scan(*provider, QStringLiteral("telegram://saved"));
    if (!saved.finished) {
        return fail(QStringLiteral("Saved Messages scan should finish"));
    }
    if (!saved.success && saved.error.isEmpty()) {
        return fail(QStringLiteral("Saved Messages scan failure should include a sanitized error"));
    }

    ScanResult downloads = scan(*provider, QStringLiteral("telegram://downloads"));
    if (!downloads.finished || !downloads.success) {
        return fail(QStringLiteral("Downloads scan should finish successfully"));
    }

    if (provider->normalizedPath(QStringLiteral("@news_channel")) != QStringLiteral("telegram://channel/news_channel")) {
        return fail(QStringLiteral("Provider should normalize Telegram public source shortcuts"));
    }

    if (provider->removePath(QStringLiteral("telegram://saved")) || provider->lastErrorString().isEmpty()) {
        return fail(QStringLiteral("Remove should fail with read-only error"));
    }

    QTextStream(stdout) << "Telegram provider skeleton tests passed successfully!\n";
    return 0;
}
