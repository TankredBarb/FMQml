#include "ArchiveFileProvider.h"
#include "ArchiveCache.h"

#include <bit7z/bit7z.hpp>
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QBuffer>
#include <QTemporaryFile>
#include <QtConcurrent>
#include <QDebug>

namespace {
    const QString SCHEME = QStringLiteral("archive://");
    const QString DELIMITER = QStringLiteral("|");

    bit7z::tstring toTString(const QString& str) {
#if defined(_WIN32) && defined(BIT7Z_USE_NATIVE_STRING)
        return str.toStdWString();
#else
        return str.toStdString();
#endif
    }
}

ArchiveFileProvider::ArchiveFileProvider(QObject *parent)
    : FileProvider(parent)
{
}

ArchiveFileProvider::~ArchiveFileProvider()
{
}

QString ArchiveFileProvider::scheme() const
{
    return SCHEME;
}

bool ArchiveFileProvider::canHandle(const QString &path) const
{
    return path.startsWith(SCHEME);
}

FileProvider::Capabilities ArchiveFileProvider::capabilities() const
{
    return Capabilities(Browse | ReadMetadata | Transfer);
}

std::optional<ArchiveFileProvider::ArchiveContext> ArchiveFileProvider::parsePath(const QString &fullPath) const
{
    if (!canHandle(fullPath)) {
        return std::nullopt;
    }

    QString stripped = fullPath.mid(SCHEME.length());
    int delimIndex = stripped.indexOf(DELIMITER);
    if (delimIndex == -1) {
        return ArchiveContext{stripped, QStringLiteral("/")};
    }

    QString diskPath = stripped.left(delimIndex);
    QString internalPath = stripped.mid(delimIndex + DELIMITER.length());
    if (internalPath.isEmpty()) {
        internalPath = QStringLiteral("/");
    } else {
        internalPath = QDir::cleanPath(internalPath);
    }
    
    if (!internalPath.startsWith(QLatin1Char('/'))) {
        internalPath.prepend(QLatin1Char('/'));
    }

    return ArchiveContext{diskPath, internalPath};
}

bool ArchiveFileProvider::loadArchive(const QString &archiveDiskPath)
{
    if (m_loadedArchiveDiskPath == archiveDiskPath && m_rootNode) {
        return true;
    }

    m_reader = ArchiveCache::instance().getArchive(archiveDiskPath, &m_rootNode);
    if (m_reader && m_rootNode) {
        m_loadedArchiveDiskPath = archiveDiskPath;
        return true;
    }
    
    m_rootNode = nullptr;
    m_reader.reset();
    return false;
}

ArchiveNode* ArchiveFileProvider::findNode(const QString &internalPath) const
{
    if (!m_rootNode) return nullptr;
    if (internalPath == QStringLiteral("/") || internalPath.isEmpty()) {
        return m_rootNode;
    }

    QString cleanPath = QDir::cleanPath(internalPath);
    if (cleanPath.startsWith(QLatin1Char('/'))) {
        cleanPath = cleanPath.mid(1);
    }

    QStringList parts = cleanPath.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    ArchiveNode* current = m_rootNode;

    for (const QString &part : parts) {
        auto it = current->children.find(part);
        if (it == current->children.end()) {
            return nullptr;
        }
        current = it->second.get();
    }
    return current;
}

FileEntry ArchiveFileProvider::entryFromNode(const ArchiveContext &ctx, const ArchiveNode *node) const
{
    FileEntry entry;
    if (!node) return entry;

    entry.name = node->name;
    entry.path = SCHEME + ctx.archiveDiskPath + DELIMITER + node->path;
    entry.isDirectory = node->isDirectory;
    entry.size = node->size;
    entry.modified = node->modified;
    entry.created = node->created;

    if (!node->isDirectory) {
        int dotIdx = node->name.lastIndexOf(QLatin1Char('.'));
        if (dotIdx >= 0 && dotIdx < node->name.length() - 1) {
            entry.suffix = node->name.mid(dotIdx + 1);
        }
    }

    QLocale loc;
    entry.sizeText = entry.isDirectory ? QString() : loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
    
    static const QStringList imageSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp"),
        QStringLiteral("ico")
    };
    entry.isImage = !entry.isDirectory && imageSuffixes.contains(entry.suffix.toLower());
    entry.hasThumbnail = entry.isImage;

    return entry;
}

void ArchiveFileProvider::scan(const QString &path)
{
    m_abort = false;
    m_isRunning = true;
    m_currentPath = path;
    m_generation++;
    
    emit started();

    auto ctx = parsePath(path);
    if (!ctx) {
        m_isRunning = false;
        emit finished(path, false, m_generation, QStringLiteral("Invalid path format"));
        return;
    }

    QtConcurrent::run([this, path, ctx = *ctx, generation = m_generation]() {
        if (!loadArchive(ctx.archiveDiskPath)) {
            QMetaObject::invokeMethod(this, [this, path, generation]() {
                m_isRunning = false;
                emit finished(path, false, generation, QStringLiteral("Could not load archive"));
            });
            return;
        }

        ArchiveNode* node = findNode(ctx.internalPath);
        if (!node || !node->isDirectory) {
            QMetaObject::invokeMethod(this, [this, path, generation]() {
                m_isRunning = false;
                emit finished(path, false, generation, QStringLiteral("Path not found or not a directory"));
            });
            return;
        }

        QList<FileEntry> entries;
        for (const auto &childPair : node->children) {
            if (m_abort) break;
            ArchiveNode* child = childPair.second.get();
            if (!m_showHidden && child->name.startsWith(QLatin1Char('.'))) {
                continue;
            }
            entries.append(entryFromNode(ctx, child));
        }

        QMetaObject::invokeMethod(this, [this, path, generation, entries]() {
            if (m_abort) {
                m_isRunning = false;
                emit finished(path, false, generation, QStringLiteral("Aborted"));
                return;
            }
            emit batchReady(entries, generation);
            m_isRunning = false;
            emit finished(path, true, generation, {});
        });
    });
}

void ArchiveFileProvider::cancel()
{
    m_abort = true;
}

void ArchiveFileProvider::setShowHidden(bool show)
{
    m_showHidden = show;
}

bool ArchiveFileProvider::isRunning() const
{
    return m_isRunning;
}

QString ArchiveFileProvider::currentPath() const
{
    return m_currentPath;
}

int ArchiveFileProvider::currentGeneration() const
{
    return m_generation;
}

bool ArchiveFileProvider::pathExists(const QString &path) const
{
    auto ctx = parsePath(path);
    if (!ctx) return false;
    
    auto* self = const_cast<ArchiveFileProvider*>(this);
    if (!self->loadArchive(ctx->archiveDiskPath)) return false;
    
    return findNode(ctx->internalPath) != nullptr;
}

bool ArchiveFileProvider::isDirectory(const QString &path) const
{
    auto ctx = parsePath(path);
    if (!ctx) return false;
    
    auto* self = const_cast<ArchiveFileProvider*>(this);
    if (!self->loadArchive(ctx->archiveDiskPath)) return false;
    
    ArchiveNode* node = findNode(ctx->internalPath);
    return node ? node->isDirectory : false;
}

bool ArchiveFileProvider::isSymLink(const QString &path) const
{
    Q_UNUSED(path);
    return false;
}

QString ArchiveFileProvider::normalizedPath(const QString &path) const
{
    auto ctx = parsePath(path);
    if (!ctx) return path;
    return SCHEME + QDir::cleanPath(ctx->archiveDiskPath) + DELIMITER + ctx->internalPath;
}

QString ArchiveFileProvider::fileName(const QString &path) const
{
    auto ctx = parsePath(path);
    if (!ctx) return {};
    
    if (ctx->internalPath == QStringLiteral("/")) {
        return QFileInfo(ctx->archiveDiskPath).fileName();
    }
    
    return ctx->internalPath.mid(ctx->internalPath.lastIndexOf(QLatin1Char('/')) + 1);
}

QString ArchiveFileProvider::absolutePath(const QString &path) const
{
    return normalizedPath(path);
}

QString ArchiveFileProvider::parentPath(const QString &path) const
{
    auto ctx = parsePath(path);
    if (!ctx) return {};
    
    if (ctx->internalPath == QStringLiteral("/")) {
        return QFileInfo(ctx->archiveDiskPath).absolutePath();
    }
    
    int lastSlash = ctx->internalPath.lastIndexOf(QLatin1Char('/'));
    QString parentInternal = (lastSlash > 0) ? ctx->internalPath.left(lastSlash) : QStringLiteral("/");
    
    return SCHEME + ctx->archiveDiskPath + DELIMITER + parentInternal;
}

QString ArchiveFileProvider::childPath(const QString &parentPath, const QString &name) const
{
    auto ctx = parsePath(parentPath);
    if (!ctx) return {};
    
    QString childInternal = ctx->internalPath;
    if (!childInternal.endsWith(QLatin1Char('/'))) {
        childInternal += QLatin1Char('/');
    }
    childInternal += name;
    
    return SCHEME + ctx->archiveDiskPath + DELIMITER + childInternal;
}

std::optional<FileEntry> ArchiveFileProvider::entryInfo(const QString &path) const
{
    auto ctx = parsePath(path);
    if (!ctx) return std::nullopt;
    
    auto* self = const_cast<ArchiveFileProvider*>(this);
    if (!self->loadArchive(ctx->archiveDiskPath)) return std::nullopt;
    
    ArchiveNode* node = findNode(ctx->internalPath);
    if (!node) return std::nullopt;
    
    return entryFromNode(*ctx, node);
}

bool ArchiveFileProvider::ensureParentDirectory(const QString &path) const
{
    return false;
}

bool ArchiveFileProvider::makePath(const QString &path) const
{
    return false;
}

bool ArchiveFileProvider::removePath(const QString &path) const
{
    return false;
}

QStringList ArchiveFileProvider::childPaths(const QString &path, bool includeHidden) const
{
    auto ctx = parsePath(path);
    if (!ctx) return {};
    
    auto* self = const_cast<ArchiveFileProvider*>(this);
    if (!self->loadArchive(ctx->archiveDiskPath)) return {};
    
    ArchiveNode* node = findNode(ctx->internalPath);
    if (!node || !node->isDirectory) return {};
    
    QStringList result;
    for (const auto &childPair : node->children) {
        if (!includeHidden && childPair.second->name.startsWith(QLatin1Char('.'))) continue;
        result.append(SCHEME + ctx->archiveDiskPath + DELIMITER + childPair.second->path);
    }
    return result;
}

bool ArchiveFileProvider::movePath(const QString &sourcePath, const QString &destinationPath) const
{
    return false;
}

class ArchiveBuffer : public QBuffer {
public:
    ArchiveBuffer(std::vector<bit7z::byte_t>&& data)
        : m_data(std::move(data)) {
        setData(reinterpret_cast<const char*>(m_data.data()), static_cast<int>(m_data.size()));
    }
private:
    std::vector<bit7z::byte_t> m_data;
};

std::unique_ptr<QIODevice> ArchiveFileProvider::openRead(const QString &path) const
{
    auto ctx = parsePath(path);
    if (!ctx) return nullptr;
    
    auto* self = const_cast<ArchiveFileProvider*>(this);
    if (!self->loadArchive(ctx->archiveDiskPath)) return nullptr;
    
    ArchiveNode* node = findNode(ctx->internalPath);
    if (!node || node->isDirectory || node->itemIndex == static_cast<uint32_t>(-1)) {
        return nullptr;
    }
    
    try {
        if (node->size < 50 * 1024 * 1024) {
            std::vector<bit7z::byte_t> buffer;
            m_reader->extractTo(buffer, node->itemIndex);
            auto bufDev = std::make_unique<ArchiveBuffer>(std::move(buffer));
            bufDev->open(QIODevice::ReadOnly);
            return bufDev;
        } else {
            auto tempFile = std::make_unique<QTemporaryFile>();
            if (tempFile->open()) {
                QString tempPath = tempFile->fileName();
                tempFile->close();
                m_reader->extractTo(toTString(tempPath), std::vector<uint32_t>{node->itemIndex});
                if (tempFile->open()) {
                    return tempFile;
                }
            }
        }
    } catch (const bit7z::BitException& ex) {
        qWarning() << "Extraction failed:" << ex.what();
    }
    return nullptr;
}

std::unique_ptr<QIODevice> ArchiveFileProvider::openWrite(const QString &path, bool truncate) const
{
    return nullptr;
}

bool ArchiveFileProvider::renamePath(const QString &oldPath, const QString &newName)
{
    return false;
}

bool ArchiveFileProvider::createFolder(const QString &parentPath, const QString &name, QString *createdPath)
{
    return false;
}

bool ArchiveFileProvider::createFile(const QString &parentPath, const QString &name, QString *createdPath)
{
    return false;
}
