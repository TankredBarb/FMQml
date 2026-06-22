#include "MegaFileProviderPlugin.h"
#include "MegaPath.h"
#include "MegaCache.h"
#include "MegaClient.h"

#include <megaapi.h>

using namespace mega;

#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <QEventLoop>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include <QTimer>

class MegaFileProvider final : public FileProvider
{
    Q_OBJECT

public:
    explicit MegaFileProvider(QObject *parent = nullptr)
        : FileProvider(parent)
        , m_currentGeneration(0)
        , m_pendingScanGeneration(0)
    {
        connect(&MegaClient::instance(), &MegaClient::publicLinkLoaded, this, &MegaFileProvider::onPublicLinkLoaded);
    }

    ~MegaFileProvider() override = default;

    QString scheme() const override
    {
        return QStringLiteral("mega");
    }

    bool canHandle(const QString &path) const override
    {
        if (MegaPath::isSchemePath(path)) {
            return true;
        }
        QString linkId, linkKey;
        bool isFolder = false;
        return !MegaPath::fromUserInput(path, linkId, linkKey, isFolder).isEmpty();
    }

    Capabilities capabilities() const override
    {
        return Browse | ReadMetadata | Transfer;
    }

    void scan(const QString &path) override
    {
        const QString normalized = MegaPath::normalizedPath(path);
        m_currentPath = normalized;
        m_currentGeneration++;

        emit started();

        if (!MegaPath::isLinkPath(normalized)) {
            emit finished(normalized, false, m_currentGeneration, QStringLiteral("Not a public MEGA link path"));
            return;
        }

        const QString linkId = MegaPath::linkIdForPath(normalized);

        // If the path is already cached
        if (MegaCache::getChildren(normalized).has_value()) {
            emitChildEntries(normalized, m_currentGeneration);
            emit finished(normalized, true, m_currentGeneration);
            return;
        }

        // If the root of this link is loaded but this sub-path is not cached -> not found
        const QString rootPath = QStringLiteral("mega://link/") + linkId;
        if (MegaCache::getEntry(rootPath).has_value()) {
            emit finished(normalized, false, m_currentGeneration, QStringLiteral("Path not found"));
            return;
        }

        // Otherwise, fetch from SDK
        m_pendingScanPath = normalized;
        m_pendingScanGeneration = m_currentGeneration;

        MegaClient::instance().getPublicNode(linkId);
    }

    void cancel() override
    {
        MegaClient::instance().cancelAll();
    }

    void setShowHidden(bool show) override
    {
        Q_UNUSED(show)
    }

    bool isRunning() const override
    {
        return !m_pendingScanPath.isEmpty();
    }

    QString currentPath() const override
    {
        return m_currentPath;
    }

    int currentGeneration() const override
    {
        return m_currentGeneration;
    }

    bool pathExists(const QString &path) const override
    {
        return MegaCache::getEntry(MegaPath::normalizedPath(path)).has_value();
    }

    bool isDirectory(const QString &path) const override
    {
        const auto entry = MegaCache::getEntry(MegaPath::normalizedPath(path));
        return entry.has_value() && entry->isDirectory;
    }

    bool isSymLink(const QString &path) const override
    {
        Q_UNUSED(path)
        return false;
    }

    QString normalizedPath(const QString &path) const override
    {
        return MegaPath::normalizedPath(path);
    }

    QString fileName(const QString &path) const override
    {
        return MegaPath::fallbackFileNameForPath(path);
    }

    QString absolutePath(const QString &path) const override
    {
        return MegaPath::normalizedPath(path);
    }

    QString parentPath(const QString &path) const override
    {
        return MegaPath::parentPath(path);
    }

    QString childPath(const QString &parentPath, const QString &name) const override
    {
        return MegaPath::childPath(parentPath, name);
    }

    std::optional<FileEntry> entryInfo(const QString &path) const override
    {
        return MegaCache::getEntry(MegaPath::normalizedPath(path));
    }

    bool ensureParentDirectory(const QString &path) const override
    {
        Q_UNUSED(path)
        return false;
    }

    bool makePath(const QString &path) const override
    {
        Q_UNUSED(path)
        return false;
    }

    bool removePath(const QString &path) const override
    {
        Q_UNUSED(path)
        return false;
    }

    QStringList childPaths(const QString &path, bool includeHidden = true) const override
    {
        Q_UNUSED(includeHidden)
        return MegaCache::getChildren(MegaPath::normalizedPath(path)).value_or(QStringList{});
    }

    bool movePath(const QString &sourcePath, const QString &destinationPath) const override
    {
        Q_UNUSED(sourcePath)
        Q_UNUSED(destinationPath)
        return false;
    }

    std::unique_ptr<QIODevice> openRead(const QString &path) const override
    {
        return openRead(path, QDir::tempPath());
    }

    std::unique_ptr<QIODevice> openRead(const QString &path, const QString &stagingParentPath) const override
    {
        const QString normalized = MegaPath::normalizedPath(path);

        QString templatePath = stagingParentPath.isEmpty() ? QDir::tempPath() : stagingParentPath;
        if (!templatePath.endsWith(QLatin1Char('/'))) {
            templatePath += QLatin1Char('/');
        }
        templatePath += QStringLiteral("mega_preview_XXXXXX");

        auto tempFile = std::make_unique<QTemporaryFile>(templatePath);
        if (!tempFile->open()) {
            return nullptr;
        }

        QString tempPath = tempFile->fileName();
        tempFile->close();

        if (!copyToLocalFile(normalized, tempPath, nullptr, nullptr)) {
            return nullptr;
        }

        if (!tempFile->QFile::open(QIODevice::ReadOnly)) {
            return nullptr;
        }

        return tempFile;
    }

    bool copyToLocalFile(const QString &sourcePath,
                         const QString &destinationFilePath,
                         const std::function<bool(qint64 processedBytes, qint64 totalBytes)> &progressCallback,
                         QString *errorStr) const override
    {
        const QString normalized = MegaPath::normalizedPath(sourcePath);

        const QString partialPath = destinationFilePath + QStringLiteral(".part");
        QFile::remove(partialPath);

        QEventLoop loop;
        bool transferSuccess = false;
        bool transferFinished = false;
        QString transferError;

        QTimer timeoutTimer;
        timeoutTimer.setSingleShot(true);
        QMetaObject::Connection timeoutConn = connect(&timeoutTimer, &QTimer::timeout, &loop, [&]() {
            transferError = QStringLiteral("MEGA download timed out");
            MegaClient::instance().cancelAll();
            loop.quit();
        });

        QMetaObject::Connection progressConn = connect(&MegaClient::instance(), &MegaClient::downloadProgress, &loop,
            [&](const QString &path, qint64 processed, qint64 total) {
                if (MegaPath::normalizedPath(path) == normalized) {
                    if (progressCallback) {
                        if (!progressCallback(processed, total)) {
                            MegaClient::instance().cancelAll();
                        }
                    }
                }
            });

        QMetaObject::Connection finishedConn = connect(&MegaClient::instance(), &MegaClient::downloadFinished, &loop,
            [&](const QString &path, bool success, const QString &errorString) {
                if (MegaPath::normalizedPath(path) == normalized) {
                    transferSuccess = success;
                    transferFinished = true;
                    transferError = errorString;
                    loop.quit();
                }
            });

        MegaClient::instance().startDownload(normalized, partialPath);
        timeoutTimer.start(30 * 60 * 1000);

        loop.exec();

        disconnect(progressConn);
        disconnect(finishedConn);
        disconnect(timeoutConn);

        if (!transferFinished || !transferSuccess) {
            QFile::remove(partialPath);
            if (errorStr) {
                *errorStr = transferError.isEmpty() ? QStringLiteral("Unknown download error") : transferError;
            }
            return false;
        }

        QFile::remove(destinationFilePath);
        if (!QFile::rename(partialPath, destinationFilePath)) {
            QFile::remove(partialPath);
            if (errorStr) {
                *errorStr = QStringLiteral("Could not move MEGA download into place");
            }
            return false;
        }

        return true;
    }

    std::unique_ptr<QIODevice> openWrite(const QString &path, bool truncate = true) const override
    {
        Q_UNUSED(path)
        Q_UNUSED(truncate)
        return nullptr;
    }

    bool renamePath(const QString &oldPath, const QString &newName) override
    {
        Q_UNUSED(oldPath)
        Q_UNUSED(newName)
        return false;
    }

    bool createFolder(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override
    {
        Q_UNUSED(parentPath)
        Q_UNUSED(name)
        Q_UNUSED(createdPath)
        return false;
    }

    bool createFile(const QString &parentPath, const QString &name, QString *createdPath = nullptr) override
    {
        Q_UNUSED(parentPath)
        Q_UNUSED(name)
        Q_UNUSED(createdPath)
        return false;
    }

private slots:
    void onPublicLinkLoaded(const QString &linkId, bool success, const QString &errorString)
    {
        if (m_pendingScanPath.isEmpty()) {
            return;
        }

        const QString pendingLinkId = MegaPath::linkIdForPath(m_pendingScanPath);
        if (pendingLinkId != linkId) {
            return;
        }

        const QString scanPath = m_pendingScanPath;
        const int gen = m_pendingScanGeneration;
        m_pendingScanPath.clear();

        if (success) {
            if (MegaCache::getEntry(scanPath).has_value()) {
                emitChildEntries(scanPath, gen);
                emit finished(scanPath, true, gen);
            } else {
                emit finished(scanPath, false, gen, QStringLiteral("Path not found after loading link"));
            }
        } else {
            emit finished(scanPath, false, gen, errorString);
        }
    }

private:
    void emitChildEntries(const QString &parentPath, int generation)
    {
        const QList<FileEntry> entries = MegaCache::childEntries(parentPath);
        if (!entries.isEmpty()) {
            emit batchReady(entries, generation);
        }
    }

    QString m_currentPath;
    int m_currentGeneration;
    QString m_pendingScanPath;
    int m_pendingScanGeneration;
};

// MegaFileProviderPlugin implementation

MegaFileProviderPlugin::MegaFileProviderPlugin()
{
    // Force initialization of MegaClient in the main thread
    MegaClient::instance();
}

int MegaFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}

QString MegaFileProviderPlugin::pluginId() const
{
    return QStringLiteral("mega");
}

QString MegaFileProviderPlugin::displayName() const
{
    return QStringLiteral("MEGA");
}

QStringList MegaFileProviderPlugin::schemes() const
{
    return { QStringLiteral("mega") };
}

bool MegaFileProviderPlugin::canHandle(const QString &path) const
{
    if (MegaPath::isSchemePath(path)) {
        return true;
    }
    QString linkId, linkKey; bool isFolder;
    return !MegaPath::fromUserInput(path, linkId, linkKey, isFolder).isEmpty();
}

std::unique_ptr<FileProvider> MegaFileProviderPlugin::createProvider()
{
    return std::make_unique<MegaFileProvider>();
}

QString MegaFileProviderPlugin::preprocessPath(const QString &path) const
{
    QString linkId, linkKey;
    bool isFolder = false;
    QString result = MegaPath::fromUserInput(path, linkId, linkKey, isFolder);
    if (!result.isEmpty()) {
        MegaCache::storeKey(linkId, linkKey, isFolder);
        return result;
    }
    return path;
}

int MegaFileProviderPlugin::actionApiVersion() const
{
    return FM_FILE_ACTION_PLUGIN_API_VERSION;
}

QString MegaFileProviderPlugin::actionPluginId() const
{
    return pluginId();
}

QString MegaFileProviderPlugin::actionDisplayName() const
{
    return displayName();
}

QList<FileActionDescriptor> MegaFileProviderPlugin::actionsForContext(const FileActionContext &context) const
{
    Q_UNUSED(context)
    return {};
}

QVariantMap MegaFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    Q_UNUSED(actionId)
    Q_UNUSED(context)
    return {};
}

#include "MegaFileProviderPlugin.moc"
