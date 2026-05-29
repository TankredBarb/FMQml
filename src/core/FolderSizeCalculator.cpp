#include "FolderSizeCalculator.h"
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QSet>
#include <QFileInfo>
#include <QStack>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace {
#ifdef Q_OS_WIN
quint64 getFileId(const QString &path) {
    HANDLE hFile = CreateFileW(
        reinterpret_cast<const wchar_t*>(path.utf16()),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );
    
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }

    BY_HANDLE_FILE_INFORMATION fileInfo;
    quint64 fileId = 0;
    
    if (GetFileInformationByHandle(hFile, &fileInfo)) {
        // More robust ID: VolumeSerialNumber + Index (high/low)
        // We use a simpler 64-bit hash of volume + index for QSet performance
        fileId = (static_cast<quint64>(fileInfo.nFileIndexHigh) << 32) | static_cast<quint64>(fileInfo.nFileIndexLow);
    }
    
    CloseHandle(hFile);
    return fileId;
}
#else
quint64 getFileId(const QString &path) {
    struct stat st;
    if (lstat(path.toLocal8Bit().constData(), &st) == 0) {
        return static_cast<quint64>(st.st_ino);
    }
    return 0;
}
#endif
}

void FolderSizeCalculator::run() {
    qint64 totalSize = 0;
    int fileCount = 0;
    int folderCount = 0;
    
    QElapsedTimer timer;
    timer.start();

    QSet<quint64> visitedDirIds;
    QStack<QString> dirStack;
    
    QFileInfo rootInfo(m_path);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        emit resultReady(0, 0, 0, m_generation);
        return;
    }

    dirStack.push(rootInfo.absoluteFilePath());
    quint64 rootId = getFileId(rootInfo.absoluteFilePath());
    if (rootId > 0) visitedDirIds.insert(rootId);

    while (!dirStack.isEmpty()) {
        if (m_cancelled) return;

        QString currentPath = dirStack.pop();
        QDir dir(currentPath);

        // Using QDirIterator for better performance and memory efficiency in large folders
        QDirIterator it(currentPath, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden, QDirIterator::NoIteratorFlags);

        while (it.hasNext()) {
            if (m_cancelled) return;

            it.next();
            QFileInfo fileInfo = it.fileInfo();
            
            if (fileInfo.isDir()) {
                if (fileInfo.isSymLink()) {
                    fileCount++; // Count symlink as a file entry
                    continue;
                }

                folderCount++;
                quint64 dirId = getFileId(fileInfo.absoluteFilePath());
                if (dirId == 0 || !visitedDirIds.contains(dirId)) {
                    if (dirId > 0) visitedDirIds.insert(dirId);
                    dirStack.push(fileInfo.absoluteFilePath());
                }
            } else {
                fileCount++;
                totalSize += fileInfo.size();
            }

            if (timer.elapsed() > 250) {
                emit progressUpdate(totalSize, fileCount, folderCount, m_generation);
                timer.restart();
            }
        }
    }

    emit resultReady(totalSize, fileCount, folderCount, m_generation);
}
