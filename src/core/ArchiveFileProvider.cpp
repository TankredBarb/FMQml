#include "ArchiveFileProvider.h"

#include "ArchiveSupport.h"
#include "OperationQueue.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMimeDatabase>
#include <QMetaObject>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtConcurrent>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <mutex>
#include <vector>

#ifdef HAS_UNOFFICIAL_BIT7Z
#include <bit7z/bit7z.hpp>
#include <bit7z/bitarchivereader.hpp>
#include <bit7z/bitexception.hpp>
#include <bit7z/bitformat.hpp>
#endif

namespace {
constexpr qsizetype kMaxCachedArchiveStates = 8;
constexpr qsizetype kMaxCachedArchiveItems = 250000;

thread_local QString g_currentThreadTemporaryParentPath;

class TemporaryFileDevice : public QFile {
public:
    explicit TemporaryFileDevice(const QString &fileName, QString cleanupRoot = {}, QObject *parent = nullptr)
        : QFile(fileName, parent)
        , m_cleanupRoot(std::move(cleanupRoot))
    {
    }

    ~TemporaryFileDevice() override
    {
        close();
        if (!m_cleanupRoot.isEmpty()) {
            QDir(m_cleanupRoot).removeRecursively();
        } else if (!fileName().isEmpty()) {
            QFile::remove(fileName());
        }
    }

private:
    QString m_cleanupRoot;
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

QMutex &archiveReaderMutex()
{
    static QMutex mutex;
    return mutex;
}

QString extractedArchiveItemPath(const QString &rootPath, const QString &relativePath, const QString &itemName)
{
    const QString directPath = QDir(rootPath).filePath(QDir::fromNativeSeparators(relativePath));
    if (QFileInfo::exists(directPath) && QFileInfo(directPath).isFile()) {
        return directPath;
    }

    const QString fileName = QFileInfo(itemName).fileName();
    if (fileName.isEmpty()) {
        return {};
    }

    QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString candidate = it.next();
        if (QFileInfo(candidate).fileName() == fileName) {
            return candidate;
        }
    }
    return {};
}

QStringList sampledExtractedFiles(const QString &rootPath, int limit = 12)
{
    QStringList files;
    QDirIterator it(rootPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext() && files.size() < limit) {
        files.append(QDir(rootPath).relativeFilePath(it.next()));
    }
    return files;
}

QString sevenZipExecutablePath()
{
    return ArchiveSupport::sevenZipExecutablePath();
}

bool extractArchiveWithSevenZip(const QString &archivePath,
                                const QString &destinationPath,
                                const std::function<bool(uint64_t)> &progressCallback,
                                QString *error,
                                const QStringList &itemPaths = {})
{
    const QString executable = sevenZipExecutablePath();
    if (executable.isEmpty()) {
        return false;
    }

    QProcess process;
    process.setProgram(executable);
    QStringList arguments = {
        QStringLiteral("x"),
        QStringLiteral("-y"),
        QStringLiteral("-aos"),
        QStringLiteral("-bso0"),
        QStringLiteral("-bsp1"),
        QStringLiteral("-bse1"),
        QStringLiteral("-o%1").arg(QDir::toNativeSeparators(destinationPath)),
        QDir::toNativeSeparators(archivePath),
    };
    for (const QString &itemPath : itemPaths) {
        arguments.append(QDir::toNativeSeparators(itemPath));
    }
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForStarted(5000)) {
        if (error) {
            *error = QStringLiteral("Could not start 7-Zip: %1").arg(process.errorString());
        }
        return false;
    }

    QByteArray outputBuffer;
    int lastPercent = -1;
    QElapsedTimer progressTimer;
    progressTimer.start();
    const uint64_t archiveSize = static_cast<uint64_t>((std::max<qint64>)(1, QFileInfo(archivePath).size()));
    const QRegularExpression percentPattern(QStringLiteral("(\\d{1,3})%"));

    auto consumeProcessOutput = [&]() -> bool {
        outputBuffer.append(process.readAll());
        if (outputBuffer.size() > 4096) {
            outputBuffer = outputBuffer.right(4096);
        }

        const QString text = QString::fromLocal8Bit(outputBuffer);
        QRegularExpressionMatchIterator matches = percentPattern.globalMatch(text);
        int percent = -1;
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            if (ok) {
                percent = std::clamp(value, 0, 100);
            }
        }

        if (percent >= 0 && percent != lastPercent && progressTimer.elapsed() >= 120) {
            lastPercent = percent;
            progressTimer.restart();
            if (progressCallback) {
                const uint64_t processed = (archiveSize * static_cast<uint64_t>(percent)) / 100U;
                if (!progressCallback(processed)) {
                    process.kill();
                    process.waitForFinished(3000);
                    if (error) {
                        *error = QStringLiteral("Archive extraction was cancelled");
                    }
                    return false;
                }
            }
        }
        return true;
    };

    while (!process.waitForFinished(100)) {
        if (!consumeProcessOutput()) {
            return false;
        }
        if (OperationQueue::isCurrentThreadAborted()) {
            process.kill();
            process.waitForFinished(3000);
            if (error) {
                *error = QStringLiteral("Archive extraction was cancelled");
            }
            return false;
        }
    }
    if (!consumeProcessOutput()) {
        return false;
    }
    if (progressCallback) {
        progressCallback(archiveSize);
    }

    const int exitCode = process.exitCode();
    if (process.exitStatus() == QProcess::NormalExit && (exitCode == 0 || exitCode == 1)) {
        return true;
    }

    if (error) {
        const QString output = QString::fromLocal8Bit(outputBuffer).trimmed();
        *error = output.isEmpty()
            ? QStringLiteral("7-Zip failed with exit code %1").arg(exitCode)
            : output.left(1000);
    }
    return false;
}

bool moveExtractedPath(const QString &sourcePath, const QString &destinationPath)
{
    if (QFileInfo(sourcePath).isDir()) {
        return QDir().rename(sourcePath, destinationPath);
    }
    return QFile::rename(sourcePath, destinationPath);
}

bool isSimpleArchiveEntryPath(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return false;
    }
    const QStringList tokens = ArchiveSupport::splitArchiveTokens(path);
    return tokens.size() == 2 && !tokens.first().isEmpty();
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
    if (lower == QLatin1String("udf")) {
        return bit7z::BitFormat::Udf;
    }
    if (lower == QLatin1String("iso")) {
        return bit7z::BitFormat::Iso;
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
    if (lower == QLatin1String("iso")) {
        return {QStringLiteral("udf"), QStringLiteral("iso")};
    }
    if (lower == QLatin1String("udf")) {
        return {QStringLiteral("udf")};
    }
    if (lower == QLatin1String("rar")) {
        return {QStringLiteral("rar"), QStringLiteral("rar5")};
    }
    if (lower == QLatin1String("rar5")) {
        return {QStringLiteral("rar5")};
    }
    return {lower};
}

QString rarFormatCandidateForFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return QStringLiteral("rar5");
    }

    const QByteArray signature = file.read(8);
    static const QByteArray rar4Signature = QByteArray::fromHex("526172211A0700");
    static const QByteArray rar5Signature = QByteArray::fromHex("526172211A070100");
    if (signature.startsWith(rar5Signature)) {
        return QStringLiteral("rar5");
    }
    if (signature.startsWith(rar4Signature)) {
        return QStringLiteral("rar");
    }
    return QStringLiteral("rar5");
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
    return ArchiveSupport::isArchivePath(path);
}

FileProvider::Capabilities ArchiveFileProvider::capabilities() const
{
    return Browse | ReadMetadata | Transfer;
}

void ArchiveFileProvider::scan(const QString &path)
{
    cancel();

    m_currentPath = normalizedPath(path);
    m_running.store(true);
    const int myGeneration = m_generation.fetch_add(1) + 1;
    const bool showHidden = m_showHidden;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    const auto cancelled = m_cancelled;
    emit started();

    const QString scanPath = m_currentPath;
    QPointer<ArchiveFileProvider> self(this);
    m_scanFuture = QtConcurrent::run([self, scanPath, myGeneration, showHidden, cancelled]() mutable {
        if (!self) {
            return;
        }

        std::shared_ptr<bit7z::Bit7zLibrary> library;
#ifdef HAS_UNOFFICIAL_BIT7Z
        library = getGlobalLibrary();
#endif
        if (!library) {
            QMetaObject::invokeMethod(self.data(), [self, scanPath, myGeneration]() {
                if (!self || myGeneration != self->m_generation.load()) {
                    return;
                }
                self->m_running.store(false);
                emit self->finished(scanPath,
                                    false,
                                    myGeneration,
                                    QStringLiteral("bit7z backend was not found or could not be loaded"));
            }, Qt::QueuedConnection);
            return;
        }

        auto emitBatch = [self, myGeneration](const QList<FileEntry> &batch) {
            if (!self || batch.isEmpty() || myGeneration != self->m_generation.load()) {
                return;
            }
            emit self->batchReady(batch, myGeneration);
        };
        ArchiveFileProvider::ArchiveState state = ArchiveFileProvider::buildStateFromScratch(
            scanPath,
            library,
            emitBatch,
            showHidden,
            cancelled);

        if (!self) {
            return;
        }
        auto statePtr = std::make_shared<ArchiveFileProvider::ArchiveState>(std::move(state));
        QMetaObject::invokeMethod(self.data(), [self, scanPath, myGeneration, library, statePtr]() mutable {
            if (!self || myGeneration != self->m_generation.load()) {
                return;
            }

            if (!statePtr->valid) {
                self->m_running.store(false);
                emit self->finished(scanPath, false, myGeneration, statePtr->error);
                return;
            }

            self->m_library = library;
            self->m_state = statePtr;
            storeStateInCache(archiveCacheKey(scanPath), statePtr);
            self->m_running.store(false);
            emit self->finished(scanPath, true, myGeneration, {});
        }, Qt::QueuedConnection);
    });
}


void ArchiveFileProvider::cancel()
{
    m_generation.fetch_add(1);
    m_running.store(false);
    if (m_cancelled) {
        m_cancelled->store(true);
    }
    if (!m_currentPath.isEmpty()) {
        invalidateCacheForPath(m_currentPath);
    }
    m_state.reset();
}

void ArchiveFileProvider::setShowHidden(bool show)
{
    m_showHidden = show;
}

bool ArchiveFileProvider::isRunning() const
{
    return m_running.load();
}

QString ArchiveFileProvider::currentPath() const
{
    return m_currentPath;
}

int ArchiveFileProvider::currentGeneration() const
{
    return m_generation.load();
}

bool ArchiveFileProvider::pathExists(const QString &path) const
{
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        const QString rel = archiveRelativeToken(browsePath);
        if (rel.isEmpty()) {
            return true;
        }
        return state->pathIndex.contains(rel) || state->directories.contains(rel);
    }

    ArchiveState state = stateForPath(path);
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
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        const QString rel = archiveRelativeToken(browsePath);
        if (rel.isEmpty()) {
            return true;
        }
        const int idx = state->pathIndex.value(rel, -1);
        if (idx >= 0 && idx < state->items.size()) {
            return state->items.at(idx).isDirectory || state->directories.contains(rel);
        }
        return state->directories.contains(rel);
    }

    ArchiveState state = stateForPath(path);
    if (!state.valid) {
        return false;
    }
    const QString rel = archiveRelativeToken(state.browsePath);
    if (rel.isEmpty()) {
        return true;
    }
    const int idx = state.pathIndex.value(rel, -1);
    if (idx >= 0 && idx < state.items.size()) {
        return state.items.at(idx).isDirectory || state.directories.contains(rel);
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
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        const QString rel = archiveRelativeToken(browsePath);
        if (rel.isEmpty()) {
            FileEntry entry;
            entry.name = ArchiveSupport::archiveFileName(path);
            entry.path = ArchiveSupport::normalizeArchivePath(path);
            entry.suffix = QFileInfo(state->sourcePath).suffix().toLower();
            entry.isDirectory = true;
            entry.sizeText = QStringLiteral("Folder");
            entry.attributesText = QStringLiteral("D");
            return entry;
        }

        const int absoluteIdx = state->pathIndex.value(rel, -1);
        if (absoluteIdx < 0 || absoluteIdx >= state->items.size()) {
            return std::nullopt;
        }
        return fileEntryFromRecord(*state, state->items.at(absoluteIdx));
    }

    ArchiveState state = stateForPath(path);
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

std::optional<FileEntry> ArchiveFileProvider::cachedEntryInfo(const QString &path)
{
    if (!ArchiveSupport::isArchivePath(path)) {
        return std::nullopt;
    }

    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    auto state = cachedStateForKey(archiveCacheKey(normalized));
    if (!state || !state->valid) {
        return std::nullopt;
    }

    const QString browsePath = archiveBrowsePathForPath(normalized);
    const QString rel = archiveRelativeToken(browsePath);
    if (rel.isEmpty()) {
        FileEntry entry;
        entry.name = ArchiveSupport::archiveFileName(normalized);
        entry.path = normalized;
        entry.suffix = QFileInfo(state->sourcePath).suffix().toLower();
        entry.isDirectory = true;
        entry.sizeText = QStringLiteral("Folder");
        entry.attributesText = QStringLiteral("D");
        return entry;
    }

    const int absoluteIdx = state->pathIndex.value(rel, -1);
    if (absoluteIdx < 0 || absoluteIdx >= state->items.size()) {
        return std::nullopt;
    }
    return fileEntryFromRecord(*state, state->items.at(absoluteIdx));
}

QByteArray ArchiveFileProvider::readCachedFilePrefix(const QString &path, qint64 maxEntrySize, qint64 maxBytes, bool *tooLarge)
{
    if (tooLarge) {
        *tooLarge = false;
    }
    if (!ArchiveSupport::isArchivePath(path) || maxEntrySize < 0 || maxBytes <= 0) {
        return {};
    }

    const auto entry = cachedEntryInfo(path);
    if (!entry || entry->isDirectory) {
        return {};
    }
    if (entry->size > maxEntrySize) {
        if (tooLarge) {
            *tooLarge = true;
        }
        return {};
    }

    const QString normalized = ArchiveSupport::normalizeArchivePath(path);
    auto state = cachedStateForKey(archiveCacheKey(normalized));
    if (!state || !state->valid || !state->reader) {
        return {};
    }

    auto device = openReadFromState(*state, archiveBrowsePathForPath(normalized));
    if (!device) {
        return {};
    }
    return device->read(maxBytes);
}

void ArchiveFileProvider::setCurrentThreadTemporaryParent(const QString &path)
{
    g_currentThreadTemporaryParentPath = QDir::fromNativeSeparators(path);
}

bool ArchiveFileProvider::extractArchiveFileTo(const QString &archivePath,
                                               const QString &destinationPath,
                                               QString *error,
                                               const std::function<bool(uint64_t)> &progressCallback,
                                               const std::function<void(const QString &)> &fileCallback)
{
    if (error) {
        error->clear();
    }

    if (archivePath.isEmpty() || destinationPath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Archive path or destination is empty");
        }
        return false;
    }

    if (!ArchiveSupport::isArchiveFilePath(archivePath)) {
        if (error) {
            *error = QStringLiteral("Path is not a supported archive: %1").arg(archivePath);
        }
        return false;
    }

    const QString normalizedArchivePath = QDir::fromNativeSeparators(QFileInfo(archivePath).absoluteFilePath());
    const QString normalizedDestinationPath = QDir::fromNativeSeparators(QFileInfo(destinationPath).absoluteFilePath());
    const QFileInfo destinationInfo(normalizedDestinationPath);
    const bool destinationExisted = destinationInfo.exists();
    const QString extractionParent = QDir::fromNativeSeparators(destinationInfo.absolutePath());
    std::unique_ptr<QTemporaryDir> stagedDir;
    QString extractionPath = normalizedDestinationPath;

    if (destinationExisted) {
        QDir destinationDir(normalizedDestinationPath);
        if (!destinationDir.exists() && !QDir().mkpath(normalizedDestinationPath)) {
            if (error) {
                *error = QStringLiteral("Cannot create folder %1").arg(normalizedDestinationPath);
            }
            return false;
        }
    } else {
        if (!QDir().mkpath(extractionParent)) {
            if (error) {
                *error = QStringLiteral("Cannot create folder %1").arg(extractionParent);
            }
            return false;
        }
        stagedDir = std::make_unique<QTemporaryDir>(
            QDir(extractionParent).filePath(QStringLiteral(".fm-full-extract-XXXXXX")));
        if (!stagedDir->isValid()) {
            if (error) {
                *error = QStringLiteral("Cannot create temporary extraction folder in %1").arg(extractionParent);
            }
            return false;
        }
        extractionPath = QDir::fromNativeSeparators(stagedDir->path());
    }

    auto finalizeStagedExtraction = [&]() -> bool {
        if (destinationExisted) {
            return true;
        }
        stagedDir->setAutoRemove(false);
        if (QFile::rename(extractionPath, normalizedDestinationPath)) {
            return true;
        }
        stagedDir->setAutoRemove(true);
        if (error) {
            *error = QStringLiteral("Cannot finalize extracted folder %1").arg(normalizedDestinationPath);
        }
        return false;
    };

    QString fastPathError;
    if (extractArchiveWithSevenZip(normalizedArchivePath, extractionPath, progressCallback, &fastPathError)) {
        if (!finalizeStagedExtraction()) {
            return false;
        }
        return true;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    const auto library = getGlobalLibrary();
    if (!library) {
        if (error) {
            *error = QStringLiteral("bit7z backend was not found or could not be loaded");
        }
        return false;
    }

    const QString suffix = QFileInfo(normalizedArchivePath).suffix().toLower();
    const QStringList candidates = suffix.compare(QStringLiteral("rar"), Qt::CaseInsensitive) == 0
        ? QStringList{rarFormatCandidateForFile(normalizedArchivePath)}
        : archiveFormatCandidatesForSuffix(suffix);

    for (const QString &candidate : candidates) {
        try {
            const auto &format = archiveFormatForSuffix(candidate);
            bit7z::BitArchiveReader reader(
                *library,
                toBit7zString(QDir::toNativeSeparators(normalizedArchivePath)),
                bit7z::ArchiveStartOffset::FileStart,
                format);
            reader.setOverwriteMode(bit7z::OverwriteMode::Skip);
            if (progressCallback) {
                reader.setProgressCallback(progressCallback);
            }
            if (fileCallback) {
                reader.setFileCallback([fileCallback](bit7z::tstring filePath) {
                    fileCallback(toQString(filePath));
                });
            }
            reader.extractTo(toBit7zString(QDir::toNativeSeparators(extractionPath)));
            if (progressCallback) {
                reader.setProgressCallback(nullptr);
            }
            if (fileCallback) {
                reader.setFileCallback(nullptr);
            }
            if (!finalizeStagedExtraction()) {
                return false;
            }
            return true;
        } catch (const std::exception &exception) {
            if (error) {
                *error = QStringLiteral("Extract failed for %1 to %2 using %3: %4")
                    .arg(normalizedArchivePath, normalizedDestinationPath, candidate, QString::fromUtf8(exception.what()));
            }
        }
    }

    if (error && error->isEmpty()) {
        *error = QStringLiteral("Cannot extract archive %1").arg(normalizedArchivePath);
    }
    return false;
#else
    if (error) {
        *error = QStringLiteral("Archive backend is not available");
    }
    Q_UNUSED(progressCallback)
    Q_UNUSED(fileCallback)
    return false;
#endif
}

bool ArchiveFileProvider::extractArchiveEntryTo(const QString &archiveEntryPath,
                                                const QString &destinationFilePath,
                                                QString *error,
                                                const std::function<bool(uint64_t)> &progressCallback)
{
    if (error) {
        error->clear();
    }

    if (!ArchiveSupport::isArchivePath(archiveEntryPath) || archiveEntryPath.isEmpty() || destinationFilePath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Archive entry path or destination is invalid");
        }
        return false;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    const auto library = getGlobalLibrary();
    if (!library) {
        if (error) {
            *error = QStringLiteral("bit7z backend was not found or could not be loaded");
        }
        return false;
    }

    const QString normalizedEntryPath = ArchiveSupport::normalizeArchivePath(archiveEntryPath);
    const QFileInfo destinationInfo(destinationFilePath);
    const QString destinationParent = QDir::fromNativeSeparators(destinationInfo.absolutePath());
    ArchiveState state = buildStateFromScratch(normalizedEntryPath, library, {}, true, {}, destinationParent);
    if (!state.valid || !state.reader) {
        if (error) {
            *error = state.error.isEmpty()
                ? QStringLiteral("Cannot read archive entry %1").arg(normalizedEntryPath)
                : state.error;
        }
        return false;
    }

    const QString rel = archiveRelativeToken(state.browsePath);
    const int idx = state.pathIndex.value(rel, -1);
    if (idx < 0 || idx >= state.items.size() || state.items.at(idx).isDirectory) {
        if (error) {
            *error = QStringLiteral("Archive entry was not found or is not a file: %1").arg(normalizedEntryPath);
        }
        return false;
    }

    const ArchiveItemRecord &record = state.items.at(idx);
    if (!QDir().mkpath(destinationParent)) {
        if (error) {
            *error = QStringLiteral("Cannot create parent directory for %1").arg(destinationFilePath);
        }
        return false;
    }

    if (QFileInfo::exists(destinationFilePath) && !QFile::remove(destinationFilePath)) {
        if (error) {
            *error = QStringLiteral("Cannot replace temporary destination %1").arg(destinationFilePath);
        }
        return false;
    }

    QTemporaryDir tempDir(QDir(destinationParent).filePath(QStringLiteral(".fm-extract-XXXXXX")));
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("Cannot create temporary extraction folder in %1").arg(destinationParent);
        }
        return false;
    }
    const QString tempRoot = QDir::fromNativeSeparators(tempDir.path());

    {
        QMutexLocker readerLocker(&archiveReaderMutex());
        if (progressCallback) {
            state.reader->setProgressCallback(progressCallback);
        }
        try {
            state.reader->extractTo(toBit7zString(QDir::toNativeSeparators(tempRoot)), std::vector<uint32_t>{record.index});
            if (progressCallback) {
                state.reader->setProgressCallback(nullptr);
            }
        } catch (const std::exception &exception) {
            if (progressCallback) {
                state.reader->setProgressCallback(nullptr);
            }
            if (error) {
                *error = QStringLiteral("Extract failed for %1: %2")
                    .arg(normalizedEntryPath, QString::fromUtf8(exception.what()));
            }
            return false;
        }
    }

    const QString extractedPath = extractedArchiveItemPath(tempRoot, record.relativePath, record.name);
    if (extractedPath.isEmpty()) {
        if (error) {
            *error = QStringLiteral("Extracted archive entry was not found in temporary folder");
        }
        return false;
    }

    if (!QFile::rename(extractedPath, destinationFilePath)) {
        if (error) {
            *error = QStringLiteral("Cannot move extracted file to %1").arg(destinationFilePath);
        }
        QFile::remove(destinationFilePath);
        return false;
    }
    return true;
#else
    if (error) {
        *error = QStringLiteral("Archive backend is not available");
    }
    Q_UNUSED(progressCallback)
    return false;
#endif
}

bool ArchiveFileProvider::extractArchiveEntriesTo(const QStringList &archiveEntryPaths,
                                                  const QStringList &destinationFilePaths,
                                                  QString *error,
                                                  const std::function<bool(uint64_t)> &progressCallback)
{
    if (error) {
        error->clear();
    }

    if (archiveEntryPaths.isEmpty() || archiveEntryPaths.size() != destinationFilePaths.size()) {
        if (error) {
            *error = QStringLiteral("Archive entry selection is invalid");
        }
        return false;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    const auto library = getGlobalLibrary();
    if (!library) {
        if (error) {
            *error = QStringLiteral("bit7z backend was not found or could not be loaded");
        }
        return false;
    }

    const QFileInfo firstDestination(destinationFilePaths.constFirst());
    const QString destinationParent = QDir::fromNativeSeparators(firstDestination.absolutePath());
    if (!QDir().mkpath(destinationParent)) {
        if (error) {
            *error = QStringLiteral("Cannot create parent directory for %1").arg(destinationParent);
        }
        return false;
    }

    const QString firstEntryPath = ArchiveSupport::normalizeArchivePath(archiveEntryPaths.constFirst());
    ArchiveState state = buildStateFromScratch(firstEntryPath, library, {}, true, {}, destinationParent);
    if (!state.valid || !state.reader) {
        if (error) {
            *error = state.error.isEmpty()
                ? QStringLiteral("Cannot read archive entry %1").arg(firstEntryPath)
                : state.error;
        }
        return false;
    }

    const QString container = archiveContainerPart(firstEntryPath);
    std::vector<uint32_t> indices;
    indices.reserve(static_cast<size_t>(archiveEntryPaths.size()));
    QStringList relativePaths;
    relativePaths.reserve(archiveEntryPaths.size());

    for (const QString &entryPath : archiveEntryPaths) {
        const QString normalizedEntryPath = ArchiveSupport::normalizeArchivePath(entryPath);
        if (archiveContainerPart(normalizedEntryPath) != container) {
            if (error) {
                *error = QStringLiteral("Selected archive entries belong to different archives");
            }
            return false;
        }

        const QString rel = archiveRelativeToken(archiveBrowsePathForPath(normalizedEntryPath));
        const int idx = state.pathIndex.value(rel, -1);
        if (idx < 0 || idx >= state.items.size() || state.items.at(idx).isDirectory) {
            if (error) {
                *error = QStringLiteral("Archive entry was not found or is not a file: %1").arg(normalizedEntryPath);
            }
            return false;
        }
        indices.push_back(state.items.at(idx).index);
        relativePaths.append(state.items.at(idx).relativePath);
    }

    for (const QString &destinationPath : destinationFilePaths) {
        const QFileInfo destinationInfo(destinationPath);
        if (QDir::fromNativeSeparators(destinationInfo.absolutePath()) != destinationParent) {
            if (error) {
                *error = QStringLiteral("Batch archive extraction requires a single destination folder");
            }
            return false;
        }
        if (QFileInfo::exists(destinationPath) && !QFile::remove(destinationPath)) {
            if (error) {
                *error = QStringLiteral("Cannot replace temporary destination %1").arg(destinationPath);
            }
            return false;
        }
    }

    QTemporaryDir tempDir(QDir(destinationParent).filePath(QStringLiteral(".fm-extract-XXXXXX")));
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("Cannot create temporary extraction folder in %1").arg(destinationParent);
        }
        return false;
    }
    const QString tempRoot = QDir::fromNativeSeparators(tempDir.path());

    {
        QMutexLocker readerLocker(&archiveReaderMutex());
        if (progressCallback) {
            state.reader->setProgressCallback(progressCallback);
        }
        try {
            state.reader->extractTo(toBit7zString(QDir::toNativeSeparators(tempRoot)), indices);
            if (progressCallback) {
                state.reader->setProgressCallback(nullptr);
            }
        } catch (const std::exception &exception) {
            if (progressCallback) {
                state.reader->setProgressCallback(nullptr);
            }
            if (error) {
                *error = QStringLiteral("Extract failed for selected archive entries: %1")
                    .arg(QString::fromUtf8(exception.what()));
            }
            return false;
        }
    }

    for (int i = 0; i < relativePaths.size(); ++i) {
        const QString extractedPath = extractedArchiveItemPath(
            tempRoot,
            relativePaths.at(i),
            QFileInfo(relativePaths.at(i)).fileName());
        if (extractedPath.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Extracted archive entry was not found in temporary folder");
            }
            return false;
        }
        if (!QFile::rename(extractedPath, destinationFilePaths.at(i))) {
            if (error) {
                *error = QStringLiteral("Cannot move extracted file to %1").arg(destinationFilePaths.at(i));
            }
            for (int cleanup = 0; cleanup <= i; ++cleanup) {
                QFile::remove(destinationFilePaths.at(cleanup));
            }
            return false;
        }
    }

    return true;
#else
    if (error) {
        *error = QStringLiteral("Archive backend is not available");
    }
    Q_UNUSED(progressCallback)
    return false;
#endif
}

bool ArchiveFileProvider::extractArchiveItemsTo(const QStringList &archiveEntryPaths,
                                                const QStringList &destinationPaths,
                                                QString *error,
                                                const std::function<bool(uint64_t)> &progressCallback)
{
    if (error) {
        error->clear();
    }

    if (archiveEntryPaths.isEmpty() || archiveEntryPaths.size() != destinationPaths.size()) {
        if (error) {
            *error = QStringLiteral("Archive item selection is invalid");
        }
        return false;
    }

    const QString firstEntryPath = ArchiveSupport::normalizeArchivePath(archiveEntryPaths.constFirst());
    if (!isSimpleArchiveEntryPath(firstEntryPath)) {
        if (error) {
            *error = QStringLiteral("7-Zip fast extraction supports only top-level archive entries");
        }
        return false;
    }

    const QString archivePath = ArchiveSupport::physicalArchivePath(firstEntryPath);
    if (!ArchiveSupport::isArchiveFilePath(archivePath)) {
        if (error) {
            *error = QStringLiteral("Path is not a supported archive: %1").arg(archivePath);
        }
        return false;
    }

    const QFileInfo firstDestination(destinationPaths.constFirst());
    const QString destinationParent = QDir::fromNativeSeparators(firstDestination.absolutePath());
    if (!QDir().mkpath(destinationParent)) {
        if (error) {
            *error = QStringLiteral("Cannot create destination folder %1").arg(destinationParent);
        }
        return false;
    }

    QStringList relativePaths;
    QStringList itemPatterns;
    relativePaths.reserve(archiveEntryPaths.size());
    itemPatterns.reserve(archiveEntryPaths.size());

    for (const QString &entryPath : archiveEntryPaths) {
        const QString normalizedEntryPath = ArchiveSupport::normalizeArchivePath(entryPath);
        if (!isSimpleArchiveEntryPath(normalizedEntryPath)
            || ArchiveSupport::physicalArchivePath(normalizedEntryPath) != archivePath) {
            if (error) {
                *error = QStringLiteral("Selected archive items belong to different archives");
            }
            return false;
        }

        const QString rel = archiveRelativeToken(ArchiveSupport::archiveBrowsePath(normalizedEntryPath));
        if (rel.isEmpty()) {
            if (error) {
                *error = QStringLiteral("Cannot extract archive root as a selected item");
            }
            return false;
        }

        const auto entry = cachedEntryInfo(normalizedEntryPath);
        if (!entry) {
            if (error) {
                *error = QStringLiteral("Archive item metadata is not cached: %1").arg(normalizedEntryPath);
            }
            return false;
        }

        relativePaths.append(rel);
        if (entry->isDirectory) {
            itemPatterns.append(rel + QStringLiteral("/*"));
        } else {
            itemPatterns.append(rel);
        }
    }

    for (const QString &destinationPath : destinationPaths) {
        const QFileInfo destinationInfo(destinationPath);
        if (QDir::fromNativeSeparators(destinationInfo.absolutePath()) != destinationParent) {
            if (error) {
                *error = QStringLiteral("Batch archive extraction requires a single destination folder");
            }
            return false;
        }
        if (QFileInfo::exists(destinationPath)) {
            if (error) {
                *error = QStringLiteral("Destination already exists: %1").arg(destinationPath);
            }
            return false;
        }
    }

    QTemporaryDir tempDir(QDir(destinationParent).filePath(QStringLiteral(".fm-7z-extract-XXXXXX")));
    if (!tempDir.isValid()) {
        if (error) {
            *error = QStringLiteral("Cannot create temporary extraction folder in %1").arg(destinationParent);
        }
        return false;
    }
    const QString tempRoot = QDir::fromNativeSeparators(tempDir.path());

    QString fastPathError;
    if (!extractArchiveWithSevenZip(archivePath, tempRoot, progressCallback, &fastPathError, itemPatterns)) {
        if (error) {
            *error = fastPathError.isEmpty()
                ? QStringLiteral("7-Zip could not extract selected archive items")
                : fastPathError;
        }
        return false;
    }

    for (int i = 0; i < relativePaths.size(); ++i) {
        if (OperationQueue::isCurrentThreadAborted()) {
            if (error) {
                *error = QStringLiteral("Archive extraction was cancelled");
            }
            return false;
        }

        const QString extractedPath = QDir(tempRoot).filePath(relativePaths.at(i));
        const QString destinationPath = destinationPaths.at(i);
        const auto entry = cachedEntryInfo(archiveEntryPaths.at(i));
        if (!QFileInfo::exists(extractedPath)) {
            if (entry && entry->isDirectory && QDir().mkpath(destinationPath)) {
                continue;
            }
            if (error) {
                *error = QStringLiteral("Extracted archive item was not found in temporary folder: %1")
                    .arg(relativePaths.at(i));
            }
            return false;
        }

        if (!QDir().mkpath(QFileInfo(destinationPath).absolutePath())
            || !moveExtractedPath(extractedPath, destinationPath)) {
            if (error) {
                *error = QStringLiteral("Cannot move extracted item to %1").arg(destinationPath);
            }
            return false;
        }
    }

    return true;
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
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        const QString browse = archiveRelativeToken(browsePath);
        QStringList result;
        for (const ArchiveItemRecord &record : state->items) {
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

    ArchiveState state = stateForPath(path);
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
    QString browsePath;
    if (auto state = cachedStateForPath(path, &browsePath)) {
        if (state->reader) {
            return openReadFromState(*state, browsePath);
        }
    }

    ArchiveState state = stateForPath(path);
    if (!state.valid) {
        return {};
    }
    return openReadFromState(state, state.browsePath);
}

std::unique_ptr<QIODevice> ArchiveFileProvider::openReadFromState(const ArchiveState &state, const QString &browsePath)
{
    const QString rel = archiveRelativeToken(browsePath);
    const int idx = state.pathIndex.value(rel, -1);
    if (idx < 0 || idx >= state.items.size()) {
        qWarning() << "[FM_ARCHIVE_READ] item not found"
                   << "browsePath" << browsePath
                   << "rel" << rel
                   << "items" << state.items.size();
        return {};
    }

    const ArchiveItemRecord &record = state.items.at(idx);
    if (record.isDirectory) {
        return {};
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    if (!state.reader) {
        qWarning() << "[FM_ARCHIVE_READ] missing reader"
                   << "sourcePath" << state.sourcePath
                   << "browsePath" << browsePath
                   << "rel" << rel;
        return {};
    }

    try {
        auto tempDir = g_currentThreadTemporaryParentPath.isEmpty()
            ? std::make_unique<QTemporaryDir>()
            : std::make_unique<QTemporaryDir>(
                QDir(g_currentThreadTemporaryParentPath).filePath(QStringLiteral(".fm-read-XXXXXX")));
        if (!tempDir->isValid()) {
            qWarning() << "[FM_ARCHIVE_READ] temp dir invalid"
                       << "path" << tempDir->path()
                       << "sourcePath" << state.sourcePath
                       << "rel" << rel;
            return {};
        }
        tempDir->setAutoRemove(false);
        const QString tempRoot = QDir::fromNativeSeparators(tempDir->path());
        const auto cleanupTempRoot = [&tempRoot]() {
            if (!tempRoot.isEmpty()) {
                QDir(tempRoot).removeRecursively();
            }
        };

        {
            QMutexLocker readerLocker(&archiveReaderMutex());
            state.reader->setProgressCallback([](uint64_t processedBytes) -> bool {
                const uint64_t maxBytes = static_cast<uint64_t>((std::numeric_limits<qint64>::max)());
                OperationQueue::reportCurrentThreadProgressBytes(
                    static_cast<qint64>((std::min)(processedBytes, maxBytes)));
                return !OperationQueue::isCurrentThreadAborted();
            });
            try {
                state.reader->extractTo(toBit7zString(QDir::toNativeSeparators(tempRoot)), std::vector<uint32_t>{record.index});
                state.reader->setProgressCallback(nullptr);
            } catch (const bit7z::BitException &exception) {
                state.reader->setProgressCallback(nullptr);
                if (OperationQueue::isCurrentThreadAborted()) {
                } else {
                    qWarning() << "[FM_ARCHIVE_READ] extract selected item failed"
                               << "sourcePath" << state.sourcePath
                               << "browsePath" << browsePath
                               << "rel" << rel
                               << "recordName" << record.name
                               << "recordIndex" << record.index
                               << "recordSize" << record.size
                               << "tempRoot" << tempRoot
                               << "message" << QString::fromUtf8(exception.what())
                               << "nativeCode" << exception.nativeCode()
                               << "hresult" << exception.hresultCode();
                    for (const auto &failedFile : exception.failedFiles()) {
                        qWarning() << "[FM_ARCHIVE_READ] failed file"
                                   << toQString(failedFile.first)
                                   << failedFile.second.value()
                                   << QString::fromStdString(failedFile.second.message());
                    }
                }
                cleanupTempRoot();
                throw;
            } catch (const std::exception &exception) {
                state.reader->setProgressCallback(nullptr);
                qWarning() << "[FM_ARCHIVE_READ] extract selected item failed"
                           << "sourcePath" << state.sourcePath
                           << "browsePath" << browsePath
                           << "rel" << rel
                           << "recordName" << record.name
                           << "recordIndex" << record.index
                           << "recordSize" << record.size
                           << "tempRoot" << tempRoot
                           << "message" << QString::fromUtf8(exception.what());
                cleanupTempRoot();
                throw;
            } catch (...) {
                state.reader->setProgressCallback(nullptr);
                qWarning() << "[FM_ARCHIVE_READ] extract selected item failed with unknown exception"
                           << "sourcePath" << state.sourcePath
                           << "browsePath" << browsePath
                           << "rel" << rel
                           << "recordName" << record.name
                           << "recordIndex" << record.index
                           << "recordSize" << record.size
                           << "tempRoot" << tempRoot;
                cleanupTempRoot();
                throw;
            }
        }

        const QString extractedPath = extractedArchiveItemPath(tempRoot, record.relativePath, record.name);
        if (extractedPath.isEmpty()) {
            qWarning() << "[FM_ARCHIVE_READ] extracted item missing"
                       << "sourcePath" << state.sourcePath
                       << "browsePath" << browsePath
                       << "rel" << rel
                       << "recordName" << record.name
                       << "recordIndex" << record.index
                       << "recordSize" << record.size
                       << "tempRoot" << tempRoot
                       << "files" << sampledExtractedFiles(tempRoot);
            cleanupTempRoot();
            return {};
        }

        auto device = std::make_unique<TemporaryFileDevice>(extractedPath, tempRoot);
        if (!device->open(QIODevice::ReadOnly)) {
            qWarning() << "[FM_ARCHIVE_READ] extracted item open failed"
                       << "sourcePath" << state.sourcePath
                       << "browsePath" << browsePath
                       << "rel" << rel
                       << "extractedPath" << extractedPath
                       << "error" << device->errorString();
            cleanupTempRoot();
            return {};
        }
        return device;
    } catch (const std::exception &exception) {
        if (OperationQueue::isCurrentThreadAborted()) {
            return {};
        }
        qWarning() << "[FM_ARCHIVE_READ] openRead failed"
                   << "sourcePath" << state.sourcePath
                   << "browsePath" << browsePath
                   << "rel" << rel
                   << "recordName" << record.name
                   << "recordIndex" << record.index
                   << "message" << QString::fromUtf8(exception.what());
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

ArchiveFileProvider::ArchiveState ArchiveFileProvider::stateForPath(const QString &path) const
{
    if (!ensureLibrary()) {
        ArchiveState state;
        state.currentPath = normalizedPath(path);
        state.error = QStringLiteral("bit7z backend was not found or could not be loaded");
        return state;
    }
    return buildStateFromScratch(path, m_library);
}

std::shared_ptr<ArchiveFileProvider::ArchiveState> ArchiveFileProvider::cachedStateForPath(const QString &path, QString *browsePath) const
{
    const QString normalized = normalizedPath(path);
    if (m_state && m_state->valid
        && archiveContainerPart(normalized) == archiveContainerPart(m_state->currentPath)) {
        if (browsePath) {
            *browsePath = archiveBrowsePathForPath(normalized);
        }
        return m_state;
    }

    const QString key = archiveCacheKey(normalized);
    auto cached = cachedStateForKey(key);
    if (!cached || !cached->valid) {
        return nullptr;
    }
    if (browsePath) {
        *browsePath = archiveBrowsePathForPath(normalized);
    }
    m_state = cached;
    return cached;
}

QList<FileEntry> ArchiveFileProvider::visibleEntriesForState(const ArchiveState &state, bool showHidden)
{
    QList<FileEntry> entries;
    entries.reserve(state.items.size());
    for (const ArchiveItemRecord &record : std::as_const(state.items)) {
        if (record.relativePath == state.browsePath) {
            continue;
        }
        const QString parent = parentRelativePath(record.relativePath);
        if (parent != state.browsePath) {
            continue;
        }
        if (!showHidden && record.isHidden) {
            continue;
        }
        entries.append(fileEntryFromRecord(state, record));
    }

    std::sort(entries.begin(), entries.end(), [](const FileEntry &lhs, const FileEntry &rhs) {
        if (lhs.isDirectory != rhs.isDirectory) {
            return lhs.isDirectory;
        }
        return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
    });
    return entries;
}

QString ArchiveFileProvider::archiveContainerPart(const QString &path)
{
    const int lastPipe = path.lastIndexOf(QLatin1Char('|'));
    return lastPipe >= 0 ? path.left(lastPipe) : path;
}

QString ArchiveFileProvider::archiveBrowsePathForPath(const QString &path)
{
    QString working = path;
    if (ArchiveSupport::isArchiveFilePath(working)) {
        working = ArchiveSupport::archiveRootPath(working);
    }
    const QStringList tokens = archiveTokenPath(working).split(QLatin1Char('|'), Qt::KeepEmptyParts);
    if (tokens.isEmpty()) {
        return {};
    }
    QString browse = normalizeRelativePath(tokens.last());
    if (tokens.last() == QLatin1String("/")) {
        browse.clear();
    }
    return browse;
}

QString ArchiveFileProvider::archiveCacheKey(const QString &path)
{
    QString normalized = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::normalizeArchivePath(path)
        : (ArchiveSupport::isArchiveFilePath(path)
            ? ArchiveSupport::archiveRootPath(path)
            : ArchiveSupport::normalizeArchivePath(path));
    const QString physicalPath = ArchiveSupport::physicalArchivePath(normalized);
    const QFileInfo info(physicalPath);
    return QStringLiteral("%1|%2|%3|%4")
        .arg(archiveContainerPart(normalized))
        .arg(info.size())
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(info.exists());
}

std::shared_ptr<ArchiveFileProvider::ArchiveState> ArchiveFileProvider::cachedStateForKey(const QString &key)
{
    QMutexLocker locker(&archiveCacheMutex());
    auto &cache = archiveCache();
    auto it = cache.find(key);
    if (it == cache.end()) {
        return {};
    }
    return it.value();
}

void ArchiveFileProvider::invalidateCacheForPath(const QString &path)
{
    if (path.isEmpty()) {
        return;
    }

    const QString physicalPath = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::physicalArchivePath(path)
        : path;
    const QString normalizedPhysicalPath = QDir::fromNativeSeparators(QFileInfo(physicalPath).absoluteFilePath());
    if (normalizedPhysicalPath.isEmpty()) {
        return;
    }

    QMutexLocker locker(&archiveCacheMutex());
    auto &cache = archiveCache();
    auto &order = archiveCacheOrder();

    QStringList keysToRemove;
    for (auto it = cache.cbegin(); it != cache.cend(); ++it) {
        const std::shared_ptr<ArchiveState> &state = it.value();
        if (!state) {
            keysToRemove.append(it.key());
            continue;
        }
        const QString stateSourcePath = QDir::fromNativeSeparators(QFileInfo(state->sourcePath).absoluteFilePath());
        if (stateSourcePath.compare(normalizedPhysicalPath, Qt::CaseInsensitive) == 0) {
            keysToRemove.append(it.key());
        }
    }

    for (const QString &key : std::as_const(keysToRemove)) {
        cache.remove(key);
        order.removeAll(key);
    }
}

void ArchiveFileProvider::storeStateInCache(const QString &key, const std::shared_ptr<ArchiveState> &state)
{
    if (key.isEmpty() || !state || !state->valid) {
        return;
    }

    QMutexLocker locker(&archiveCacheMutex());
    auto &cache = archiveCache();
    auto &order = archiveCacheOrder();

    if (state->items.size() > kMaxCachedArchiveItems) {
        cache.remove(key);
        order.removeAll(key);
        return;
    }

    order.removeAll(key);
    cache.insert(key, state);
    order.append(key);

    qsizetype cachedItems = 0;
    for (const QString &cacheKey : std::as_const(order)) {
        if (const auto cached = cache.value(cacheKey)) {
            cachedItems += cached->items.size();
        }
    }

    while ((!order.isEmpty() && order.size() > kMaxCachedArchiveStates)
           || (!order.isEmpty() && cachedItems > kMaxCachedArchiveItems)) {
        const QString evictedKey = order.takeFirst();
        if (const auto evicted = cache.take(evictedKey)) {
            cachedItems -= evicted->items.size();
        }
    }
}

QHash<QString, std::shared_ptr<ArchiveFileProvider::ArchiveState>> &ArchiveFileProvider::archiveCache()
{
    static QHash<QString, std::shared_ptr<ArchiveState>> cache;
    return cache;
}

QStringList &ArchiveFileProvider::archiveCacheOrder()
{
    static QStringList order;
    return order;
}

QMutex &ArchiveFileProvider::archiveCacheMutex()
{
    static QMutex mutex;
    return mutex;
}

ArchiveFileProvider::ArchiveState ArchiveFileProvider::buildStateFromScratch(
    const QString &path,
    const std::shared_ptr<bit7z::Bit7zLibrary> &library,
    const std::function<void(const QList<FileEntry> &)> &batchCallback,
    bool showHidden,
    const std::shared_ptr<std::atomic_bool> &cancelled,
    const QString &temporaryParentPath)
{
    QString normalized = ArchiveSupport::isArchivePath(path)
        ? ArchiveSupport::normalizeArchivePath(path)
        : (ArchiveSupport::isArchiveFilePath(path)
            ? ArchiveSupport::archiveRootPath(path)
            : ArchiveSupport::normalizeArchivePath(path));
    ArchiveState state;
    state.currentPath = normalized;
    if (state.currentPath.isEmpty()) {
        state.error = QStringLiteral("Invalid archive path");
        return state;
    }

    QString working = state.currentPath;
    if (ArchiveSupport::isArchiveFilePath(working)) {
        working = ArchiveSupport::archiveRootPath(working);
    }
    if (!ArchiveSupport::isArchivePath(working)) {
        state.error = QStringLiteral("Path is not an archive");
        return state;
    }

    const QStringList tokens = archiveTokenPath(working).split(QLatin1Char('|'), Qt::KeepEmptyParts);
    if (tokens.isEmpty()) {
        state.error = QStringLiteral("Archive path is empty");
        return state;
    }

    const QString sourcePath = tokens.first();
    if (sourcePath.isEmpty() || !QFileInfo::exists(sourcePath)) {
        state.error = QStringLiteral("Archive file was not found");
        return state;
    }

    if (!library) {
        state.error = QStringLiteral("bit7z backend was not found or could not be loaded");
        return state;
    }

#ifdef HAS_UNOFFICIAL_BIT7Z
    try {
        const QStringList chain = tokens.mid(1, qMax(0, tokens.size() - 2));
        const QString browsePathToken = tokens.last();

        std::unique_ptr<bit7z::BitArchiveReader> reader;
        std::unique_ptr<QTemporaryDir> currentTempDir;
        std::unique_ptr<QTemporaryFile> currentTempFile;
        const QString effectiveTemporaryParent = !temporaryParentPath.isEmpty()
            ? QDir::fromNativeSeparators(temporaryParentPath)
            : g_currentThreadTemporaryParentPath;

        auto openReaderFromFile = [&](const QString &archivePath, const QString &formatSuffix) -> std::unique_ptr<bit7z::BitArchiveReader> {
            const QStringList candidates = formatSuffix.compare(QStringLiteral("rar"), Qt::CaseInsensitive) == 0
                ? QStringList{rarFormatCandidateForFile(archivePath)}
                : archiveFormatCandidatesForSuffix(formatSuffix);
            for (const QString &candidate : candidates) {
                try {
                    const auto &format = archiveFormatForSuffix(candidate);
                    return std::make_unique<bit7z::BitArchiveReader>(
                        *library,
                        toBit7zString(archivePath),
                        bit7z::ArchiveStartOffset::FileStart,
                        format);
                } catch (const std::exception &) {
                    continue;
                }
            }
            return {};
        };

        reader = openReaderFromFile(sourcePath, QFileInfo(sourcePath).suffix().toLower());
        if (!reader) {
            state.error = QStringLiteral("Unsupported archive format");
            return state;
        }

        for (const QString &segment : chain) {
            if (cancelled && cancelled->load()) {
                state.error = QStringLiteral("Archive scan was cancelled");
                return state;
            }
            const QString rel = normalizeRelativePath(segment);
            bool found = false;
            const uint32_t itemCount = reader->itemsCount();
            for (uint32_t i = 0; i < itemCount; ++i) {
                if (cancelled && cancelled->load()) {
                    state.error = QStringLiteral("Archive scan was cancelled");
                    return state;
                }
                const auto item = reader->itemAt(i);
                const QString itemRel = normalizeRelativePath(toQString(item.path()));
                if (itemRel != rel) {
                    continue;
                }
                if (!isArchiveLike(QFileInfo(itemRel).suffix().toLower())) {
                    state.error = QStringLiteral("Nested archive item is not an archive");
                    return state;
                }

                std::unique_ptr<QTemporaryDir> nextTempDir;
                std::unique_ptr<QTemporaryFile> nextTempFile;
                const QString normalizedTemporaryParent = QDir::fromNativeSeparators(effectiveTemporaryParent);
                if (!normalizedTemporaryParent.isEmpty()) {
                    QDir().mkpath(normalizedTemporaryParent);
                    nextTempDir = std::make_unique<QTemporaryDir>(
                        QDir(normalizedTemporaryParent).filePath(QStringLiteral(".fm-nested-XXXXXX")));
                    if (!nextTempDir->isValid()) {
                        state.error = QStringLiteral("Could not create temporary folder for nested archive");
                        return state;
                    }
                    nextTempFile = std::make_unique<QTemporaryFile>(
                        QDir(nextTempDir->path()).filePath(QStringLiteral("nested-XXXXXX")));
                } else {
                    nextTempFile = std::make_unique<QTemporaryFile>();
                }
                if (!nextTempFile->open()) {
                    state.error = QStringLiteral("Could not create temporary file for nested archive");
                    return state;
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
                        return state;
                    }

                    reader->setProgressCallback([cancelled](uint64_t) -> bool {
                        if (cancelled && cancelled->load()) {
                            return false;
                        }
                        return !OperationQueue::isCurrentThreadAborted();
                    });
                    reader->extractTo(outFile, item.index());
                    reader->setProgressCallback(nullptr);
                }

                const QString itemSuffix = QFileInfo(itemRel).suffix().toLower();
                const QStringList candidates = itemSuffix == QLatin1String("rar")
                    ? QStringList{rarFormatCandidateForFile(tempPath)}
                    : archiveFormatCandidatesForSuffix(itemSuffix);
                std::unique_ptr<bit7z::BitArchiveReader> nestedReader;
                for (const QString &candidate : candidates) {
                    try {
                        const auto &format = archiveFormatForSuffix(candidate);
                        nestedReader = std::make_unique<bit7z::BitArchiveReader>(
                            *library,
                            toBit7zString(tempPath),
                            bit7z::ArchiveStartOffset::FileStart,
                            format);
                        break;
                    } catch (const std::exception &) {
                        continue;
                    }
                }
                if (!nestedReader) {
                    state.error = QStringLiteral("Nested archive format is not supported");
                    return state;
                }
                reader = std::move(nestedReader);
                currentTempDir = std::move(nextTempDir);
                currentTempFile = std::move(nextTempFile);
                found = true;
                break;
            }
            if (!found) {
                state.error = QStringLiteral("Nested archive entry was not found");
                return state;
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
        state.tempDir = std::move(currentTempDir);
        state.tempFile = std::move(currentTempFile);

        const uint32_t itemCount = state.reader->itemsCount();
        state.items.reserve(static_cast<int>(itemCount));
        QList<FileEntry> visibleBatch;
        visibleBatch.reserve(512);
        bool firstVisibleBatchSent = false;

        for (uint32_t i = 0; i < itemCount; ++i) {
            if (cancelled && cancelled->load()) {
                state.error = QStringLiteral("Archive scan was cancelled");
                return state;
            }
            const auto item = state.reader->itemAt(i);
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
            const bool isVisibleDirectChild = record.relativePath != state.browsePath
                && parentRelativePath(record.relativePath) == state.browsePath
                && (showHidden || !record.isHidden);
            state.pathIndex.insert(record.relativePath, state.items.size());
            state.items.append(record);

            if (batchCallback && isVisibleDirectChild) {
                visibleBatch.append(fileEntryFromRecord(state, state.items.constLast()));
                if (!firstVisibleBatchSent || visibleBatch.size() >= 512) {
                    batchCallback(visibleBatch);
                    visibleBatch.clear();
                    firstVisibleBatchSent = true;
                }
            }

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

        for (const QString &directoryPath : std::as_const(state.directories)) {
            const QString normalizedDirectory = normalizeRelativePath(directoryPath);
            if (normalizedDirectory.isEmpty() || state.pathIndex.contains(normalizedDirectory)) {
                continue;
            }

            ArchiveItemRecord record;
            record.relativePath = normalizedDirectory;
            record.name = QFileInfo(normalizedDirectory).fileName();
            record.suffix = archiveSuffixFromName(record.name);
            record.isDirectory = true;
            record.isHidden = isHiddenName(record.name);
            record.isArchive = false;
            record.absolutePath = itemAbsolutePath(state.archivePrefix, record.relativePath);

            const bool isVisibleDirectChild = parentRelativePath(record.relativePath) == state.browsePath
                && (showHidden || !record.isHidden);
            state.pathIndex.insert(record.relativePath, state.items.size());
            state.items.append(record);

            if (batchCallback && isVisibleDirectChild) {
                visibleBatch.append(fileEntryFromRecord(state, state.items.constLast()));
                if (!firstVisibleBatchSent || visibleBatch.size() >= 512) {
                    batchCallback(visibleBatch);
                    visibleBatch.clear();
                    firstVisibleBatchSent = true;
                }
            }
        }

        if (batchCallback && !visibleBatch.isEmpty()) {
            batchCallback(visibleBatch);
        }

        state.directories.insert(QString());
        return state;
    } catch (const std::exception &ex) {
        state.error = QString::fromUtf8(ex.what());
        return state;
    }
#else
    state.error = QStringLiteral("bit7z support is disabled");
    return state;
#endif
}
