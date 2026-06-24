#include "FolderSizeCalculator.h"
#ifdef Q_OS_LINUX
#include "LinuxFileEnumerator.h"
#endif

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSet>
#include <QStack>

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
struct SizeChildEntry {
    QString path;
    qint64 size = 0;
    quint64 device = 0;
    quint64 inode = 0;
    quint64 hardLinkCount = 0;
    bool isDirectory = false;
    bool isReparseDirectory = false;
    bool isMountBoundary = false;
};

#ifdef Q_OS_WIN
QString extendedLengthWindowsPath(const QString &path)
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

QString windowsSearchPattern(const QString &path)
{
    QString pattern = extendedLengthWindowsPath(path);
    if (!pattern.endsWith(QLatin1Char('\\'))) {
        pattern += QLatin1Char('\\');
    }
    pattern += QLatin1Char('*');
    return pattern;
}

HANDLE findFirstFileBasic(const QString &pattern, WIN32_FIND_DATAW *findData)
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

qint64 sizeFromFindData(const WIN32_FIND_DATAW &findData)
{
    ULARGE_INTEGER value{};
    value.LowPart = findData.nFileSizeLow;
    value.HighPart = findData.nFileSizeHigh;
    return static_cast<qint64>(value.QuadPart);
}

bool enumerateChildren(const QString &path, QList<SizeChildEntry> *children)
{
    WIN32_FIND_DATAW findData{};
    HANDLE handle = findFirstFileBasic(windowsSearchPattern(path), &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        return code == ERROR_FILE_NOT_FOUND || code == ERROR_NO_MORE_FILES;
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
        children->append({
            parentPrefix + QString::fromWCharArray(rawName),
            isDirectory ? 0 : sizeFromFindData(findData),
            0,
            0,
            0,
            isDirectory,
            isDirectory && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0,
            false
        });
    } while (FindNextFileW(handle, &findData));

    const DWORD code = GetLastError();
    FindClose(handle);
    return code == ERROR_NO_MORE_FILES;
}
#else
[[maybe_unused]] bool enumerateChildren(const QString &path, QList<SizeChildEntry> *children)
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
            isDirectory ? 0 : info.size(),
            0,
            0,
            0,
            isDirectory,
            isDirectory && info.isSymLink(),
            false
        });
    }
    return true;
}
#endif

#ifdef Q_OS_LINUX
bool enumerateChildrenLinux(const QString &path, QList<SizeChildEntry> *children, bool stayOnRootDevice, dev_t rootDevice)
{
    LinuxFileEnumerator::Options options;
    options.includeHidden = true;
    options.stayOnRootDevice = stayOnRootDevice;
    options.rootDevice = rootDevice;

    QList<LinuxFileEnumerator::Entry> entries;
    QString error;
    if (!LinuxFileEnumerator::enumerateChildren(path, options, &entries, &error)) {
        return false;
    }

    children->reserve(children->size() + entries.size());
    for (const LinuxFileEnumerator::Entry &entry : std::as_const(entries)) {
        children->append({
            entry.path,
            entry.apparentSize,
            entry.device,
            entry.inode,
            entry.hardLinkCount,
            entry.isDirectory,
            entry.isDirectory && entry.isSymlink,
            entry.isMountBoundary
        });
    }
    return true;
}
#endif

bool enumerateChildrenForPlatform(const QString &path, QList<SizeChildEntry> *children, bool stayOnRootDevice, quint64 rootDevice)
{
#ifdef Q_OS_LINUX
    return enumerateChildrenLinux(path, children, stayOnRootDevice, static_cast<dev_t>(rootDevice));
#else
    Q_UNUSED(stayOnRootDevice)
    Q_UNUSED(rootDevice)
    return enumerateChildren(path, children);
#endif
}
}

void FolderSizeCalculator::run()
{
    qint64 totalSize = 0;
    int fileCount = 0;
    int folderCount = 0;

    QFileInfo rootInfo(m_path);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        emit resultReady(0, 0, 0, m_generation);
        return;
    }

    QElapsedTimer timer;
    timer.start();

    QStack<QString> stack;
    stack.push(rootInfo.absoluteFilePath());
    QSet<QString> countedHardLinks;
    bool stayOnRootDevice = QDir::cleanPath(rootInfo.absoluteFilePath()) == QLatin1String("/");
    quint64 rootDevice = 0;
#ifdef Q_OS_LINUX
    totalSize += LinuxFileEnumerator::apparentSizeForPath(rootInfo.absoluteFilePath());
    if (stayOnRootDevice) {
        const std::optional<dev_t> device = LinuxFileEnumerator::deviceForPath(rootInfo.absoluteFilePath());
        stayOnRootDevice = device.has_value();
        rootDevice = static_cast<quint64>(device.value_or(0));
    }
#endif

    while (!stack.isEmpty()) {
        if (m_cancelled) {
            return;
        }

        const QString currentPath = stack.pop();
        QList<SizeChildEntry> children;
        if (!enumerateChildrenForPlatform(currentPath, &children, stayOnRootDevice, rootDevice)) {
            continue;
        }

        for (const SizeChildEntry &child : std::as_const(children)) {
            if (m_cancelled) {
                return;
            }

            if (child.isDirectory) {
                if (child.isReparseDirectory) {
                    totalSize += child.size;
                    continue;
                }
                if (child.isMountBoundary) {
                    continue;
                }
                ++folderCount;
                totalSize += child.size;
                stack.push(child.path);
            } else {
                ++fileCount;
                if (child.hardLinkCount > 1 && child.device != 0 && child.inode != 0) {
                    const QString key = QString::number(child.device) + QLatin1Char(':') + QString::number(child.inode);
                    if (countedHardLinks.contains(key)) {
                        continue;
                    }
                    countedHardLinks.insert(key);
                }
                totalSize += child.size;
            }

            if (timer.elapsed() > 250) {
                emit progressUpdate(totalSize, fileCount, folderCount, m_generation);
                timer.restart();
            }
        }
    }

    emit resultReady(totalSize, fileCount, folderCount, m_generation);
}
