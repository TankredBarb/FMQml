#include "FileSearchScanner.h"
#ifdef Q_OS_LINUX
#include "LinuxFileEnumerator.h"
#endif

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStack>
#include <QTextStream>

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
constexpr qint64 MaxContentSearchBytes = 10 * 1024 * 1024;
constexpr int MaxContentMatchesPerFile = 3;
constexpr qsizetype BinaryProbeBytes = 4096;
constexpr qsizetype MaxContentExcerptLength = 190;

struct ContentExcerpt {
    QString text;
    int matchStart = -1;
    int matchLength = 0;
};

QString nativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

bool looksBinary(const QByteArray &bytes)
{
    const qsizetype probeSize = std::min(bytes.size(), BinaryProbeBytes);
    for (qsizetype i = 0; i < probeSize; ++i) {
        if (bytes.at(i) == '\0') {
            return true;
        }
    }
    return false;
}

const QSet<QString> &textContentSuffixes()
{
    static const QSet<QString> suffixes = {
        QStringLiteral("txt"), QStringLiteral("text"), QStringLiteral("log"), QStringLiteral("service"),
        QStringLiteral("socket"), QStringLiteral("timer"), QStringLiteral("target"), QStringLiteral("desktop"),
        QStringLiteral("md"),
        QStringLiteral("markdown"), QStringLiteral("rst"), QStringLiteral("csv"), QStringLiteral("tsv"),
        QStringLiteral("json"), QStringLiteral("jsonl"), QStringLiteral("xml"), QStringLiteral("xaml"),
        QStringLiteral("yaml"), QStringLiteral("yml"), QStringLiteral("toml"), QStringLiteral("ini"),
        QStringLiteral("cfg"), QStringLiteral("conf"), QStringLiteral("config"), QStringLiteral("properties"),
        QStringLiteral("env"), QStringLiteral("editorconfig"), QStringLiteral("cmake"), QStringLiteral("qml"),
        QStringLiteral("js"), QStringLiteral("mjs"), QStringLiteral("cjs"), QStringLiteral("ts"),
        QStringLiteral("tsx"), QStringLiteral("jsx"), QStringLiteral("html"), QStringLiteral("htm"),
        QStringLiteral("css"), QStringLiteral("scss"), QStringLiteral("sass"), QStringLiteral("less"),
        QStringLiteral("vue"), QStringLiteral("svelte"), QStringLiteral("py"), QStringLiteral("pyw"),
        QStringLiteral("cpp"), QStringLiteral("cxx"), QStringLiteral("cc"), QStringLiteral("c"),
        QStringLiteral("h"), QStringLiteral("hpp"), QStringLiteral("hh"), QStringLiteral("cs"),
        QStringLiteral("java"), QStringLiteral("kt"), QStringLiteral("kts"), QStringLiteral("go"),
        QStringLiteral("rs"), QStringLiteral("php"), QStringLiteral("rb"), QStringLiteral("sh"),
        QStringLiteral("bash"), QStringLiteral("zsh"), QStringLiteral("ps1"), QStringLiteral("bat"),
        QStringLiteral("cmd"), QStringLiteral("sql"), QStringLiteral("swift"), QStringLiteral("dart"),
        QStringLiteral("lua"), QStringLiteral("pl"), QStringLiteral("pm"), QStringLiteral("r"),
        QStringLiteral("scala"), QStringLiteral("clj"), QStringLiteral("fs"), QStringLiteral("fsx"),
        QStringLiteral("vb")
    };
    return suffixes;
}

ContentExcerpt makeContentExcerpt(QString line, qsizetype matchStart, qsizetype matchLength)
{
    line.replace(QLatin1Char('\t'), QLatin1Char(' '));

    qsizetype start = 0;
    qsizetype end = line.size();
    if (line.size() > MaxContentExcerptLength) {
        const qsizetype roomAroundMatch = std::max<qsizetype>(20, MaxContentExcerptLength - matchLength);
        const qsizetype contextBefore = roomAroundMatch / 2;
        start = std::max<qsizetype>(0, matchStart - contextBefore);
        end = std::min<qsizetype>(line.size(), start + MaxContentExcerptLength);
        start = std::max<qsizetype>(0, end - MaxContentExcerptLength);
    }

    QString excerpt = line.mid(start, end - start);
    qsizetype leadingTrimmed = 0;
    while (leadingTrimmed < excerpt.size() && excerpt.at(leadingTrimmed).isSpace()) {
        ++leadingTrimmed;
    }
    excerpt = excerpt.trimmed();
    int adjustedStart = static_cast<int>(matchStart - start - leadingTrimmed);

    if (start > 0) {
        excerpt.prepend(QStringLiteral("..."));
        adjustedStart += 3;
    }
    if (end < line.size()) {
        excerpt.append(QStringLiteral("..."));
    }

    if (adjustedStart < 0 || adjustedStart >= excerpt.size()) {
        adjustedStart = -1;
        matchLength = 0;
    }

    return {excerpt, adjustedStart, static_cast<int>(matchLength)};
}

#ifdef Q_OS_WIN
QString searchExtendedLengthWindowsPath(const QString &path)
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

QString searchWindowsSearchPattern(const QString &path)
{
    QString pattern = searchExtendedLengthWindowsPath(path);
    if (!pattern.endsWith(QLatin1Char('\\'))) {
        pattern += QLatin1Char('\\');
    }
    pattern += QLatin1Char('*');
    return pattern;
}

HANDLE searchFindFirstFileBasic(const QString &pattern, WIN32_FIND_DATAW *findData)
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

QDateTime searchDateTimeFromFileTime(const FILETIME &fileTime)
{
    ULARGE_INTEGER value{};
    value.LowPart = fileTime.dwLowDateTime;
    value.HighPart = fileTime.dwHighDateTime;
    if (value.QuadPart == 0) {
        return {};
    }

    constexpr quint64 windowsEpochToUnixEpoch100ns = Q_UINT64_C(116444736000000000);
    if (value.QuadPart < windowsEpochToUnixEpoch100ns) {
        return {};
    }

    const qint64 msecs = static_cast<qint64>((value.QuadPart - windowsEpochToUnixEpoch100ns) / 10000);
    return QDateTime::fromMSecsSinceEpoch(msecs);
}

qint64 searchSizeFromFindData(const WIN32_FIND_DATAW &findData)
{
    ULARGE_INTEGER value{};
    value.LowPart = findData.nFileSizeLow;
    value.HighPart = findData.nFileSizeHigh;
    return static_cast<qint64>(value.QuadPart);
}

QString windowsErrorText(DWORD errorCode)
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

FileSearchScannerEntry entryFromFindData(const WIN32_FIND_DATAW &findData, const QString &parentPath)
{
    QString parentPrefix = QDir::fromNativeSeparators(parentPath);
    if (!parentPrefix.endsWith(QLatin1Char('/'))) {
        parentPrefix += QLatin1Char('/');
    }

    const DWORD attributes = findData.dwFileAttributes;
    const QString name = QString::fromWCharArray(findData.cFileName);
    const bool isDirectory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

    return {
        parentPrefix + name,
        name,
        QDir::fromNativeSeparators(parentPath),
        isDirectory ? 0 : searchSizeFromFindData(findData),
        searchDateTimeFromFileTime(findData.ftLastWriteTime),
        isDirectory,
        (attributes & FILE_ATTRIBUTE_HIDDEN) != 0 || name.startsWith(QLatin1Char('.')),
        isDirectory && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0,
        false
    };
}
#endif

[[maybe_unused]] FileSearchScannerEntry entryFromFileInfo(const QFileInfo &info)
{
    return {
        info.absoluteFilePath(),
        info.fileName(),
        info.absolutePath(),
        info.isDir() ? 0 : info.size(),
        info.lastModified(),
        info.isDir(),
        info.isHidden(),
        info.isDir() && info.isSymLink(),
        false
    };
}

#ifdef Q_OS_LINUX
FileSearchScannerEntry entryFromLinuxEntry(const LinuxFileEnumerator::Entry &entry)
{
    return {
        entry.path,
        entry.name,
        entry.parentPath,
        entry.isDirectory ? 0 : entry.size,
        entry.modified,
        entry.isDirectory,
        entry.isHidden,
        entry.isDirectory && entry.isSymlink,
        entry.isMountBoundary
    };
}
#endif
}

FileSearchScanner::FileSearchScanner(const QString &rootPath, const QString &query, bool includeHidden, bool searchContents, bool caseSensitive, int matchMode, bool includeFolders, int generation)
    : m_rootPath(QDir::fromNativeSeparators(rootPath))
    , m_query(query)
    , m_includeHidden(includeHidden)
    , m_searchContents(searchContents)
    , m_caseSensitive(caseSensitive)
    , m_includeFolders(includeFolders)
    , m_useWildcardNameMatch(matchMode == WildcardMatch
                             || (matchMode == ContainsMatch && query.contains(QLatin1Char('*'))))
    , m_matchMode(matchMode)
    , m_generation(generation)
{
    if (m_useWildcardNameMatch) {
        QRegularExpression::PatternOptions options;
        if (!m_caseSensitive) {
            options |= QRegularExpression::CaseInsensitiveOption;
        }
        m_wildcardExpression = QRegularExpression(QRegularExpression::wildcardToRegularExpression(m_query), options);
    }
    setAutoDelete(false);
}

void FileSearchScanner::cancel()
{
    m_cancelled = true;
}

void FileSearchScanner::run()
{
    const QFileInfo rootInfo(m_rootPath);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        emit finished(false, QStringLiteral("Folder does not exist"), 0, 0, 0, 0, 0, 0, 0, {}, {}, m_generation);
        return;
    }

    QStack<QString> pending;
    pending.push(rootInfo.absoluteFilePath());

    while (!pending.isEmpty()) {
        if (m_cancelled) {
            emitBatchIfNeeded(true);
            emit finished(false, {}, m_scannedFiles, m_scannedFolders, m_skippedPaths, m_inaccessiblePaths, m_reparsePaths, m_contentFilesScanned, m_contentFilesSkipped, m_inaccessiblePathDetails, m_reparsePathDetails, m_generation);
            return;
        }

        if (!enumerateFolder(pending.pop(), pending)) {
            emitBatchIfNeeded(false);
            continue;
        }

        if (m_cancelled) {
            emitBatchIfNeeded(true);
            emit finished(false, {}, m_scannedFiles, m_scannedFolders, m_skippedPaths, m_inaccessiblePaths, m_reparsePaths, m_contentFilesScanned, m_contentFilesSkipped, m_inaccessiblePathDetails, m_reparsePathDetails, m_generation);
            return;
        }

        emitBatchIfNeeded(false);
    }

    emitBatchIfNeeded(true);
    emit finished(true, {}, m_scannedFiles, m_scannedFolders, m_skippedPaths, m_inaccessiblePaths, m_reparsePaths, m_contentFilesScanned, m_contentFilesSkipped, m_inaccessiblePathDetails, m_reparsePathDetails, m_generation);
}

void FileSearchScanner::processEntry(const FileSearchScannerEntry &entry, QStack<QString> &pending)
{
    if (entry.isHidden && !m_includeHidden) {
        return;
    }

    if (entry.isReparseDirectory || entry.isMountBoundary) {
        ++m_skippedPaths;
        ++m_reparsePaths;
        addSkippedDetail(m_reparsePathDetails, nativePath(entry.path));
        return;
    }

    if (entry.isDirectory) {
        ++m_scannedFolders;
    } else {
        ++m_scannedFiles;
    }

    if (fileNameMatches(entry.name) && (!entry.isDirectory || m_includeFolders)) {
        appendNameMatch(entry);
    } else if (m_searchContents && !entry.isDirectory) {
        appendContentMatches(entry);
    }

    if (entry.isDirectory) {
        pending.push(entry.path);
    }
}

void FileSearchScanner::appendNameMatch(const FileSearchScannerEntry &entry)
{
    appendResultBatch({
        entry.path,
        entry.name,
        entry.parentPath,
        entry.isDirectory ? 0 : entry.size,
        entry.modified,
        entry.isDirectory,
        QStringLiteral("name"),
        0,
        {}
    });
}

void FileSearchScanner::appendContentMatches(const FileSearchScannerEntry &entry)
{
    if (!canSearchFileContents(entry) || entry.size <= 0 || entry.size > MaxContentSearchBytes) {
        ++m_contentFilesSkipped;
        return;
    }

    QFile file(entry.path);
    if (!file.open(QIODevice::ReadOnly)) {
        ++m_contentFilesSkipped;
        return;
    }

    const QByteArray bytes = file.readAll();
    if (looksBinary(bytes)) {
        ++m_contentFilesSkipped;
        return;
    }

    ++m_contentFilesScanned;
    QTextStream stream(bytes);
    QString line;
    int lineNumber = 0;
    int matchCount = 0;
    while (!stream.atEnd()) {
        if (m_cancelled) {
            return;
        }
        line = stream.readLine();
        ++lineNumber;
        const qsizetype matchIndex = line.indexOf(m_query, 0, m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
        if (matchIndex < 0) {
            continue;
        }

        const ContentExcerpt excerpt = makeContentExcerpt(line, matchIndex, m_query.size());

        appendResultBatch({
            entry.path,
            entry.name,
            entry.parentPath,
            entry.size,
            entry.modified,
            false,
            QStringLiteral("content"),
            lineNumber,
            excerpt.text,
            excerpt.matchStart,
            excerpt.matchLength
        });

        ++matchCount;
        if (matchCount >= MaxContentMatchesPerFile) {
            return;
        }
    }
}

bool FileSearchScanner::canSearchFileContents(const FileSearchScannerEntry &entry) const
{
    const qsizetype dot = entry.name.lastIndexOf(QLatin1Char('.'));
    if (entry.isDirectory || dot <= 0 || dot == entry.name.size() - 1) {
        return false;
    }
    return textContentSuffixes().contains(entry.name.mid(dot + 1).toLower());
}

bool FileSearchScanner::enumerateFolder(const QString &folderPath, QStack<QString> &pending)
{
    m_currentPath = folderPath;

#ifdef Q_OS_WIN
    WIN32_FIND_DATAW findData{};
    HANDLE handle = searchFindFirstFileBasic(searchWindowsSearchPattern(folderPath), &findData);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_NO_MORE_FILES) {
            return true;
        }

        ++m_skippedPaths;
        ++m_inaccessiblePaths;
        m_lastError = QStringLiteral("Cannot read %1: %2").arg(nativePath(folderPath), windowsErrorText(code));
        addSkippedDetail(m_inaccessiblePathDetails, m_lastError);
        return false;
    }

    do {
        if (m_cancelled) {
            FindClose(handle);
            return true;
        }

        const wchar_t *rawName = findData.cFileName;
        if (rawName[0] == L'.'
            && (rawName[1] == L'\0' || (rawName[1] == L'.' && rawName[2] == L'\0'))) {
            continue;
        }

        processEntry(entryFromFindData(findData, folderPath), pending);
    } while (FindNextFileW(handle, &findData));

    const DWORD code = GetLastError();
    FindClose(handle);
    if (code != ERROR_NO_MORE_FILES) {
        ++m_skippedPaths;
        ++m_inaccessiblePaths;
        m_lastError = QStringLiteral("Cannot read %1: %2").arg(nativePath(folderPath), windowsErrorText(code));
        addSkippedDetail(m_inaccessiblePathDetails, m_lastError);
        return false;
    }
    return true;
#elif defined(Q_OS_LINUX)
    LinuxFileEnumerator::Options options;
    options.includeHidden = true;
    if (QDir::cleanPath(QDir::fromNativeSeparators(m_rootPath)) == QLatin1String("/")) {
        const std::optional<dev_t> rootDevice = LinuxFileEnumerator::deviceForPath(m_rootPath);
        if (rootDevice) {
            options.stayOnRootDevice = true;
            options.rootDevice = *rootDevice;
        }
    }

    QList<LinuxFileEnumerator::Entry> entries;
    QString error;
    if (!LinuxFileEnumerator::enumerateChildren(folderPath, options, &entries, &error)) {
        ++m_skippedPaths;
        ++m_inaccessiblePaths;
        m_lastError = error.isEmpty()
            ? QStringLiteral("Cannot read %1").arg(nativePath(folderPath))
            : error;
        addSkippedDetail(m_inaccessiblePathDetails, m_lastError);
        return false;
    }

    for (const LinuxFileEnumerator::Entry &entry : std::as_const(entries)) {
        if (m_cancelled) {
            return true;
        }
        processEntry(entryFromLinuxEntry(entry), pending);
    }
    return true;
#else
    QDirIterator it(folderPath,
                    QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
                    QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        if (m_cancelled) {
            return true;
        }
        it.next();
        processEntry(entryFromFileInfo(it.fileInfo()), pending);
    }
    return true;
#endif
}

bool FileSearchScanner::fileNameMatches(const QString &fileName) const
{
    if (m_useWildcardNameMatch) {
        return m_wildcardExpression.isValid()
            && m_wildcardExpression.match(fileName).hasMatch();
    }

    switch (m_matchMode) {
    case ExactMatch:
        return fileName.compare(m_query, m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive) == 0;
    case ContainsMatch:
    case WildcardMatch:
    default:
        return fileName.contains(m_query, m_caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
    }
}

void FileSearchScanner::appendResultBatch(const FileSearchResult &result)
{
    m_pendingResults.append(result);
}

void FileSearchScanner::addSkippedDetail(QStringList &details, const QString &detail)
{
    constexpr int maxSkippedDetails = 200;
    if (detail.isEmpty() || details.size() >= maxSkippedDetails) {
        return;
    }
    details.append(detail);
}

void FileSearchScanner::emitBatchIfNeeded(bool force)
{
    static thread_local QElapsedTimer timer;
    static thread_local bool timerStarted = false;
    if (!timerStarted) {
        timer.start();
        timerStarted = true;
    }

    const qint64 now = timer.elapsed();
    if (!force && m_pendingResults.size() < 64 && now - m_lastBatchMsec < 160) {
        return;
    }

    m_lastBatchMsec = now;
    emit resultsReady(m_pendingResults,
                      m_scannedFiles,
                      m_scannedFolders,
                      m_skippedPaths,
                      m_inaccessiblePaths,
                      m_reparsePaths,
                      m_contentFilesScanned,
                      m_contentFilesSkipped,
                      m_inaccessiblePathDetails,
                      m_reparsePathDetails,
                      m_currentPath,
                      m_lastError,
                      m_generation);
    m_pendingResults.clear();
}
