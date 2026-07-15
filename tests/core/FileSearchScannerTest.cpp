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
                    && writeFile(directory.filePath(QStringLiteral("binary")), QByteArray("ELF\0needle", 10)),
                "Search fixtures must be created")) return 1;

    QList<FileSearchResult> results;
    int contentScanned = -1;
    int contentSkipped = -1;
    FileSearchScanner scanner(directory.path(), QStringLiteral("needle"), true, true, false,
                              FileSearchScanner::ContainsMatch, true, 1);
    QObject::connect(&scanner, &FileSearchScanner::resultsReady,
                     [&results](const QList<FileSearchResult> &batch, auto...) { results.append(batch); });
    QObject::connect(&scanner, &FileSearchScanner::finished,
                     [&contentScanned, &contentSkipped](bool, const QString &, int, int, int, int, int,
                                                        int scanned, int skipped, const QStringList &,
                                                        const QStringList &, int) {
        contentScanned = scanned;
        contentSkipped = skipped;
    });
    scanner.run();

    QSet<QString> names;
    for (const FileSearchResult &result : std::as_const(results)) names.insert(result.name);
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
    if (!expect(contentScanned == 5 && contentSkipped == 2,
                "Content accounting must include probed and cheaply rejected files")) return 1;
    return 0;
}
