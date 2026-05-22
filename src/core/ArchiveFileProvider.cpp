#include "ArchiveFileProvider.h"

#include "ArchiveSupport.h"
#include "OperationQueue.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <mutex>

#ifdef HAS_UNOFFICIAL_BIT7Z
#include <bit7z/bit7z.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitformat.hpp>
#endif

namespace {
class TemporaryFileDevice : public QFile {
public:
    explicit TemporaryFileDevice(const QString &fileName, QObject *parent = nullptr)
        : QFile(fileName, parent)
    {
    }

    ~TemporaryFileDevice() override
    {
        close();
        if (!fileName().isEmpty()) {
            QFile::remove(fileName());
        }
    }
};

#ifdef HAS_UNOFFICIAL_BIT7Z
std::shared_ptr<bit7z::Bit7zLibrary> getGlobalLibrary()
{
    static std::mutex s_mutex;
    static std::shared_ptr<bit7z::Bit7zLibrary> s_library;
    std::lock_guard<std::mutex> lock(s_mutex);
    if (!s_library) {
        try {
            s_library = std::make_shared<bit7z::Bit7zLibrary>();
        } catch (const std::exception &) {
            try {
                const QString libraryPath = ArchiveSupport::archiveLibraryPath();
                if (!libraryPath.isEmpty()) {
                    s_library = std::make_shared<bit7z::Bit7zLibrary>(libraryPath.toStdString());
                }
            } catch (const std::exception &) {
                // Ignore failure
            }
        }
    }
    return s_library;
}
#endif

bool isHiddenName(const QString &name)
{
    return name.startsWith(QLatin1Char('.'));
}

QString archiveTokenPath(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    const QString stripped = ArchiveSupport::stripArchiveScheme(path);
    return stripped;
}

QString archiveRelativeToken(const QString &token)
{
    QString out = QDir::fromNativeSeparators(token.trimmed());
    if (out == QLatin1String("/")) {
        return {};
    }
    if (out.startsWith(QLatin1Char('/'))) {
        out.remove(0, 1);
    }
    while (out.endsWith(QLatin1Char('/'))) {
        out.chop(1);
    }
    return out;
}

QString archiveParentOfRelative(const QString &path)
{
    const QString rel = archiveRelativeToken(path);
    if (rel.isEmpty()) {
        return {};
    }
    const int slash = rel.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return {};
    }
    return rel.left(slash);
}

QString archiveSuffixFromName(const QString &name)
{
    const QString lower = name.toLower();
    if (lower.endsWith(QStringLiteral(".tar.gz"))) {
        return QStringLiteral("tar.gz");
    }
    if (lower.endsWith(QStringLiteral(".tar.bz2"))) {
        return QStringLiteral("tar.bz2");
    }
    if (lower.endsWith(QStringLiteral(".tar.xz"))) {
        return QStringLiteral("tar.xz");
    }
    if (lower.endsWith(QStringLiteral(".tar.zst"))) {
        return QStringLiteral("tar.zst");
    }
    return QFileInfo(name).suffix().toLower();
}

#ifdef HAS_UNOFFICIAL_BIT7Z
QString toQString(const bit7z::tstring &value)
{
    return QString::fromUtf8(value.c_str());
}

const bit7z::BitInFormat &archiveFormatForSuffix(const QString &suffix)
{
    const QString lower = suffix.toLower();
    if (lower == QLatin1String("7z")) {
        return bit7z::BitFormat::SevenZip;
    }
    if (lower == QLatin1String("rar") || lower == QLatin1String("rev")) {
        return bit7z::BitFormat::Rar;
    }
    if (lower == QLatin1String("rar5")) {
        return bit7z::BitFormat::Rar5;
    }
    if (lower == QLatin1String("cab")) {
        return bit7z::BitFormat::Cab;
    }
    if (lower == QLatin1String("tar")) {
        return bit7z::BitFormat::Tar;
    }
    if (lower == QLatin1String("gz") || lower == QLatin1String("tgz")) {
        return bit7z::BitFormat::GZip;
    }
    if (lower == QLatin1String("bz2") || lower == QLatin1String("tbz2")) {
        return bit7z::BitFormat::BZip2;
    }
    if (lower == QLatin1String("xz") || lower == QLatin1String("txz")) {
        return bit7z::BitFormat::Xz;
    }
    return bit7z::BitFormat::Zip;
}

QStringList archiveFormatCandidatesForSuffix(const QString &suffix)
{
    const QString lower = suffix.toLower();
    if (lower == QLatin1String("rar")) {
        return {QStringLiteral("rar"), QStringLiteral("rar5")};
    }
    if (lower == QLatin1String("rar5")) {
        return {QStringLiteral("rar5")};
    }
    return {lower};
}
#endif
}

ArchiveFileProvider::ArchiveFileProvider(QObject *parent)
    : FileProvider(parent)
{
}

ArchiveFileProvider::~ArchiveFileProvider()
{
    cancel();
}

QString ArchiveFileProvider::scheme() const
{
    return QStringLiteral("archive");
}

bool ArchiveFileProvider::canHandle(const QString &path) const
{
    return ArchiveSupport::isArchivePath(path) || ArchiveSupport::isArchiveFilePath(path);
}

FileProvider::Capabilities ArchiveFileProvider::capabilities() const
{
    return Browse | ReadMetadata | Transfer;
}

void ArchiveFileProvider::scan(const QString &path)
{
    m_currentPath = normalizedPath(path);
    m_running = true;
    ++m_generation;
    emit started();

    m_state = buildState(m_currentPath);
    if (!m_state.valid) {
        m_running = false;
        emit finished(m_currentPath, false, m_generation, m_state.error);
        return;
    }

    QList<FileEntry> entries;
    entries.reserve(m_state.items.size());
    for (const ArchiveItemRecord &record : std::as_const(m_state.items)) {
        if (record.relativePath == m_state.browsePath) {
            continue;
        }
        const QString parent = parentRelativePath(record.relativePath);
        if (parent != m_state.browsePath) {
            continue;
        }
        if (!m_showHidden && record.isHidden) {
            continue;
        }
        entries.append(fileEntryFromRecord(m_state, record));
    }

    std::sort(entries.begin(), entries.end(), [](const FileEntry &lhs, const FileEntry &rhs) {
        if (lhs.isDirectory != rhs.isDirectory) {
            return lhs.isDirectory;
        }
        return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
    });

    emit batchReady(entries, m_generation);
    m_running = false;
    emit finished(m_currentPath, true, m_generation, {});
}

void ArchiveFileProvider::cancel()
{
    m_running = false;
}

void ArchiveFileProvider::setShowHidden(bool show)
{
    m_showHidden = show;
}

bool ArchiveFileProvider::isRunning() const
{
    return m_running;
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
    const ArchiveState &state = buildState(path);
    if (!state.valid) {
        return false;
    }
    const QString rel = archiveRelativeToken(state.browsePath);
    if (rel.isEmpty()) {
        return true;
    }
    return state.pathIndex.contains(rel) || state.directories.contains(rel);
}

bool ArchiveFileProvider::isDirectory(const QString &path) const
{
    const ArchiveState &state = buildState(path);
    if (!state.valid) {
        return false;
    }
    const QString rel = archiveRelativeToken(state.browsePath);
    if (rel.isEmpty()) {
        return true;
    }
    return state.directories.contains(rel);
}

bool ArchiveFileProvider::isSymLink(const QString &path) const
{
    const auto info = entryInfo(path);
    return info ? info->isSystem : false;
}

QString ArchiveFileProvider::normalizedPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::normalizeArchivePath(path);
    }
    if (ArchiveSupport::isArchiveFilePath(path)) {
        return ArchiveSupport::archiveRootPath(path);
    }
    return ArchiveSupport::normalizeArchivePath(path);
}

QString ArchiveFileProvider::fileName(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveFileName(path);
    }
    return QFileInfo(path).fileName();
}

QString ArchiveFileProvider::absolutePath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::normalizeArchivePath(path);
    }
    return QFileInfo(path).absoluteFilePath();
}

QString ArchiveFileProvider::parentPath(const QString &path) const
{
    if (ArchiveSupport::isArchivePath(path)) {
        return ArchiveSupport::archiveParentPath(path);
    }
    return QFileInfo(path).absoluteDir().absolutePath();
}

QString ArchiveFileProvider::childPath(const QString &parentPath, const QString &name) const
{
    if (ArchiveSupport::isArchivePath(parentPath)) {
        return ArchiveSupport::archiveChildPath(parentPath, name);
    }
    return QDir(parentPath).filePath(name);
}

std::optional<FileEntry> ArchiveFileProvider::entryInfo(const QString &path) const
{
    const ArchiveState &state = buildState(path);
    if (!state.valid) {
        return std::nullopt;
    }

    const QString rel = archiveRelativeToken(state.browsePath);
    if (rel.isEmpty()) {
        FileEntry entry;
        entry.name = ArchiveSupport::archiveFileName(path);
        entry.path = state.currentPath;
        entry.suffix = QFileInfo(state.sourcePath).suffix().toLower();
        entry.isDirectory = true;
        entry.sizeText = QStringLiteral("Folder");
        entry.modifiedText = {};
        entry.createdText = {};
        entry.attributesText = QStringLiteral("D");
        return entry;
    }

    const int absoluteIdx = state.pathIndex.value(rel, -1);
    if (absoluteIdx < 0 || absoluteIdx >= state.items.size()) {
        return std::nullopt;
    }
    return fileEntryFromRecord(state, state.items.at(absoluteIdx));
}

bool ArchiveFileProvider::ensureParentDirectory(const QString &path) const
{
    Q_UNUSED(path)
    return false;
}

bool ArchiveFileProvider::makePath(const QString &path) const
{
    Q_UNUSED(path)
    return false;
}

bool ArchiveFileProvider::removePath(const QString &path) const
{
    Q_UNUSED(path)
    return false;
}

QStringList ArchiveFileProvider::childPaths(const QString &path, bool includeHidden) const
{
    const ArchiveState &state = buildState(path);
    if (!state.valid) {
        return {};
    }

    const QString browse = archiveRelativeToken(state.browsePath);
    QStringList result;
    for (const ArchiveItemRecord &record : state.items) {
        if (record.relativePath == browse) {
            continue;
        }
        const QString parent = parentRelativePath(record.relativePath);
        if (parent != browse) {
            continue;
        }
        if (!includeHidden && record.isHidden) {
            continue;
        }
        result.append(record.absolutePath);
    }
    return result;
}

bool ArchiveFileProvider::movePath(const QString &sourcePath, const QString &destinationPath) const
{
    Q_UNUSED(sourcePath)
    Q_UNUSED(destinationPath)
    return false;
}

std::unique_ptr<QIODevice> ArchiveFileProvider::openRead(const QString &path) const
{
    const ArchiveState &state = buildState(path);
    if (!state.valid) {
        return {};
    }

    const QString rel = archiveRelativeToken(state.browsePath);
    const int idx = state.pathIndex.value(rel, -1);
    if (idx < 0 || idx >= state.items.size()) {
        return {};
    }

    const ArchiveItemRecord &record = state.items.at(idx);
    if (record.isDirectory) {
        return {};
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    try {
        auto tempFile = std::make_unique<QTemporaryFile>();
        if (!tempFile->open()) {
            return {};
        }
        QString tempPath = tempFile->fileName();
        tempFile->setAutoRemove(false);
        tempFile->close();

        {
#ifdef Q_OS_WIN
            std::ofstream outFile(tempPath.toStdWString(), std::ios::binary);
#else
            std::ofstream outFile(tempPath.toStdString(), std::ios::binary);
#endif
            if (!outFile.is_open()) {
                QFile::remove(tempPath);
                return {};
            }

            state.reader->setProgressCallback([](uint64_t) -> bool {
                return !OperationQueue::isCurrentThreadAborted();
            });
            state.reader->extractTo(outFile, record.index);
            state.reader->setProgressCallback(nullptr);
        }

        auto device = std::make_unique<TemporaryFileDevice>(tempPath);
        if (!device->open(QIODevice::ReadOnly)) {
            return {};
        }
        return device;
    } catch (const std::exception &) {
        return {};
    }
#else
    Q_UNUSED(record)
    return {};
#endif
}

std::unique_ptr<QIODevice> ArchiveFileProvider::openWrite(const QString &path, bool truncate) const
{
    Q_UNUSED(path)
    Q_UNUSED(truncate)
    return {};
}

bool ArchiveFileProvider::renamePath(const QString &oldPath, const QString &newName)
{
    Q_UNUSED(oldPath)
    Q_UNUSED(newName)
    return false;
}

bool ArchiveFileProvider::createFolder(const QString &parentPath, const QString &name, QString *createdPath)
{
    Q_UNUSED(parentPath)
    Q_UNUSED(name)
    Q_UNUSED(createdPath)
    return false;
}

bool ArchiveFileProvider::createFile(const QString &parentPath, const QString &name, QString *createdPath)
{
    Q_UNUSED(parentPath)
    Q_UNUSED(name)
    Q_UNUSED(createdPath)
    return false;
}

bool ArchiveFileProvider::ensureLibrary() const
{
#ifdef HAS_UNOFFICIAL_BIT7Z
    if (m_library) {
        return true;
    }
    m_library = getGlobalLibrary();
    return m_library != nullptr;
#else
    return false;
#endif
}

QString ArchiveFileProvider::toArchiveToken(const QString &path)
{
    if (ArchiveSupport::isArchivePath(path)) {
        return path;
    }
    if (ArchiveSupport::isArchiveFilePath(path)) {
        return ArchiveSupport::archiveRootPath(path);
    }
    return {};
}

QString ArchiveFileProvider::normalizeRelativePath(QString path)
{
    path = QDir::fromNativeSeparators(path.trimmed());
    if (path == QLatin1String("/")) {
        return {};
    }
    if (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    return path;
}

QString ArchiveFileProvider::parentRelativePath(const QString &path)
{
    const QString normalized = normalizeRelativePath(path);
    if (normalized.isEmpty()) {
        return {};
    }
    const int slash = normalized.lastIndexOf(QLatin1Char('/'));
    if (slash < 0) {
        return {};
    }
    return normalized.left(slash);
}

QString ArchiveFileProvider::joinRelativePath(const QString &parent, const QString &child)
{
    const QString normalizedParent = normalizeRelativePath(parent);
    const QString normalizedChild = normalizeRelativePath(child);
    if (normalizedParent.isEmpty()) {
        return normalizedChild;
    }
    if (normalizedChild.isEmpty()) {
        return normalizedParent;
    }
    return normalizedParent + QLatin1Char('/') + normalizedChild;
}

bool ArchiveFileProvider::isArchiveLike(const QString &suffix)
{
    return ArchiveSupport::isArchiveExtension(suffix);
}

std::string ArchiveFileProvider::toBit7zString(const QString &path)
{
    return path.toStdString();
}

QDateTime ArchiveFileProvider::toDateTime(const std::chrono::time_point<std::chrono::system_clock> &timePoint)
{
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(timePoint.time_since_epoch()).count();
    return QDateTime::fromSecsSinceEpoch(static_cast<qint64>(secs));
}

QString ArchiveFileProvider::itemAbsolutePath(const QString &archivePrefix, const QString &relativePath)
{
    if (relativePath.isEmpty()) {
        return archivePrefix;
    }
    // archivePrefix is expected to end with '|' (e.g., "archive://C:/a.zip|")
    // or "|/" for root. We want to ensure that for items inside the archive,
    // we use '|/' as the base and then the relative path.
    QString base = archivePrefix;
    if (base.endsWith(QLatin1Char('|'))) {
        base.append(QLatin1Char('/'));
    }
    return base + relativePath;
}

FileEntry ArchiveFileProvider::fileEntryFromRecord(const ArchiveState &state, const ArchiveItemRecord &record)
{
    FileEntry entry;
    entry.name = record.name;
    entry.path = record.absolutePath;
    entry.suffix = record.suffix;
    entry.size = record.size;
    entry.modified = record.modified;
    entry.created = record.created;
    entry.isDirectory = record.isDirectory;
    entry.isHidden = record.isHidden;
    entry.isReadOnly = false;
    entry.isSystem = record.isSymLink;

    QLocale loc;
    entry.sizeText = entry.isDirectory ? QString() : loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
    entry.modifiedText = entry.modified.isValid() ? loc.toString(entry.modified, QLocale::ShortFormat) : QString();
    entry.createdText = entry.created.isValid() ? loc.toString(entry.created, QLocale::ShortFormat) : QString();

    QString attrs;
    if (entry.isDirectory) attrs += QLatin1Char('D');
    if (entry.isHidden) attrs += QLatin1Char('H');
    if (entry.isReadOnly) attrs += QLatin1Char('R');
    if (entry.isSystem) attrs += QLatin1Char('L');
    entry.attributesText = attrs;
    entry.isImage = false;
    entry.hasThumbnail = false;
    Q_UNUSED(state)
    return entry;
}

QString ArchiveFileProvider::currentBrowsePathFromPath(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return {};
    }
    const QStringList tokens = archiveTokenPath(path).split(QLatin1Char('|'), Qt::KeepEmptyParts);
    if (tokens.isEmpty()) {
        return {};
    }
    return tokens.last();
}

ArchiveFileProvider::ArchiveState ArchiveFileProvider::buildState(const QString &path) const
{
    QString normalized = normalizedPath(path);
    int lastPipe = normalized.lastIndexOf(QLatin1Char('|'));
    QString containerPart = (lastPipe != -1) ? normalized.left(lastPipe) : normalized;

    int lastPipeState = m_state.currentPath.lastIndexOf(QLatin1Char('|'));
    QString containerPartState = (lastPipeState != -1) ? m_state.currentPath.left(lastPipeState) : m_state.currentPath;

    if (m_state.valid && !containerPart.isEmpty() && containerPart == containerPartState) {
        QString working = normalized;
        if (ArchiveSupport::isArchiveFilePath(working)) {
            working = ArchiveSupport::archiveRootPath(working);
        }
        const QStringList tokens = archiveTokenPath(working).split(QLatin1Char('|'), Qt::KeepEmptyParts);
        if (!tokens.isEmpty()) {
            m_state.currentPath = normalized;
            m_state.browsePath = normalizeRelativePath(tokens.last());
            if (tokens.last() == QLatin1String("/")) {
                m_state.browsePath.clear();
            }
            return std::move(m_state);
        }
    }

    ArchiveState state;
    state.currentPath = normalized;
    if (state.currentPath.isEmpty()) {
        state.error = QStringLiteral("Invalid archive path");
        m_state = std::move(state);
        return std::move(m_state);
    }

    QString working = state.currentPath;
    if (ArchiveSupport::isArchiveFilePath(working)) {
        working = ArchiveSupport::archiveRootPath(working);
    }
    if (!ArchiveSupport::isArchivePath(working)) {
        state.error = QStringLiteral("Path is not an archive");
        m_state = std::move(state);
        return std::move(m_state);
    }

    const QStringList tokens = archiveTokenPath(working).split(QLatin1Char('|'), Qt::KeepEmptyParts);
    if (tokens.isEmpty()) {
        state.error = QStringLiteral("Archive path is empty");
        m_state = std::move(state);
        return std::move(m_state);
    }

    const QString sourcePath = tokens.first();
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        state.error = QStringLiteral("Archive file was not found");
        m_state = std::move(state);
        return std::move(m_state);
    }

    if (!ensureLibrary()) {
        state.error = QStringLiteral("bit7z backend was not found or could not be loaded");
        m_state = std::move(state);
        return std::move(m_state);
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    try {
        const QStringList chain = tokens.mid(1, qMax(0, tokens.size() - 2));
        const QString browsePathToken = tokens.last();

        std::unique_ptr<bit7z::BitArchiveReader> reader;
        std::unique_ptr<QTemporaryFile> currentTempFile;

        auto openReaderFromFile = [&](const QString &archivePath, const QString &formatSuffix) -> std::unique_ptr<bit7z::BitArchiveReader> {
            const QStringList candidates = archiveFormatCandidatesForSuffix(formatSuffix);
            for (const QString &candidate : candidates) {
                try {
                    const auto &format = archiveFormatForSuffix(candidate);
                    return std::make_unique<bit7z::BitArchiveReader>(*m_library, toBit7zString(archivePath), format);
                } catch (const std::exception &) {
                    continue;
                }
            }
            return {};
        };

        reader = openReaderFromFile(sourcePath, QFileInfo(sourcePath).suffix().toLower());
        if (!reader) {
            state.error = QStringLiteral("Unsupported archive format");
            m_state = std::move(state);
            return std::move(m_state);
        }

        for (const QString &segment : chain) {
            const QString rel = normalizeRelativePath(segment);
            bool found = false;
            std::vector<bit7z::BitArchiveItemInfo> items = reader->items();
            for (const auto &item : items) {
                const QString itemRel = normalizeRelativePath(toQString(item.path()));
                if (itemRel != rel) {
                    continue;
                }
                if (!isArchiveLike(QFileInfo(itemRel).suffix().toLower())) {
                    state.error = QStringLiteral("Nested archive item is not an archive");
                    m_state = std::move(state);
                    return std::move(m_state);
                }

                auto nextTempFile = std::make_unique<QTemporaryFile>();
                if (!nextTempFile->open()) {
                    state.error = QStringLiteral("Could not create temporary file for nested archive");
                    m_state = std::move(state);
                    return std::move(m_state);
                }
                QString tempPath = nextTempFile->fileName();
                nextTempFile->close();

                {
#ifdef Q_OS_WIN
                    std::ofstream outFile(tempPath.toStdWString(), std::ios::binary);
#else
                    std::ofstream outFile(tempPath.toStdString(), std::ios::binary);
#endif
                    if (!outFile.is_open()) {
                        state.error = QStringLiteral("Could not open temporary file stream for nested archive");
                        m_state = std::move(state);
                        return std::move(m_state);
                    }

                    bool wasRunning = m_running;
                    reader->setProgressCallback([this, wasRunning](uint64_t) -> bool {
                        if (wasRunning && !m_running) {
                            return false;
                        }
                        return !OperationQueue::isCurrentThreadAborted();
                    });
                    reader->extractTo(outFile, item.index());
                    reader->setProgressCallback(nullptr);
                }

                const QString itemSuffix = QFileInfo(itemRel).suffix().toLower();
                const QStringList candidates = archiveFormatCandidatesForSuffix(itemSuffix);
                std::unique_ptr<bit7z::BitArchiveReader> nestedReader;
                for (const QString &candidate : candidates) {
                    try {
                        const auto &format = archiveFormatForSuffix(candidate);
                        nestedReader = std::make_unique<bit7z::BitArchiveReader>(*m_library, toBit7zString(tempPath), format);
                        break;
                    } catch (const std::exception &) {
                        continue;
                    }
                }
                if (!nestedReader) {
                    state.error = QStringLiteral("Nested archive format is not supported");
                    m_state = std::move(state);
                    return std::move(m_state);
                }
                reader = std::move(nestedReader);
                currentTempFile = std::move(nextTempFile);
                found = true;
                break;
            }
            if (!found) {
                state.error = QStringLiteral("Nested archive entry was not found");
                m_state = std::move(state);
                return std::move(m_state);
            }
        }

        state.valid = true;
        state.sourcePath = sourcePath;
        state.browsePath = normalizeRelativePath(browsePathToken);
        if (browsePathToken == QLatin1String("/")) {
            state.browsePath.clear();
        }

        // Correctly build prefix for nested archives
        QStringList prefixParts;
        prefixParts << sourcePath;
        for (const QString &segment : chain) {
            prefixParts << normalizeRelativePath(segment);
        }
        state.archivePrefix = QStringLiteral("archive://") + prefixParts.join(QLatin1Char('|')) + QLatin1Char('|');

        state.reader = std::move(reader);
        state.tempFile = std::move(currentTempFile);

        const std::vector<bit7z::BitArchiveItemInfo> items = state.reader->items();
        state.items.reserve(static_cast<int>(items.size()));

        for (const auto &item : items) {
            ArchiveItemRecord record;
            record.relativePath = normalizeRelativePath(toQString(item.path()));
            record.name = toQString(item.name());
            record.suffix = archiveSuffixFromName(record.name);
            record.size = static_cast<qint64>(item.size());
            record.modified = toDateTime(item.lastWriteTime());
            record.created = toDateTime(item.creationTime());
            record.isDirectory = item.isDir();
            record.isHidden = isHiddenName(record.name);
            record.isSymLink = item.isSymLink();
            record.isArchive = isArchiveLike(record.suffix);
            record.index = item.index();
            record.absolutePath = itemAbsolutePath(state.archivePrefix, record.relativePath);
            state.pathIndex.insert(record.relativePath, state.items.size());
            state.items.append(record);

            QString parent = parentRelativePath(record.relativePath);
            while (!parent.isEmpty()) {
                state.directories.insert(parent);
                const int slash = parent.lastIndexOf(QLatin1Char('/'));
                if (slash < 0) {
                    break;
                }
                parent = parent.left(slash);
            }
            if (!record.relativePath.isEmpty()) {
                state.directories.insert(parentRelativePath(record.relativePath));
            }
        }

        state.directories.insert(QString());
        m_state = std::move(state);
        return std::move(m_state);
    } catch (const std::exception &ex) {
        state.error = QString::fromUtf8(ex.what());
        m_state = std::move(state);
        return std::move(m_state);
    }
#else
    state.error = QStringLiteral("bit7z support is disabled");
    m_state = std::move(state);
    return std::move(m_state);
#endif
}
