#include "SftpClient.h"
#include "SftpFileProvider.h"
#include "SftpConnectionController.h"
#include "SftpFileProviderPlugin.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QCryptographicHash>
#include <QDir>
#include <QScopeGuard>
#include <QSettings>
#include <QtTest>

#include "../support/SftpTestServer.h"
#include "OperationQueue.h"

// The standalone client test has no queue; queue integration has its own probe.
std::function<bool()> OperationQueue::currentThreadAbortChecker() { return {}; }

class SftpClientTest : public QObject {
    Q_OBJECT
private:
    QTemporaryDir settingsDirectory;
private slots:
    void initTestCase() {
        QCOMPARE(ssh_init(), SSH_OK);
        QVERIFY(settingsDirectory.isValid());
        QCoreApplication::setOrganizationName(QStringLiteral("FM-SFTP-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("profiles"));
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory.path());
    }
    void paths()
    {
        const QString root = QStringLiteral("sftp://tester@host/");
        QCOMPARE(SftpPath::parse(QStringLiteral("SFTP://tester@HOST:22/a/../b/")).encoded(), QStringLiteral("sftp://tester@host/b"));
        QCOMPARE(SftpPath::parent(root), root);
        const QString name = QStringLiteral(" données #?%@\\.txt ");
        const QString child = SftpPath::child(root, name);
        QVERIFY(!child.isEmpty());
        QCOMPARE(SftpPath::fileName(child), name);
        QCOMPARE(SftpPath::parent(child), root);
        for (const QString &bad : {QStringLiteral("sftp://user:secret@host/"), QStringLiteral("sftp://user:@host/"),
                                  QStringLiteral("sftp://user@host/file#fragment"), QStringLiteral("sftp://user@host/file?query"),
                                  QStringLiteral("sftp://user@host:0/"), QStringLiteral("sftp://user@host/%00")})
            QVERIFY2(!SftpPath::parse(bad).valid(), qPrintable(bad));
        QVERIFY(!SftpPath::fromFields(QStringLiteral("host"), -1, QStringLiteral("user"), {}).valid());
        QCOMPARE(SftpPath::parse(QStringLiteral("sftp://user@[::1]:2222/path")).url.port(), 2222);
        QVERIFY(SftpPath::child(root, QStringLiteral("../other")).isEmpty());
        QVERIFY(SftpPath::child(root, QStringLiteral("..")).isEmpty());
    }
    void pluginContract()
    {
        SftpFileProviderPlugin plugin;
        QCOMPARE(plugin.apiVersion(), FM_FILE_PROVIDER_PLUGIN_API_VERSION);
        QVERIFY(plugin.canHandle(QStringLiteral("SFTP://tester@host/")));
        QVERIFY(!plugin.canHandle(QStringLiteral("ftp://host/")));
        QVERIFY(plugin.preprocessPath(QStringLiteral("sftp://tester:secret@host/")).isEmpty());
        QCOMPARE(plugin.preprocessPath(QStringLiteral("sftp://tester@host:22/a")), QStringLiteral("sftp://tester@host/a"));
        auto provider = plugin.createProvider();
        QCOMPARE(provider->scheme(), QStringLiteral("sftp"));
        QVERIFY(qobject_cast<PluginSettingsUi *>(&plugin));
        QVERIFY(qobject_cast<PlacesProviderPlugin *>(&plugin));
        QVERIFY(plugin.places().isEmpty());
    }
    void trustAndPassword()
    {
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        TestServer server;
        QVERIFY(server.port() > 0);
        const auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("ssh/known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(fingerprint.startsWith(QLatin1String("SHA256:")));
        QVERIFY(!QFile::exists(knownHosts));
        {
            SftpClient client({}, knownHosts);
            QVERIFY2(client.connect(credentials, fingerprint), qPrintable(client.error()));
            QVERIFY(QFile::exists(knownHosts));
            const auto entry = client.stat(credentials.address);
            QVERIFY(entry && entry->isDirectory);
            QList<FileEntry> entries;
            QVERIFY(client.list(credentials.address, false, [&](const auto &batch) { entries.append(batch); }));
            QCOMPARE(entries.size(), 3);
            QVERIFY(entries.at(0).isDirectory);
            QCOMPARE(entries.at(0).iconName, QStringLiteral("folder"));
            QCOMPARE(entries.at(1).name, QStringLiteral("données #?.txt"));
            QVERIFY(entries.at(1).iconName.isEmpty()); // File type resolution owns the bundled fallback.
            QCOMPARE(entries.at(1).size, 123);
            QVERIFY(entries.at(2).isSymLink);
            entries.clear();
            QVERIFY(client.list(credentials.address, true, [&](const auto &batch) { entries.append(batch); }));
            QCOMPARE(entries.size(), 4);
        }
        {
            SftpClient client({}, knownHosts);
            QVERIFY2(client.connect(credentials), qPrintable(client.error()));
            auto missing = credentials.address;
            missing.url.setPath(QStringLiteral("/missing"));
            QVERIFY(!client.stat(missing));
            QVERIFY(client.error().contains(QLatin1String("does not exist")));
        }
        auto wrong = credentials;
        wrong.password = QStringLiteral("wrong-password");
        {
            SftpClient client({}, knownHosts);
            QVERIFY(!client.connect(wrong));
            QVERIFY(client.error().contains(QLatin1String("login failed")));
            QVERIFY(!client.error().contains(wrong.password));
        }
        const int attempts = server.passwordRequests.load();
        {
            SftpClient client({}, knownHosts);
            QVERIFY(!client.connect(credentials, QStringLiteral("SHA256:different")));
            QVERIFY(client.error().contains(QLatin1String("changed")));
        }
        QCOMPARE(server.passwordRequests.load(), attempts);
        QFile file(knownHosts);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        ssh_key other = nullptr;
        QCOMPARE(generateKey(&other), SSH_OK);
        char *publicKey = nullptr;
        QCOMPARE(ssh_pki_export_pubkey_base64(other, &publicKey), SSH_OK);
        file.write(QStringLiteral("[127.0.0.1]:%1 ssh-ed25519 %2\n").arg(server.port()).arg(QString::fromLatin1(publicKey)).toUtf8());
        file.close();
        ssh_string_free_char(publicKey);
        ssh_key_free(other);
        SftpClient changed({}, knownHosts);
        QVERIFY(!changed.connect(credentials));
        QVERIFY(changed.error().contains(QLatin1String("changed")));
        QCOMPARE(server.passwordRequests.load(), attempts);
    }
    void noSubsystem()
    {
        QTemporaryDir temporary;
        TestServer server(false);
        QVERIFY(server.port() > 0);
        const auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        SftpClient client({}, knownHosts);
        QVERIFY(!client.connect(credentials, fingerprint));
        QVERIFY(client.error().contains(QLatin1String("subsystem")));
    }
    void handshakeCancellation()
    {
        TestServer server;
        QVERIFY(server.port() > 0);
        server.stallHandshake.store(true);
        std::atomic_bool cancel{false};
        SftpClient client([&] { return cancel.load(); });
        std::jthread canceller([&](std::stop_token stop) {
            while (!server.handshakeStarted.load() && !stop.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cancel.store(true);
        });
        QElapsedTimer timer;
        timer.start();
        QVERIFY(!client.connect(credentialsFor(server)));
        QVERIFY2(timer.elapsed() < 1000, "Cancelling the SSH handshake blocked until the connection timeout");
        QVERIFY(client.error().contains(QLatin1String("cancelled")));
    }
    void cancellation()
    {
        QTemporaryDir temporary;
        TestServer server(true, true);
        QVERIFY(server.port() > 0);
        const auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        std::atomic_bool cancel{false};
        SftpClient client([&]() { return cancel.load(); }, knownHosts);
        QVERIFY(client.connect(credentials, fingerprint));
        std::jthread canceller([&]() {
            while (!server.listingStarted.load()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            cancel.store(true);
        });
        QElapsedTimer timer;
        timer.start();
        QVERIFY(!client.list(credentials.address, false, [](const auto &) {}));
        QVERIFY(timer.elapsed() < 2000);
        QVERIFY(client.error().contains(QLatin1String("cancelled")));
    }
    void closeProviderDuringHandshake()
    {
        TestServer server;
        QVERIFY(server.port() > 0);
        server.stallHandshake.store(true);
        const auto credentials = credentialsFor(server);
        SftpSessions::remember(credentials);
        const auto forget = qScopeGuard([&] { SftpSessions::forget(credentials.address.identity()); });
        auto provider = std::make_unique<SftpFileProvider>();
        provider->scan(credentials.address.encoded());
        QTRY_VERIFY(server.handshakeStarted.load());
        QElapsedTimer timer;
        timer.start();
        provider.reset();
        QVERIFY2(timer.elapsed() < 1000, "Closing a loading SFTP provider blocked on the SSH handshake");
    }
    void credentialsAreSessionScoped()
    {
        const auto address = SftpPath::parse(QStringLiteral("sftp://first@host:2222/one"));
        SftpSessions::remember({address, QStringLiteral("first-password"), {}});
        const auto cached = SftpSessions::credentials(address);
        QVERIFY(cached);
        QVERIFY(!SftpSessions::credentials(SftpPath::parse(QStringLiteral("sftp://second@host:2222/one"))));
        QVERIFY(!SftpSessions::credentials(SftpPath::parse(QStringLiteral("sftp://first@host/one"))));
        QVERIFY(SftpSessions::credentials(SftpPath::parse(QStringLiteral("sftp://first@host:2222/two"))));
        SftpSessions::forget(address.identity());
        QVERIFY(cached->revoked->load());
        QVERIFY(!SftpSessions::credentials(address));
        SftpFileProvider provider;
        QVERIFY(provider.capabilities() & FileProvider::Transfer);
        QVERIFY(!provider.openWrite(address.encoded()));
        QSignalSpy finished(&provider, &FileProvider::finished);
        provider.scan(address.encoded());
        QVERIFY(finished.wait(2000));
        QVERIFY(!finished.at(0).at(1).toBool());
        QVERIFY(finished.at(0).at(3).toString().contains(QLatin1String("Sign in")));
    }
    void connectionController()
    {
        QTemporaryDir temporary;
        TestServer server;
        QVERIFY(server.port() > 0);
        const auto credentials = credentialsFor(server);
        SftpConnectionController controller(nullptr, temporary.filePath(QStringLiteral("known_hosts")));
        QSignalSpy connected(&controller, &SftpConnectionController::connected);
        controller.signIn(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("tester"),
                          QStringLiteral("test-password"), QStringLiteral("/"));
        QVERIFY(controller.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QVERIFY(!controller.fingerprint().isEmpty());
        QCOMPARE(server.passwordRequests.load(), 0);
        controller.cancel();
        controller.confirmHost();
        QVERIFY(!controller.busy());
        QVERIFY(controller.fingerprint().isEmpty());
        QVERIFY(!SftpSessions::credentials(credentials.address));
        controller.signIn(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("tester"),
                          QStringLiteral("test-password"), QStringLiteral("/"));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QVERIFY(!controller.fingerprint().isEmpty());
        controller.confirmHost();
        QVERIFY(connected.wait(2000));
        QCOMPARE(connected.size(), 1);
        QCOMPARE(controller.openPath(), credentials.address.encoded());
        QVERIFY(SftpSessions::credentials(credentials.address));
        SftpSessions::forget(credentials.address.identity());
        controller.signIn(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("tester"),
                          QStringLiteral("wrong-password"), QStringLiteral("/"));
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QVERIFY(controller.hasError());
        QVERIFY(controller.status().contains(QLatin1String("login failed")));
        QCOMPARE(connected.size(), 1);
        QVERIFY(!SftpSessions::credentials(credentials.address));
        controller.signIn(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("tester"),
                          QStringLiteral("test-password"), QStringLiteral("/"));
        QVERIFY(!controller.hasError());
        controller.cancel();
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QCOMPARE(connected.size(), 1);
        QVERIFY(!SftpSessions::credentials(credentials.address));
    }
    void savedConnections()
    {
        SftpFileProviderPlugin plugin;
        QTemporaryDir temporary;
        TestServer server;
        QVERIFY(server.port() > 0);
        const auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        SftpConnectionController controller(nullptr, knownHosts);
        QSignalSpy connected(&controller, &SftpConnectionController::connected);
        controller.signIn(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("tester"),
                          QStringLiteral("test-password"), QStringLiteral("/"), QStringLiteral("Vedro"), true, true);
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 2000);
        QVERIFY(!controller.fingerprint().isEmpty());
        controller.confirmHost();
        QVERIFY(connected.wait(2000));
        auto profileFor = [&]() {
            for (const auto &item : SftpProfiles::connections()) {
                const auto profile = item.toMap();
                if (profile.value("url").toString() == credentials.address.encoded()) return profile;
            }
            return QVariantMap{};
        };
        QCOMPARE(profileFor().value("name").toString(), QStringLiteral("Vedro"));
        QVERIFY(profileFor().value("saved").toBool());
        QVERIFY(profileFor().value("rememberPassword").toBool());
        QVERIFY(profileFor().value("connected").toBool());
        QCOMPARE(plugin.places().size(), 2);
        QCOMPARE(plugin.places().first().path, QStringLiteral("sftp://"));
        QCOMPARE(plugin.places().first().icon, QStringLiteral("sftp"));
        QCOMPARE(plugin.places().last().path, credentials.address.encoded());
        QFile settings(QSettings().fileName());
        QVERIFY(settings.open(QIODevice::ReadOnly));
        QVERIFY(!settings.readAll().contains("test-password"));
        controller.disconnectFrom(credentials.address.encoded());
        QVERIFY(!SftpSessions::credentials(credentials.address));
        QVERIFY(!profileFor().value("connected").toBool());
        QVERIFY(profileFor().value("saved").toBool());
        QVERIFY(profileFor().value("rememberPassword").toBool());
        QVERIFY(plugin.places().isEmpty());

        SftpConnectionController restored(nullptr, knownHosts);
        QSignalSpy reconnected(&restored, &SftpConnectionController::connected);
        restored.connectSaved(credentials.address.encoded());
        QVERIFY(reconnected.wait(2000));
        QVERIFY(SftpSessions::credentials(credentials.address));
        QCOMPARE(SftpSessions::credentials(credentials.address)->password, QStringLiteral("test-password"));
        QCOMPARE(plugin.places().size(), 2);
        QString error;
        QVERIFY(!SftpProfiles::password(SftpPath::parse(QStringLiteral("sftp://another@host/")), &error));
        QVERIFY(!SftpProfiles::save(credentials.address, QStringLiteral("Cancelled"), QStringLiteral("replacement"), true,
                                    &error, [] { return true; }));
        QCOMPARE(*SftpProfiles::password(credentials.address, &error), QStringLiteral("test-password"));
        QCOMPARE(profileFor().value("name").toString(), QStringLiteral("Vedro"));

        QVERIFY(SftpProfiles::save(credentials.address, QStringLiteral("Updated"), QStringLiteral("test-password"), false, &error));
        QVERIFY(!profileFor().value("rememberPassword").toBool());
        QVERIFY(!SftpProfiles::password(credentials.address, &error));
        restored.forget(credentials.address.encoded());
        QTRY_VERIFY_WITH_TIMEOUT(!restored.busy(), 2000);
        QVERIFY(profileFor().isEmpty());
        QVERIFY(!SftpSessions::credentials(credentials.address));
        QVERIFY(plugin.places().isEmpty());
    }

    void providerScan()
    {
        QTemporaryDir temporary;
        TestServer server;
        QVERIFY(server.port() > 0);
        const auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        {
            SftpClient client({}, knownHosts);
            QVERIFY(client.connect(credentials, fingerprint));
        }
        SftpSessions::remember(credentials);
        SftpFileProvider provider(knownHosts);
        QSignalSpy batches(&provider, &FileProvider::batchReady);
        QSignalSpy finished(&provider, &FileProvider::finished);
        provider.scan(credentials.address.encoded());
        QVERIFY(finished.wait(2000));
        QVERIFY(finished.at(0).at(1).toBool());
        QCOMPARE(batches.size(), 1);
        const QString path = SftpPath::child(credentials.address.encoded(), QStringLiteral("données #?.txt"));
        const int attempts = server.passwordRequests.load();
        const auto cached = provider.entryInfo(path);
        QVERIFY(cached && !cached->isDirectory);
        QCOMPARE(server.passwordRequests.load(), attempts);
        SftpSessions::remember(credentials);
        const auto fresh = provider.entryInfo(path);
        QVERIFY(fresh && fresh->isDirectory); // fixture stat differs from list
        QVERIFY(server.passwordRequests.load() > attempts);
        SftpSessions::forget(credentials.address.identity());
        QVERIFY(!provider.entryInfo(path));
    }
    void replacedScanDoesNotPublish()
    {
        QTemporaryDir temporary;
        TestServer server(true, true);
        QVERIFY(server.port() > 0);
        const auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        {
            SftpClient client({}, knownHosts);
            QVERIFY(client.connect(credentials, fingerprint));
        }
        SftpSessions::remember(credentials);
        SftpFileProvider provider(knownHosts);
        QSignalSpy batches(&provider, &FileProvider::batchReady);
        QSignalSpy finished(&provider, &FileProvider::finished);
        provider.scan(credentials.address.encoded());
        QTRY_VERIFY_WITH_TIMEOUT(server.listingStarted.load(), 2000);
        provider.scan(QStringLiteral("sftp://other@host/"));
        QVERIFY(finished.wait(2000));
        QTRY_VERIFY_WITH_TIMEOUT(!provider.isRunning(), 2000);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(finished.at(0).at(2).toInt(), provider.currentGeneration());
        QCOMPARE(batches.size(), 0);
        SftpSessions::forget(credentials.address.identity());
    }
    void fileTransfers()
    {
        QTemporaryDir temporary;
        TestServer server(true, false, true);
        QVERIFY(server.port() > 0);
        auto credentials = credentialsFor(server);
        const QString remoteDirectory = temporary.filePath(QStringLiteral("remote"));
        QVERIFY(QDir().mkpath(remoteDirectory));
        credentials.address.url.setPath(remoteDirectory);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        { SftpClient client({}, knownHosts); QVERIFY2(client.connect(credentials, fingerprint), qPrintable(client.error())); }
        SftpSessions::remember(credentials);
        const auto forget = qScopeGuard([&]() { SftpSessions::forget(credentials.address.identity()); });
        SftpFileProvider provider(knownHosts);
        const QByteArray data = QByteArray::fromHex("00010203ff807f20").repeated(128 * 1024);
        QFile source(temporary.filePath(QStringLiteral("source.bin")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(data), data.size());
        source.close();
        QVERIFY(source.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner));
        const QString name = QStringLiteral(" données #?%.bin ");
        const QString remote = SftpPath::child(credentials.address.encoded(), name);
        // FilePanelController asks this from the GUI thread using a fresh
        // provider instance. Capability checks must not open SSH sessions.
        const int beforeCapabilityCheck = server.passwordRequests.load();
        QVERIFY(provider.canCopyPath(remote));
        QCOMPARE(server.passwordRequests.load(), beforeCapabilityCheck);
        QString error;
        qint64 last = -1;
        bool monotonic = true;
        const auto progress = [&](qint64 processed, qint64 total) {
            monotonic &= processed >= last && total == data.size();
            last = processed;
            return true;
        };
        QVERIFY2(provider.copyFromLocalFile(source.fileName(), remote, progress, &error), qPrintable(error));
        QVERIFY(monotonic);
        QCOMPARE(last, data.size());
        QVERIFY(!(QFileInfo(QDir(remoteDirectory).filePath(name)).permissions() & QFileDevice::ReadOther));
        const QString downloaded = temporary.filePath(QStringLiteral("downloaded.bin"));
        last = -1;
        QVERIFY2(provider.copyToLocalFile(remote, downloaded, progress, &error), qPrintable(error));
        QVERIFY(monotonic);
        QCOMPARE(last, data.size());
        QVERIFY(!(QFileInfo(downloaded).permissions() & QFileDevice::ReadOther));
        QFile output(downloaded);
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(QCryptographicHash::hash(output.readAll(), QCryptographicHash::Sha256), QCryptographicHash::hash(data, QCryptographicHash::Sha256));
        output.close();
        // Replace an existing file only after upload completes.
        QVERIFY(source.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(source.write("replacement"), 11);
        source.close();
        QVERIFY2(provider.copyFromLocalFile(source.fileName(), remote, {}, &error), qPrintable(error));
        QFile remoteFile(QDir(remoteDirectory).filePath(name));
        QVERIFY(remoteFile.open(QIODevice::ReadOnly));
        QCOMPARE(remoteFile.readAll(), QByteArray("replacement"));
        remoteFile.close();
        QVERIFY(provider.copyToLocalFile(remote, downloaded, {}, &error));
        QVERIFY(output.open(QIODevice::ReadOnly));
        QCOMPARE(output.readAll(), QByteArray("replacement"));
        output.close();
        QVERIFY(provider.createFile(credentials.address.encoded(), QStringLiteral("empty")));
        QVERIFY(provider.createFile(credentials.address.encoded(), QStringLiteral("empty")));
        QVERIFY(QFileInfo::exists(QDir(remoteDirectory).filePath(QStringLiteral("empty (1)"))));
        QVERIFY(provider.copyToLocalFile(SftpPath::child(credentials.address.encoded(), QStringLiteral("empty")), downloaded, {}, &error));
        QCOMPARE(QFileInfo(downloaded).size(), 0);
        QVERIFY(provider.makePath(SftpPath::child(credentials.address.encoded(), QStringLiteral("nested"))));
        QVERIFY(provider.renamePath(remote, QStringLiteral("renamed")));
        QVERIFY(provider.removePath(SftpPath::child(credentials.address.encoded(), QStringLiteral("renamed"))));
        auto device = credentials.address;
        device.url.setPath(QStringLiteral("/dev/zero"));
        QVERIFY(!provider.copyToLocalFile(device.encoded(), downloaded, {}, &error));
        QVERIFY(error.contains(QLatin1String("regular file")));
        QCOMPARE(QFileInfo(downloaded).size(), 0);
    }
    void manualCreation()
    {
        QTemporaryDir temporary;
        TestServer server(true, false, true);
        QVERIFY(server.port() > 0);
        auto credentials = credentialsFor(server);
        credentials.address.url.setPath(temporary.path());
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        { SftpClient client({}, knownHosts); QVERIFY(client.connect(credentials, fingerprint)); }
        SftpFileProvider provider(knownHosts);
        const QString parent = credentials.address.encoded();
        QVERIFY(!provider.canCreateChildren(parent));
        SftpSessions::remember(credentials);
        const auto cleanup = qScopeGuard([&] { SftpSessions::forget(credentials.address.identity()); });
        const int requests = server.passwordRequests.load();
        QVERIFY(provider.canCreateChildren(parent));
        QCOMPARE(server.passwordRequests.load(), requests);
        QString created;
        QVERIFY2(provider.createFolder(parent, QStringLiteral(" New Folder "), &created), qPrintable(provider.lastErrorString()));
        QCOMPARE(created, SftpPath::child(parent, QStringLiteral("New Folder")));
        QVERIFY(QFileInfo(temporary.filePath(QStringLiteral("New Folder"))).isDir());
        const int beforeDefaultConfirmation = server.passwordRequests.load();
        QVERIFY(provider.renamePath(created, QStringLiteral("New Folder")));
        QVERIFY(provider.lastErrorString().isEmpty());
        QCOMPARE(server.passwordRequests.load(), beforeDefaultConfirmation);
        QVERIFY(provider.createFolder(parent, QStringLiteral("New Folder"), &created));
        QCOMPARE(created, SftpPath::child(parent, QStringLiteral("New Folder (1)")));
        QVERIFY(QFileInfo(temporary.filePath(QStringLiteral("New Folder (1)"))).isDir());
        QFile existing(temporary.filePath(QStringLiteral("données.txt")));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("preserve"), 8);
        existing.close();
        QVERIFY(provider.createFile(parent, QStringLiteral("données.txt"), &created));
        QCOMPARE(created, SftpPath::child(parent, QStringLiteral("données (1).txt")));
        QCOMPARE(QFileInfo(temporary.filePath(QStringLiteral("données (1).txt"))).size(), 0);
        QVERIFY(provider.renamePath(created, QStringLiteral("données (1).txt")));
        QVERIFY(provider.lastErrorString().isEmpty());
        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), QByteArray("preserve"));
        for (const auto &name : {QStringLiteral(""), QStringLiteral(" "), QStringLiteral(".."), QStringLiteral("../escape")}) {
            QVERIFY(!provider.createFile(parent, name, &created));
            QVERIFY(created.isEmpty());
            QVERIFY(!provider.createFolder(parent, name, &created));
            QVERIFY(created.isEmpty());
        }
        QVERIFY(!provider.lastErrorString().isEmpty());
        QVERIFY(provider.renamePath(SftpPath::child(parent, QStringLiteral("New Folder")), QStringLiteral("New Folder")));
        QVERIFY(provider.lastErrorString().isEmpty());
        SftpSessions::forget(credentials.address.identity());
        QVERIFY(!provider.canCreateChildren(parent));
    }
    void pipelinedDownload_data()
    {
        QTest::addColumn<bool>("shortRead");
        QTest::newRow("normal") << false;
        QTest::newRow("short-first-block") << true;
    }
    void pipelinedDownload()
    {
        QFETCH(bool, shortRead);
        QTemporaryDir temporary;
        TestServer server(true, false, true, 0, false, false, 0, 50, 0, 0, shortRead);
        auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        SftpClient client({}, knownHosts);
        QVERIFY2(client.connect(credentials, fingerprint), qPrintable(client.error()));
        QByteArray data(40 * 32768 + 71, Qt::Uninitialized);
        for (int i = 0; i < data.size(); ++i) data[i] = char((i * 31 + i / 32768) % 251);
        QFile source(temporary.filePath(QStringLiteral("source.bin")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(data), data.size());
        source.close();
        credentials.address.url.setPath(source.fileName());
        const QString destination = temporary.filePath(QStringLiteral("download.bin"));
        qint64 acknowledged = 0;
        bool validProgress = true;
        QVERIFY2(client.downloadToFile(credentials.address, destination, [&](qint64 processed, qint64 total) {
            validProgress &= processed >= acknowledged && processed <= total && total == data.size();
            acknowledged = processed;
            return true;
        }), qPrintable(client.error()));
        qInfo() << "Reads before first reply:" << server.readsBeforeFirstReply.load();
        QVERIFY(server.readsBeforeFirstReply.load() > 1);
        QVERIFY(validProgress);
        QCOMPARE(acknowledged, data.size());
        QFile downloaded(destination);
        QVERIFY(downloaded.open(QIODevice::ReadOnly));
        QCOMPARE(QCryptographicHash::hash(downloaded.readAll(), QCryptographicHash::Sha256),
                 QCryptographicHash::hash(data, QCryptographicHash::Sha256));
    }
    void failedDownloadPreservesDestination_data()
    {
        QTest::addColumn<QString>("failure");
        for (const QString &failure : {QStringLiteral("cancel"), QStringLiteral("drop"),
                                      QStringLiteral("shrink"), QStringLiteral("grow")})
            QTest::newRow(qPrintable(failure)) << failure;
    }
    void failedDownloadPreservesDestination()
    {
        QFETCH(QString, failure);
        QTemporaryDir temporary;
        TestServer server(true, false, true, 0, false, false, 0, 0, failure == QLatin1String("drop") ? 2 : 0);
        auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        SftpClient client({}, knownHosts);
        QVERIFY(client.connect(credentials, fingerprint));
        QFile source(temporary.filePath(QStringLiteral("source.bin")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(QByteArray(2 * 1024 * 1024, 'x')), 2 * 1024 * 1024);
        source.close();
        credentials.address.url.setPath(source.fileName());
        QFile destination(temporary.filePath(QStringLiteral("destination.bin")));
        QVERIFY(destination.open(QIODevice::WriteOnly));
        QCOMPARE(destination.write("preserve"), 8);
        destination.close();
        bool changed = false;
        QVERIFY(!client.downloadToFile(credentials.address, destination.fileName(), [&](qint64 processed, qint64) {
            if (processed > 0 && !changed) {
                changed = true;
                if (failure == QLatin1String("cancel")) return false;
                if (failure == QLatin1String("shrink") || failure == QLatin1String("grow")) {
                    QFile modified(source.fileName());
                    if (!modified.open(QIODevice::ReadWrite)
                        || !modified.resize(failure == QLatin1String("shrink") ? 32768 : 3 * 1024 * 1024))
                        return false;
                }
            }
            return true;
        }));
        QVERIFY(!client.error().isEmpty());
        QVERIFY(destination.open(QIODevice::ReadOnly));
        QCOMPARE(destination.readAll(), QByteArray("preserve"));
        QCOMPARE(QDir(temporary.path()).entryList(QDir::Files | QDir::Hidden).size(), 3);
    }
    void directoryLinks()
    {
        QTemporaryDir temporary;
        TestServer server(true, false, true);
        auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        SftpClient client({}, knownHosts);
        QVERIFY(client.connect(credentials, fingerprint));
        const QString target = temporary.filePath(QStringLiteral("target"));
        const QString folder = temporary.filePath(QStringLiteral("folder"));
        QVERIFY(QDir().mkpath(target));
        QVERIFY(QDir().mkpath(folder));
        QFile preserved(QDir(target).filePath(QStringLiteral("preserve.txt")));
        QVERIFY(preserved.open(QIODevice::WriteOnly));
        QCOMPARE(preserved.write("preserve"), 8);
        preserved.close();
        QVERIFY(QFile::link(target, QDir(folder).filePath(QStringLiteral("link"))));
        QVERIFY(QFile::link(temporary.filePath(QStringLiteral("missing")), QDir(folder).filePath(QStringLiteral("broken"))));
        QVERIFY(QFile::link(QDir(folder).filePath(QStringLiteral("loop")), QDir(folder).filePath(QStringLiteral("loop"))));
        credentials.address.url.setPath(folder);
        QList<FileEntry> entries;
        QVERIFY(client.list(credentials.address, true, [&](const auto &batch) { entries.append(batch); }));
        QCOMPARE(entries.size(), 3);
        const auto link = SftpPath::parse(SftpPath::child(credentials.address.encoded(), QStringLiteral("link")));
        const auto info = client.stat(link);
        QVERIFY(info && info->isDirectory && info->isSymLink);
        entries.clear();
        QVERIFY(client.list(link, true, [&](const auto &batch) { entries.append(batch); }));
        QCOMPARE(entries.size(), 1);
        QVERIFY(client.makePath(link));
        QVERIFY(client.remove(credentials.address));
        QVERIFY(!QFileInfo::exists(folder));
        QVERIFY(preserved.open(QIODevice::ReadOnly));
        QCOMPARE(preserved.readAll(), QByteArray("preserve"));
    }
    void pipelinedUpload()
    {
        QTemporaryDir temporary;
        // Withhold the first write acknowledgement: the client must send more
        // data while it waits, rather than serializing one block per round trip.
        TestServer server(true, false, true, 0, false, false, 50);
        QVERIFY(server.port() > 0);
        auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        SftpClient client({}, knownHosts);
        QVERIFY2(client.connect(credentials, fingerprint), qPrintable(client.error()));
        QByteArray data(40 * 32768 + 71, Qt::Uninitialized);
        for (int i = 0; i < data.size(); ++i) data[i] = char((i * 31 + i / 32768) % 251);
        QFile source(temporary.filePath(QStringLiteral("source.bin")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(data), data.size());
        source.close();
        const QString target = temporary.filePath(QStringLiteral("uploaded.bin"));
        credentials.address.url.setPath(target);
        qint64 acknowledged = 0;
        bool validProgress = true;
        QElapsedTimer timer;
        timer.start();
        QVERIFY2(client.uploadFromFile(source.fileName(), credentials.address, [&](qint64 processed, qint64 total) {
            validProgress &= processed >= acknowledged && processed <= total && total == data.size();
            acknowledged = processed;
            return true;
        }), qPrintable(client.error()));
        qInfo() << "Upload elapsed ms:" << timer.elapsed()
                << "writes before first acknowledgement:" << server.writesBeforeFirstReply.load();
        QVERIFY(server.writesBeforeFirstReply.load() > 1);
        QVERIFY(validProgress);
        QCOMPARE(acknowledged, data.size());
        QFile uploaded(target);
        QVERIFY(uploaded.open(QIODevice::ReadOnly));
        QCOMPARE(QCryptographicHash::hash(uploaded.readAll(), QCryptographicHash::Sha256),
                 QCryptographicHash::hash(data, QCryptographicHash::Sha256));
    }
    void failedTransfersPreserveDestination_data()
    {
        QTest::addColumn<bool>("dropConnection");
        QTest::addColumn<bool>("hideRename");
        QTest::newRow("cancel") << false << false;
        QTest::newRow("connection-lost") << true << false;
        QTest::newRow("atomic-rename-unavailable") << false << true;
    }
    void failedTransfersPreserveDestination()
    {
        QFETCH(bool, dropConnection);
        QFETCH(bool, hideRename);
        QTemporaryDir temporary;
        TestServer server(true, false, true, dropConnection ? 2 : 0, hideRename);
        QVERIFY(server.port() > 0);
        auto credentials = credentialsFor(server);
        const QString remoteDirectory = temporary.filePath(QStringLiteral("remote"));
        QVERIFY(QDir().mkpath(remoteDirectory));
        credentials.address.url.setPath(remoteDirectory);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        QVERIFY(!fingerprint.isEmpty());
        { SftpClient client({}, knownHosts); QVERIFY(client.connect(credentials, fingerprint)); }
        SftpSessions::remember(credentials);
        const auto forget = qScopeGuard([&]() { SftpSessions::forget(credentials.address.identity()); });
        SftpFileProvider provider(knownHosts);
        QFile source(temporary.filePath(QStringLiteral("source.bin")));
        QVERIFY(source.open(QIODevice::WriteOnly));
        QCOMPARE(source.write(QByteArray(256 * 1024, 'x')), 256 * 1024);
        source.close();
        QFile existing(QDir(remoteDirectory).filePath(QStringLiteral("existing.bin")));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("preserve"), 8);
        existing.close();
        QString error;
        const auto progress = [&](qint64 processed, qint64) { return dropConnection || hideRename || processed < 32768; };
        QVERIFY(!provider.copyFromLocalFile(source.fileName(), SftpPath::child(credentials.address.encoded(), QStringLiteral("existing.bin")), progress, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), QByteArray("preserve"));
        existing.close();
        const QStringList partials = QDir(remoteDirectory).entryList({QStringLiteral(".fm-upload-*.part")}, QDir::Files | QDir::Hidden);
        if (dropConnection) { QCOMPARE(partials.size(), 1); QVERIFY(error.contains(QLatin1String("may remain"))); }
        else QVERIFY(partials.isEmpty());
        if (!dropConnection && !hideRename) {
            // A cancelled download preserves both an old target and its siblings.
            const QString remoteSource = SftpPath::child(credentials.address.encoded(), QStringLiteral("large.bin"));
            QFile remoteFile(QDir(remoteDirectory).filePath(QStringLiteral("large.bin")));
            QVERIFY(remoteFile.open(QIODevice::WriteOnly));
            remoteFile.write(QByteArray(256 * 1024, 'x'));
            remoteFile.close();
            QVERIFY(!provider.copyToLocalFile(remoteSource, existing.fileName(), progress, &error));
            QVERIFY(existing.open(QIODevice::ReadOnly));
            QCOMPARE(existing.readAll(), QByteArray("preserve"));
        }
    }
};

QTEST_GUILESS_MAIN(SftpClientTest)
#include "SftpClientTest.moc"
