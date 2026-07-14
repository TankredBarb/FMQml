#include "FolderCompareScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QDateTime>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QCryptographicHash>
#include <QSet>
#include <QStack>

#include <algorithm>
#include <optional>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

#ifdef Q_OS_LINUX
#include "LinuxFileEnumerator.h"
#endif

namespace {

struct ScannedEntry {
    QString path;
    qint64 size = 0;
    QDateTime modified;
    bool directory = false;
    bool symlink = false;
    bool inaccessible = false;
    QString linkTarget;
};

using EntryMap = QHash<QString, ScannedEntry>;

bool isCancelled(const std::atomic_bool *cancelled)
{
    return cancelled && cancelled->load();
}

QString linkTargetForPath(const QString &path)
{
#ifdef Q_OS_UNIX
    const QByteArray encodedPath = QFile::encodeName(path);
    QByteArray target(4096, Qt::Uninitialized);
    const ssize_t length = ::readlink(encodedPath.constData(), target.data(), static_cast<size_t>(target.size()));
    return length < 0 ? QString() : QString::fromLocal8Bit(target.constData(), static_cast<qsizetype>(length));
#else
    return QFileInfo(path).symLinkTarget();
#endif
}

bool collectEntries(const QString &root, const FolderCompareOptions &options,
                    EntryMap *entries, const std::atomic_bool *cancelled, QString *error)
{
    const QDir rootDir(root);
#ifdef Q_OS_LINUX
    LinuxFileEnumerator::Options enumerationOptions;
    enumerationOptions.includeHidden = options.includeHidden;
    if (const auto rootDevice = LinuxFileEnumerator::deviceForPath(root)) {
        enumerationOptions.stayOnRootDevice = true;
        enumerationOptions.rootDevice = *rootDevice;
    }
    QStack<QString> pending;
    pending.push(root);
    while (!pending.isEmpty()) {
        if (isCancelled(cancelled)) return false;
        const QString folder = pending.pop();
        QList<LinuxFileEnumerator::Entry> children;
        if (!LinuxFileEnumerator::enumerateChildren(folder, enumerationOptions, &children, error)) {
            if (folder == root) return false;
            const QString relativePath = QDir::cleanPath(rootDir.relativeFilePath(folder));
            auto inaccessibleEntry = entries->find(relativePath);
            if (inaccessibleEntry != entries->end()) inaccessibleEntry->inaccessible = true;
            if (error) error->clear();
            continue;
        }
        for (const LinuxFileEnumerator::Entry &entry : std::as_const(children)) {
            if (isCancelled(cancelled)) return false;
            const QString relativePath = QDir::cleanPath(rootDir.relativeFilePath(entry.path));
            entries->insert(relativePath, {entry.path, entry.size, entry.modified.toUTC(), entry.isDirectory && !entry.isSymlink,
                                           entry.isSymlink, entry.isLocked && !entry.isSymlink,
                                           entry.isSymlink ? linkTargetForPath(entry.path) : QString()});
            if (options.recursive && entry.isDirectory && !entry.isSymlink && !entry.isMountBoundary && !entry.isLocked) {
                pending.push(entry.path);
            }
        }
    }
    return true;
#else
    const QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System
        | (options.includeHidden ? QDir::Hidden : QDir::NoFilter);
    const auto iteratorFlags = options.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
    QDirIterator it(root, filters, iteratorFlags);
    while (it.hasNext()) {
        if (isCancelled(cancelled)) return false;
        it.next();
        const QFileInfo info = it.fileInfo();
        if (info.fileName() == QLatin1String(".") || info.fileName() == QLatin1String("..")) continue;
        const QString relativePath = QDir::cleanPath(rootDir.relativeFilePath(info.absoluteFilePath()));
        entries->insert(relativePath, {info.absoluteFilePath(), info.size(), info.lastModified().toUTC(), info.isDir() && !info.isSymLink(),
                                       info.isSymLink(), !info.isSymLink() && !info.isReadable(),
                                       info.isSymLink() ? linkTargetForPath(info.absoluteFilePath()) : QString()});
    }
    if (it.hasNext()) {
        *error = QStringLiteral("Cannot read %1").arg(QDir::toNativeSeparators(root));
        return false;
    }
    return true;
#endif
}

FolderCompareState compareEntries(const ScannedEntry *left, const ScannedEntry *right,
                                  const FolderCompareOptions &options,
                                  const std::atomic_bool *cancelled)
{
    if (left && left->inaccessible) return FolderCompareState::InaccessibleLeft;
    if (right && right->inaccessible) return FolderCompareState::InaccessibleRight;
    if (!left) return FolderCompareState::RightOnly;
    if (!right) return FolderCompareState::LeftOnly;
    if (left->symlink || right->symlink) {
        return left->symlink && right->symlink && left->linkTarget == right->linkTarget
            ? FolderCompareState::EqualMetadata : FolderCompareState::LinkConflict;
    }
    if (left->directory != right->directory) return FolderCompareState::TypeConflict;
    if (left->directory) return FolderCompareState::EqualMetadata;
    const auto timestampState = [&]() -> std::optional<FolderCompareState> {
        if (!options.compareTimestamps) return std::nullopt;
        const qint64 delta = left->modified.secsTo(right->modified);
        if (qAbs(delta) > options.timestampToleranceSeconds) {
            return delta > 0 ? std::optional(FolderCompareState::RightNewer)
                             : std::optional(FolderCompareState::LeftNewer);
        }
        return std::nullopt;
    };
    if (left->size != right->size) return timestampState().value_or(FolderCompareState::DifferentSize);
    if (options.compareContents) {
        QFile leftFile(left->path);
        QFile rightFile(right->path);
        if (!leftFile.open(QIODevice::ReadOnly)) return FolderCompareState::InaccessibleLeft;
        if (!rightFile.open(QIODevice::ReadOnly)) return FolderCompareState::InaccessibleRight;
        QCryptographicHash leftHash(QCryptographicHash::Sha256), rightHash(QCryptographicHash::Sha256);
        while (!leftFile.atEnd() && !rightFile.atEnd()) {
            if (isCancelled(cancelled)) return FolderCompareState::DifferentContent;
            const QByteArray leftChunk = leftFile.read(1024 * 1024);
            const QByteArray rightChunk = rightFile.read(1024 * 1024);
            if (leftFile.error() != QFileDevice::NoError) return FolderCompareState::InaccessibleLeft;
            if (rightFile.error() != QFileDevice::NoError) return FolderCompareState::InaccessibleRight;
            if (leftChunk.isEmpty() != rightChunk.isEmpty()
                || (leftChunk.isEmpty() && (!leftFile.atEnd() || !rightFile.atEnd()))) {
                return FolderCompareState::DifferentContent;
            }
            leftHash.addData(leftChunk);
            rightHash.addData(rightChunk);
        }
        if (leftHash.result() == rightHash.result()) return FolderCompareState::EqualContent;
        return timestampState().value_or(FolderCompareState::DifferentContent);
    }
    return timestampState().value_or(FolderCompareState::EqualMetadata);
}

} // namespace

FolderCompareResult FolderCompareScanner::compare(const QString &leftRoot, const QString &rightRoot,
                                                   const FolderCompareOptions &options,
                                                   const std::atomic_bool *cancelled)
{
    FolderCompareResult result;
    const QString left = QFileInfo(leftRoot).absoluteFilePath();
    const QString right = QFileInfo(rightRoot).absoluteFilePath();
    if (!QFileInfo(left).isDir() || !QFileInfo(right).isDir()) {
        result.error = QStringLiteral("Both locations must be local folders.");
        return result;
    }

    EntryMap leftEntries;
    EntryMap rightEntries;
    QString error;
    if (!collectEntries(left, options, &leftEntries, cancelled, &error)) {
        result.cancelled = isCancelled(cancelled);
        result.inaccessibleLeft = result.cancelled ? 0 : 1;
        result.error = error;
        return result;
    }
    if (!collectEntries(right, options, &rightEntries, cancelled, &error)) {
        result.cancelled = isCancelled(cancelled);
        result.inaccessibleRight = result.cancelled ? 0 : 1;
        result.error = error;
        return result;
    }

    QSet<QString> paths(leftEntries.keyBegin(), leftEntries.keyEnd());
    paths.unite(QSet<QString>(rightEntries.keyBegin(), rightEntries.keyEnd()));
    QStringList orderedPaths = paths.values();
    std::sort(orderedPaths.begin(), orderedPaths.end(), [](const QString &a, const QString &b) {
        return QString::compare(a, b, Qt::CaseSensitive) < 0;
    });
    for (const QString &relativePath : orderedPaths) {
        if (isCancelled(cancelled)) { result.cancelled = true; return result; }
        const auto leftIt = leftEntries.constFind(relativePath);
        const auto rightIt = rightEntries.constFind(relativePath);
        const ScannedEntry *leftEntry = leftIt == leftEntries.cend() ? nullptr : &leftIt.value();
        const ScannedEntry *rightEntry = rightIt == rightEntries.cend() ? nullptr : &rightIt.value();
        FolderCompareEntry entry;
        entry.relativePath = relativePath;
        entry.state = compareEntries(leftEntry, rightEntry, options, cancelled);
        if (entry.state == FolderCompareState::InaccessibleLeft) ++result.inaccessibleLeft;
        if (entry.state == FolderCompareState::InaccessibleRight) ++result.inaccessibleRight;
        if (isCancelled(cancelled)) { result.cancelled = true; return result; }
        if (leftEntry) { entry.leftPath = leftEntry->path; entry.leftSize = leftEntry->size; entry.leftModified = leftEntry->modified; entry.leftDirectory = leftEntry->directory; entry.leftSymlink = leftEntry->symlink; }
        if (rightEntry) { entry.rightPath = rightEntry->path; entry.rightSize = rightEntry->size; entry.rightModified = rightEntry->modified; entry.rightDirectory = rightEntry->directory; entry.rightSymlink = rightEntry->symlink; }
        result.entries.append(entry);
    }
    return result;
}
