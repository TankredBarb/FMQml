#include "DirectoryModelAlgorithms.h"

#include "../core/ArchiveSupport.h"
#include "../core/FileEntrySortPolicy.h"
#include "../core/IsoSupport.h"

#include <QDir>
#include <QSet>

#include <algorithm>

namespace {
const QSet<QString> kExecutableSuffixes = {
    QStringLiteral("exe"), QStringLiteral("bat"), QStringLiteral("cmd"),
    QStringLiteral("com"), QStringLiteral("ps1"), QStringLiteral("msi"),
    QStringLiteral("scr"), QStringLiteral("jar")
};
const QSet<QString> kLibrarySuffixes = {
    QStringLiteral("dll"), QStringLiteral("lib"), QStringLiteral("a"),
    QStringLiteral("so"), QStringLiteral("dylib"), QStringLiteral("ocx")
};
const QSet<QString> kFilterImageSuffixes = {
    QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
    QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp"),
    QStringLiteral("ico"), QStringLiteral("svg"), QStringLiteral("svgz"),
    QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("avif"),
    QStringLiteral("heic")
};
const QSet<QString> kAudioSuffixes = {
    QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"),
    QStringLiteral("m4a"), QStringLiteral("m4b"), QStringLiteral("wav"),
    QStringLiteral("wma")
};
const QSet<QString> kVideoSuffixes = {
    QStringLiteral("mp4"), QStringLiteral("avi"), QStringLiteral("mkv"),
    QStringLiteral("mov"), QStringLiteral("wmv"), QStringLiteral("webm"),
    QStringLiteral("m4v")
};
const QSet<QString> kDocumentSuffixes = {
    QStringLiteral("pdf"), QStringLiteral("txt"), QStringLiteral("rtf"),
    QStringLiteral("md"), QStringLiteral("json"), QStringLiteral("xml"),
    QStringLiteral("html"), QStringLiteral("htm"), QStringLiteral("css"),
    QStringLiteral("js"), QStringLiteral("ts"), QStringLiteral("cpp"),
    QStringLiteral("c"), QStringLiteral("h"), QStringLiteral("hpp"),
    QStringLiteral("py"), QStringLiteral("rs"), QStringLiteral("go"),
    QStringLiteral("java"), QStringLiteral("kt"), QStringLiteral("qml"),
    QStringLiteral("ini"), QStringLiteral("yaml"), QStringLiteral("yml"),
    QStringLiteral("toml"), QStringLiteral("csv"), QStringLiteral("doc"),
    QStringLiteral("docx"), QStringLiteral("odt"), QStringLiteral("xls"),
    QStringLiteral("xlsx"), QStringLiteral("ods"), QStringLiteral("ppt"),
    QStringLiteral("pptx"), QStringLiteral("odp"), QStringLiteral("epub"),
    QStringLiteral("fb2")
};
}

namespace DirectoryModelAlgorithms {

QString pathKey(const QString &path)
{
    QString key = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    key = key.toLower();
#endif
    return key;
}

bool matchesFilter(const FileEntry &entry,
                   const QString &searchText,
                   DirectoryModel::CategoryFilter categoryFilter)
{
    if (!searchText.isEmpty() && !entry.name.contains(searchText, Qt::CaseInsensitive)) {
        return false;
    }
    if (categoryFilter == DirectoryModel::FilterAll) {
        return true;
    }
    if (entry.isDirectory) {
        return false;
    }

    const QString suffix = entry.suffix.toLower();
    switch (categoryFilter) {
    case DirectoryModel::FilterExecutables: return kExecutableSuffixes.contains(suffix);
    case DirectoryModel::FilterLibraries: return kLibrarySuffixes.contains(suffix);
    case DirectoryModel::FilterImages: return kFilterImageSuffixes.contains(suffix);
    case DirectoryModel::FilterArchives:
        return ArchiveSupport::isArchiveExtension(suffix) || IsoSupport::isIsoImageExtension(suffix);
    case DirectoryModel::FilterMedia:
        return kAudioSuffixes.contains(suffix) || kVideoSuffixes.contains(suffix);
    case DirectoryModel::FilterDocuments:
        return kDocumentSuffixes.contains(suffix)
            || entry.name.endsWith(QStringLiteral(".fb2.zip"), Qt::CaseInsensitive);
    case DirectoryModel::FilterAll: break;
    }
    return true;
}

bool lessThan(const FileEntry &a,
              const FileEntry &b,
              bool mixFilesAndFolders,
              DirectoryModel::SortRole sortRole,
              Qt::SortOrder sortOrder)
{
    return FileEntrySortPolicy::lessThan(a, b, mixFilesAndFolders, int(sortRole), sortOrder);
}

QList<int> filteredAndSortedIndices(const QList<FileEntry> &entries,
                                    bool showHidden,
                                    const QString &searchText,
                                    DirectoryModel::CategoryFilter categoryFilter,
                                    bool mixFilesAndFolders,
                                    DirectoryModel::SortRole sortRole,
                                    Qt::SortOrder sortOrder)
{
    QList<int> indices;
    indices.reserve(entries.size());
    for (int index = 0; index < entries.size(); ++index) {
        const FileEntry &entry = entries.at(index);
        if ((showHidden || !entry.isHidden) && matchesFilter(entry, searchText, categoryFilter)) {
            indices.append(index);
        }
    }
    std::stable_sort(indices.begin(), indices.end(), [&](int left, int right) {
        return lessThan(entries.at(left), entries.at(right), mixFilesAndFolders, sortRole, sortOrder);
    });
    return indices;
}

}
