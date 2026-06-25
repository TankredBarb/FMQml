#include "MegaCache.h"
#include "MegaClientInterface.h"
#include "MegaFileProviderPlugin.h"
#include "MegaPath.h"
#include "MegaPresentation.h"
#include "MegaDiagnostics.h"
#include "FileProvider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <atomic>
#include <functional>
#include <thread>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << "FAILED: " << message << '\n';
    return 1;
}

FileEntry makeEntry(const QString &path, const QString &name, bool directory, qint64 size = 0)
{
    FileEntry entry;
    entry.path = path;
    entry.name = name;
    entry.isDirectory = directory;
    entry.isReadOnly = true;
    entry.size = size;
    if (!directory) {
        const int suffixIndex = name.lastIndexOf(QLatin1Char('.'));
        entry.suffix = suffixIndex >= 0 ? name.mid(suffixIndex + 1).toLower() : QString{};
    }
    entry.iconName = directory ? QStringLiteral("folder") : QString{};
    MegaPresentation::enrichEntryPresentation(entry);
    return entry;
}

class FakeMegaClient final : public MegaClientInterface
{
    Q_OBJECT

public:
    enum class DownloadMode {
        Success,
        Failure,
        CancelAfterProgress
    };

    using MegaClientInterface::MegaClientInterface;

    int getPublicNode(const QString &linkId) override
    {
        ++getPublicNodeCalls;
        lastRequestedLinkId = linkId;

        if (!publicLoadSucceeds) {
            MegaCache::markLinkLoaded(linkId, false, publicLoadError);
            emit publicLinkLoaded(linkId, false, publicLoadError);
            return 0;
        }

        const QString rootPath = QStringLiteral("mega://link/") + linkId;
        const QString docsPath = rootPath + QStringLiteral("/Docs");
        const QString filePath = docsPath + QStringLiteral("/readme.txt");

        MegaCache::removeSubtree(rootPath);
        MegaCache::cacheEntry(rootPath, makeEntry(rootPath, QStringLiteral("PublicRoot"), true), QStringLiteral("100"));
        MegaCache::cacheEntry(docsPath, makeEntry(docsPath, QStringLiteral("Docs"), true), QStringLiteral("101"));
        MegaCache::cacheEntry(filePath, makeEntry(filePath, QStringLiteral("readme.txt"), false, downloadPayload.size()), QStringLiteral("102"));
        MegaCache::cacheChildren(rootPath, { docsPath });
        MegaCache::cacheChildren(docsPath, { filePath });
        MegaCache::markLinkLoaded(linkId, true);

        emit publicLinkLoaded(linkId, true, {});
        return 0;
    }


    int loginToAccount(const QString &email, const QString &password) override
    {
        accountSignedIn = !email.trimmed().isEmpty() && !password.isEmpty();
        accountEmailValue = email.trimmed();
        accountSessionValue = accountSignedIn ? QStringLiteral("fake-session") : QString{};
        accountNodesFresh = false;
        emit accountAuthorizationChanged(accountSignedIn, accountEmailValue, accountSessionValue);
        return accountSignedIn ? 0 : -1;
    }

    int resumeAccountSession(const QString &session) override
    {
        accountSignedIn = !session.trimmed().isEmpty();
        accountSessionValue = session.trimmed();
        accountNodesFresh = false;
        emit accountAuthorizationChanged(accountSignedIn, accountEmailValue, accountSessionValue);
        return accountSignedIn ? 0 : -1;
    }

    bool logoutAccount(QString *errorString = nullptr) override
    {
        accountSignedIn = false;
        accountEmailValue.clear();
        accountSessionValue.clear();
        accountNodesFresh = false;
        MegaCache::removeSubtree(MegaPath::Root);
        if (errorString) {
            errorString->clear();
        }
        emit accountAuthorizationChanged(false, {}, {});
        return true;
    }

    bool isAccountAuthenticated() const override
    {
        return accountSignedIn;
    }

    QString accountEmail() const override
    {
        return accountEmailValue;
    }

    QString accountSessionToken() const override
    {
        return accountSessionValue;
    }

    bool hasFreshAccountNodes() const override
    {
        return accountSignedIn && accountNodesFresh;
    }

    int loadAccountRoot() override
    {
        ++loadAccountRootCalls;
        if (!accountSignedIn) {
            emit accountNodesLoaded(false, QStringLiteral("MEGA account is not signed in"));
            return -1;
        }

        const QString cloudPath = QStringLiteral("mega:///Cloud Drive");
        const QString docsPath = cloudPath + QLatin1Char('/') + accountFolderName;
        const QString filePath = docsPath + QStringLiteral("/account.txt");
        MegaCache::removeSubtree(MegaPath::Root);
        MegaCache::cacheEntry(MegaPath::Root, makeEntry(MegaPath::Root, QStringLiteral("MEGA"), true), {});
        MegaCache::cacheEntry(cloudPath, makeEntry(cloudPath, QStringLiteral("Cloud Drive"), true), QStringLiteral("200"));
        MegaCache::cacheEntry(docsPath, makeEntry(docsPath, accountFolderName, true), QStringLiteral("201"));
        MegaCache::cacheEntry(filePath, makeEntry(filePath, QStringLiteral("account.txt"), false, downloadPayload.size()), QStringLiteral("202"));
        MegaCache::cacheChildren(MegaPath::Root, { cloudPath });
        MegaCache::cacheChildren(cloudPath, { docsPath });
        MegaCache::cacheChildren(docsPath, { filePath });
        accountNodesFresh = true;
        emit accountNodesLoaded(true, {});
        return 0;
    }

    qint64 startDownload(const QString &path, const QString &localPath) override
    {
        ++downloadCalls;
        const qint64 requestId = ++nextRequestId;
        lastDownloadPath = path;
        lastLocalPath = localPath;

        std::thread([this, requestId, path, localPath]() {
            QThread::msleep(10);
            emit downloadProgress(requestId, path, 1, downloadPayload.size());

            if (downloadMode == DownloadMode::CancelAfterProgress) {
                QThread::msleep(20);
                emit downloadFinished(requestId, path, false,
                                      cancelled.load() ? QStringLiteral("Cancelled") : QStringLiteral("Expected cancellation"));
                return;
            }

            if (downloadMode == DownloadMode::Failure) {
                QFile partial(localPath);
                if (partial.open(QIODevice::WriteOnly)) {
                    partial.write("partial");
                }
                emit downloadFinished(requestId, path, false, QStringLiteral("Bandwidth quota exceeded"));
                return;
            }

            QFile partial(localPath);
            if (!partial.open(QIODevice::WriteOnly)) {
                emit downloadFinished(requestId, path, false, QStringLiteral("Could not write fake partial file"));
                return;
            }
            partial.write(downloadPayload);
            partial.close();
            emit downloadFinished(requestId, path, true, {});
        }).detach();

        return requestId;
    }

    qint64 startUpload(const QString &sourceFilePath, const QString &destinationPath) override
    {
        const qint64 requestId = ++nextRequestId;
        ++uploadCalls;
        lastUploadSource = sourceFilePath;
        lastUploadDestination = destinationPath;
        QFile source(sourceFilePath);
        if (!source.open(QIODevice::ReadOnly)) {
            emit mutationFinished(requestId, QStringLiteral("upload"), destinationPath, false,
                                  QStringLiteral("Could not read fake upload source"), {});
            return requestId;
        }
        const QByteArray payload = source.readAll();
        emit uploadProgress(requestId, destinationPath, payload.size(), payload.size());
        const QString parent = MegaPath::parentPath(destinationPath);
        const QString name = MegaPath::fallbackFileNameForPath(destinationPath);
        MegaCache::cacheEntry(destinationPath, makeEntry(destinationPath, name, false, payload.size()),
                              QString::number(++nextHandle));
        MegaCache::appendChild(parent, destinationPath);
        emit mutationFinished(requestId, QStringLiteral("upload"), destinationPath, true, {}, destinationPath);
        return requestId;
    }

    qint64 startCreateFolder(const QString &parentPath, const QString &name) override
    {
        const qint64 requestId = ++nextRequestId;
        ++createFolderCalls;
        const QString path = MegaPath::childPath(parentPath, name);
        MegaCache::cacheEntry(path, makeEntry(path, name, true), QString::number(++nextHandle));
        MegaCache::cacheChildren(path, {});
        MegaCache::appendChild(parentPath, path);
        emit mutationFinished(requestId, QStringLiteral("createFolder"), path, true, {}, path);
        return requestId;
    }

    qint64 startRename(const QString &path, const QString &newName) override
    {
        const qint64 requestId = ++nextRequestId;
        ++renameCalls;
        const QString newPath = MegaPath::childPath(MegaPath::parentPath(path), newName);
        MegaCache::renameSubtree(path, newPath, newName);
        emit mutationFinished(requestId, QStringLiteral("rename"), path, true, {}, newPath);
        return requestId;
    }

    qint64 startMove(const QString &sourcePath, const QString &destinationPath) override
    {
        const qint64 requestId = ++nextRequestId;
        ++moveCalls;
        MegaCache::renameSubtree(sourcePath, destinationPath, MegaPath::fallbackFileNameForPath(destinationPath));
        emit mutationFinished(requestId, QStringLiteral("move"), sourcePath, true, {}, destinationPath);
        return requestId;
    }

    qint64 startRemove(const QString &path) override
    {
        const qint64 requestId = ++nextRequestId;
        ++removeCalls;
        MegaCache::removeChild(MegaPath::parentPath(path), path);
        MegaCache::removeSubtree(path);
        emit mutationFinished(requestId, QStringLiteral("remove"), path, true, {}, {});
        return requestId;
    }

    qint64 accountStorageUsedBytes() const override
    {
        return downloadPayload.size();
    }

    qint64 accountStorageMaxBytes() const override
    {
        return 50ll * 1024ll * 1024ll * 1024ll;
    }

    void requestAccountDetails() override
    {
    }

    void cancelAll() override
    {
        cancelled.store(true);
        ++cancelAllCalls;
    }

    void simulateRemoteAccountChange(const QString &folderName)
    {
        accountFolderName = folderName;
        accountNodesFresh = false;
        emit accountNodesChanged(QStringLiteral("remoteChange"));
    }

    bool publicLoadSucceeds = true;
    QString publicLoadError = QStringLiteral("Invalid public link");
    QByteArray downloadPayload = QByteArrayLiteral("hello from fake mega");
    DownloadMode downloadMode = DownloadMode::Success;
    int getPublicNodeCalls = 0;
    int loadAccountRootCalls = 0;
    int cancelAllCalls = 0;
    int downloadCalls = 0;
    bool accountSignedIn = false;
    bool accountNodesFresh = false;
    QString accountEmailValue;
    QString accountSessionValue;
    QString accountFolderName = QStringLiteral("AccountDocs");
    QString lastRequestedLinkId;
    QString lastDownloadPath;
    QString lastLocalPath;
    std::atomic_bool cancelled = false;
    qint64 nextRequestId = 0;
    qint64 nextHandle = 300;
    int uploadCalls = 0;
    int createFolderCalls = 0;
    int renameCalls = 0;
    int moveCalls = 0;
    int removeCalls = 0;
    QString lastUploadSource;
    QString lastUploadDestination;
};

MegaClientInterface *g_defaultClient = nullptr;

std::unique_ptr<FileProvider> makeProvider(FakeMegaClient &client)
{
    MegaFileProviderPlugin::setClientForTesting(&client);
    MegaFileProviderPlugin plugin;
    return plugin.createProvider();
}

bool waitForFileProviderFinish(FileProvider &provider,
                               const std::function<void()> &action,
                               bool *successOut,
                               QString *errorOut,
                               QList<FileEntry> *entriesOut = nullptr,
                               QStringList *progressMessagesOut = nullptr)
{
    bool finished = false;
    bool success = false;
    QString error;
    QList<FileEntry> entries;
    QStringList progressMessages;

    const QMetaObject::Connection conn0 = QObject::connect(&provider, &FileProvider::progress, &provider,
                     [&](qint64, qint64, const QString &message, int) { progressMessages.append(message); });
    const QMetaObject::Connection conn1 = QObject::connect(&provider, &FileProvider::batchReady, &provider,
                     [&](const QList<FileEntry> &batch, int) { entries.append(batch); });
    const QMetaObject::Connection conn2 = QObject::connect(&provider, &FileProvider::finished, &provider,
                     [&](const QString &, bool ok, int, const QString &errorString) {
                         finished = true;
                         success = ok;
                         error = errorString;
                     });

    action();

    QObject::disconnect(conn0);
    QObject::disconnect(conn1);
    QObject::disconnect(conn2);

    if (successOut) {
        *successOut = success;
    }
    if (errorOut) {
        *errorOut = error;
    }
    if (entriesOut) {
        *entriesOut = entries;
    }
    if (progressMessagesOut) {
        *progressMessagesOut = progressMessages;
    }
    return finished;
}

} // namespace

MegaClientInterface &defaultMegaClient()
{
    Q_ASSERT(g_defaultClient);
    return *g_defaultClient;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    const QString redactedDiagnostics = MegaDiagnostics::redactSensitiveText(
        QStringLiteral("https://mega.nz/file/publicId#secretKey session=abc123 /tmp/FMQml/mega-sdk/account/state"));
    if (redactedDiagnostics.contains(QStringLiteral("secretKey"))
        || redactedDiagnostics.contains(QStringLiteral("abc123"))
        || redactedDiagnostics.contains(QStringLiteral("account/state"))
        || !redactedDiagnostics.contains(QStringLiteral("publicId#<redacted>"))) {
        return fail(QStringLiteral("MEGA diagnostics redaction should remove link keys, session tokens, and SDK state details"));
    }

    {
        MegaCache::clear();
        FakeMegaClient client;
        g_defaultClient = &client;
        auto provider = makeProvider(client);

        bool success = false;
        QString error;
        QList<FileEntry> entries;
        bool finished = waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega://link/public123")); },
            &success,
            &error,
            &entries);

        if (!finished || !success || !error.isEmpty()) {
            return fail(QStringLiteral("public folder scan should finish successfully"));
        }
        if (client.getPublicNodeCalls != 1 || client.lastRequestedLinkId != QStringLiteral("public123")) {
            return fail(QStringLiteral("public folder scan should request exactly one SDK load"));
        }
        if (entries.size() != 1 || entries.first().path != QStringLiteral("mega://link/public123/Docs")) {
            return fail(QStringLiteral("public folder scan should emit cached root children"));
        }
        if (entries.first().iconName != QStringLiteral("folder")) {
            return fail(QStringLiteral("public folder entries should keep shared folder icon metadata"));
        }

        entries.clear();
        finished = waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega://link/public123/Docs")); },
            &success,
            &error,
            &entries);
        if (!finished || !success || entries.size() != 1
            || entries.first().mimeType != QStringLiteral("text/plain")
            || entries.first().iconName != QStringLiteral("text")) {
            return fail(QStringLiteral("public file entries should expose enriched MIME and icon metadata"));
        }

        entries.clear();
        finished = waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega://link/public123")); },
            &success,
            &error,
            &entries);
        if (!finished || !success || client.getPublicNodeCalls != 1) {
            return fail(QStringLiteral("second scan should be served from cache"));
        }
    }

    {
        MegaCache::clear();
        FakeMegaClient client;
        client.publicLoadSucceeds = false;
        g_defaultClient = &client;
        auto provider = makeProvider(client);

        bool success = true;
        QString error;
        const bool finished = waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega://link/badlink")); },
            &success,
            &error);
        if (!finished || success || error != client.publicLoadError) {
            return fail(QStringLiteral("failed public link load should surface fake client error"));
        }
    }

    {
        MegaCache::clear();
        FakeMegaClient client;
        g_defaultClient = &client;
        auto provider = makeProvider(client);
        bool success = false;
        QString error;
        waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega://link/download123")); },
            &success,
            &error);

        QTemporaryDir tempDir;
        if (!tempDir.isValid()) {
            return fail(QStringLiteral("could not create temporary directory"));
        }
        const QString destination = QDir(tempDir.path()).filePath(QStringLiteral("readme.txt"));
        if (!provider->copyToLocalFile(QStringLiteral("mega://link/download123/Docs/readme.txt"), destination, nullptr, &error)) {
            return fail(QStringLiteral("successful fake download failed: %1").arg(error));
        }
        QFile output(destination);
        if (!output.open(QIODevice::ReadOnly) || output.readAll() != client.downloadPayload) {
            return fail(QStringLiteral("successful fake download should atomically create destination payload"));
        }
        if (QFile::exists(destination + QStringLiteral(".part"))) {
            return fail(QStringLiteral("successful fake download should remove .part file"));
        }
    }


    {
        MegaCache::clear();
        FakeMegaClient client;
        g_defaultClient = &client;
        auto provider = makeProvider(client);

        const QString largePath = QStringLiteral("mega://link/public123/Docs/huge-video.mp4");
        MegaCache::cacheEntry(QStringLiteral("mega://link/public123"), makeEntry(QStringLiteral("mega://link/public123"), QStringLiteral("PublicRoot"), true), QStringLiteral("100"));
        MegaCache::cacheEntry(QStringLiteral("mega://link/public123/Docs"), makeEntry(QStringLiteral("mega://link/public123/Docs"), QStringLiteral("Docs"), true), QStringLiteral("101"));
        MegaCache::cacheEntry(largePath, makeEntry(largePath, QStringLiteral("huge-video.mp4"), false, 2ll * 1024ll * 1024ll * 1024ll), QStringLiteral("999"));
        MegaCache::cacheChildren(QStringLiteral("mega://link/public123"), { QStringLiteral("mega://link/public123/Docs") });
        MegaCache::cacheChildren(QStringLiteral("mega://link/public123/Docs"), { largePath });

        const std::unique_ptr<QIODevice> device = provider->openRead(largePath, QDir::tempPath());
        if (device || client.downloadCalls != 0) {
            return fail(QStringLiteral("openRead should refuse over-limit MEGA fallback materialization before starting a download"));
        }
    }

    {
        MegaCache::clear();
        FakeMegaClient client;
        client.downloadMode = FakeMegaClient::DownloadMode::Failure;
        g_defaultClient = &client;
        auto provider = makeProvider(client);
        bool success = false;
        QString error;
        waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega://link/faildownload")); },
            &success,
            &error);

        QTemporaryDir tempDir;
        const QString destination = QDir(tempDir.path()).filePath(QStringLiteral("readme.txt"));
        if (provider->copyToLocalFile(QStringLiteral("mega://link/faildownload/Docs/readme.txt"), destination, nullptr, &error)) {
            return fail(QStringLiteral("failed fake download should return false"));
        }
        if (error != QStringLiteral("Bandwidth quota exceeded")) {
            return fail(QStringLiteral("failed fake download should surface transfer error"));
        }
        if (QFile::exists(destination) || QFile::exists(destination + QStringLiteral(".part"))) {
            return fail(QStringLiteral("failed fake download should remove destination and .part artifacts"));
        }
    }

    {
        MegaCache::clear();
        FakeMegaClient client;
        client.downloadMode = FakeMegaClient::DownloadMode::CancelAfterProgress;
        g_defaultClient = &client;
        auto provider = makeProvider(client);
        bool success = false;
        QString error;
        waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega://link/cancelme")); },
            &success,
            &error);

        QTemporaryDir tempDir;
        const QString destination = QDir(tempDir.path()).filePath(QStringLiteral("readme.txt"));
        const bool copied = provider->copyToLocalFile(QStringLiteral("mega://link/cancelme/Docs/readme.txt"), destination,
            [](qint64, qint64) { return false; },
            &error);
        if (copied || client.cancelAllCalls == 0 || !client.cancelled.load()) {
            return fail(QStringLiteral("cancelled fake download should call cancelAll and return false"));
        }
        if (QFile::exists(destination) || QFile::exists(destination + QStringLiteral(".part"))) {
            return fail(QStringLiteral("cancelled fake download should not leave payload artifacts"));
        }
    }


    {
        MegaCache::clear();
        FakeMegaClient client;
        g_defaultClient = &client;
        auto provider = makeProvider(client);

        bool success = true;
        QString error;
        bool finished = waitForFileProviderFinish(*provider,
            [&]() { provider->scan(MegaPath::Root); },
            &success,
            &error);
        if (!finished || success || error != QStringLiteral("MEGA account is not signed in")) {
            return fail(QStringLiteral("signed-out account scan should fail clearly"));
        }

        if (client.loginToAccount(QStringLiteral("tester@example.com"), QStringLiteral("password")) != 0) {
            return fail(QStringLiteral("fake account login should succeed"));
        }

        QList<FileEntry> entries;
        finished = waitForFileProviderFinish(*provider,
            [&]() { provider->scan(MegaPath::Root); },
            &success,
            &error,
            &entries);
        if (!finished || !success || entries.size() != 1 || entries.first().path != QStringLiteral("mega:///Cloud Drive")) {
            return fail(QStringLiteral("signed-in account root should expose Cloud Drive"));
        }
        if (client.loadAccountRootCalls != 1) {
            return fail(QStringLiteral("account root scan should load account nodes once"));
        }

        entries.clear();
        finished = waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega:///Cloud Drive")); },
            &success,
            &error,
            &entries);
        if (!finished || !success || entries.size() != 1 || entries.first().path != QStringLiteral("mega:///Cloud Drive/AccountDocs")) {
            return fail(QStringLiteral("account Cloud Drive scan should expose cached children"));
        }

        const QVariantMap storageInfo = provider->storageInfo(MegaPath::Root);
        if (storageInfo.value(QStringLiteral("used")).toLongLong() != client.downloadPayload.size()
            || storageInfo.value(QStringLiteral("fs")).toString() != QStringLiteral("MEGA")) {
            return fail(QStringLiteral("account storageInfo should expose cached MEGA usage"));
        }

        QStringList remoteStatusMessages;
        const int loadCallsBeforeRemoteChange = client.loadAccountRootCalls;
        const QMetaObject::Connection remoteStatusConn = QObject::connect(provider.get(), &FileProvider::statusMessage, provider.get(),
            [&](const QString &message) { remoteStatusMessages.append(message); });
        client.simulateRemoteAccountChange(QStringLiteral("RemoteDocs"));
        QObject::disconnect(remoteStatusConn);
        if (!remoteStatusMessages.contains(QStringLiteral("MEGA changed remotely; refresh to update."))) {
            return fail(QStringLiteral("remote MEGA account changes should emit a user-visible refresh status message"));
        }
        if (client.loadAccountRootCalls != loadCallsBeforeRemoteChange) {
            return fail(QStringLiteral("remote MEGA account changes should not auto-refresh the account tree"));
        }

        entries.clear();
        finished = waitForFileProviderFinish(*provider,
            [&]() { provider->scan(QStringLiteral("mega:///Cloud Drive")); },
            &success,
            &error,
            &entries);
        if (!finished || !success || client.loadAccountRootCalls != loadCallsBeforeRemoteChange
            || entries.size() != 1
            || entries.first().path != QStringLiteral("mega:///Cloud Drive/AccountDocs")) {
            return fail(QStringLiteral("normal navigation after remote change should keep using cached MEGA children"));
        }

        entries.clear();
        finished = waitForFileProviderFinish(*provider,
            [&]() { provider->refresh(QStringLiteral("mega:///Cloud Drive")); },
            &success,
            &error,
            &entries);
        if (!finished || !success || client.loadAccountRootCalls != loadCallsBeforeRemoteChange + 1
            || entries.size() != 1
            || entries.first().path != QStringLiteral("mega:///Cloud Drive/RemoteDocs")) {
            return fail(QStringLiteral("explicit account refresh after remote change should reload stale MEGA children"));
        }

        QTemporaryDir uploadDir;
        const QString uploadSource = QDir(uploadDir.path()).filePath(QStringLiteral("local.txt"));
        QFile uploadFile(uploadSource);
        if (!uploadFile.open(QIODevice::WriteOnly)) {
            return fail(QStringLiteral("could not create local upload source"));
        }
        uploadFile.write("uploaded payload");
        uploadFile.close();
        QString uploadError;
        const QString uploadedPath = QStringLiteral("mega:///Cloud Drive/RemoteDocs/local.txt");
        if (!provider->copyFromLocalFile(uploadSource, uploadedPath, nullptr, &uploadError)
            || !provider->pathExists(uploadedPath)
            || client.uploadCalls != 1) {
            return fail(QStringLiteral("copyFromLocalFile should upload local files into the account: %1").arg(uploadError));
        }
        const auto uploadedEntry = provider->entryInfo(uploadedPath);
        if (!uploadedEntry || uploadedEntry->mimeType != QStringLiteral("text/plain")
            || uploadedEntry->iconName != QStringLiteral("text")
            || uploadedEntry->hasThumbnail) {
            return fail(QStringLiteral("uploaded account files should expose enriched text metadata without thumbnail eligibility"));
        }

        if (!provider->removePath(uploadedPath)
            || provider->pathExists(uploadedPath)
            || client.removeCalls != 1) {
            return fail(QStringLiteral("removePath should remove cached MEGA account nodes"));
        }

        QString publicUploadError;
        if (provider->copyFromLocalFile(uploadSource,
                                        QStringLiteral("mega://link/public123/Docs/local.txt"),
                                        nullptr,
                                        &publicUploadError)
            || provider->removePath(QStringLiteral("mega://link/public123/Docs/readme.txt"))) {
            return fail(QStringLiteral("public links should remain read-only for account storage mutations"));
        }

        MegaFileProviderPlugin plugin;
        const QVariantMap status = plugin.triggerAction(QStringLiteral("authStatus"), {});
        if (!status.value(QStringLiteral("signedIn")).toBool()
            || status.value(QStringLiteral("accountEmail")).toString() != QStringLiteral("tester@example.com")) {
            return fail(QStringLiteral("authStatus should report signed-in fake account"));
        }
        const QVariantMap signOut = plugin.triggerAction(QStringLiteral("signOut"), {});
        if (!signOut.value(QStringLiteral("ok")).toBool() || client.isAccountAuthenticated()) {
            return fail(QStringLiteral("signOut action should clear fake account auth"));
        }

        FileActionContext signInContext;
        signInContext.targetPath = MegaPath::Root;
        signInContext.parameters.insert(QStringLiteral("email"), QStringLiteral("tester@example.com"));
        signInContext.parameters.insert(QStringLiteral("password"), QStringLiteral("password"));
        const QVariantMap signIn = plugin.triggerAction(QStringLiteral("signIn"), signInContext);
        if (!signIn.value(QStringLiteral("ok")).toBool() || !client.isAccountAuthenticated()) {
            return fail(QStringLiteral("signIn action should start fake account login"));
        }

        const QList<FileActionDescriptor> signedInActions = plugin.actionsForContext(signInContext);
        bool hasStatus = false;
        bool hasSignOut = false;
        bool hasSignIn = false;
        for (const FileActionDescriptor &action : signedInActions) {
            hasStatus = hasStatus || action.id == QStringLiteral("authStatus");
            hasSignOut = hasSignOut || action.id == QStringLiteral("signOut");
            hasSignIn = hasSignIn || action.id == QStringLiteral("signIn");
        }
        if (!hasStatus || !hasSignOut || hasSignIn) {
            return fail(QStringLiteral("signed-in MEGA action list should expose status and sign out only"));
        }

        client.logoutAccount();
        const QList<FileActionDescriptor> signedOutActions = plugin.actionsForContext(signInContext);
        hasStatus = false;
        hasSignOut = false;
        hasSignIn = false;
        for (const FileActionDescriptor &action : signedOutActions) {
            hasStatus = hasStatus || action.id == QStringLiteral("authStatus");
            hasSignOut = hasSignOut || action.id == QStringLiteral("signOut");
            hasSignIn = hasSignIn || action.id == QStringLiteral("signIn");
        }
        if (!hasStatus || hasSignOut || !hasSignIn) {
            return fail(QStringLiteral("signed-out MEGA action list should expose status and sign in only"));
        }
    }

    MegaFileProviderPlugin::setClientForTesting(nullptr);
    QTextStream(stdout) << "All MEGA provider public-link integration tests passed successfully!\n";
    return 0;
}

#include "MegaProviderPublicLinkTest.moc"
