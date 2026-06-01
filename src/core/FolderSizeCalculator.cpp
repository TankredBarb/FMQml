#include "FolderSizeCalculator.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
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
    bool isDirectory = false;
    bool isReparseDirectory = false;
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
            isDirectory,
            isDirectory && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0
        });
    } while (FindNextFileW(handle, &findData));

    const DWORD code = GetLastError();
    FindClose(handle);
    return code == ERROR_NO_MORE_FILES;
}
#else
bool enumerateChildren(const QString &path, QList<SizeChildEntry> *children)
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
            isDirectory,
            isDirectory && info.isSymLink()
        });
    }
    return true;
}
#endif
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

    while (!stack.isEmpty()) {
        if (m_cancelled) {
            return;
        }

        const QString currentPath = stack.pop();
        QList<SizeChildEntry> children;
        if (!enumerateChildren(currentPath, &children)) {
            continue;
        }

        for (const SizeChildEntry &child : std::as_const(children)) {
            if (m_cancelled) {
                return;
            }

            if (child.isDirectory) {
                if (child.isReparseDirectory) {
                    continue;
                }
                ++folderCount;
                stack.push(child.path);
            } else {
                ++fileCount;
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
