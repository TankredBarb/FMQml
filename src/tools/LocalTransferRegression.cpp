#include "LocalTransferRegression.h"

#include "../controllers/WorkspaceController.h"
#include "../core/ArchiveFileProvider.h"
#include "../core/ArchiveSupport.h"
#include "../core/FileProviderPluginRegistry.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>

#include <functional>

#ifdef Q_OS_UNIX
#include <sys/stat.h>
#endif

namespace {
constexpr int OperationTimeoutMs = 15000;

bool writeTransferFixtureFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Truncate)
        && file.write(contents) == contents.size();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return file.readAll();
}

bool waitUntil(const std::function<bool()> &condition, int timeoutMs = OperationTimeoutMs)
{
    QEventLoop loop;
    QTimer timeout;
    QTimer poll;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(&poll, &QTimer::timeout, &loop, [&]() {
        if (condition()) loop.quit();
    });
    timeout.start(timeoutMs);
    poll.start(10);
    if (!condition()) loop.exec();
    return condition();
}

QVariantMap runOperation(OperationQueue &queue,
                         const std::function<void()> &start,
                         const std::function<void()> &beforeReplace = {})
{
    QVariantMap completion;
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
    const QMetaObject::Connection conflictConnection = QObject::connect(
        &queue, &OperationQueue::conflictDetected, &loop,
        [&](const QString &, const QString &, qint64, const QDateTime &, qint64, const QDateTime &) {
            if (beforeReplace) beforeReplace();
            queue.resolveConflict(OperationQueue::ConflictResolution::Replace, true);
        });
    const QMetaObject::Connection completedConnection = QObject::connect(
        &queue, &OperationQueue::operationCompleted, &loop, [&](const QVariantMap &result) {
            completion = result;
            loop.quit();
        });
    timeout.start(OperationTimeoutMs);
    start();
    loop.exec();
    QObject::disconnect(conflictConnection);
    QObject::disconnect(completedConnection);
    return completion;
}

bool succeeded(const QVariantMap &completion)
{
    return completion.value(QStringLiteral("succeededCount")).toInt() > 0
        && completion.value(QStringLiteral("failedCount")).toInt() == 0;
}

bool pathsUseDifferentFileSystems(const QString &left, const QString &right)
{
#ifdef Q_OS_UNIX
    struct stat leftStat {};
    struct stat rightStat {};
    const QByteArray leftPath = QFile::encodeName(left);
    const QByteArray rightPath = QFile::encodeName(right);
    return ::stat(leftPath.constData(), &leftStat) == 0
        && ::stat(rightPath.constData(), &rightStat) == 0
        && leftStat.st_dev != rightStat.st_dev;
#else
    Q_UNUSED(left)
    Q_UNUSED(right)
    return false;
#endif
}

void setExternalFileClipboard(const QStringList &paths, bool cut, bool includeUrls = true)
{
    auto *mime = new QMimeData;
    QList<QUrl> urls;
    QByteArray gnome(cut ? "cut\n" : "copy\n");
    for (const QString &path : paths) {
        const QUrl url = QUrl::fromLocalFile(path);
        urls.append(url);
        gnome += url.toEncoded();
        gnome += '\n';
    }
    if (includeUrls) mime->setUrls(urls);
    mime->setData("x-special/gnome-copied-files", gnome);
    mime->setData("application/x-kde-cutselection", cut ? QByteArrayLiteral("1") : QByteArrayLiteral("0"));
    QApplication::clipboard()->setMimeData(mime);
}
}

namespace LocalTransferRegression {

int run(QApplication &app)
{
    Q_UNUSED(app)
    QTemporaryDir fixture;
    QString failure;
    if (!fixture.isValid()) failure = QStringLiteral("cannot create temporary fixture");

    WorkspaceController workspace;
    workspace.setSplitEnabled(true);
    OperationQueue *queue = workspace.operationQueue();
    const QString copySourceDir = fixture.filePath(QStringLiteral("copy-source"));
    const QString copyDestinationDir = fixture.filePath(QStringLiteral("copy-destination"));
    const QString moveSourceDir = fixture.filePath(QStringLiteral("move-source"));
    const QString moveDestinationDir = fixture.filePath(QStringLiteral("move-destination"));
    QDir().mkpath(copySourceDir);
    QDir().mkpath(copyDestinationDir);
    QDir().mkpath(moveSourceDir);
    QDir().mkpath(moveDestinationDir);

    const QString copySource = QDir(copySourceDir).filePath(QStringLiteral("replace.bin"));
    const QString copyTarget = QDir(copyDestinationDir).filePath(QStringLiteral("replace.bin"));
    writeTransferFixtureFile(copySource, QByteArrayLiteral("copy-new"));
    writeTransferFixtureFile(copyTarget, QByteArrayLiteral("copy-old"));

    workspace.leftPanel()->openPath(copySourceDir);
    if (failure.isEmpty() && !waitUntil([&]() {
            return !workspace.leftPanel()->directoryModel()->loading()
                && workspace.leftPanel()->directoryModel()->indexOfPath(copySource) >= 0;
        })) {
        failure = QStringLiteral("source panel did not load clipboard fixture");
    }
    const bool localClipboardExported = workspace.copyPathsToClipboard({copySource}, 0)
        && QApplication::clipboard()->mimeData()->hasUrls()
        && QApplication::clipboard()->mimeData()->urls().contains(QUrl::fromLocalFile(copySource))
        && QApplication::clipboard()->mimeData()->data("x-special/gnome-copied-files").startsWith("copy\n");
    if (failure.isEmpty() && !localClipboardExported) {
        failure = QStringLiteral("local Copy was not exported to the system clipboard");
    }

    const QString externalCopySource = QDir(copySourceDir).filePath(QStringLiteral("external-copy.txt"));
    const QString externalCopyDestination = fixture.filePath(QStringLiteral("external-copy-destination"));
    QDir().mkpath(externalCopyDestination);
    writeTransferFixtureFile(externalCopySource, QByteArrayLiteral("external-copy"));
    setExternalFileClipboard({externalCopySource}, false, false);
    workspace.rightPanel()->openPath(externalCopyDestination);
    workspace.activateRight();
    const bool externalCopyDestinationLoaded = waitUntil([&]() {
        return !workspace.rightPanel()->directoryModel()->loading()
            && workspace.rightPanel()->currentPath() == externalCopyDestination;
    });
    const bool externalCopyImported = waitUntil([&]() {
        return workspace.clipboardPaths() == QStringList{externalCopySource} && !workspace.clipboardCut();
    });
    const QVariantMap externalCopyResult = runOperation(*queue,
        [&]() { workspace.pasteFromClipboard(); });
    const bool externalCopyPasted = externalCopyDestinationLoaded
        && externalCopyImported
        && succeeded(externalCopyResult)
        && readFile(QDir(externalCopyDestination).filePath(QStringLiteral("external-copy.txt")))
            == QByteArrayLiteral("external-copy");
    if (failure.isEmpty() && !externalCopyPasted) {
        failure = QStringLiteral("external local Copy failed: imported=%1 succeeded=%2 clipboard=%3 cut=%4")
            .arg(externalCopyImported)
            .arg(succeeded(externalCopyResult))
            .arg(workspace.clipboardPaths().join(QLatin1Char(',')))
            .arg(workspace.clipboardCut());
    }

    const QString externalCutSource = QDir(moveSourceDir).filePath(QStringLiteral("external-cut.txt"));
    const QString externalCutDestination = fixture.filePath(QStringLiteral("external-cut-destination"));
    QDir().mkpath(externalCutDestination);
    writeTransferFixtureFile(externalCutSource, QByteArrayLiteral("external-cut"));
    setExternalFileClipboard({externalCutSource}, true);
    workspace.rightPanel()->openPath(externalCutDestination);
    workspace.activateRight();
    const bool externalCutDestinationLoaded = waitUntil([&]() {
        return !workspace.rightPanel()->directoryModel()->loading()
            && workspace.rightPanel()->currentPath() == externalCutDestination;
    });
    const bool externalCutImported = waitUntil([&]() {
        return workspace.clipboardPaths() == QStringList{externalCutSource} && workspace.clipboardCut();
    });
    const QVariantMap externalCutResult = runOperation(*queue,
        [&]() { workspace.pasteFromClipboard(); });
    const bool externalCutPasted = externalCutDestinationLoaded
        && externalCutImported
        && succeeded(externalCutResult)
        && !QFile::exists(externalCutSource)
        && readFile(QDir(externalCutDestination).filePath(QStringLiteral("external-cut.txt")))
            == QByteArrayLiteral("external-cut")
        && !workspace.hasClipboard();
    if (failure.isEmpty() && !externalCutPasted) {
        failure = QStringLiteral("external local Cut was not moved and cleared after success");
    }

    const QVariantMap copyResult = runOperation(*queue,
        [&]() { queue->copyTo({copySource}, copyDestinationDir); });
    if (failure.isEmpty() && (!succeeded(copyResult)
                              || readFile(copyTarget) != QByteArrayLiteral("copy-new")
                              || !QFile::exists(copySource))) {
        failure = QStringLiteral("Copy Replace did not commit the new file correctly");
    }

    const QString failedSource = QDir(copySourceDir).filePath(QStringLiteral("fail.bin"));
    const QString preservedTarget = QDir(copyDestinationDir).filePath(QStringLiteral("fail.bin"));
    writeTransferFixtureFile(failedSource, QByteArrayLiteral("unreadable-after-conflict"));
    writeTransferFixtureFile(preservedTarget, QByteArrayLiteral("preserve-old"));
    const QVariantMap failedResult = runOperation(*queue,
        [&]() { queue->copyTo({failedSource}, copyDestinationDir); },
        [&]() { QFile::remove(failedSource); });
    if (failure.isEmpty() && (failedResult.value(QStringLiteral("failedCount")).toInt() != 1
                              || readFile(preservedTarget) != QByteArrayLiteral("preserve-old"))) {
        failure = QStringLiteral("failed Copy Replace did not preserve the old target");
    }

    const QString moveSource = QDir(moveSourceDir).filePath(QStringLiteral("replace.bin"));
    const QString moveTarget = QDir(moveDestinationDir).filePath(QStringLiteral("replace.bin"));
    writeTransferFixtureFile(moveSource, QByteArrayLiteral("move-new"));
    writeTransferFixtureFile(moveTarget, QByteArrayLiteral("move-old"));
    const QVariantMap moveResult = runOperation(*queue,
        [&]() { queue->moveTo({moveSource}, moveDestinationDir); });
    if (failure.isEmpty() && (!succeeded(moveResult)
                              || readFile(moveTarget) != QByteArrayLiteral("move-new")
                              || QFile::exists(moveSource))) {
        failure = QStringLiteral("Move Replace did not atomically move the new file");
    }

    const QString bulkSourceDir = fixture.filePath(QStringLiteral("bulk-source"));
    const QString bulkDestinationDir = fixture.filePath(QStringLiteral("bulk-destination"));
    QDir().mkpath(bulkSourceDir);
    QDir().mkpath(bulkDestinationDir);
    QStringList bulkSources;
    for (int i = 0; i < 48; ++i) {
        const QString path = QDir(bulkSourceDir).filePath(QStringLiteral("item-%1.bin").arg(i, 3, 10, QLatin1Char('0')));
        writeTransferFixtureFile(path, QByteArray(64 * 1024, static_cast<char>(i)));
        bulkSources.append(path);
    }

    workspace.leftPanel()->openPath(bulkSourceDir);
    workspace.rightPanel()->openPath(bulkDestinationDir);
    if (failure.isEmpty() && !waitUntil([&]() {
            return !workspace.leftPanel()->directoryModel()->loading()
                && !workspace.rightPanel()->directoryModel()->loading()
                && workspace.leftPanel()->directoryModel()->count() == bulkSources.size();
        })) {
        failure = QStringLiteral("panels did not load bulk-transfer fixtures");
    }

    int destinationRowInsertSignals = 0;
    int destinationModelResets = 0;
    QObject::connect(workspace.rightPanel()->directoryModel(), &QAbstractItemModel::rowsInserted,
                     &workspace, [&](const QModelIndex &, int, int) { ++destinationRowInsertSignals; });
    QObject::connect(workspace.rightPanel()->directoryModel(), &QAbstractItemModel::modelReset,
                     &workspace, [&]() { ++destinationModelResets; });
    const QVariantMap bulkResult = runOperation(*queue,
        [&]() { queue->copyTo(bulkSources, bulkDestinationDir); });
    const bool bulkSettled = waitUntil([&]() {
        return !workspace.rightPanel()->directoryModel()->loading()
            && workspace.rightPanel()->directoryModel()->count() == bulkSources.size();
    });
    if (failure.isEmpty() && (!succeeded(bulkResult)
                              || !bulkSettled
                              || destinationRowInsertSignals != 0
                              || destinationModelResets != 1)) {
        failure = QStringLiteral("bulk Copy did not converge through one consolidated model reset");
    }
    const int bulkRowInsertSignalResult = destinationRowInsertSignals;
    const int bulkModelResetResult = destinationModelResets;
    const int bulkCountResult = workspace.rightPanel()->directoryModel()->count();

    FileProviderPluginRegistry::instance().loadDefaultPluginDirectories();
    bool providerNavigationHistoryPreserved = false;
    if (FileProviderPluginRegistry::instance().hasProviderForPath(QStringLiteral("mock://Documents"))) {
        const QString providerPreviousPath = workspace.leftPanel()->currentPath();
        workspace.leftPanel()->openPath(QStringLiteral("mock://Documents"));
        const bool providerOpened = waitUntil([&]() {
            return !workspace.leftPanel()->directoryModel()->loading()
                && workspace.leftPanel()->currentPath() == QLatin1String("mock://Documents");
        });
        const bool providerBackAvailable = providerOpened && workspace.leftPanel()->canGoBack();
        if (providerBackAvailable) workspace.leftPanel()->goBack();
        const bool providerBackRestored = providerBackAvailable && waitUntil([&]() {
            return !workspace.leftPanel()->directoryModel()->loading()
                && workspace.leftPanel()->currentPath() == providerPreviousPath;
        });
        providerNavigationHistoryPreserved = providerOpened
            && providerBackAvailable
            && providerBackRestored;
        if (failure.isEmpty() && !providerNavigationHistoryPreserved) {
            failure = QStringLiteral("provider navigation did not update currentPath or Back history");
        }
    }

    bool archiveClipboardPreserved = false;
    bool archiveNavigationHistoryPreserved = false;
    if (ArchiveSupport::archiveBackendAvailable()) {
        const QString archiveFixtureSource = fixture.filePath(QStringLiteral("inside-archive.txt"));
        const QString archivePath = fixture.filePath(QStringLiteral("clipboard-archive.zip"));
        const QString archivePasteDestination = fixture.filePath(QStringLiteral("archive-paste-destination"));
        writeTransferFixtureFile(archiveFixtureSource, QByteArrayLiteral("archive-clipboard"));
        QDir().mkpath(archivePasteDestination);
        const QVariantMap archiveResult = runOperation(*queue,
            [&]() { queue->compressToArchive({archiveFixtureSource}, archivePath); });
        const QString archiveRoot = ArchiveSupport::archiveRootPath(archivePath);
        const QString archivePreviousPath = workspace.leftPanel()->currentPath();
        workspace.leftPanel()->openPath(archivePath);
        const bool archiveOpened = waitUntil([&]() {
            return !workspace.leftPanel()->directoryModel()->loading()
                && workspace.leftPanel()->currentPath() == archiveRoot;
        });
        const bool archiveBackAvailable = archiveOpened && workspace.leftPanel()->canGoBack();
        if (archiveBackAvailable) workspace.leftPanel()->goBack();
        const bool archiveBackRestored = archiveBackAvailable && waitUntil([&]() {
            return !workspace.leftPanel()->directoryModel()->loading()
                && workspace.leftPanel()->currentPath() == archivePreviousPath;
        });
        archiveNavigationHistoryPreserved = archiveOpened
            && archiveBackAvailable
            && archiveBackRestored;
        if (failure.isEmpty() && !archiveNavigationHistoryPreserved) {
            failure = QStringLiteral("archive navigation did not update currentPath or Back history");
        }
        const QString archivedItem = ArchiveSupport::archiveChildPath(
            archiveRoot, QStringLiteral("inside-archive.txt"));
        const bool archivedItemCopied = succeeded(archiveResult)
            && workspace.copyPathsToClipboard({archivedItem}, 0);
        const QMimeData *archiveSystemMime = QApplication::clipboard()->mimeData();
        const bool systemUrlsSuppressed = !archiveSystemMime
            || archiveSystemMime->formats().isEmpty();
        workspace.rightPanel()->openPath(archivePasteDestination);
        workspace.activateRight();
        const bool archiveDestinationLoaded = waitUntil([&]() {
            return !workspace.rightPanel()->directoryModel()->loading()
                && workspace.rightPanel()->currentPath() == archivePasteDestination;
        });
        const QVariantMap archivePasteResult = archivedItemCopied && archiveDestinationLoaded
            ? runOperation(*queue, [&]() { workspace.pasteFromClipboard(); }) : QVariantMap{};
        archiveClipboardPreserved = archivedItemCopied
            && systemUrlsSuppressed
            && succeeded(archivePasteResult)
            && readFile(QDir(archivePasteDestination).filePath(QStringLiteral("inside-archive.txt")))
                == QByteArrayLiteral("archive-clipboard");
        if (failure.isEmpty() && !archiveClipboardPreserved) {
            const QMimeData *systemMime = QApplication::clipboard()->mimeData();
            failure = QStringLiteral("archive clipboard failed: compressed=%1 exists=%2 copied=%3 urlsSuppressed=%4 pasteSucceeded=%5 item=%6 formats=%7")
                .arg(succeeded(archiveResult))
                .arg(QFile::exists(archivePath))
                .arg(archivedItemCopied)
                .arg(systemUrlsSuppressed)
                .arg(succeeded(archivePasteResult))
                .arg(archivedItem)
                .arg(systemMime ? systemMime->formats().join(QLatin1Char(',')) : QStringLiteral("<null>"));
        }
    }

    QTemporaryDir crossFileSystemFixture(QStringLiteral("/dev/shm/fmqml-transfer-regression-XXXXXX"));
    const bool crossFileSystemAvailable = crossFileSystemFixture.isValid()
        && pathsUseDifferentFileSystems(fixture.path(), crossFileSystemFixture.path());
    bool canceledMovePreserved = false;
    bool sourceRemovalFailureDuplicatedSafely = false;
    bool directoryMoveTransactional = false;
    bool failedDirectoryMoveCleaned = false;
    bool crossFileSystemReplaceResolvedOnce = false;
    if (crossFileSystemAvailable) {
        const QString replaceSource = fixture.filePath(QStringLiteral("cross-filesystem-replace.bin"));
        const QString replaceTarget = QDir(crossFileSystemFixture.path()).filePath(QStringLiteral("cross-filesystem-replace.bin"));
        writeTransferFixtureFile(replaceSource, QByteArrayLiteral("new-cross-filesystem-content"));
        writeTransferFixtureFile(replaceTarget, QByteArrayLiteral("old-cross-filesystem-content"));
        int replaceConflictCount = 0;
        const QMetaObject::Connection replaceConflictCounter = QObject::connect(
            queue, &OperationQueue::conflictDetected, &workspace,
            [&replaceConflictCount]() { ++replaceConflictCount; });
        const QVariantMap replaceResult = runOperation(*queue,
            [&]() { queue->moveTo({replaceSource}, crossFileSystemFixture.path()); });
        QObject::disconnect(replaceConflictCounter);
        crossFileSystemReplaceResolvedOnce = succeeded(replaceResult)
            && replaceConflictCount == 1
            && !QFile::exists(replaceSource)
            && readFile(replaceTarget) == QByteArrayLiteral("new-cross-filesystem-content");

        const QString cancelSource = fixture.filePath(QStringLiteral("cancel-move.bin"));
        QFile cancelFile(cancelSource);
        if (cancelFile.open(QIODevice::WriteOnly) && cancelFile.resize(128 * 1024 * 1024)) {
            cancelFile.close();
            QMetaObject::Connection cancelConnection;
            cancelConnection = QObject::connect(queue, &OperationQueue::operationStarted,
                &workspace, [&](OperationQueue::Type type, const QStringList &, const QString &) {
                    if (type == OperationQueue::Type::Move) QTimer::singleShot(0, queue, &OperationQueue::cancel);
                });
            setExternalFileClipboard({cancelSource}, true);
            workspace.rightPanel()->openPath(crossFileSystemFixture.path());
            workspace.activateRight();
            const bool cancelDestinationLoaded = waitUntil([&]() {
                return !workspace.rightPanel()->directoryModel()->loading()
                    && workspace.rightPanel()->currentPath() == crossFileSystemFixture.path();
            });
            const bool cutImported = waitUntil([&]() {
                return workspace.clipboardCut() && workspace.clipboardPaths() == QStringList{cancelSource};
            });
            const QVariantMap cancelResult = runOperation(*queue,
                [&]() { workspace.pasteFromClipboard(); });
            QObject::disconnect(cancelConnection);
            const QString cancelTarget = QDir(crossFileSystemFixture.path()).filePath(QStringLiteral("cancel-move.bin"));
            canceledMovePreserved = cancelDestinationLoaded
                && cutImported
                && cancelResult.value(QStringLiteral("aborted")).toBool()
                && QFile::exists(cancelSource)
                && !QFile::exists(cancelTarget)
                && !QFile::exists(cancelTarget + QStringLiteral(".part"))
                && workspace.clipboardCut()
                && workspace.clipboardPaths() == QStringList{cancelSource};
        }

        const QString protectedParent = fixture.filePath(QStringLiteral("protected-source"));
        QDir().mkpath(protectedParent);
        const QString protectedSource = QDir(protectedParent).filePath(QStringLiteral("remove-fails.bin"));
        const QString protectedTarget = QDir(crossFileSystemFixture.path()).filePath(QStringLiteral("remove-fails.bin"));
        const QString partialSuccessSource = fixture.filePath(QStringLiteral("partial-success.bin"));
        const QString partialSuccessTarget = QDir(crossFileSystemFixture.path()).filePath(QStringLiteral("partial-success.bin"));
        writeTransferFixtureFile(protectedSource, QByteArrayLiteral("complete-copy"));
        writeTransferFixtureFile(partialSuccessSource, QByteArrayLiteral("partial-success"));
        QFile::setPermissions(protectedParent, QFileDevice::ReadOwner | QFileDevice::ExeOwner);
        setExternalFileClipboard({partialSuccessSource, protectedSource}, true);
        workspace.rightPanel()->openPath(crossFileSystemFixture.path());
        workspace.activateRight();
        const bool protectedDestinationLoaded = waitUntil([&]() {
            return !workspace.rightPanel()->directoryModel()->loading()
                && workspace.rightPanel()->currentPath() == crossFileSystemFixture.path();
        });
        const bool protectedCutImported = waitUntil([&]() {
            return workspace.clipboardCut()
                && workspace.clipboardPaths() == QStringList{partialSuccessSource, protectedSource};
        });
        const QVariantMap removalResult = runOperation(*queue,
            [&]() { workspace.pasteFromClipboard(); });
        QFile::setPermissions(protectedParent, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
        sourceRemovalFailureDuplicatedSafely = protectedDestinationLoaded
            && protectedCutImported
            && removalResult.value(QStringLiteral("succeededCount")).toInt() == 1
            && removalResult.value(QStringLiteral("failedCount")).toInt() == 1
            && !QFile::exists(partialSuccessSource)
            && readFile(partialSuccessTarget) == QByteArrayLiteral("partial-success")
            && readFile(protectedSource) == QByteArrayLiteral("complete-copy")
            && readFile(protectedTarget) == QByteArrayLiteral("complete-copy")
            && workspace.clipboardCut()
            && workspace.clipboardPaths() == QStringList{protectedSource};

        const QString directorySource = fixture.filePath(QStringLiteral("directory-source"));
        QDir().mkpath(QDir(directorySource).filePath(QStringLiteral("nested")));
        writeTransferFixtureFile(QDir(directorySource).filePath(QStringLiteral("one.txt")), QByteArrayLiteral("one"));
        writeTransferFixtureFile(QDir(directorySource).filePath(QStringLiteral("nested/two.txt")), QByteArrayLiteral("two"));
        const QVariantMap directoryResult = runOperation(*queue,
            [&]() { queue->moveTo({directorySource}, crossFileSystemFixture.path()); });
        const QString directoryTarget = QDir(crossFileSystemFixture.path()).filePath(QStringLiteral("directory-source"));
        directoryMoveTransactional = succeeded(directoryResult)
            && !QFile::exists(directorySource)
            && readFile(QDir(directoryTarget).filePath(QStringLiteral("one.txt"))) == QByteArrayLiteral("one")
            && readFile(QDir(directoryTarget).filePath(QStringLiteral("nested/two.txt"))) == QByteArrayLiteral("two");

        const QString failingDirectorySource = fixture.filePath(QStringLiteral("failing-directory"));
        QDir().mkpath(failingDirectorySource);
        const QString readableChild = QDir(failingDirectorySource).filePath(QStringLiteral("a-readable.txt"));
        const QString unreadableChild = QDir(failingDirectorySource).filePath(QStringLiteral("z-unreadable.txt"));
        writeTransferFixtureFile(readableChild, QByteArrayLiteral("copied-before-error"));
        writeTransferFixtureFile(unreadableChild, QByteArrayLiteral("must-fail"));
        QFile::setPermissions(unreadableChild, {});
        const QVariantMap failingDirectoryResult = runOperation(*queue,
            [&]() { queue->moveTo({failingDirectorySource}, crossFileSystemFixture.path()); });
        QFile::setPermissions(unreadableChild, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
        const QString failingDirectoryTarget = QDir(crossFileSystemFixture.path()).filePath(QStringLiteral("failing-directory"));
        const QString stagingParent = QDir(crossFileSystemFixture.path()).filePath(QStringLiteral(".fm-tmp"));
        const bool stagingRemoved = waitUntil([&]() {
            return !QFileInfo::exists(stagingParent)
                || QDir(stagingParent).entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty();
        });
        failedDirectoryMoveCleaned = failingDirectoryResult.value(QStringLiteral("failedCount")).toInt() == 1
            && QFile::exists(failingDirectorySource)
            && !QFile::exists(failingDirectoryTarget)
            && stagingRemoved;
    }

    if (failure.isEmpty() && crossFileSystemAvailable
        && (!canceledMovePreserved
            || !crossFileSystemReplaceResolvedOnce
            || !sourceRemovalFailureDuplicatedSafely
            || !directoryMoveTransactional
            || !failedDirectoryMoveCleaned)) {
        failure = QStringLiteral("cross-filesystem Move safety regression failed");
    }

    QApplication::clipboard()->setText(QStringLiteral("plain text replaces file clipboard"));
    const bool systemClipboardReplacementCleared = waitUntil([&]() {
        return !workspace.hasClipboard() && !workspace.clipboardCut();
    });
    if (failure.isEmpty() && !systemClipboardReplacementCleared) {
        failure = QStringLiteral("plain-text system clipboard replacement did not clear file Paste state");
    }

    QJsonObject report{
        {QStringLiteral("version"), 1},
        {QStringLiteral("ok"), failure.isEmpty()},
        {QStringLiteral("copyReplace"), succeeded(copyResult)},
        {QStringLiteral("failedReplacePreserved"), readFile(preservedTarget) == QByteArrayLiteral("preserve-old")},
        {QStringLiteral("moveReplace"), succeeded(moveResult)},
        {QStringLiteral("bulkCount"), bulkCountResult},
        {QStringLiteral("bulkRowInsertSignals"), bulkRowInsertSignalResult},
        {QStringLiteral("bulkModelResets"), bulkModelResetResult},
        {QStringLiteral("localClipboardExported"), localClipboardExported},
        {QStringLiteral("externalCopyPasted"), externalCopyPasted},
        {QStringLiteral("externalCutPasted"), externalCutPasted},
        {QStringLiteral("archiveClipboardPreserved"), archiveClipboardPreserved},
        {QStringLiteral("archiveNavigationHistoryPreserved"), archiveNavigationHistoryPreserved},
        {QStringLiteral("providerNavigationHistoryPreserved"), providerNavigationHistoryPreserved},
        {QStringLiteral("crossFileSystemAvailable"), crossFileSystemAvailable},
        {QStringLiteral("crossFileSystemReplaceResolvedOnce"), crossFileSystemReplaceResolvedOnce},
        {QStringLiteral("canceledMovePreserved"), canceledMovePreserved},
        {QStringLiteral("sourceRemovalFailureDuplicatedSafely"), sourceRemovalFailureDuplicatedSafely},
        {QStringLiteral("systemClipboardReplacementCleared"), systemClipboardReplacementCleared},
        {QStringLiteral("directoryMoveTransactional"), directoryMoveTransactional},
        {QStringLiteral("failedDirectoryMoveCleaned"), failedDirectoryMoveCleaned},
        {QStringLiteral("error"), failure}
    };
    ArchiveFileProvider::clearCache();
    fprintf(stdout, "%s\n", QJsonDocument(report).toJson(QJsonDocument::Compact).constData());
    workspace.operationQueue()->cancel();
    return failure.isEmpty() ? 0 : 1;
}

}
