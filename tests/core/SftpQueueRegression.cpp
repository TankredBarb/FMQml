#include "../support/SftpTestServer.h"
#include "SftpFileProviderPlugin.h"
#include "OperationQueue.h"
#include "controllers/FilePanelController.h"
#include "controllers/QuickLookController.h"
#include "controllers/WorkspaceController.h"
#include "core/FileEntryPresentationResolver.h"
#include "preview/PreviewData.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDesktopServices>
#include <QImage>
#include <QFile>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

namespace {
bool writeFile(const QString &path, const QByteArray &data)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray{};
}
QVariantMap completed(OperationQueue &queue, const std::function<void()> &start)
{
    QSignalSpy completion(&queue, &OperationQueue::operationCompleted);
    start();
    if (completion.isEmpty() && !completion.wait(30000)) return {};
    return completion.isEmpty() ? QVariantMap{} : completion.at(0).at(0).toMap();
}
void resolve(OperationQueue &queue, OperationQueue::ConflictResolution resolution)
{
    QObject::connect(&queue, &OperationQueue::conflictDetected, &queue, [&queue, resolution]() {
        queue.resolveConflict(resolution, false);
    });
}

// Like Google Drive, this destination accepts uploads only after the existing
// item has been removed. Keep the transfer on the real queue's staged path.
class NewItemUploadProvider final : public FileProvider {
public:
    mutable bool exists = true;
    mutable int removals = 0;
    mutable QByteArray bytes = "old cloud data";
    QString scheme() const override { return QStringLiteral("test-cloud"); }
    bool canHandle(const QString &) const override { return true; }
    Capabilities capabilities() const override { return Browse | ReadMetadata | Create | Remove | Transfer; }
    void scan(const QString &) override {}
    void cancel() override {}
    void setShowHidden(bool) override {}
    bool isRunning() const override { return false; }
    QString currentPath() const override { return {}; }
    int currentGeneration() const override { return 0; }
    bool pathExists(const QString &path) const override { return isDirectory(path) || exists; }
    bool isDirectory(const QString &path) const override { return path == QStringLiteral("test-cloud://root"); }
    bool isSymLink(const QString &) const override { return false; }
    QString normalizedPath(const QString &path) const override { return path; }
    QString absolutePath(const QString &path) const override { return path; }
    QString fileName(const QString &path) const override { return path.section(QLatin1Char('/'), -1); }
    QString parentPath(const QString &) const override { return QStringLiteral("test-cloud://root"); }
    QString childPath(const QString &parent, const QString &name) const override { return parent + QLatin1Char('/') + name; }
    std::optional<FileEntry> entryInfo(const QString &path) const override {
        if (!pathExists(path)) return std::nullopt;
        FileEntry entry; entry.path = path; entry.name = fileName(path);
        entry.isDirectory = isDirectory(path); entry.size = bytes.size();
        return entry;
    }
    bool ensureParentDirectory(const QString &) const override { return true; }
    bool makePath(const QString &) const override { return true; }
    bool removePath(const QString &) const override {
        exists = false; ++removals; return true;
    }
    QStringList childPaths(const QString &, bool) const override { return {}; }
    bool movePath(const QString &, const QString &) const override { return false; }
    std::unique_ptr<QIODevice> openRead(const QString &) const override { return {}; }
    std::unique_ptr<QIODevice> openWrite(const QString &, bool) const override { return {}; }
    bool renamePath(const QString &, const QString &) override { return false; }
    bool createFolder(const QString &, const QString &, QString *) override { return false; }
    bool createFile(const QString &, const QString &, QString *) override { return false; }
    bool copyFromLocalFile(const QString &source, const QString &,
                           const std::function<bool(qint64, qint64)> &, QString *error) const override {
        if (exists) { *error = QStringLiteral("Upload requires a new item path"); return false; }
        QFile file(source);
        if (!file.open(QIODevice::ReadOnly)) { *error = file.errorString(); return false; }
        bytes = file.readAll(); exists = true;
        return true;
    }
};
class Fixture {
public:
    QTemporaryDir directory;
    TestServer server;
    SftpCredentials credentials;
    QString remoteDirectory;
    QString localDirectory;
    bool ready = false;

    explicit Fixture(int dropAfterWrites = 0, bool hideRename = false, bool readOnly = false, int mutationDelayMs = 0)
        : server(true, false, true, dropAfterWrites, hideRename, readOnly, 0, 0, 0, mutationDelayMs), credentials(credentialsFor(server))
    {
        remoteDirectory = directory.filePath(QStringLiteral("remote"));
        localDirectory = directory.filePath(QStringLiteral("local"));
        QDir().mkpath(remoteDirectory);
        QDir().mkpath(localDirectory);
        credentials.address.url.setPath(remoteDirectory);
        const QString knownHosts = directory.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        if (fingerprint.isEmpty()) return;
        {
            SftpClient client({}, knownHosts);
            if (!client.connect(credentials, fingerprint)) return;
        }
        SftpSessions::remember(credentials);
        SftpFileProviderPlugin::setKnownHostsFileForTesting(knownHosts);
        ready = true;
    }
    ~Fixture()
    {
        SftpSessions::forget(credentials.address.identity());
        SftpFileProviderPlugin::setKnownHostsFileForTesting({});
    }
    QString url(const QString &name = {}) const
    {
        return name.isEmpty() ? credentials.address.encoded() : SftpPath::child(credentials.address.encoded(), name);
    }
};
}

class RemoteLaunchRecorder : public QObject {
    Q_OBJECT
public:
    QUrl opened;
public slots:
    void record(const QUrl &url) { opened = url; }
};

class SftpQueueRegression : public QObject {
    Q_OBJECT
private slots:
    void replaceOnOtherProvider_data()
    {
        QTest::addColumn<bool>("move");
        QTest::newRow("copy") << false;
        QTest::newRow("move") << true;
    }
    void replaceOnOtherProvider()
    {
        QFETCH(bool, move);
        Fixture fixture;
        QVERIFY(fixture.ready);
        const QString source = QDir(fixture.remoteDirectory).filePath(QStringLiteral("replace.txt"));
        QVERIFY(writeFile(source, "new SFTP data"));
        OperationQueue queue;
        auto destination = std::make_shared<NewItemUploadProvider>();
        // Unregistered test schemes use the queue's fallback provider cache key.
        queue.m_providerCache.insert(QStringLiteral("local"), destination);
        resolve(queue, OperationQueue::ConflictResolution::Replace);
        const auto result = completed(queue, [&] {
            if (move) queue.moveTo({fixture.url(QStringLiteral("replace.txt"))}, QStringLiteral("test-cloud://root"));
            else queue.copyTo({fixture.url(QStringLiteral("replace.txt"))}, QStringLiteral("test-cloud://root"));
        });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(destination->removals, 1);
        QCOMPARE(destination->bytes, QByteArray("new SFTP data"));
        QCOMPARE(QFile::exists(source), !move);
    }
    void asynchronousMutations()
    {
        Fixture fixture(0, false, false, 200);
        QVERIFY(fixture.ready);
        FilePanelController panel;
        QVERIFY(panel.openPath(fixture.url()));
        QTRY_VERIFY(!panel.navigationPending() && !panel.directoryModel()->loading());
        QSignalSpy created(&panel, &FilePanelController::entryCreated);
        QSignalSpy finished(&panel, &FilePanelController::remoteMutationFinished);
        QTimer heartbeat;
        heartbeat.setInterval(10);
        int ticks = 0;
        connect(&heartbeat, &QTimer::timeout, &panel, [&] { ++ticks; });
        heartbeat.start();
        QElapsedTimer elapsed;
        elapsed.start();
        panel.createFolder(QStringLiteral("New Folder"));
        QVERIFY(elapsed.elapsed() < 100);
        QVERIFY(panel.remoteMutationPending());
        QVERIFY(!panel.canCreateManualItem());
        QTRY_VERIFY(!finished.isEmpty());
        QVERIFY(finished.last().at(2).toBool());
        QVERIFY(ticks >= 5);
        QCOMPARE(created.size(), 1);
        QVERIFY(panel.lastError().isEmpty());
        const QString path = created.first().first().toString();
        QTRY_VERIFY(!panel.directoryModel()->loading());
        QVERIFY(panel.renamePath(path, QStringLiteral("New Folder")));
        QTRY_COMPARE(finished.size(), 2);
        QVERIFY(finished.last().at(2).toBool());
        QVERIFY(panel.lastError().isEmpty());
        QTRY_VERIFY(!panel.directoryModel()->loading());
        QVERIFY(panel.renamePath(path, QStringLiteral("Renamed folder")));
        QTRY_COMPARE(finished.size(), 3);
        QVERIFY(finished.last().at(2).toBool());
        QVERIFY(QFileInfo(QDir(fixture.remoteDirectory).filePath(QStringLiteral("Renamed folder"))).isDir());
    }
    void mutationCancellationAndNavigation()
    {
        Fixture fixture(0, false, false, 300);
        QVERIFY(fixture.ready);
        FilePanelController panel;
        QVERIFY(panel.openPath(fixture.url()));
        QTRY_VERIFY(!panel.navigationPending() && !panel.directoryModel()->loading());
        QSignalSpy created(&panel, &FilePanelController::entryCreated);
        QSignalSpy finished(&panel, &FilePanelController::remoteMutationFinished);
        panel.createFile(QStringLiteral("New File"));
        QVERIFY(panel.remoteMutationPending());
        QTRY_VERIFY(fixture.server.mutationStarted.load());
        panel.cancelRemoteMutation();
        QTRY_VERIFY(!panel.remoteMutationPending());
        QVERIFY(!finished.first().at(2).toBool());
        QVERIFY(created.isEmpty());
        QVERIFY(panel.lastError().isEmpty());
        QTRY_VERIFY(!panel.directoryModel()->loading());
        panel.createFolder(QStringLiteral("New Folder"));
        QVERIFY(panel.remoteMutationPending());
        QVERIFY(panel.openPath(fixture.localDirectory));
        QTRY_VERIFY(!panel.remoteMutationPending());
        QTRY_VERIFY(!panel.navigationPending() && !panel.directoryModel()->loading());
        QCOMPARE(panel.currentPath(), fixture.localDirectory);
        QVERIFY(created.isEmpty());
        QVERIFY(panel.lastError().isEmpty());
        auto closing = std::make_unique<FilePanelController>();
        QVERIFY(closing->openPath(fixture.url()));
        QTRY_VERIFY(!closing->navigationPending() && !closing->directoryModel()->loading());
        closing->createFile(QStringLiteral("New File"));
        QVERIFY(closing->remoteMutationPending());
        QElapsedTimer elapsed;
        elapsed.start();
        closing.reset();
        QVERIFY(elapsed.elapsed() < 100);
        QTest::qWait(200);
    }
    void mutationPermissionError()
    {
        Fixture fixture(0, false, true);
        QVERIFY(fixture.ready);
        FilePanelController panel;
        QVERIFY(panel.openPath(fixture.url()));
        QTRY_VERIFY(!panel.navigationPending() && !panel.directoryModel()->loading());
        QSignalSpy created(&panel, &FilePanelController::entryCreated);
        panel.createFolder(QStringLiteral("New Folder"));
        QVERIFY(panel.remoteMutationPending());
        QTRY_VERIFY(!panel.remoteMutationPending());
        QVERIFY(created.isEmpty());
        const QString error = panel.lastError().value(QStringLiteral("message")).toString();
        QVERIFY(!error.isEmpty());
        QCOMPARE(panel.lastError().value(QStringLiteral("code")).toString(), QStringLiteral("accessDenied"));
        QVERIFY(QDir(fixture.remoteDirectory).entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty());
    }
    void previewAndReadOnlyLaunch()
    {
        Fixture fixture;
        QVERIFY(fixture.ready);
        const QString textPath = QDir(fixture.remoteDirectory).filePath(QStringLiteral("données #?.txt"));
        QVERIFY(writeFile(textPath, "SFTP preview\nSecond line\n"));
        const QString url = fixture.url(QStringLiteral("données #?.txt"));
        QuickLookController preview;
        QVERIFY(preview.isRemotePreviewContentPath(url));
        QVERIFY(!preview.canRequestThumbnailForPath(url));
        preview.preview(url);
        QTRY_VERIFY(!preview.loading());
        QCOMPARE(preview.type(), QStringLiteral("text"));
        QVERIFY2(preview.content().contains(QLatin1String("SFTP preview")), qPrintable(preview.content()));
        const QString imagePath = QDir(fixture.remoteDirectory).filePath(QStringLiteral("image.png"));
        QImage image(24, 18, QImage::Format_ARGB32);
        image.fill(Qt::green);
        QVERIFY(image.save(imagePath));
        preview.preview(fixture.url(QStringLiteral("image.png")));
        QTRY_VERIFY(!preview.loading());
        QCOMPARE(preview.type(), QStringLiteral("image"));
        const QImage downloadedImage(QUrl(preview.mediaSourceUrl()).toLocalFile());
        QCOMPARE(downloadedImage.size(), image.size());
        QCOMPARE(downloadedImage.pixelColor(0, 0), QColor(Qt::green));
        const QString largePath = QDir(fixture.remoteDirectory).filePath(QStringLiteral("large.txt"));
        QFile large(largePath);
        QVERIFY(large.open(QIODevice::WriteOnly));
        QVERIFY(large.resize(PreviewInternal::kRemotePreviewMaterializeLimit + 1));
        large.close();
        preview.preview(fixture.url(QStringLiteral("large.txt")));
        QTRY_VERIFY(!preview.loading());
        QCOMPARE(preview.type(), QStringLiteral("info"));
        QVERIFY(preview.sizeText().contains(QLatin1String("Large file")));
        RemoteLaunchRecorder recorder;
        QDesktopServices::setUrlHandler(QStringLiteral("file"), &recorder, "record");
        const auto clearHandler = qScopeGuard([] { QDesktopServices::unsetUrlHandler(QStringLiteral("file")); });
        FilePanelController panel;
        QVERIFY(panel.openPath(fixture.url()));
        QTRY_VERIFY(!panel.navigationPending() && !panel.directoryModel()->loading());
        panel.openFilePath(url);
        QVERIFY(panel.remoteMutationPending());
        QTRY_VERIFY(!panel.remoteMutationPending());
        QVERIFY2(!recorder.opened.isEmpty(), qPrintable(panel.lastError().value(QStringLiteral("message")).toString()));
        const QString staged = recorder.opened.toLocalFile();
        QVERIFY(staged != textPath);
        QCOMPARE(readFile(staged), readFile(textPath));
        QVERIFY(!QFileInfo(staged).permissions().testFlag(QFileDevice::WriteOwner));
        QFile::remove(staged);
        SftpSessions::forget(fixture.credentials.address.identity());
        preview.preview(url);
        QTRY_VERIFY(!preview.loading());
        QCOMPARE(preview.type(), QStringLiteral("info"));
        QVERIFY(!preview.content().contains(QLatin1String("SFTP preview")));
    }
    void panelCutPasteAndMove()
    {
        Fixture fixture;
        QVERIFY(fixture.ready);
        WorkspaceController workspace;
        workspace.setSplitEnabled(true);
        auto *left = workspace.leftPanel();
        auto *right = workspace.rightPanel();
        QVERIFY(writeFile(QDir(fixture.remoteDirectory).filePath(QStringLiteral("move.txt")), "move"));
        QVERIFY(left->openPath(fixture.url()));
        QVERIFY(right->openPath(fixture.localDirectory));
        QTRY_VERIFY(!left->navigationPending() && !left->directoryModel()->loading()
                    && !right->navigationPending() && !right->directoryModel()->loading());
        const int row = left->directoryModel()->indexOfPath(fixture.url(QStringLiteral("move.txt")));
        QVERIFY(row >= 0);
        left->directoryModel()->clearSelection();
        QVERIFY(workspace.oppositePanelDropCapabilities(0, {fixture.url(QStringLiteral("move.txt"))}, 1)
                    .value(QStringLiteral("canMove")).toBool());
        left->directoryModel()->selectRows({row});
        QVERIFY(left->canRenameSelection());
        QVERIFY(left->canCutSelection());
        QVERIFY(workspace.oppositePanelDropCapabilities(0, left->selectedPaths(), 1).value(QStringLiteral("canMove")).toBool());
        workspace.setActivePanel(0);
        workspace.cutToClipboard();
        QVERIFY(workspace.clipboardCut());
        QCOMPARE(workspace.clipboardPaths(), left->selectedPaths());
        workspace.setActivePanel(1);
        auto result = completed(*workspace.operationQueue(), [&] { workspace.pasteFromClipboard(); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QVERIFY(!QFile::exists(QDir(fixture.remoteDirectory).filePath(QStringLiteral("move.txt"))));
        QCOMPARE(readFile(QDir(fixture.localDirectory).filePath(QStringLiteral("move.txt"))), QByteArray("move"));
        QTRY_VERIFY(!right->directoryModel()->loading());
        const int localRow = right->directoryModel()->indexOfPath(QDir(fixture.localDirectory).filePath(QStringLiteral("move.txt")));
        QVERIFY(localRow >= 0);
        right->directoryModel()->selectRows({localRow});
        result = completed(*workspace.operationQueue(), [&] { workspace.moveActiveSelectionToOpposite(); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(readFile(QDir(fixture.remoteDirectory).filePath(QStringLiteral("move.txt"))), QByteArray("move"));
        QVERIFY(!QFile::exists(QDir(fixture.localDirectory).filePath(QStringLiteral("move.txt"))));
    }
    void cancelledLaunchDoesNotOpen()
    {
        Fixture fixture(0, false, false, 300);
        QVERIFY(fixture.ready);
        const QString source = QDir(fixture.remoteDirectory).filePath(QStringLiteral("cancel.txt"));
        QVERIFY(writeFile(source, QByteArray(1024 * 1024, 'x')));
        RemoteLaunchRecorder recorder;
        QDesktopServices::setUrlHandler(QStringLiteral("file"), &recorder, "record");
        const auto clearHandler = qScopeGuard([] { QDesktopServices::unsetUrlHandler(QStringLiteral("file")); });
        FilePanelController panel;
        QVERIFY(panel.openPath(fixture.url()));
        QTRY_VERIFY(!panel.navigationPending() && !panel.directoryModel()->loading());
        panel.openFilePath(fixture.url(QStringLiteral("cancel.txt")));
        QVERIFY(panel.remoteMutationPending());
        QTRY_VERIFY(fixture.server.mutationStarted.load());
        panel.cancelRemoteMutation();
        QTRY_VERIFY(!panel.remoteMutationPending());
        QVERIFY(recorder.opened.isEmpty());
        QVERIFY(panel.lastError().isEmpty());
        panel.openFilePath(fixture.url(QStringLiteral("cancel.txt")));
        QVERIFY(panel.openPath(fixture.localDirectory));
        QTRY_VERIFY(!panel.remoteMutationPending());
        QVERIFY(recorder.opened.isEmpty());
        QCOMPARE(QFileInfo(source).size(), qint64(1024 * 1024));
    }
    void directoryLinkNavigationAndCopy()
    {
        Fixture fixture;
        QVERIFY(fixture.ready);
        const QString external = fixture.directory.filePath(QStringLiteral("external"));
        const QString folder = QDir(fixture.remoteDirectory).filePath(QStringLiteral("folder"));
        QVERIFY(QDir().mkpath(folder));
        QVERIFY(writeFile(QDir(external).filePath(QStringLiteral("preserve.txt")), "preserve"));
        QVERIFY(QFile::link(external, QDir(folder).filePath(QStringLiteral("link"))));
        FilePanelController panel;
        const QString folderUrl = fixture.url(QStringLiteral("folder"));
        QVERIFY(panel.openPath(folderUrl));
        QTRY_VERIFY(!panel.navigationPending() && !panel.directoryModel()->loading());
        const QString linkUrl = SftpPath::child(folderUrl, QStringLiteral("link"));
        const int row = panel.directoryModel()->indexOfPath(linkUrl);
        QVERIFY(row >= 0);
        QVERIFY(panel.directoryModel()->isDirectoryAt(row));
        panel.openItem(row);
        QTRY_VERIFY(panel.currentPath() == linkUrl && !panel.navigationPending() && !panel.directoryModel()->loading());
        QVERIFY(panel.directoryModel()->indexOfPath(SftpPath::child(linkUrl, QStringLiteral("preserve.txt"))) >= 0);
        OperationQueue queue;
        auto result = completed(queue, [&] { queue.copyTo({folderUrl}, fixture.localDirectory); });
        QCOMPARE(result.value(QStringLiteral("failedCount")).toInt(), 1);
        QVERIFY(!QFile::exists(QDir(fixture.localDirectory).filePath(QStringLiteral("folder/link/preserve.txt"))));
        result = completed(queue, [&] { queue.deletePaths({folderUrl}); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(readFile(QDir(external).filePath(QStringLiteral("preserve.txt"))), QByteArray("preserve"));
    }
    void remotePanelCopyAndMove()
    {
        Fixture fixture;
        QVERIFY(fixture.ready);
        const QString source = QDir(fixture.remoteDirectory).filePath(QStringLiteral("source.bin"));
        const QString destination = QDir(fixture.remoteDirectory).filePath(QStringLiteral("destination"));
        const QByteArray data = QByteArray::fromHex("0001ff80").repeated(32768);
        QVERIFY(writeFile(source, data));
        QVERIFY(QDir().mkpath(destination));
        const QString destinationUrl = fixture.url(QStringLiteral("destination"));
        OperationQueue queue;
        resolve(queue, OperationQueue::ConflictResolution::Replace);
        auto result = completed(queue, [&] { queue.copyTo({fixture.url(QStringLiteral("source.bin"))}, destinationUrl); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("source.bin"))), data);
        QCOMPARE(readFile(source), data);
        QVERIFY(writeFile(source, "replacement"));
        result = completed(queue, [&] { queue.moveTo({fixture.url(QStringLiteral("source.bin"))}, destinationUrl); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QVERIFY(!QFile::exists(source));
        QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("source.bin"))), QByteArray("replacement"));
    }
    void roundTripAndConflicts()
    {
        Fixture fixture;
        QVERIFY(fixture.ready);
        OperationQueue queue;
        resolve(queue, OperationQueue::ConflictResolution::Replace);
        const QString name = QStringLiteral(" données #?%.bin ");
        const QString source = QDir(fixture.localDirectory).filePath(name);
        const QByteArray data = QByteArray::fromHex("000102ff8020").repeated(32 * 1024);
        QVERIFY(writeFile(source, data));
        auto result = completed(queue, [&]() { queue.copyTo({source}, fixture.url()); });
        QCOMPARE(result.value(QStringLiteral("failedCount")).toInt(), 0);
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        const QString target = QDir(fixture.remoteDirectory).filePath(name);
        QCOMPARE(QCryptographicHash::hash(readFile(target), QCryptographicHash::Sha256), QCryptographicHash::hash(data, QCryptographicHash::Sha256));
        QVERIFY(writeFile(target, "replacement"));
        result = completed(queue, [&]() { queue.copyTo({fixture.url(name)}, fixture.localDirectory); });
        QCOMPARE(result.value(QStringLiteral("failedCount")).toInt(), 0);
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(readFile(source), QByteArray("replacement"));
        QVERIFY(writeFile(source, "uploaded-replacement"));
        result = completed(queue, [&]() { queue.copyTo({source}, fixture.url()); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(readFile(target), QByteArray("uploaded-replacement"));
        OperationQueue skip;
        resolve(skip, OperationQueue::ConflictResolution::Skip);
        QVERIFY(writeFile(source, "skip"));
        result = completed(skip, [&]() { skip.copyTo({source}, fixture.url()); });
        QCOMPARE(result.value(QStringLiteral("failedCount")).toInt(), 0);
        QCOMPARE(readFile(target), QByteArray("uploaded-replacement"));
        OperationQueue keepBoth;
        resolve(keepBoth, OperationQueue::ConflictResolution::KeepBoth);
        result = completed(keepBoth, [&]() { keepBoth.copyTo({source}, fixture.url()); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(QDir(fixture.remoteDirectory).entryList(QDir::Files).size(), 2);
        QCOMPARE(readFile(target), QByteArray("uploaded-replacement"));
    }
    void directoriesAndMoves()
    {
        Fixture fixture;
        QVERIFY(fixture.ready);
        OperationQueue queue;
        resolve(queue, OperationQueue::ConflictResolution::Replace);
        const QString folder = QDir(fixture.localDirectory).filePath(QStringLiteral("folder"));
        QVERIFY(writeFile(QDir(folder).filePath(QStringLiteral("sub/data.txt")), "nested"));
        QVERIFY(writeFile(QDir(folder).filePath(QStringLiteral("empty")), {}));
        QVERIFY(writeFile(QDir(fixture.remoteDirectory).filePath(QStringLiteral("folder/keep.txt")), "keep"));
        auto result = completed(queue, [&]() { queue.copyTo({folder}, fixture.url()); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(readFile(QDir(fixture.remoteDirectory).filePath(QStringLiteral("folder/sub/data.txt"))), QByteArray("nested"));
        QCOMPARE(readFile(QDir(fixture.remoteDirectory).filePath(QStringLiteral("folder/keep.txt"))), QByteArray("keep"));
        const QString destination = QDir(fixture.localDirectory).filePath(QStringLiteral("download"));
        QVERIFY(QDir().mkpath(destination));
        result = completed(queue, [&]() { queue.copyTo({fixture.url(QStringLiteral("folder"))}, destination); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QCOMPARE(readFile(QDir(destination).filePath(QStringLiteral("folder/sub/data.txt"))), QByteArray("nested"));
        const QString moving = QDir(fixture.localDirectory).filePath(QStringLiteral("move.txt"));
        QVERIFY(writeFile(moving, "move"));
        result = completed(queue, [&]() { queue.moveTo({moving}, fixture.url()); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QVERIFY(!QFile::exists(moving));
        QCOMPARE(readFile(QDir(fixture.remoteDirectory).filePath(QStringLiteral("move.txt"))), QByteArray("move"));
        result = completed(queue, [&]() { queue.moveTo({fixture.url(QStringLiteral("move.txt"))}, fixture.localDirectory); });
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 1);
        QVERIFY(!QFile::exists(QDir(fixture.remoteDirectory).filePath(QStringLiteral("move.txt"))));
        QCOMPARE(readFile(moving), QByteArray("move"));
    }
    void failedMovePreservesBothFiles_data()
    {
        QTest::addColumn<bool>("drop");
        QTest::addColumn<bool>("readOnly");
        QTest::newRow("connection-lost") << true << false;
        QTest::newRow("atomic-rename-unavailable") << false << false;
        QTest::newRow("permission-denied") << false << true;
    }
    void failedMovePreservesBothFiles()
    {
        QFETCH(bool, drop);
        QFETCH(bool, readOnly);
        Fixture fixture(drop ? 2 : 0, !drop && !readOnly, readOnly);
        QVERIFY(fixture.ready);
        OperationQueue queue;
        resolve(queue, OperationQueue::ConflictResolution::Replace);
        const QString source = QDir(fixture.localDirectory).filePath(QStringLiteral("existing.bin"));
        const QString target = QDir(fixture.remoteDirectory).filePath(QStringLiteral("existing.bin"));
        const QByteArray data(128 * 1024, 'x');
        QVERIFY(writeFile(source, data));
        QVERIFY(writeFile(target, "preserve"));
        const auto result = completed(queue, [&]() { queue.moveTo({source}, fixture.url()); });
        QCOMPARE(result.value(QStringLiteral("failedCount")).toInt(), 1);
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 0);
        QCOMPARE(readFile(source), data);
        QCOMPARE(readFile(target), QByteArray("preserve"));
        if (readOnly) {
            // OpenSSH -R also removes atomic-rename support from its advertised
            // extensions. A new name reaches the actual write-permission check.
            const QString denied = QDir(fixture.localDirectory).filePath(QStringLiteral("denied.bin"));
            QVERIFY(writeFile(denied, data));
            const auto deniedResult = completed(queue, [&]() { queue.moveTo({denied}, fixture.url()); });
            QCOMPARE(deniedResult.value(QStringLiteral("failedCount")).toInt(), 1);
            QVERIFY2(queue.error().contains(QLatin1String("permission denied")), qPrintable(queue.error()));
            QVERIFY(QFile::exists(denied));
            QCOMPARE(readFile(target), QByteArray("preserve"));
        }
    }
    void cancelDownload()
    {
        Fixture fixture;
        QVERIFY(fixture.ready);
        OperationQueue queue;
        resolve(queue, OperationQueue::ConflictResolution::Replace);
        const QString source = QDir(fixture.remoteDirectory).filePath(QStringLiteral("existing.bin"));
        const QString target = QDir(fixture.localDirectory).filePath(QStringLiteral("existing.bin"));
        QVERIFY(writeFile(source, QByteArray(2 * 1024 * 1024, 'x')));
        QVERIFY(writeFile(target, "preserve"));
        connect(&queue, &OperationQueue::progressChanged, &queue, [&]() {
            if (queue.progress() > 0.05 && queue.progress() < 1) queue.cancel();
        });
        const auto result = completed(queue, [&]() { queue.copyTo({fixture.url(QStringLiteral("existing.bin"))}, fixture.localDirectory); });
        QVERIFY(result.value(QStringLiteral("aborted")).toBool());
        QCOMPARE(readFile(target), QByteArray("preserve"));
        QCOMPARE(QDir(fixture.localDirectory).entryList(QDir::Files | QDir::Hidden).size(), 1);
    }
    void cancelUpload()
    {
        Fixture fixture;
        QVERIFY(fixture.ready);
        OperationQueue queue;
        resolve(queue, OperationQueue::ConflictResolution::Replace);
        const QString source = QDir(fixture.localDirectory).filePath(QStringLiteral("existing.bin"));
        const QString target = QDir(fixture.remoteDirectory).filePath(QStringLiteral("existing.bin"));
        QVERIFY(writeFile(source, QByteArray(2 * 1024 * 1024, 'x')));
        QVERIFY(writeFile(target, "preserve"));
        connect(&queue, &OperationQueue::progressChanged, &queue, [&]() {
            if (queue.progress() > 0.05 && queue.progress() < 1) queue.cancel();
        });
        const auto result = completed(queue, [&]() { queue.moveTo({source}, fixture.url()); });
        QVERIFY(result.value(QStringLiteral("aborted")).toBool());
        QVERIFY(QFile::exists(source));
        QCOMPARE(readFile(target), QByteArray("preserve"));
        const auto partials = QDir(fixture.remoteDirectory).entryList({QStringLiteral(".fm-upload-*.part")}, QDir::Files | QDir::Hidden);
        if (!partials.isEmpty()) QVERIFY(queue.statusMessage().contains(QLatin1String("Temporary file may remain")));
    }
    void cancelDirectoryScan()
    {
        // Cancel while the native queue is estimating a directory, before any
        // transfer begins. Its worker cancellation must reach the socket watcher.
        QTemporaryDir temporary;
        TestServer server(true, true);
        QVERIFY(server.port() > 0);
        auto credentials = credentialsFor(server);
        const QString knownHosts = temporary.filePath(QStringLiteral("known_hosts"));
        const QString fingerprint = confirm(server, credentials, knownHosts);
        { SftpClient client({}, knownHosts); QVERIFY(client.connect(credentials, fingerprint)); }
        SftpSessions::remember(credentials);
        SftpFileProviderPlugin::setKnownHostsFileForTesting(knownHosts);
        const auto cleanup = qScopeGuard([&]() {
            SftpSessions::forget(credentials.address.identity());
            SftpFileProviderPlugin::setKnownHostsFileForTesting({});
        });
        OperationQueue queue;
        QTimer cancellation;
        cancellation.setInterval(10);
        connect(&cancellation, &QTimer::timeout, &queue, [&]() {
            if (server.listingStarted.load()) queue.cancel();
        });
        cancellation.start();
        QElapsedTimer elapsed;
        elapsed.start();
        const auto result = completed(queue, [&]() { queue.moveTo({credentials.address.encoded()}, temporary.path()); });
        QVERIFY(result.value(QStringLiteral("aborted")).toBool());
        QCOMPARE(result.value(QStringLiteral("succeededCount")).toInt(), 0);
        QCOMPARE(result.value(QStringLiteral("failedCount")).toInt(), 0);
        QVERIFY(queue.error().isEmpty());
        QVERIFY(elapsed.elapsed() < 2000);
    }
};

int runSftpQueueRegression()
{
    SftpQueueRegression regression;
    return QTest::qExec(&regression, QStringList{QStringLiteral("sftp-transfer-regression")});
}

// GUI sidebar fixtures exercise the plugin's in-memory authorization state.
// Actual password authentication is covered by SftpClientTest.
void setSftpPlacesFixture(bool enabled)
{
    for (int i = 0; i < 12; ++i) {
        const auto address = SftpPath::parse(QStringLiteral("sftp://sidebar-test-%1@127.0.0.1/").arg(i));
        if (enabled) SftpSessions::remember({address, QStringLiteral("sidebar-fixture"), {}});
        else SftpSessions::forget(address.identity());
    }
}

QVariantMap sftpCreationFixture(bool enabled, int mutationDelayMs)
{
    static std::unique_ptr<Fixture> fixture;
    fixture.reset();
    if (!enabled) return {{QStringLiteral("ok"), true}};
    fixture = std::make_unique<Fixture>(0, false, false, mutationDelayMs);
    return {{QStringLiteral("ok"), fixture->ready},
            {QStringLiteral("url"), fixture->url()},
            {QStringLiteral("directory"), fixture->remoteDirectory}};
}

#include "SftpQueueRegression.moc"
