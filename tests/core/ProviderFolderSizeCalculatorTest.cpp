#include "ProviderFolderSizeCalculator.h"
#include "FileProvider.h"

#include <QCoreApplication>
#include <QBuffer>
#include <QHash>
#include <QStringList>
#include <QTextStream>

#include <memory>
#include <optional>

namespace {

struct TestNode {
    bool directory = false;
    qint64 size = 0;
    QStringList children;
};

class TestProvider final : public FileProvider {
public:
    explicit TestProvider(QHash<QString, TestNode> nodes, QObject *parent = nullptr)
        : FileProvider(parent)
        , m_nodes(std::move(nodes))
    {
    }

    QString scheme() const override { return QStringLiteral("test"); }
    bool canHandle(const QString &path) const override { return path.startsWith(QStringLiteral("test://")); }
    Capabilities capabilities() const override { return Browse | ReadMetadata; }
    void scan(const QString &) override {}
    void cancel() override {}
    void setShowHidden(bool) override {}
    bool isRunning() const override { return false; }
    QString currentPath() const override { return {}; }
    int currentGeneration() const override { return 0; }
    bool pathExists(const QString &path) const override { return m_nodes.contains(path); }
    bool isDirectory(const QString &path) const override { return m_nodes.value(path).directory; }
    bool isSymLink(const QString &) const override { return false; }
    QString normalizedPath(const QString &path) const override { return path; }
    QString fileName(const QString &path) const override { return path.section(QLatin1Char('/'), -1); }
    QString absolutePath(const QString &path) const override { return path; }
    QString parentPath(const QString &) const override { return {}; }
    QString childPath(const QString &parentPath, const QString &name) const override
    {
        return parentPath.endsWith(QLatin1Char('/')) ? parentPath + name : parentPath + QLatin1Char('/') + name;
    }

    std::optional<FileEntry> entryInfo(const QString &path) const override
    {
        const auto it = m_nodes.constFind(path);
        if (it == m_nodes.cend()) {
            m_lastError = QStringLiteral("Missing metadata for %1").arg(path);
            return std::nullopt;
        }
        FileEntry entry;
        entry.path = path;
        entry.name = fileName(path);
        entry.isDirectory = it->directory;
        entry.size = it->size;
        entry.sizeText = QString::number(it->size);
        return entry;
    }

    bool ensureParentDirectory(const QString &) const override { return false; }
    bool makePath(const QString &) const override { return false; }
    bool removePath(const QString &) const override { return false; }
    QStringList childPaths(const QString &path, bool includeHidden = true) const override
    {
        Q_UNUSED(includeHidden)
        const auto it = m_nodes.constFind(path);
        if (it == m_nodes.cend()) {
            m_lastError = QStringLiteral("Missing children for %1").arg(path);
            return {};
        }
        return it->children;
    }
    bool movePath(const QString &, const QString &) const override { return false; }
    std::unique_ptr<QIODevice> openRead(const QString &) const override { return nullptr; }
    std::unique_ptr<QIODevice> openWrite(const QString &, bool truncate = true) const override
    {
        Q_UNUSED(truncate)
        return nullptr;
    }
    bool renamePath(const QString &, const QString &) override { return false; }
    bool createFolder(const QString &, const QString &, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        return false;
    }
    bool createFile(const QString &, const QString &, QString *createdPath = nullptr) override
    {
        if (createdPath) {
            createdPath->clear();
        }
        return false;
    }
    QString lastErrorString() const override { return m_lastError; }
    void clearLastError() const override { m_lastError.clear(); }

private:
    QHash<QString, TestNode> m_nodes;
    mutable QString m_lastError;
};

struct Result {
    qint64 bytes = -1;
    int files = -1;
    int folders = -1;
    bool exact = false;
    bool cancelled = false;
    QString error;
};

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

Result calculate(QHash<QString, TestNode> nodes, const QString &path, bool cancelBeforeRun = false)
{
    ProviderFolderSizeCalculator calculator(std::make_unique<TestProvider>(std::move(nodes)), path, 7);
    Result result;
    QObject::connect(&calculator,
                     &ProviderFolderSizeCalculator::resultReady,
                     [&result](qint64 bytes, int files, int folders, bool exact, bool cancelled, const QString &error, int generation) {
                         if (generation != 7) {
                             result.error = QStringLiteral("Unexpected generation");
                             return;
                         }
                         result.bytes = bytes;
                         result.files = files;
                         result.folders = folders;
                         result.exact = exact;
                         result.cancelled = cancelled;
                         result.error = error;
                     });
    if (cancelBeforeRun) {
        calculator.cancel();
    }
    calculator.run();
    return result;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    Result file = calculate({
        {QStringLiteral("test://file.txt"), TestNode{false, 42, {}}},
    }, QStringLiteral("test://file.txt"));
    if (file.bytes != 42 || file.files != 1 || file.folders != 0 || !file.exact || file.cancelled) {
        return fail(QStringLiteral("File root result was incorrect"));
    }

    Result nested = calculate({
        {QStringLiteral("test://root"), TestNode{true, 0, {QStringLiteral("test://root/a.txt"), QStringLiteral("test://root/folder")}}},
        {QStringLiteral("test://root/a.txt"), TestNode{false, 10, {}}},
        {QStringLiteral("test://root/folder"), TestNode{true, 0, {QStringLiteral("test://root/folder/b.bin")}}},
        {QStringLiteral("test://root/folder/b.bin"), TestNode{false, 15, {}}},
    }, QStringLiteral("test://root"));
    if (nested.bytes != 25 || nested.files != 2 || nested.folders != 1 || !nested.exact || nested.cancelled) {
        return fail(QStringLiteral("Nested folder result was incorrect"));
    }

    Result empty = calculate({
        {QStringLiteral("test://empty"), TestNode{true, 0, {}}},
    }, QStringLiteral("test://empty"));
    if (empty.bytes != 0 || empty.files != 0 || empty.folders != 0 || !empty.exact || empty.cancelled) {
        return fail(QStringLiteral("Empty folder result was incorrect"));
    }

    Result partial = calculate({
        {QStringLiteral("test://partial"), TestNode{true, 0, {QStringLiteral("test://partial/missing.txt")}}},
    }, QStringLiteral("test://partial"));
    if (partial.bytes != 0 || partial.files != 0 || partial.folders != 0 || partial.exact || partial.error.isEmpty()) {
        return fail(QStringLiteral("Missing child metadata did not produce a partial result"));
    }

    Result cancelled = calculate({
        {QStringLiteral("test://cancel"), TestNode{true, 0, {QStringLiteral("test://cancel/a.txt")}}},
        {QStringLiteral("test://cancel/a.txt"), TestNode{false, 10, {}}},
    }, QStringLiteral("test://cancel"), true);
    if (!cancelled.cancelled || cancelled.exact || cancelled.error != QStringLiteral("Cancelled")) {
        return fail(QStringLiteral("Cancellation result was incorrect"));
    }

    return 0;
}
