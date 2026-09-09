#include "preview/PreviewInternal.h"
#include "preview/RemotePreviewCache.h"
#include "core/FileProviderFactory.h"
#include "core/DriveUtils.h"
#include "core/CleanupSubsystem.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThreadPool>

namespace {
QString stagingRoot;
QString session = "session1";
int version = 1;
int downloads = 0;
bool validDownload = true;

class PreviewProvider final : public FileProvider {
public:
    QString scheme() const override { return "test"; }
    bool canHandle(const QString &) const override { return true; }
    Capabilities capabilities() const override { return Browse | ReadMetadata; }
    void scan(const QString &) override {}
    void cancel() override {}
    void setShowHidden(bool) override {}
    bool isRunning() const override { return false; }
    QString currentPath() const override { return {}; }
    int currentGeneration() const override { return 0; }
    bool pathExists(const QString &) const override { return true; }
    bool isDirectory(const QString &) const override { return false; }
    bool isSymLink(const QString &) const override { return false; }
    QString normalizedPath(const QString &p) const override { return p; }
    QString fileName(const QString &) const override { return "image.png"; }
    QString absolutePath(const QString &p) const override { return p; }
    QString parentPath(const QString &) const override { return "test://"; }
    QString childPath(const QString &, const QString &) const override { return {}; }
    std::optional<FileEntry> entryInfo(const QString &p) const override {
        FileEntry entry;
        entry.path = p;
        entry.name = "image.png";
        entry.suffix = "png";
        entry.size = 5;
        return entry;
    }
    QString previewCacheIdentity(const QString &p) const override {
        return session.isEmpty() ? QString{} : session + p + QString::number(version);
    }
    bool ensureParentDirectory(const QString &) const override { return false; }
    bool makePath(const QString &) const override { return false; }
    bool removePath(const QString &) const override { return false; }
    QStringList childPaths(const QString &, bool = true) const override { return {}; }
    bool movePath(const QString &, const QString &) const override { return false; }
    std::unique_ptr<QIODevice> openRead(const QString &) const override { return {}; }
    std::unique_ptr<QIODevice> openWrite(const QString &, bool = true) const override { return {}; }
    bool renamePath(const QString &, const QString &) override { return false; }
    bool createFolder(const QString &, const QString &, QString * = nullptr) override { return false; }
    bool createFile(const QString &, const QString &, QString * = nullptr) override { return false; }
    bool copyToLocalFileForPreview(const QString &, const QString &dest,
                                  const std::function<bool(qint64, qint64)> &progress, QString *) const override {
        ++downloads;
        if (progress && !progress(0, 5)) return false;
        QFile file(dest);
        return file.open(QIODevice::WriteOnly) && file.write(validDownload ? "valid" : "wrong") == 5;
    }
};

bool expect(bool value, const char *message) {
    if (!value) QTextStream(stderr) << "FAILED: " << message << '\n';
    return value;
}
}

std::unique_ptr<FileProvider> FileProviderFactory::createProvider(const QString &) {
    return std::make_unique<PreviewProvider>();
}
QString DriveUtils::formatSize(qint64 bytes) { return QString::number(bytes); }

// Keep the real materialization/cache/cleanup path; replace format-specific
// decoding and presentation with deterministic fixtures for these flow tests.
namespace PreviewInternal {
QString cheapFileName(QString) { return "image.png"; }
QString googleDriveAccessSummary(const FileEntry &) { return {}; }
QString materializedPreviewSuffix(const FileEntry &) { return "png"; }
QString remotePreviewTooLargeText(const FileEntry &) { return "too large"; }
QString remotePreviewRoot(bool) { return stagingRoot; }
void removeRemotePreviewDir(const QString &path) { if (!path.isEmpty()) QDir(path).removeRecursively(); }
QString safePreviewFileName(QString name) { return name; }
bool materializedRemotePreviewLooksUsable(const QString &path, const FileEntry &) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) && file.readAll() == "valid";
}
QString redactedPreviewPathForLog(const QString &) { return "test fixture"; }
bool quickLookPreviewTraceEnabled() { return false; }
LocalPreviewData loadLocalPreviewData(const QString &path, bool) {
    LocalPreviewData data;
    data.type = "image";
    data.content = path;
    return data;
}
QString materializeAudioCoverSource(const QString &, const QString &, const QString &) { return {}; }
}

int main(int argc, char **argv) {
    QTemporaryDir home;
    if (!home.isValid()) return 1;
    stagingRoot = home.path();
    qputenv("XDG_CACHE_HOME", home.path().toUtf8());
    QCoreApplication app(argc, argv);
    using namespace PreviewInternal;
    bool ok = true;
    {
        auto a = loadProviderPreviewData("test://A");
        auto b = loadProviderPreviewData("test://B");
        auto again = loadProviderPreviewData("test://A");
        ok &= expect(downloads == 2 && a.cachedArtifact && a.cachedArtifact == again.cachedArtifact,
                     "A-B-A downloaded A twice");
        auto refreshed = loadProviderPreviewData("test://A", {}, {}, false);
        ok &= expect(downloads == 3 && refreshed.materializedPath != a.materializedPath,
                     "explicit reload reused cached data");
        ++version;
        auto changed = loadProviderPreviewData("test://A");
        ok &= expect(downloads == 4, "changed version reused the old file");
        session = "session2";
        auto otherAccount = loadProviderPreviewData("test://A");
        ok &= expect(downloads == 5, "new session reused the old session's file");
        auto cancelled = loadProviderPreviewData("test://C", {}, [](qint64, qint64, bool) { return false; });
        ok &= expect(downloads == 5 && cancelled.materializedPath.isEmpty(), "cancelled request started downloading");
        validDownload = false;
        auto invalid = loadProviderPreviewData("test://invalid");
        ok &= expect(!invalid.cachedArtifact && invalid.materializedPath.isEmpty(), "invalid data entered cache");
        validDownload = true;
        auto recovered = loadProviderPreviewData("test://invalid");
        ok &= expect(downloads == 8 && recovered.cachedArtifact, "retry reused invalid data");
        auto abandoned = loadProviderPreviewData("test://abandoned", {}, [](qint64 bytes, qint64, bool) {
            return bytes == 0;
        });
        ok &= expect(downloads == 9 && !abandoned.cachedArtifact && abandoned.materializedPath.isEmpty(),
                     "request cancelled after downloading entered cache");
        RemotePreviewCache::instance().clear();
        QThreadPool::globalInstance()->waitForDone();
        ok &= expect(QFile::exists(a.materializedPath), "cache clear deleted a held preview");
    }
    RemotePreviewCache::instance().clear();
    QThreadPool::globalInstance()->waitForDone();
    return ok ? 0 : 1;
}
