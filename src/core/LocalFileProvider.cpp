#include "LocalFileProvider.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QStringList>
#include <QtConcurrent>
#include <optional>

namespace {
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

FileEntry entryFromInfo(const QFileInfo &fileInfo)
{
    FileEntry entry;
    entry.name = fileInfo.fileName();
    entry.path = fileInfo.absoluteFilePath();
    entry.suffix = fileInfo.suffix();
    entry.size = fileInfo.size();
    entry.modified = fileInfo.lastModified();
    entry.isDirectory = fileInfo.isDir();
    entry.isHidden = fileInfo.isHidden();

    QLocale loc;
    entry.sizeText = entry.isDirectory
        ? QStringLiteral("Folder")
        : loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
    entry.modifiedText = loc.toString(entry.modified, QLocale::ShortFormat);
    entry.isImage = !entry.isDirectory && isImageSuffix(entry.suffix);
    return entry;
}
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
    return QDir().mkpath(QFileInfo(path).absolutePath());
}

bool LocalFileProvider::makePath(const QString &path) const
{
    return QDir().mkpath(path);
}

bool LocalFileProvider::removePath(const QString &path) const
{
    QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }

    if (info.isDir() && !info.isSymLink()) {
        return QDir(path).removeRecursively();
    }
    return QFile::remove(path);
}

QStringList LocalFileProvider::childPaths(const QString &path, bool includeHidden) const
{
    QStringList children;
    QDir dir(path);
    const QFileInfoList infos = dir.entryInfoList(
        QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System | (includeHidden ? QDir::Hidden : QDir::NoFilter));
    children.reserve(infos.size());
    for (const QFileInfo &info : infos) {
        if (!includeHidden && info.fileName().startsWith('.')) {
            continue;
        }
        children.append(info.absoluteFilePath());
    }
    return children;
}

bool LocalFileProvider::movePath(const QString &sourcePath, const QString &destinationPath) const
{
    if (sourcePath.isEmpty() || destinationPath.isEmpty()) {
        return false;
    }
    if (QFileInfo::exists(destinationPath)) {
        return false;
    }
    return QFile::rename(sourcePath, destinationPath);
}

std::unique_ptr<QIODevice> LocalFileProvider::openRead(const QString &path) const
{
    auto file = std::make_unique<QFile>(path);
    if (!file->open(QIODevice::ReadOnly)) {
        return nullptr;
    }
    return std::unique_ptr<QIODevice>(file.release());
}

std::unique_ptr<QIODevice> LocalFileProvider::openWrite(const QString &path, bool truncate) const
{
    auto file = std::make_unique<QFile>(path);
    QIODevice::OpenMode mode = QIODevice::WriteOnly;
    if (truncate) {
        mode |= QIODevice::Truncate;
    }
    if (!file->open(mode)) {
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
            if (!m_showHidden && fileInfo.fileName().startsWith('.')) {
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
    const QString trimmedName = newName.trimmed();
    if (oldPath.isEmpty() || trimmedName.isEmpty()) {
        return false;
    }

    QFileInfo oldInfo(oldPath);
    if (oldInfo.fileName() == trimmedName) {
        return true;
    }

    if (trimmedName.contains('/') || trimmedName.contains('\\')) {
        return false;
    }

    const QString newPath = oldInfo.absoluteDir().filePath(trimmedName);
    if (QFileInfo::exists(newPath)) {
        return false;
    }

    return QFile::rename(oldPath, newPath);
}

bool LocalFileProvider::createFolder(const QString &parentPath, const QString &name, QString *createdPath)
{
    QDir dir(parentPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QString folderName = name.trimmed();
    if (folderName.isEmpty()) {
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

    if (!dir.mkdir(folderName)) {
        return false;
    }

    if (createdPath) {
        *createdPath = dir.absoluteFilePath(folderName);
    }
    return true;
}

bool LocalFileProvider::createFile(const QString &parentPath, const QString &name, QString *createdPath)
{
    QDir dir(parentPath);
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QString fileName = name.trimmed();
    if (fileName.isEmpty()) {
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

    QFile file(dir.absoluteFilePath(fileName));
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.close();

    if (createdPath) {
        *createdPath = dir.absoluteFilePath(fileName);
    }
    return true;
}
