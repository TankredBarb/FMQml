#include "LocalFileProvider.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QLocale>
#include <QStringList>
#include <QtConcurrent>
#include <filesystem>
#include <optional>

#ifdef Q_OS_WIN
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {
enum class FileMutationKind {
    DeleteFile,
    DeleteFolder,
    Move,
    Rename,
    CreateFolder,
    CreateFile,
    Read,
    Write
};

bool isImageSuffix(const QString &suffix)
{
    static const QStringList imageSuffixes = {
        QStringLiteral("jpg"),
        QStringLiteral("jpeg"),
        QStringLiteral("png"),
        QStringLiteral("gif"),
        QStringLiteral("bmp"),
        QStringLiteral("webp"),
        QStringLiteral("ico")
    };
    return imageSuffixes.contains(suffix.toLower());
}

bool hasThumbnailSuffix(const QString &suffix)
{
    static const QStringList thumbnailSuffixes = {
        QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("gif"),  QStringLiteral("bmp"),  QStringLiteral("webp"),
        QStringLiteral("ico"),
        QStringLiteral("svg"),  QStringLiteral("svgz"),
        QStringLiteral("mp3"),  QStringLiteral("flac"), QStringLiteral("ogg"),
        QStringLiteral("m4a"),  QStringLiteral("m4b"),  QStringLiteral("wav"),
        QStringLiteral("wma"),
        QStringLiteral("mp4"),  QStringLiteral("avi"),  QStringLiteral("mkv"),
        QStringLiteral("mov"),  QStringLiteral("wmv"),
        QStringLiteral("pdf"),
        QStringLiteral("ttf"),  QStringLiteral("otf"),  QStringLiteral("woff"),
        QStringLiteral("woff2")
    };
    return thumbnailSuffixes.contains(suffix.toLower());
}

FileEntry entryFromInfo(const QFileInfo &fileInfo)
{
    FileEntry entry;
    entry.name = fileInfo.fileName();
    entry.path = fileInfo.absoluteFilePath();
    entry.suffix = fileInfo.suffix();
    entry.size = fileInfo.size();
    entry.modified = fileInfo.lastModified();
    entry.created = fileInfo.birthTime().isValid() ? fileInfo.birthTime() : fileInfo.lastModified();
    entry.isDirectory = fileInfo.isDir();
    entry.isHidden = fileInfo.isHidden() || fileInfo.fileName().startsWith(QLatin1Char('.'));
    entry.isReadOnly = !fileInfo.isWritable();

    QLocale loc;
    entry.sizeText = entry.isDirectory
        ? QString()
        : loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
    entry.modifiedText = loc.toString(entry.modified, QLocale::ShortFormat);
    entry.createdText  = loc.toString(entry.created,  QLocale::ShortFormat);

    // Build attributes string
    QString attrs;
    if (entry.isDirectory) attrs += QLatin1Char('D');
    if (entry.isHidden)    attrs += QLatin1Char('H');
    if (entry.isReadOnly)  attrs += QLatin1Char('R');
    if (fileInfo.isSymLink()) attrs += QLatin1Char('L');
    entry.attributesText = attrs;

    entry.isImage = !entry.isDirectory && isImageSuffix(entry.suffix);
    entry.hasThumbnail = !entry.isDirectory && hasThumbnailSuffix(entry.suffix);
    return entry;
}

#ifdef Q_OS_WIN
QString verbForMutation(FileMutationKind kind)
{
    switch (kind) {
    case FileMutationKind::DeleteFile:
    case FileMutationKind::DeleteFolder:
        return QStringLiteral("delete");
    case FileMutationKind::Move:
        return QStringLiteral("move");
    case FileMutationKind::Rename:
        return QStringLiteral("rename");
    case FileMutationKind::CreateFolder:
        return QStringLiteral("create folder in");
    case FileMutationKind::CreateFile:
        return QStringLiteral("create file in");
    case FileMutationKind::Read:
        return QStringLiteral("read");
    case FileMutationKind::Write:
        return QStringLiteral("write");
    }
    return QStringLiteral("modify");
}

QString targetForMutation(FileMutationKind kind, const QString &path)
{
    switch (kind) {
    case FileMutationKind::CreateFolder:
    case FileMutationKind::CreateFile:
        return QDir::toNativeSeparators(QFileInfo(path).absolutePath());
    default:
        return QDir::toNativeSeparators(path);
    }
}

QString windowsMutationErrorMessage(FileMutationKind kind, const QString &path, DWORD errorCode)
{
    const QString target = targetForMutation(kind, path);
    const QString verb = verbForMutation(kind);

    switch (errorCode) {
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
        return QStringLiteral("Access denied: cannot %1 %2").arg(verb, target);
    case ERROR_SHARING_VIOLATION:
    case ERROR_LOCK_VIOLATION:
        return QStringLiteral("Cannot %1 %2: the item is being used by another process").arg(verb, target);
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        return QStringLiteral("Cannot %1 %2: the item no longer exists").arg(verb, target);
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return QStringLiteral("Cannot %1 %2: an item with the same name already exists").arg(verb, target);
    case ERROR_DIR_NOT_EMPTY:
        return QStringLiteral("Cannot %1 %2: the folder is not empty").arg(verb, target);
    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return QStringLiteral("Cannot %1 %2: the drive is full").arg(verb, target);
    case ERROR_WRITE_PROTECT:
        return QStringLiteral("Cannot %1 %2: the destination is write-protected").arg(verb, target);
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_INVALID_NAME:
        return QStringLiteral("Cannot %1 %2: the name is invalid").arg(verb, target);
    default:
        break;
    }

    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER
        | FORMAT_MESSAGE_FROM_SYSTEM
        | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags,
                                        nullptr,
                                        errorCode,
                                        0,
                                        reinterpret_cast<LPWSTR>(&buffer),
                                        0,
                                        nullptr);
    QString detail;
    if (length > 0 && buffer) {
        detail = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
        LocalFree(buffer);
    }

    if (detail.isEmpty()) {
        return QStringLiteral("Cannot %1 %2").arg(verb, target);
    }
    return QStringLiteral("Cannot %1 %2: %3").arg(verb, target, detail);
}

bool canEnumerateDirectoryWindows(const QString &path, QString *errorMessage)
{
    const QString nativePath = QDir::toNativeSeparators(path);
    QString pattern = nativePath;
    if (!pattern.endsWith(QLatin1Char('\\'))) {
        pattern += QLatin1Char('\\');
    }
    pattern += QLatin1Char('*');

    WIN32_FIND_DATAW findData{};
    HANDLE handle = FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()),
                                     FindExInfoBasic,
                                     &findData,
                                     FindExSearchNameMatch,
                                     nullptr,
                                     0);
    if (handle != INVALID_HANDLE_VALUE) {
        FindClose(handle);
        return true;
    }

    const DWORD errorCode = GetLastError();
    if (errorCode == ERROR_FILE_NOT_FOUND || errorCode == ERROR_NO_MORE_FILES) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = windowsMutationErrorMessage(FileMutationKind::Read, path, errorCode);
    }
    return false;
}
#endif
}

LocalFileProvider::LocalFileProvider(QObject *parent)
    : FileProvider(parent)
{
}

LocalFileProvider::~LocalFileProvider()
{
    cancel();
    m_watcher.waitForFinished();
}

QString LocalFileProvider::scheme() const
{
    return QStringLiteral("file");
}

bool LocalFileProvider::canHandle(const QString &path) const
{
    if (path.isEmpty()) {
        return false;
    }

    const QFileInfo info(path);
    return info.isAbsolute()
        || path.startsWith('/')
        || path.startsWith(QStringLiteral("\\\\"))
        || path.startsWith(QStringLiteral("file:"));
}

LocalFileProvider::Capabilities LocalFileProvider::capabilities() const
{
    return Browse
        | ReadMetadata
        | Create
        | Rename
        | Remove
        | Transfer
        | Watch;
}

bool LocalFileProvider::pathExists(const QString &path) const
{
    return QFileInfo::exists(path);
}

bool LocalFileProvider::isDirectory(const QString &path) const
{
    return QFileInfo(path).isDir();
}

bool LocalFileProvider::isSymLink(const QString &path) const
{
    return QFileInfo(path).isSymLink();
}

QString LocalFileProvider::normalizedPath(const QString &path) const
{
    return QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
}

QString LocalFileProvider::fileName(const QString &path) const
{
    return QFileInfo(path).fileName();
}

QString LocalFileProvider::absolutePath(const QString &path) const
{
    return QFileInfo(path).absoluteFilePath();
}

QString LocalFileProvider::parentPath(const QString &path) const
{
    return QFileInfo(path).absoluteDir().absolutePath();
}

QString LocalFileProvider::childPath(const QString &parentPath, const QString &name) const
{
    return QDir(parentPath).filePath(name);
}

std::optional<FileEntry> LocalFileProvider::entryInfo(const QString &path) const
{
    QFileInfo info(path);
    if (!info.exists()) {
        return std::nullopt;
    }
    return entryFromInfo(info);
}

bool LocalFileProvider::ensureParentDirectory(const QString &path) const
{
    clearLastError();
    const QString parentPath = QFileInfo(path).absolutePath();
    if (QDir().mkpath(parentPath)) {
        return true;
    }
    setLastError(QStringLiteral("Cannot create folder in %1")
                     .arg(QDir::toNativeSeparators(parentPath)));
    return false;
}

bool LocalFileProvider::makePath(const QString &path) const
{
    clearLastError();
    if (QDir().mkpath(path)) {
        return true;
    }
    setLastError(QStringLiteral("Cannot create folder in %1")
                     .arg(QDir::toNativeSeparators(QFileInfo(path).absolutePath())));
    return false;
}

bool LocalFileProvider::removePath(const QString &path) const
{
    clearLastError();
    QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }

#ifdef Q_OS_WIN
    std::error_code errorCode;
    const std::filesystem::path nativePath = std::filesystem::path(QDir::toNativeSeparators(path).toStdWString());
    if (info.isDir() && !info.isSymLink()) {
        std::filesystem::remove_all(nativePath, errorCode);
        if (errorCode) {
            setLastError(windowsMutationErrorMessage(FileMutationKind::DeleteFolder, path, static_cast<DWORD>(errorCode.value())));
            return false;
        }
        return true;
    }

    const bool removed = std::filesystem::remove(nativePath, errorCode);
    if (errorCode) {
        setLastError(windowsMutationErrorMessage(FileMutationKind::DeleteFile, path, static_cast<DWORD>(errorCode.value())));
        return false;
    }
    return removed;
#else
    if (info.isDir() && !info.isSymLink()) {
        return QDir(path).removeRecursively();
    }
    return QFile::remove(path);
#endif
}

QStringList LocalFileProvider::childPaths(const QString &path, bool includeHidden) const
{
    QStringList children;
    QDir dir(path);
    const QFileInfoList infos = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | (includeHidden ? QDir::Hidden : QDir::NoFilter));
    children.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        const bool isHidden = info.isHidden() || info.fileName().startsWith('.');
        if (!includeHidden && isHidden) {
            continue;
        }
        children.append(info.absoluteFilePath());
    }
    return children;
}

bool LocalFileProvider::movePath(const QString &sourcePath, const QString &destinationPath) const
{
    clearLastError();
    if (sourcePath.isEmpty() || destinationPath.isEmpty()) {
        setLastError(QStringLiteral("Cannot move item: the source or destination path is empty"));
        return false;
    }
    if (QFileInfo::exists(destinationPath)) {
        setLastError(QStringLiteral("Cannot move %1: an item with the same name already exists")
                         .arg(QDir::toNativeSeparators(sourcePath)));
        return false;
    }
#ifdef Q_OS_WIN
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(sourcePath).utf16()),
                     reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(destinationPath).utf16()),
                     MOVEFILE_COPY_ALLOWED)) {
        setLastError(windowsMutationErrorMessage(FileMutationKind::Move, sourcePath, GetLastError()));
        return false;
    }
    return true;
#else
    if (!QFile::rename(sourcePath, destinationPath)) {
        setLastError(QStringLiteral("Cannot move %1").arg(QDir::toNativeSeparators(sourcePath)));
        return false;
    }
    return true;
#endif
}

std::unique_ptr<QIODevice> LocalFileProvider::openRead(const QString &path) const
{
    clearLastError();
    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::ReadOnly)) {
        setLastError(QStringLiteral("Cannot read %1: %2")
                         .arg(QDir::toNativeSeparators(path), file->errorString()));
        return nullptr;
    }
    return std::unique_ptr<QIODevice>(file.release());
}

std::unique_ptr<QIODevice> LocalFileProvider::openWrite(const QString &path, bool truncate) const
{
    clearLastError();
    auto file = std::make_unique<QFile>(path);
    QIODevice::OpenMode mode = QIODevice::WriteOnly;
    if (truncate) {
        mode |= QIODevice::Truncate;
    }
    if (!file->open(mode)) {
        setLastError(QStringLiteral("Cannot write %1: %2")
                         .arg(QDir::toNativeSeparators(path), file->errorString()));
        return nullptr;
    }
    return std::unique_ptr<QIODevice>(file.release());
}

void LocalFileProvider::setShowHidden(bool show)
{
    m_showHidden = show;
}

void LocalFileProvider::scan(const QString &path)
{
    cancel();

    const int myGen = ++m_scanGeneration;
    m_currentPath = path;

    emit started();

    m_watcher.setFuture(QtConcurrent::run([this, path, myGen]() {
        QFileInfo info(path);
        if (!info.exists() || !info.isDir()) {
            if (myGen == m_scanGeneration.load()) {
                emit finished(path, false, myGen, QStringLiteral("Folder does not exist"));
            }
            return;
        }

        const QString canonicalPath = info.canonicalFilePath();
        QDir dir(canonicalPath);
        if (!dir.isReadable()) {
            if (myGen == m_scanGeneration.load()) {
                emit finished(path, false, myGen, QStringLiteral("Folder is not readable"));
            }
            return;
        }

#ifdef Q_OS_WIN
        QString enumerationError;
        if (!canEnumerateDirectoryWindows(canonicalPath, &enumerationError)) {
            if (myGen == m_scanGeneration.load()) {
                emit finished(path, false, myGen,
                              enumerationError.isEmpty()
                                  ? QStringLiteral("Folder is not readable")
                                  : enumerationError);
            }
            return;
        }
#endif

        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
        if (m_showHidden) {
            filters |= QDir::Hidden;
        }

        QList<FileEntry> batch;
        batch.reserve(512);
        QDirIterator it(dir.absolutePath(), filters);

        while (it.hasNext()) {
            it.next();
            if (myGen != m_scanGeneration.load()) {
                return;
            }

            QFileInfo fileInfo = it.fileInfo();
            const bool isHidden = fileInfo.isHidden() || fileInfo.fileName().startsWith('.');
            if (!m_showHidden && isHidden) {
                continue;
            }

            FileEntry entry = entryFromInfo(fileInfo);
            batch.append(entry);

            if (batch.size() >= 512) {
                emit batchReady(batch, myGen);
                batch.clear();
            }
        }

        if (!batch.isEmpty()) {
            emit batchReady(batch, myGen);
        }

        emit finished(canonicalPath, true, myGen);
    }));
}

void LocalFileProvider::cancel()
{
    ++m_scanGeneration;
}

bool LocalFileProvider::isRunning() const
{
    return m_watcher.isRunning();
}

QString LocalFileProvider::currentPath() const
{
    return m_currentPath;
}

int LocalFileProvider::currentGeneration() const
{
    return m_scanGeneration.load();
}

bool LocalFileProvider::renamePath(const QString &oldPath, const QString &newName)
{
    clearLastError();
    const QString trimmedName = newName.trimmed();
    if (oldPath.isEmpty() || trimmedName.isEmpty()) {
        setLastError(QStringLiteral("Cannot rename %1: the name is empty")
                         .arg(QDir::toNativeSeparators(oldPath)));
        return false;
    }

    QFileInfo oldInfo(oldPath);
    if (oldInfo.fileName() == trimmedName) {
        return true;
    }

    if (trimmedName.contains('/') || trimmedName.contains('\\')) {
        setLastError(QStringLiteral("Cannot rename %1: the name is invalid")
                         .arg(QDir::toNativeSeparators(oldPath)));
        return false;
    }

    const QString newPath = oldInfo.absoluteDir().filePath(trimmedName);
    if (QFileInfo::exists(newPath)) {
        setLastError(QStringLiteral("Cannot rename %1: an item with the same name already exists")
                         .arg(QDir::toNativeSeparators(oldPath)));
        return false;
    }

#ifdef Q_OS_WIN
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(oldPath).utf16()),
                     reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(newPath).utf16()),
                     MOVEFILE_COPY_ALLOWED)) {
        setLastError(windowsMutationErrorMessage(FileMutationKind::Rename, oldPath, GetLastError()));
        return false;
    }
    return true;
#else
    if (!QFile::rename(oldPath, newPath)) {
        setLastError(QStringLiteral("Cannot rename %1").arg(QDir::toNativeSeparators(oldPath)));
        return false;
    }
    return true;
#endif
}

bool LocalFileProvider::createFolder(const QString &parentPath, const QString &name, QString *createdPath)
{
    clearLastError();
    QDir dir(parentPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        setLastError(QStringLiteral("Cannot create folder in %1")
                         .arg(QDir::toNativeSeparators(parentPath)));
        return false;
    }

    QString folderName = name.trimmed();
    if (folderName.isEmpty()) {
        setLastError(QStringLiteral("Cannot create folder in %1: the name is empty")
                         .arg(QDir::toNativeSeparators(parentPath)));
        return false;
    }

    if (dir.exists(folderName)) {
        for (int i = 1; i < 1000; ++i) {
            const QString candidate = QStringLiteral("%1 (%2)").arg(folderName).arg(i);
            if (!dir.exists(candidate)) {
                folderName = candidate;
                break;
            }
        }
    }

#ifdef Q_OS_WIN
    const QString folderPath = dir.absoluteFilePath(folderName);
    if (!CreateDirectoryW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(folderPath).utf16()), nullptr)) {
        setLastError(windowsMutationErrorMessage(FileMutationKind::CreateFolder, folderPath, GetLastError()));
        return false;
    }
#else
    if (!dir.mkdir(folderName)) {
        setLastError(QStringLiteral("Cannot create folder in %1").arg(QDir::toNativeSeparators(parentPath)));
        return false;
    }
#endif

    if (createdPath) {
        *createdPath = dir.absoluteFilePath(folderName);
    }
    return true;
}

bool LocalFileProvider::createFile(const QString &parentPath, const QString &name, QString *createdPath)
{
    clearLastError();
    QDir dir(parentPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        setLastError(QStringLiteral("Cannot create file in %1")
                         .arg(QDir::toNativeSeparators(parentPath)));
        return false;
    }

    QString fileName = name.trimmed();
    if (fileName.isEmpty()) {
        setLastError(QStringLiteral("Cannot create file in %1: the name is empty")
                         .arg(QDir::toNativeSeparators(parentPath)));
        return false;
    }

    if (dir.exists(fileName)) {
        const int dot = fileName.lastIndexOf(QChar('.'));
        const QString base = (dot > 0) ? fileName.left(dot) : fileName;
        const QString ext = (dot > 0) ? fileName.mid(dot) : QString();
        for (int i = 1; i < 1000; ++i) {
            const QString candidate = ext.isEmpty()
                ? QStringLiteral("%1 (%2)").arg(base).arg(i)
                : QStringLiteral("%1 (%2)%3").arg(base).arg(i).arg(ext);
            if (!dir.exists(candidate)) {
                fileName = candidate;
                break;
            }
        }
    }

    const QString filePath = dir.absoluteFilePath(fileName);
#ifdef Q_OS_WIN
    HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(filePath).utf16()),
                                GENERIC_WRITE,
                                FILE_SHARE_READ,
                                nullptr,
                                CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        setLastError(windowsMutationErrorMessage(FileMutationKind::CreateFile, filePath, GetLastError()));
        return false;
    }
    CloseHandle(handle);
#else
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        setLastError(QStringLiteral("Cannot create file in %1: %2")
                         .arg(QDir::toNativeSeparators(parentPath), file.errorString()));
        return false;
    }
    file.close();
#endif

    if (createdPath) {
        *createdPath = filePath;
    }
    return true;
}

QString LocalFileProvider::lastErrorString() const
{
    return m_lastError;
}

void LocalFileProvider::clearLastError() const
{
    m_lastError.clear();
}

void LocalFileProvider::setLastError(const QString &error) const
{
    m_lastError = error;
}
