// NOTE: This file is currently not registered in CMakeLists.txt and not used in the project.
// It is a candidate for deletion if Windows-native scanning is no longer planned.

#include "WinLocalFileProvider.h"

#ifdef Q_OS_WIN

#include <QtConcurrent>
#include <QElapsedTimer>
#include <QLocale>
#include <QDir>
#include <QFileInfo>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------
namespace {

static const QStringList kImageSuffixes = {
    QStringLiteral("jpg"),  QStringLiteral("jpeg"), QStringLiteral("png"),
    QStringLiteral("gif"),  QStringLiteral("bmp"),  QStringLiteral("webp"),
    QStringLiteral("ico")
};

static const QStringList kThumbnailSuffixes = {
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

// FILETIME → QDateTime (no extra syscall, uses values from WIN32_FIND_DATA)
inline QDateTime filetimeToQDateTime(const FILETIME &ft)
{
    ULARGE_INTEGER ull;
    ull.LowPart  = ft.dwLowDateTime;
    ull.HighPart = ft.dwHighDateTime;
    const qint64 msec = (static_cast<qint64>(ull.QuadPart) - Q_INT64_C(116444736000000000)) / 10000;
    return QDateTime::fromMSecsSinceEpoch(msec);
}

// Extract file extension (without dot) directly from wide-char filename.
inline QString suffixFromWcs(const wchar_t *name)
{
    const wchar_t *dot = nullptr;
    for (const wchar_t *p = name; *p; ++p) {
        if (*p == L'.') dot = p;
    }
    if (!dot || dot == name) return {};
    return QString::fromWCharArray(dot + 1);
}

// Build FileEntry entirely from WIN32_FIND_DATAW — zero extra syscalls.
FileEntry entryFromFindData(const WIN32_FIND_DATAW &fd,
                             const QString          &parentDir,
                             const QLocale          &loc)
{
    const bool isDir    = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    const bool isHidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN)    != 0;

    FileEntry entry;
    entry.name        = QString::fromWCharArray(fd.cFileName);
    entry.path        = parentDir + entry.name;
    entry.suffix      = suffixFromWcs(fd.cFileName);
    entry.isDirectory = isDir;
    entry.isHidden    = isHidden;
    entry.modified    = filetimeToQDateTime(fd.ftLastWriteTime);

    if (isDir) {
        entry.size     = 0;
        entry.sizeText = QStringLiteral("Folder");
    } else {
        ULARGE_INTEGER sz;
        sz.LowPart  = fd.nFileSizeLow;
        sz.HighPart = fd.nFileSizeHigh;
        entry.size     = static_cast<qint64>(sz.QuadPart);
        entry.sizeText = loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
        
        const QString lowerSuffix = entry.suffix.toLower();
        entry.isImage      = kImageSuffixes.contains(lowerSuffix);
        entry.hasThumbnail = kThumbnailSuffixes.contains(lowerSuffix);
    }

    entry.modifiedText = loc.toString(entry.modified, QLocale::ShortFormat);
    return entry;
}

} // namespace

// ---------------------------------------------------------------------------
// WinLocalFileProvider
// ---------------------------------------------------------------------------

WinLocalFileProvider::WinLocalFileProvider(QObject *parent)
    : LocalFileProvider(parent)
{
}

void WinLocalFileProvider::scan(const QString &path)
{
    // Cancel any in-flight scan from the base class or ourselves.
    cancel();

    const int myGen  = ++m_scanGeneration;
    m_currentPath    = path;

    emit started();

    m_watcher.setFuture(QtConcurrent::run([this, path, myGen]() {

        // ----------------------------------------------------------------
        // 1. Validate the directory (cheap QFileInfo calls, done once).
        // ----------------------------------------------------------------
        const QFileInfo dirInfo(path);
        if (!dirInfo.exists() || !dirInfo.isDir()) {
            if (myGen == m_scanGeneration.load()) {
                emit finished(path, false, myGen, QStringLiteral("Folder does not exist"));
            }
            return;
        }

        // Canonical path resolves symlinks; use it as the "official" path.
        const QString canonicalPath = dirInfo.canonicalFilePath();
        if (canonicalPath.isEmpty()) {
            if (myGen == m_scanGeneration.load()) {
                emit finished(path, false, myGen, QStringLiteral("Folder is not readable"));
            }
            return;
        }

        // ----------------------------------------------------------------
        // 2. Build the search pattern: "C:\Dir\*"
        //    Use the \\?\ prefix to support paths > MAX_PATH.
        // ----------------------------------------------------------------
        QString searchPath = QDir::toNativeSeparators(canonicalPath);
        if (!searchPath.endsWith('\\')) searchPath += QLatin1Char('\\');
        // parentDir is used to build entry.path — keep forward slashes for Qt.
        const QString parentDir = QDir::fromNativeSeparators(canonicalPath)
                                  + QLatin1Char('/');

        // \\?\ prefix enables long-path support (> MAX_PATH).
        const QString searchPattern = QStringLiteral("\\\\?\\") + searchPath + QLatin1Char('*');
        const std::wstring wSearchPattern = searchPattern.toStdWString();

        // ----------------------------------------------------------------
        // 3. Enumerate with FindFirstFileExW.
        //    FindExInfoBasic  → skip 8.3 short name (faster).
        //    FIND_FIRST_EX_LARGE_FETCH → hints OS to buffer more entries.
        // ----------------------------------------------------------------
        WIN32_FIND_DATAW fd;
        HANDLE hFind = FindFirstFileExW(
            wSearchPattern.c_str(),
            FindExInfoBasic,          // skip cAlternateFileName (8.3 name)
            &fd,
            FindExSearchNameMatch,
            nullptr,
            FIND_FIRST_EX_LARGE_FETCH // perf hint: larger OS-side buffer
        );

        if (hFind == INVALID_HANDLE_VALUE) {
            if (myGen == m_scanGeneration.load()) {
                emit finished(path, false, myGen, QStringLiteral("Folder is not readable"));
            }
            return;
        }

        // ----------------------------------------------------------------
        // 4. Iterate and batch-emit entries.
        // ----------------------------------------------------------------
        QLocale loc;
        QList<FileEntry> batch;
        batch.reserve(512);

        QElapsedTimer batchTimer;
        batchTimer.start();

        do {
            if (myGen != m_scanGeneration.load()) {
                FindClose(hFind);
                return;
            }

            // Skip "." and ".."
            if (fd.cFileName[0] == L'.' &&
                (fd.cFileName[1] == L'\0' ||
                 (fd.cFileName[1] == L'.' && fd.cFileName[2] == L'\0')))
            {
                continue;
            }

            // Skip hidden files unless requested.
            const bool isHidden = (fd.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0;
            if (!m_showHidden && isHidden) continue;

            // Also skip dot-prefixed names (Unix-style hidden) unless requested.
            if (!m_showHidden && fd.cFileName[0] == L'.') continue;

            batch.append(entryFromFindData(fd, parentDir, loc));

            // Flush early: first screenful within one frame, then keep
            // batches large for throughput.
            if (batch.size() >= 512 || (batch.size() >= 16 && batchTimer.hasExpired(16))) {
                emit batchReady(batch, myGen);
                batch.clear();
                batchTimer.restart();
            }

        } while (FindNextFileW(hFind, &fd));

        FindClose(hFind);

        if (!batch.isEmpty()) {
            emit batchReady(batch, myGen);
        }

        if (myGen == m_scanGeneration.load()) {
            // Use the canonical forward-slash path so the model's path
            // comparison (canonicalPath vs m_currentPath) works correctly.
            emit finished(parentDir.chopped(1), true, myGen);
        }
    }));
}

#endif // Q_OS_WIN
