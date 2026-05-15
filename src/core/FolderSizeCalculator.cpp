#include "FolderSizeCalculator.h"
#include <QDir>
#include <QElapsedTimer>
#include <QSet>
#include <QFileInfo>
#include <QStack>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <sys/stat.h>
#endif

struct DirEntry {
    QString path;
    quint64 fileId;
};

quint64 getFileId(const QString &path);

void FolderSizeCalculator::run() {
    qint64 totalSize = 0;
    QElapsedTimer timer;
    timer.start();

    QSet<quint64> visitedDirIds;
    QStack<DirEntry> dirStack;
    
    QFileInfo rootInfo(m_path);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        emit resultReady(0, m_generation);
        return;
    }

    quint64 rootId = getFileId(rootInfo.absoluteFilePath());
    dirStack.push({rootInfo.absoluteFilePath(), rootId});
    visitedDirIds.insert(rootId);

    while (!dirStack.isEmpty()) {
        if (m_cancelled) {
            return;
        }

        DirEntry current = dirStack.pop();
        QDir dir(current.path);

        QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
            QDir::DirsLast
        );

        for (const QFileInfo &fileInfo : entries) {
            if (m_cancelled) {
                return;
            }

            const bool isDirectory = fileInfo.isDir();
            const bool isSymLink = fileInfo.isSymLink();
            const qint64 fileSize = fileInfo.size();

            if (isDirectory) {
                if (isSymLink) {
                    continue;
                }

                quint64 dirId = getFileId(fileInfo.absoluteFilePath());
                
                if (dirId > 0 && !visitedDirIds.contains(dirId)) {
                    visitedDirIds.insert(dirId);
                    dirStack.push({fileInfo.absoluteFilePath(), dirId});
                }
            } else {
                totalSize += fileSize;
            }

            if (timer.elapsed() > 500) {
                emit progressUpdate(totalSize, m_generation);
                timer.restart();
            }
        }
    }

    emit resultReady(totalSize, m_generation);
}

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
        fileId = (static_cast<quint64>(fileInfo.dwVolumeSerialNumber) << 32) | 
                 (static_cast<quint64>(fileInfo.nFileIndexHigh) << 32 | fileInfo.nFileIndexLow);
    }
    
    CloseHandle(hFile);
    return fileId;
}
#else
quint64 getFileId(const QString &path) {
    struct stat st;
    if (stat(path.toStdString().c_str(), &st) == 0) {
        return st.st_ino;
    }
    return 0;
}
#endif
