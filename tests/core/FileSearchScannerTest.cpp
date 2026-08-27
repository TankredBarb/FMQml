#include "FileSearchScanner.h"

#include <QCoreApplication>
#include <QFile>
#include <QSet>
#include <QTemporaryDir>

#include <cstdio>
#include <utility>
namespace {
bool expect(bool condition, const char *message)
{
    if (!condition) std::fprintf(stderr, "%s\n", message);
    return condition;
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(contents) == contents.size();
}

struct ScanResult {
    QList<FileSearchResult> results;
    int contentScanned = -1;
    int contentSkipped = -1;
};

ScanResult runScan(const QString &rootPath,
                   const QString &query,
                   bool includeHidden,
                   int searchTarget,
                   bool caseSensitive = false,
                   int matchMode = FileSearchScanner::ContainsMatch,
                   int kindFilter = FileSearchRequest::AllKinds,
                   const QString &extension = {},
                   const QDateTime &modifiedSince = {})
{
    ScanResult output;
    FileSearchRequest request;
    request.rootPath = rootPath;
    request.query = query;
    request.includeHidden = includeHidden;
    request.searchTarget = searchTarget;
    request.caseSensitive = caseSensitive;
    request.matchMode = matchMode;
    request.kindFilter = kindFilter;
    request.extension = extension;
    request.modifiedSince = modifiedSince;
    request.generation = 1;
    FileSearchScanner scanner(std::move(request));
    QObject::connect(&scanner, &FileSearchScanner::resultsReady,
                     [&output](const QList<FileSearchResult> &batch, auto...) {
        output.results.append(batch);
    });
    QObject::connect(&scanner, &FileSearchScanner::finished,
                     [&output](bool, const QString &, int, int, int, int, int,
                               int scanned, int skipped, const QStringList &,
                               const QStringList &, int) {
        output.contentScanned = scanned;
        output.contentSkipped = skipped;
    });
    scanner.run();
    return output;
}

bool containsResult(const QList<FileSearchResult> &results,
                    const QString &name,
                    const QString &matchKind = {})
{
    for (const FileSearchResult &result : results) {
        if (result.name == name && (matchKind.isEmpty() || result.matchKind == matchKind)) {
            return true;
        }
    }
    return false;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!expect(directory.isValid(), "Temporary search directory must be available")) return 1;

    const QByteArray text("alpha needle omega\n");
    if (!expect(writeFile(directory.filePath(QStringLiteral("script")), text)
                    && writeFile(directory.filePath(QStringLiteral("Makefile")), text)
                    && writeFile(directory.filePath(QStringLiteral(".env")), text)
                    && writeFile(directory.filePath(QStringLiteral("unknown.blob")), text)
                    && writeFile(directory.filePath(QStringLiteral("needle-name.txt")), QByteArray("no match here\n"))
                    && writeFile(directory.filePath(QStringLiteral("needle-both.txt")), text)
                    && writeFile(directory.filePath(QStringLiteral(".hidden-needle.txt")), QByteArray("hidden\n"))
                    && writeFile(directory.filePath(QStringLiteral("Needle-Case.txt")), QByteArray("case\n"))
                    && writeFile(directory.filePath(QStringLiteral("report-final.txt")), QByteArray("report\n"))
                    && writeFile(directory.filePath(QStringLiteral("binary")), QByteArray("ELF\0needle", 10)),
                "Search fixtures must be created")) return 1;

    const ScanResult contents = runScan(directory.path(), QStringLiteral("needle"), true,
                                        FileSearchScanner::ContentsTarget);

    QSet<QString> names;
    for (const FileSearchResult &result : contents.results) names.insert(result.name);
    if (!expect(names.contains(QStringLiteral("script"))
                    && names.contains(QStringLiteral("Makefile"))
                    && names.contains(QStringLiteral(".env")),
                "Extensionless text files and dotfiles must be content-searchable")) return 1;
    if (!expect(!names.contains(QStringLiteral("binary"))
                    && !names.contains(QStringLiteral("unknown.blob"))
                    && !names.contains(QStringLiteral("needle-name.txt")),
                "Content mode must exclude binaries, unknown suffixes, and name-only matches")) return 1;
    if (!expect(names.contains(QStringLiteral("needle-both.txt")),
                "A filename match must not suppress a valid content match")) return 1;
    if (!expect(contents.contentScanned == 8 && contents.contentSkipped == 2,
                "Content accounting must include probed and cheaply rejected files")) return 1;

    const ScanResult namesOnly = runScan(directory.path(), QStringLiteral("needle"), true,
                                         FileSearchScanner::NameTarget);
    if (!expect(containsResult(namesOnly.results, QStringLiteral("needle-name.txt"), QStringLiteral("name"))
                    && containsResult(namesOnly.results, QStringLiteral("needle-both.txt"), QStringLiteral("name"))
                    && !containsResult(namesOnly.results, QStringLiteral("script")),
                "Name mode must return name matches without content-only matches")) return 1;

    const ScanResult combined = runScan(directory.path(), QStringLiteral("needle"), true,
                                        FileSearchScanner::NameAndContentsTarget);
    if (!expect(containsResult(combined.results, QStringLiteral("script"), QStringLiteral("content"))
                    && containsResult(combined.results, QStringLiteral("needle-name.txt"), QStringLiteral("name"))
                    && containsResult(combined.results, QStringLiteral("needle-both.txt"), QStringLiteral("name"))
                    && containsResult(combined.results, QStringLiteral("needle-both.txt"), QStringLiteral("content")),
                "Combined mode must preserve distinct name and content results")) return 1;

    const ScanResult withoutHidden = runScan(directory.path(), QStringLiteral("hidden-needle"), false,
                                             FileSearchScanner::NameTarget);
    if (!expect(withoutHidden.results.isEmpty(),
                "Hidden entries must be excluded when hidden search is disabled")) return 1;

    const ScanResult caseSensitive = runScan(directory.path(), QStringLiteral("needle-case"), true,
                                             FileSearchScanner::NameTarget, true);
    const ScanResult caseInsensitive = runScan(directory.path(), QStringLiteral("needle-case"), true,
                                               FileSearchScanner::NameTarget, false);
    if (!expect(caseSensitive.results.isEmpty()
                    && containsResult(caseInsensitive.results, QStringLiteral("Needle-Case.txt")),
                "Name matching must honor case sensitivity")) return 1;

    const ScanResult wildcard = runScan(directory.path(), QStringLiteral("report-*.txt"), true,
                                        FileSearchScanner::NameTarget, false,
                                        FileSearchScanner::WildcardMatch);
    if (!expect(containsResult(wildcard.results, QStringLiteral("report-final.txt")),
                "Wildcard mode must be exposed by the scanner contract")) return 1;

    const ScanResult literalWildcard = runScan(directory.path(), QStringLiteral("report-*.txt"), true,
                                               FileSearchScanner::NameTarget, false,
                                               FileSearchScanner::ContainsMatch);
    if (!expect(literalWildcard.results.isEmpty(),
                "Contains mode must treat wildcard characters literally")) return 1;

    const ScanResult documents = runScan(directory.path(), QStringLiteral("needle"), true,
                                         FileSearchScanner::NameTarget, false,
                                         FileSearchScanner::ContainsMatch,
                                         FileSearchRequest::DocumentsKind);
    if (!expect(containsResult(documents.results, QStringLiteral("needle-name.txt"))
                    && !containsResult(documents.results, QStringLiteral("script")),
                "Kind filters must reject entries outside the selected category")) return 1;

    const ScanResult txtOnly = runScan(directory.path(), QStringLiteral("needle"), true,
                                       FileSearchScanner::NameTarget, false,
                                       FileSearchScanner::ContainsMatch,
                                       FileSearchRequest::AllKinds, QStringLiteral(".txt"));
    if (!expect(containsResult(txtOnly.results, QStringLiteral("needle-name.txt"))
                    && !containsResult(txtOnly.results, QStringLiteral("script")),
                "Extension filters must normalize a leading dot")) return 1;

    const ScanResult future = runScan(directory.path(), QStringLiteral("needle"), true,
                                      FileSearchScanner::NameTarget, false,
                                      FileSearchScanner::ContainsMatch,
                                      FileSearchRequest::AllKinds, {},
                                      QDateTime::currentDateTime().addDays(1));
    if (!expect(future.results.isEmpty(),
                "Modified filters must exclude entries older than the lower bound")) return 1;

    FileSearchRequest sizeRequest;
    sizeRequest.rootPath = directory.path();
    sizeRequest.query = QStringLiteral("needle");
    sizeRequest.includeHidden = true;
    sizeRequest.minimumSize = 1024;
    sizeRequest.generation = 2;
    ScanResult sized;
    FileSearchScanner sizeScanner(std::move(sizeRequest));
    QObject::connect(&sizeScanner, &FileSearchScanner::resultsReady,
                     [&sized](const QList<FileSearchResult> &batch, auto...) { sized.results.append(batch); });
    sizeScanner.run();
    if (!expect(sized.results.isEmpty(),
                "Minimum size filters must reject smaller files before matching")) return 1;

    for (const FileSearchResult &result : namesOnly.results) {
        if (result.name == QStringLiteral("needle-name.txt")) {
            if (!expect(result.nameMatchStart == 0 && result.nameMatchLength == 6,
                        "Name results must expose the highlighted match range")) return 1;
        }
    }
    return 0;
}
