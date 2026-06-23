#include "MegaCache.h"
#include "MegaClientInterface.h"
#include "MegaFileProviderPlugin.h"
#include "MegaPath.h"
#include "FileProvider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
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
        emit accountAuthorizationChanged(accountSignedIn, accountEmailValue, accountSessionValue);
        return accountSignedIn ? 0 : -1;
    }

    int resumeAccountSession(const QString &session) override
    {
        accountSignedIn = !session.trimmed().isEmpty();
        accountSessionValue = session.trimmed();
        emit accountAuthorizationChanged(accountSignedIn, accountEmailValue, accountSessionValue);
        return accountSignedIn ? 0 : -1;
    }

    bool logoutAccount(QString *errorString = nullptr) override
    {
        accountSignedIn = false;
        accountEmailValue.clear();
        accountSessionValue.clear();
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

    int loadAccountRoot() override
    {
        ++loadAccountRootCalls;
        if (!accountSignedIn) {
            emit accountNodesLoaded(false, QStringLiteral("MEGA account is not signed in"));
            return -1;
        }

        const QString cloudPath = QStringLiteral("mega:///Cloud Drive");
        const QString docsPath = cloudPath + QStringLiteral("/AccountDocs");
        const QString filePath = docsPath + QStringLiteral("/account.txt");
        MegaCache::removeSubtree(MegaPath::Root);
        MegaCache::cacheEntry(MegaPath::Root, makeEntry(MegaPath::Root, QStringLiteral("MEGA"), true), {});
        MegaCache::cacheEntry(cloudPath, makeEntry(cloudPath, QStringLiteral("Cloud Drive"), true), QStringLiteral("200"));
        MegaCache::cacheEntry(docsPath, makeEntry(docsPath, QStringLiteral("AccountDocs"), true), QStringLiteral("201"));
        MegaCache::cacheEntry(filePath, makeEntry(filePath, QStringLiteral("account.txt"), false, downloadPayload.size()), QStringLiteral("202"));
        MegaCache::cacheChildren(MegaPath::Root, { cloudPath });
        MegaCache::cacheChildren(cloudPath, { docsPath });
        MegaCache::cacheChildren(docsPath, { filePath });
        emit accountNodesLoaded(true, {});
        return 0;
    }

    qint64 startDownload(const QString &path, const QString &localPath) override
    {
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

    void cancelAll() override
    {
        cancelled.store(true);
        ++cancelAllCalls;
    }

    bool publicLoadSucceeds = true;
    QString publicLoadError = QStringLiteral("Invalid public link");
    QByteArray downloadPayload = QByteArrayLiteral("hello from fake mega");
    DownloadMode downloadMode = DownloadMode::Success;
    int getPublicNodeCalls = 0;
    int loadAccountRootCalls = 0;
    int cancelAllCalls = 0;
    bool accountSignedIn = false;
    QString accountEmailValue;
    QString accountSessionValue;
    QString lastRequestedLinkId;
    QString lastDownloadPath;
    QString lastLocalPath;
    std::atomic_bool cancelled = false;
    qint64 nextRequestId = 0;
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
                               QList<FileEntry> *entriesOut = nullptr)
{
    bool finished = false;
    bool success = false;
    QString error;
    QList<FileEntry> entries;

    QObject::connect(&provider, &FileProvider::batchReady, &provider,
                     [&](const QList<FileEntry> &batch, int) { entries.append(batch); });
    QObject::connect(&provider, &FileProvider::finished, &provider,
                     [&](const QString &, bool ok, int, const QString &errorString) {
                         finished = true;
                         success = ok;
                         error = errorString;
                     });

    action();

    if (successOut) {
        *successOut = success;
    }
    if (errorOut) {
        *errorOut = error;
    }
    if (entriesOut) {
        *entriesOut = entries;
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
    }

    MegaFileProviderPlugin::setClientForTesting(nullptr);
    QTextStream(stdout) << "All MEGA provider public-link integration tests passed successfully!\n";
    return 0;
}

#include "MegaProviderPublicLinkTest.moc"
