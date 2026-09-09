#include "preview/RemotePreviewCache.h"
#include "core/CleanupSubsystem.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThreadPool>

using namespace PreviewInternal;

bool expect(bool value, const char *message)
{
    if (!value) QTextStream(stderr) << "FAILED: " << message << '\n';
    return value;
}

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid()) return 1;
    qputenv("XDG_CACHE_HOME", home.path().toUtf8());
    QCoreApplication app(argc, argv);
    RemotePreviewCache cache;
    int next = 0;
    const auto makeArtifact = [&](qint64 bytes) {
        auto artifact = std::make_shared<RemotePreviewArtifact>();
        artifact->directory = CleanupSubsystem::instance().allocateStagingDirectory(
            CleanupArtifactKind::RemotePreview, home.path(), QString::number(++next), &artifact->leaseId);
        artifact->path = QDir(artifact->directory).filePath(QStringLiteral("image.png"));
        QFile file(artifact->path);
        if (artifact->leaseId.isEmpty() || !file.open(QIODevice::WriteOnly) || !file.resize(bytes)) return std::shared_ptr<RemotePreviewArtifact>{};
        artifact->bytes = bytes;
        return artifact;
    };
    bool ok = true;
    auto visible = makeArtifact(10);
    if (!visible) return 1;
    const QString visiblePath = visible->path;
    cache.insert("session1:A:v1", visible);
    ok &= expect(cache.find("session1:A:v1") == visible, "cache did not reuse its artifact");
    ok &= expect(!cache.find("session2:A:v1") && !cache.find("session1:A:v2"), "session or version identity was ignored");
    cache.clear();
    QThreadPool::globalInstance()->waitForDone();
    ok &= expect(QFile::exists(visiblePath), "eviction deleted a visible preview");
    visible.reset();
    QThreadPool::globalInstance()->waitForDone();
    ok &= expect(!QFile::exists(visiblePath), "last owner did not release the cleanup lease");

    auto oldest = makeArtifact(1);
    if (!oldest) return 1;
    cache.insert("oldest", oldest);
    for (int i = 0; i < 7; ++i) cache.insert(QString::number(i), makeArtifact(1));
    ok &= expect(cache.find("oldest") == oldest, "recent access failed");
    cache.insert("ninth", makeArtifact(1));
    ok &= expect(!cache.find("0") && cache.find("oldest"), "entry budget did not follow access order");
    cache.clear();
    for (int i = 0; i < 5; ++i) cache.insert(QString::number(i), makeArtifact(32LL * 1024 * 1024));
    ok &= expect(!cache.find("0") && cache.find("1") && cache.find("4"), "byte budget was not enforced");
    auto changed = cache.find("4");
    if (!changed) return 1;
    QFile file(changed->path);
    ok &= expect(file.open(QIODevice::WriteOnly) && file.resize(1), "fixture truncation failed");
    file.close();
    ok &= expect(!cache.find("4"), "truncated cached file was reused");
    changed.reset();
    oldest.reset();
    cache.clear();
    QThreadPool::globalInstance()->waitForDone();
    return ok ? 0 : 1;
}
