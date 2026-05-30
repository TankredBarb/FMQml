#include "FileTypeIconResolver.h"

#include <QFileInfo>
#include <QSet>

namespace {
QString fileTypeIconPath(const QString &name)
{
    return QStringLiteral("qrc:/qt/qml/FM/qml/assets/filetypes/%1.svg").arg(name);
}

bool hasSuffix(const QString &suffix, const QSet<QString> &suffixes)
{
    return suffixes.contains(suffix.toLower());
}

QString suffixFromPath(const QString &path)
{
    return QFileInfo(path).suffix().toLower();
}
}

FileTypeIconResolver::FileTypeIconResolver(QObject *parent)
    : QObject(parent)
{
}

QString FileTypeIconResolver::iconForSuffix(const QString &suffix, bool isDirectory) const
{
    if (isDirectory) {
        return fileTypeIconPath(QStringLiteral("folder"));
    }

    const QString s = suffix.toLower();
    static const QSet<QString> imageSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"),
        QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("ico"), QStringLiteral("svg"),
        QStringLiteral("svgz"), QStringLiteral("avif"), QStringLiteral("heic"), QStringLiteral("heif"),
        QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("raw"), QStringLiteral("cr2"),
        QStringLiteral("nef"), QStringLiteral("dng")
    };
    static const QSet<QString> audioSuffixes = {
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"), QStringLiteral("oga"),
        QStringLiteral("m4a"), QStringLiteral("m4b"), QStringLiteral("wav"), QStringLiteral("wma"),
        QStringLiteral("aac"), QStringLiteral("opus"), QStringLiteral("aiff"), QStringLiteral("aif"),
        QStringLiteral("mid"), QStringLiteral("midi")
    };
    static const QSet<QString> videoSuffixes = {
        QStringLiteral("mp4"), QStringLiteral("avi"), QStringLiteral("mkv"), QStringLiteral("mov"),
        QStringLiteral("wmv"), QStringLiteral("webm"), QStringLiteral("flv"), QStringLiteral("m4v"),
        QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("3gp"), QStringLiteral("ts")
    };
    static const QSet<QString> archiveSuffixes = {
        QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"), QStringLiteral("tar"),
        QStringLiteral("gz"), QStringLiteral("tgz"), QStringLiteral("bz2"), QStringLiteral("xz"),
        QStringLiteral("cab"), QStringLiteral("iso"), QStringLiteral("img"), QStringLiteral("vhd"),
        QStringLiteral("vhdx"), QStringLiteral("wim")
    };
    static const QSet<QString> spreadsheetSuffixes = {
        QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("xlsm"), QStringLiteral("csv"),
        QStringLiteral("ods"), QStringLiteral("tsv")
    };
    static const QSet<QString> presentationSuffixes = {
        QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("pps"), QStringLiteral("ppsx"),
        QStringLiteral("odp")
    };
    static const QSet<QString> codeSuffixes = {
        QStringLiteral("js"), QStringLiteral("mjs"), QStringLiteral("cjs"), QStringLiteral("ts"),
        QStringLiteral("tsx"), QStringLiteral("jsx"), QStringLiteral("html"), QStringLiteral("htm"),
        QStringLiteral("css"), QStringLiteral("scss"), QStringLiteral("sass"), QStringLiteral("less"),
        QStringLiteral("json"), QStringLiteral("xml"), QStringLiteral("yaml"), QStringLiteral("yml"),
        QStringLiteral("toml"), QStringLiteral("ini"), QStringLiteral("py"), QStringLiteral("cpp"),
        QStringLiteral("cxx"), QStringLiteral("cc"), QStringLiteral("c"), QStringLiteral("h"),
        QStringLiteral("hpp"), QStringLiteral("cs"), QStringLiteral("java"), QStringLiteral("go"),
        QStringLiteral("rs"), QStringLiteral("php"), QStringLiteral("rb"), QStringLiteral("sh"),
        QStringLiteral("sql")
    };
    static const QSet<QString> fontSuffixes = {
        QStringLiteral("ttf"), QStringLiteral("otf"), QStringLiteral("woff"), QStringLiteral("woff2"),
        QStringLiteral("fon")
    };
    static const QSet<QString> executableSuffixes = {
        QStringLiteral("exe"), QStringLiteral("bat"), QStringLiteral("cmd"), QStringLiteral("ps1"),
        QStringLiteral("com"), QStringLiteral("msi"), QStringLiteral("dll"), QStringLiteral("sys"),
        QStringLiteral("appx"), QStringLiteral("msix"), QStringLiteral("lnk")
    };

    if (hasSuffix(s, imageSuffixes)) return fileTypeIconPath(QStringLiteral("image"));
    if (hasSuffix(s, audioSuffixes)) return fileTypeIconPath(QStringLiteral("music"));
    if (hasSuffix(s, videoSuffixes)) return fileTypeIconPath(QStringLiteral("video"));
    if (hasSuffix(s, archiveSuffixes)) return fileTypeIconPath(QStringLiteral("archive"));
    if (s == QStringLiteral("pdf")) return fileTypeIconPath(QStringLiteral("pdf"));
    if (hasSuffix(s, spreadsheetSuffixes)) return fileTypeIconPath(QStringLiteral("spreadsheet"));
    if (hasSuffix(s, presentationSuffixes)) return fileTypeIconPath(QStringLiteral("presentation"));
    if (hasSuffix(s, codeSuffixes)) return fileTypeIconPath(QStringLiteral("code"));
    if (hasSuffix(s, fontSuffixes)) return fileTypeIconPath(QStringLiteral("font"));
    if (hasSuffix(s, executableSuffixes)) return fileTypeIconPath(QStringLiteral("executable"));
    return fileTypeIconPath(QStringLiteral("document"));
}

QString FileTypeIconResolver::iconForPath(const QString &path) const
{
    const QFileInfo info(path);
    return iconForSuffix(info.suffix(), info.isDir());
}

QString FileTypeIconResolver::iconForPathHint(const QString &path, bool isDirectory) const
{
    return iconForSuffix(suffixFromPath(path), isDirectory);
}
