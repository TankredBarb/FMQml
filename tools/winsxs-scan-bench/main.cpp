#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTextStream>

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

struct Options {
    QString method = QStringLiteral("both");
    QString path = QStringLiteral("C:/Windows/WinSxS");
    int runs = 1;
    int batchSize = 512;
    bool includeHidden = false;
};

struct ScanResult {
    QString method;
    qint64 elapsedMs = 0;
    qint64 firstBatchMs = -1;
    qsizetype batches = 0;
    qsizetype entries = 0;
    qsizetype directories = 0;
    qsizetype files = 0;
    qsizetype hidden = 0;
    qint64 bytes = 0;
    quint64 checksum = 0;
    QString error;
};

QString nativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

void printUsage(QTextStream &out)
{
    out << "winsxs_scan_bench [--method qdir|find|both] [--path PATH] [--runs N] [--include-hidden]\n"
        << "\n"
        << "Defaults:\n"
        << "  --method both\n"
        << "  --path C:\\Windows\\WinSxS\n"
        << "  --runs 1\n"
        << "  --batch-size 512\n";
}

Options parseOptions(const QStringList &arguments, QTextStream &err, bool *ok)
{
    Options options;
    *ok = true;

    for (int i = 1; i < arguments.size(); ++i) {
        const QString arg = arguments.at(i);
        if (arg == QStringLiteral("--help") || arg == QStringLiteral("-h")) {
            *ok = false;
            return options;
        }
        if (arg == QStringLiteral("--include-hidden")) {
            options.includeHidden = true;
            continue;
        }
        if ((arg == QStringLiteral("--method") || arg == QStringLiteral("-m")) && i + 1 < arguments.size()) {
            options.method = arguments.at(++i).toLower();
            continue;
        }
        if ((arg == QStringLiteral("--path") || arg == QStringLiteral("-p")) && i + 1 < arguments.size()) {
            options.path = QDir::fromNativeSeparators(arguments.at(++i));
            continue;
        }
        if ((arg == QStringLiteral("--runs") || arg == QStringLiteral("-r")) && i + 1 < arguments.size()) {
            bool parsed = false;
            const int runs = arguments.at(++i).toInt(&parsed);
            if (!parsed || runs < 1) {
                err << "Invalid --runs value. Expected a positive integer.\n";
                *ok = false;
                return options;
            }
            options.runs = runs;
            continue;
        }
        if (arg == QStringLiteral("--batch-size") && i + 1 < arguments.size()) {
            bool parsed = false;
            const int batchSize = arguments.at(++i).toInt(&parsed);
            if (!parsed || batchSize < 1) {
                err << "Invalid --batch-size value. Expected a positive integer.\n";
                *ok = false;
                return options;
            }
            options.batchSize = batchSize;
            continue;
        }

        err << "Unknown or incomplete argument: " << arg << '\n';
        *ok = false;
        return options;
    }

    if (options.method != QStringLiteral("qdir")
        && options.method != QStringLiteral("find")
        && options.method != QStringLiteral("both")) {
        err << "Invalid --method value. Expected qdir, find, or both.\n";
        *ok = false;
    }

    return options;
}

void accumulateEntry(ScanResult &result,
                     const QString &name,
                     qint64 size,
                     bool isDirectory,
                     bool isHidden)
{
    ++result.entries;
    if (isDirectory) {
        ++result.directories;
    } else {
        ++result.files;
        result.bytes += size;
    }
    if (isHidden) {
        ++result.hidden;
    }

    // Keep a tiny deterministic accumulator so the loop does real metadata work.
    result.checksum += static_cast<quint64>(name.size());
    result.checksum += static_cast<quint64>(size & 0xffff);
    result.checksum += isDirectory ? 17 : 31;
    result.checksum += isHidden ? 43 : 0;
}

void noteBatchProgress(ScanResult &result, const QElapsedTimer &timer, qsizetype &pendingBatch, int batchSize)
{
    ++pendingBatch;
    if (pendingBatch < batchSize) {
        return;
    }
    pendingBatch = 0;
    ++result.batches;
    if (result.firstBatchMs < 0) {
        result.firstBatchMs = timer.elapsed();
    }
}

void flushBatchProgress(ScanResult &result, const QElapsedTimer &timer, qsizetype pendingBatch)
{
    if (pendingBatch <= 0) {
        return;
    }
    ++result.batches;
    if (result.firstBatchMs < 0) {
        result.firstBatchMs = timer.elapsed();
    }
}

ScanResult scanWithQDirIterator(const QString &path, bool includeHidden, int batchSize)
{
    ScanResult result;
    result.method = QStringLiteral("QDirIterator");

    const QFileInfo rootInfo(path);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        result.error = QStringLiteral("Folder does not exist or is not a directory: %1").arg(nativePath(path));
        return result;
    }

    QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
    if (includeHidden) {
        filters |= QDir::Hidden;
    }

    QElapsedTimer timer;
    timer.start();
    qsizetype pendingBatch = 0;

    QDirIterator iterator(rootInfo.absoluteFilePath(), filters);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo info = iterator.fileInfo();
        const bool isHidden = info.isHidden() || info.fileName().startsWith(QLatin1Char('.'));
        if (!includeHidden && isHidden) {
            continue;
        }
        accumulateEntry(result, info.fileName(), info.size(), info.isDir(), isHidden);
        noteBatchProgress(result, timer, pendingBatch, batchSize);
    }

    flushBatchProgress(result, timer, pendingBatch);
    result.elapsedMs = timer.elapsed();
    return result;
}

#ifdef Q_OS_WIN
QString windowsErrorMessage(DWORD errorCode)
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
    QString message;
    if (length > 0 && buffer) {
        message = QString::fromWCharArray(buffer, static_cast<int>(length)).trimmed();
        LocalFree(buffer);
    }
    return message.isEmpty()
        ? QStringLiteral("Windows error %1").arg(errorCode)
        : message;
}

QString findPatternForPath(const QString &path)
{
    QString clean = QDir::toNativeSeparators(QDir::cleanPath(path));
    if (clean.startsWith(QStringLiteral("\\\\?\\"))) {
        // Already long-path prefixed.
    } else if (clean.startsWith(QStringLiteral("\\\\"))) {
        clean = QStringLiteral("\\\\?\\UNC\\") + clean.mid(2);
    } else {
        clean = QStringLiteral("\\\\?\\") + clean;
    }
    if (!clean.endsWith(QLatin1Char('\\'))) {
        clean += QLatin1Char('\\');
    }
    clean += QLatin1Char('*');
    return clean;
}

ScanResult scanWithFindFirstFile(const QString &path, bool includeHidden, int batchSize)
{
    ScanResult result;
    result.method = QStringLiteral("FindFirstFileExW");

    const QFileInfo rootInfo(path);
    if (!rootInfo.exists() || !rootInfo.isDir()) {
        result.error = QStringLiteral("Folder does not exist or is not a directory: %1").arg(nativePath(path));
        return result;
    }

    const QString pattern = findPatternForPath(rootInfo.absoluteFilePath());
    WIN32_FIND_DATAW data{};

    QElapsedTimer timer;
    timer.start();
    qsizetype pendingBatch = 0;

    HANDLE handle = FindFirstFileExW(reinterpret_cast<LPCWSTR>(pattern.utf16()),
                                     FindExInfoBasic,
                                     &data,
                                     FindExSearchNameMatch,
                                     nullptr,
                                     FIND_FIRST_EX_LARGE_FETCH);
    if (handle == INVALID_HANDLE_VALUE) {
        result.error = windowsErrorMessage(GetLastError());
        result.elapsedMs = timer.elapsed();
        return result;
    }

    do {
        const QString name = QString::fromWCharArray(data.cFileName);
        if (name == QLatin1String(".") || name == QLatin1String("..")) {
            continue;
        }

        const DWORD attrs = data.dwFileAttributes;
        const bool isDirectory = (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const bool isHidden = (attrs & FILE_ATTRIBUTE_HIDDEN) != 0 || name.startsWith(QLatin1Char('.'));
        if (!includeHidden && isHidden) {
            continue;
        }

        ULARGE_INTEGER size{};
        size.LowPart = data.nFileSizeLow;
        size.HighPart = data.nFileSizeHigh;
        accumulateEntry(result, name, isDirectory ? 0 : static_cast<qint64>(size.QuadPart), isDirectory, isHidden);
        noteBatchProgress(result, timer, pendingBatch, batchSize);
    } while (FindNextFileW(handle, &data));

    flushBatchProgress(result, timer, pendingBatch);
    const DWORD lastError = GetLastError();
    FindClose(handle);

    if (lastError != ERROR_NO_MORE_FILES) {
        result.error = windowsErrorMessage(lastError);
    }
    result.elapsedMs = timer.elapsed();
    return result;
}
#else
ScanResult scanWithFindFirstFile(const QString &, bool, int)
{
    ScanResult result;
    result.method = QStringLiteral("FindFirstFileExW");
    result.error = QStringLiteral("FindFirstFileExW is only available on Windows.");
    return result;
}
#endif

void printResult(QTextStream &out, int run, const ScanResult &result)
{
    out << "run=" << run
        << " method=" << result.method
        << " ms=" << result.elapsedMs
        << " firstBatchMs=" << result.firstBatchMs
        << " batches=" << result.batches
        << " entries=" << result.entries
        << " dirs=" << result.directories
        << " files=" << result.files
        << " hidden=" << result.hidden
        << " bytes=" << result.bytes
        << " checksum=" << result.checksum;
    if (!result.error.isEmpty()) {
        out << " error=\"" << result.error << "\"";
    }
    out << '\n';
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    bool ok = true;
    const Options options = parseOptions(app.arguments(), err, &ok);
    if (!ok) {
        printUsage(out);
        return 1;
    }

    out << "path=" << nativePath(options.path)
        << " includeHidden=" << (options.includeHidden ? "true" : "false")
        << " runs=" << options.runs
        << " batchSize=" << options.batchSize
        << " method=" << options.method
        << '\n';

    for (int run = 1; run <= options.runs; ++run) {
        if (options.method == QStringLiteral("qdir") || options.method == QStringLiteral("both")) {
            printResult(out, run, scanWithQDirIterator(options.path, options.includeHidden, options.batchSize));
        }
        if (options.method == QStringLiteral("find") || options.method == QStringLiteral("both")) {
            printResult(out, run, scanWithFindFirstFile(options.path, options.includeHidden, options.batchSize));
        }
    }

    return 0;
}
