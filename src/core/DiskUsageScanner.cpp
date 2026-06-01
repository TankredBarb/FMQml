#include "DiskUsageScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QStack>
#include <algorithm>

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
struct ChildEntry {
    QString path;
    QString name;
    qint64 size = 0;
    bool isDirectory = false;
    bool isReparseDirectory = false;
};

struct Frame {
    QString path;
    QString name;
    bool entered = false;
    bool root = false;
    DiskUsageScanner::Totals totals;
    QList<Frame> children;
    int nextChildIndex = 0;
};

QString displayNameForPath(const QString &path)
{
    QFileInfo info(path);
    QString name = info.fileName();
    return name.isEmpty() ? QDir::toNativeSeparators(path) : name;
}

void insertTop(QList<DiskUsageEntry> &entries, const DiskUsageEntry &entry, int maxResults)
{
    if (entry.size <= 0 && entries.size() >= maxResults) {
        return;
    }

    auto pos = std::lower_bound(entries.begin(), entries.end(), entry,
        [](const DiskUsageEntry &left, const DiskUsageEntry &right) {
            if (left.size != right.size) {
                return left.size > right.size;
            }
            return left.path.compare(right.path, Qt::CaseInsensitive) < 0;
        });
    entries.insert(pos, entry);
    if (entries.size() > maxResults) {
        entries.removeLast();
    }
}

#ifdef Q_OS_WIN
QString diskUsageExtendedLengthWindowsPath(const QString &path)
{
    QString nativePath = QDir::toNativeSeparators(path);
    if (nativePath.startsWith(QStringLiteral("\\\\?\\"))) {
        return nativePath;
    }
    if (nativePath.startsWith(QStringLiteral("\\\\"))) {
        return QStringLiteral("\\\\?\\UNC\\") + nativePath.mid(2);
    }
    return QStringLiteral("\\\\?\\") + nativePath;
}

QString diskUsageWindowsSearchPattern(const QString &path)
{
    QString pattern = diskUsageExtendedLengthWindowsPath(path);
    if (!pattern.endsWith(QLatin1Char('\\'))) {
        pattern += QLatin1Char('\\');
    }
    pattern += QLatin1Char('*');
    return pattern;
}

HANDLE diskUsageFindFirstFileBasic(const QString &pattern, WIN32_FIND_DATAW *findData)
{
    HANDLE handle = FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()),
                                     FindExInfoBasic,
                                     findData,
                                     FindExSearchNameMatch,
                                     nullptr,
                                     FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER) {
        handle = FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()),
                                  FindExInfoBasic,
                                  findData,
                                  FindExSearchNameMatch,
                                  nullptr,
                                  0);
    }
    return handle;
}

qint64 diskUsageSizeFromFindData(const WIN32_FIND_DATAW &findData)
{
    ULARGE_INTEGER value{};
    value.LowPart = findData.nFileSizeLow;
    value.HighPart = findData.nFileSizeHigh;
    return static_cast<qint64>(value.QuadPart);
}

QString diskUsageWindowsErrorText(DWORD errorCode)
{
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
    return detail.isEmpty() ? QStringLiteral("Windows error %1").arg(errorCode) : detail;
}

bool enumerateChildrenNative(const QString &path, QList<ChildEntry> *children, QString *error)
{
    WIN32_FIND_DATAW findData{};
    HANDLE handle = diskUsageFindFirstFileBasic(diskUsageWindowsSearchPattern(path), &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_NO_MORE_FILES) {
            return true;
        }
        if (error) {
            *error = diskUsageWindowsErrorText(code);
        }
        return false;
    }

    QString parentPrefix = QDir::fromNativeSeparators(path);
    if (!parentPrefix.endsWith(QLatin1Char('/'))) {
        parentPrefix += QLatin1Char('/');
    }

    do {
        const wchar_t *rawName = findData.cFileName;
        if (rawName[0] == L'.'
            && (rawName[1] == L'\0' || (rawName[1] == L'.' && rawName[2] == L'\0'))) {
            continue;
        }

        const DWORD attributes = findData.dwFileAttributes;
        const bool isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const bool isReparse = isDirectory && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        const QString name = QString::fromWCharArray(rawName);
        children->append({
            parentPrefix + name,
            name,
            isDirectory ? 0 : diskUsageSizeFromFindData(findData),
            isDirectory,
            isReparse
        });
    } while (FindNextFileW(handle, &findData));

    const DWORD code = GetLastError();
    FindClose(handle);
    if (code != ERROR_NO_MORE_FILES) {
        if (error) {
            *error = diskUsageWindowsErrorText(code);
        }
        return false;
    }
    return true;
}
#endif

bool enumerateChildrenQt(const QString &path, QList<ChildEntry> *children, QString *error)
{
    QDirIterator it(path,
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const bool isDirectory = info.isDir();
        children->append({
            info.absoluteFilePath(),
            info.fileName(),
            isDirectory ? 0 : info.size(),
            isDirectory,
            isDirectory && info.isSymLink()
        });
    }
    Q_UNUSED(error)
    return true;
}

bool enumerateChildren(const QString &path, QList<ChildEntry> *children, QString *error)
{
#ifdef Q_OS_WIN
    return enumerateChildrenNative(path, children, error);
#else
    return enumerateChildrenQt(path, children, error);
#endif
}
}

DiskUsageScanner::DiskUsageScanner(const QString &rootPath, int generation, int maxResults)
    : m_rootPath(QDir::fromNativeSeparators(rootPath))
    , m_generation(generation)
    , m_maxResults(maxResults)
{
    setAutoDelete(false);
}

void DiskUsageScanner::cancel()
{
    m_cancelled = true;
}

void DiskUsageScanner::run()
{
    QFileInfo rootInfo(m_rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        emit finished(false, QStringLiteral("Folder does not exist"), {}, {}, {}, 0, 0, 0, 0, 0, 0, {}, {}, m_generation);
        return;
    }

    QStack<Frame> stack;
    stack.push({rootInfo.absoluteFilePath(), displayNameForPath(rootInfo.absoluteFilePath()), false, true, {}, {}, 0});

    while (!stack.isEmpty()) {
        if (m_cancelled) {
            emit finished(false, {}, m_topFolders, m_topFiles, m_rootChildren, m_totalBytes, m_scannedFiles, m_scannedFolders, m_skippedPaths, m_inaccessiblePaths, m_reparsePaths, m_inaccessiblePathDetails, m_reparsePathDetails, m_generation);
            return;
        }

        if (!stack.top().entered) {
            stack.top().entered = true;
            const QString framePath = stack.top().path;
            const bool frameIsRoot = stack.top().root;
            m_currentPath = framePath;

            QList<ChildEntry> children;
            QString error;
            if (!enumerateChildren(framePath, &children, &error)) {
                ++m_skippedPaths;
                ++m_inaccessiblePaths;
                m_lastError = error.isEmpty()
                    ? QStringLiteral("Cannot read %1").arg(QDir::toNativeSeparators(framePath))
                    : QStringLiteral("%1: %2").arg(QDir::toNativeSeparators(framePath), error);
                addSkippedDetail(m_inaccessiblePathDetails, m_lastError);
                if (frameIsRoot) {
                    emit finished(false, m_lastError, {}, {}, {}, 0, 0, 0, m_skippedPaths, m_inaccessiblePaths, m_reparsePaths, m_inaccessiblePathDetails, m_reparsePathDetails, m_generation);
                    return;
                }
                stack.pop();
                emitSnapshotIfNeeded(false);
                continue;
            }

            qint64 localBytes = 0;
            int localFiles = 0;
            for (const ChildEntry &child : std::as_const(children)) {
                if (m_cancelled) {
                    emit finished(false, {}, m_topFolders, m_topFiles, m_rootChildren, m_totalBytes, m_scannedFiles, m_scannedFolders, m_skippedPaths, m_inaccessiblePaths, m_reparsePaths, m_inaccessiblePathDetails, m_reparsePathDetails, m_generation);
                    return;
                }
                if (child.isDirectory) {
                    if (child.isReparseDirectory) {
                        ++m_skippedPaths;
                        ++m_reparsePaths;
                        addSkippedDetail(m_reparsePathDetails, QDir::toNativeSeparators(child.path));
                        continue;
                    }
                    stack.top().children.append({child.path, child.name, false, false, {}, {}, 0});
                    continue;
                }

                ++localFiles;
                ++m_scannedFiles;
                localBytes += child.size;
                m_totalBytes += child.size;
                const DiskUsageEntry fileEntry{child.path, child.name, child.size, false, 1, 0};
                addFileCandidate(fileEntry);
                if (frameIsRoot) {
                    addRootChildCandidate(fileEntry);
                }
            }
            stack.top().totals.files += localFiles;
            stack.top().totals.bytes += localBytes;
            emitSnapshotIfNeeded(false);
            continue;
        }

        if (stack.top().nextChildIndex < stack.top().children.size()) {
            const Frame child = stack.top().children.at(stack.top().nextChildIndex++);
            stack.push(child);
            continue;
        }

        Frame completed = stack.pop();
        const bool parentIsRoot = !stack.isEmpty() && stack.top().root;
        if (!completed.root) {
            ++m_scannedFolders;
            const DiskUsageEntry folderEntry{completed.path,
                                             completed.name,
                                             completed.totals.bytes,
                                             true,
                                             completed.totals.files,
                                             completed.totals.folders};
            addFolderCandidate(folderEntry);
            if (parentIsRoot) {
                addRootChildCandidate(folderEntry);
            }
        }

        if (!stack.isEmpty()) {
            Frame &parent = stack.top();
            parent.totals.bytes += completed.totals.bytes;
            parent.totals.files += completed.totals.files;
            parent.totals.folders += completed.totals.folders + (completed.root ? 0 : 1);
        }
        emitSnapshotIfNeeded(false);
    }

    emitSnapshotIfNeeded(true);
    emit finished(true,
                  {},
                  m_topFolders,
                  m_topFiles,
                  m_rootChildren,
                  m_totalBytes,
                  m_scannedFiles,
                  m_scannedFolders,
                  m_skippedPaths,
                  m_inaccessiblePaths,
                  m_reparsePaths,
                  m_inaccessiblePathDetails,
                  m_reparsePathDetails,
                  m_generation);
}

void DiskUsageScanner::addFolderCandidate(const DiskUsageEntry &entry)
{
    insertTop(m_topFolders, entry, m_maxResults);
}

void DiskUsageScanner::addFileCandidate(const DiskUsageEntry &entry)
{
    insertTop(m_topFiles, entry, m_maxResults);
}

void DiskUsageScanner::addRootChildCandidate(const DiskUsageEntry &entry)
{
    insertTop(m_rootChildren, entry, 500);
}

void DiskUsageScanner::addSkippedDetail(QStringList &details, const QString &detail)
{
    constexpr int maxSkippedDetails = 200;
    if (detail.isEmpty() || details.size() >= maxSkippedDetails) {
        return;
    }
    details.append(detail);
}

void DiskUsageScanner::emitSnapshotIfNeeded(bool force)
{
    static thread_local QElapsedTimer timer;
    static thread_local bool timerStarted = false;
    if (!timerStarted) {
        timer.start();
        timerStarted = true;
    }

    const qint64 now = timer.elapsed();
    if (!force && now - m_lastSnapshotMsec < 250) {
        return;
    }
    m_lastSnapshotMsec = now;
    emit snapshotReady(m_topFolders,
                       m_topFiles,
                       m_rootChildren,
                       m_totalBytes,
                       m_scannedFiles,
                       m_scannedFolders,
                       m_skippedPaths,
                       m_inaccessiblePaths,
                       m_reparsePaths,
                       m_inaccessiblePathDetails,
                       m_reparsePathDetails,
                       m_currentPath,
                       m_lastError,
                       m_generation);
}
