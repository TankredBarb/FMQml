#include "FileTypeIconResolver.h"

#include <QFileInfo>
#include <QList>
#include <QSet>
#include <QStringList>

namespace {
struct FileIconRule {
    QString iconName;
    QStringList extensions;
};

QString fileTypeIconPath(const QString &name)
{
    return QStringLiteral("qrc:/qt/qml/FM/qml/assets/filetypes-next/%1.svg").arg(name);
}

QString normalizedVirtualPathHint(QString path)
{
    path = path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/')).toLower();
    while (path.endsWith(QLatin1Char('/')) && !path.endsWith(QStringLiteral("://"))) {
        path.chop(1);
    }
    return path;
}

QString virtualFolderIconNameForPathHint(const QString &path)
{
    const QString value = normalizedVirtualPathHint(path);
    if (value == QLatin1String("gdrive://")) {
        return QStringLiteral("gdrive");
    }
    if (value == QLatin1String("gdrive://my-drive")) {
        return QStringLiteral("folder");
    }
    if (value == QLatin1String("gdrive://shared-with-me")) {
        return QStringLiteral("folder");
    }
    if (value == QLatin1String("gdrive://shortcuts")) {
        return QStringLiteral("folder");
    }
    if (value == QLatin1String("gdrive://trash")) {
        return QStringLiteral("folder");
    }
    if (value == QLatin1String("mega://") || value == QLatin1String("mega:///")) {
        return QStringLiteral("mega");
    }
    if (value.startsWith(QStringLiteral("mega://link/"))) {
        if (value.indexOf(QLatin1Char('/'), 12) < 0) {
            return QStringLiteral("mega");
        }
    }
    if (value == QLatin1String("mega:///cloud drive") || value == QLatin1String("mega://cloud drive")) {
        return QStringLiteral("folder");
    }
    if ((value.startsWith(QStringLiteral("mega:///")) || value.startsWith(QStringLiteral("mega://")))
        && (value.endsWith(QStringLiteral("/rubbish bin")) || value.endsWith(QStringLiteral("/rubbish")) || value.endsWith(QStringLiteral("/trash")))) {
        return QStringLiteral("folder");
    }
    return {};
}

const QList<FileIconRule> &nativeIconOverrideRules()
{
    static const QList<FileIconRule> rules = {
        {QStringLiteral("archive"), {QStringLiteral("apk")}},
        {QStringLiteral("fb2"), {QStringLiteral("fb2"), QStringLiteral("fb2.zip")}},
    };
    return rules;
}

QString normalizedExtension(QString extension)
{
    extension = extension.trimmed().toLower();
    while (extension.startsWith(QLatin1Char('.'))) {
        extension.remove(0, 1);
    }
    return extension;
}

bool fileNameMatchesExtension(const QString &fileName, const QString &extension)
{
    const QString ext = normalizedExtension(extension);
    return !ext.isEmpty() && fileName.toLower().endsWith(QLatin1Char('.') + ext);
}

QString matchingRuleIconForExtension(const QString &extension, const QList<FileIconRule> &rules)
{
    const QString ext = normalizedExtension(extension);
    if (ext.isEmpty()) {
        return {};
    }

    for (const FileIconRule &rule : rules) {
        for (const QString &ruleExtension : rule.extensions) {
            if (ext == normalizedExtension(ruleExtension)) {
                return rule.iconName;
            }
        }
    }
    return {};
}

QString matchingRuleIconForFileName(const QString &fileName, const QList<FileIconRule> &rules)
{
    if (fileName.isEmpty()) {
        return {};
    }

    for (const FileIconRule &rule : rules) {
        for (const QString &extension : rule.extensions) {
            if (fileNameMatchesExtension(fileName, extension)) {
                return rule.iconName;
            }
        }
    }
    return {};
}

bool hasSuffix(const QString &suffix, const QSet<QString> &suffixes)
{
    return suffixes.contains(suffix.toLower());
}

QString fileNameFromPathHint(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const int archiveSeparator = path.lastIndexOf(QStringLiteral("|/"));
    if (archiveSeparator >= 0) {
        path = path.mid(archiveSeparator + 2);
    }

    const int slash = path.lastIndexOf(QLatin1Char('/'));
    return slash >= 0 ? path.mid(slash + 1) : QFileInfo(path).fileName();
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
    const QString explicitIcon = matchingRuleIconForExtension(s, nativeIconOverrideRules());
    if (!explicitIcon.isEmpty()) {
        return fileTypeIconPath(explicitIcon);
    }

    static const QSet<QString> imageSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"),
        QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("ico"), QStringLiteral("svg"),
        QStringLiteral("svgz"), QStringLiteral("avif"), QStringLiteral("heic"), QStringLiteral("heif"),
        QStringLiteral("tif"), QStringLiteral("tiff"), QStringLiteral("raw"), QStringLiteral("cr2"),
        QStringLiteral("nef"), QStringLiteral("dng"), QStringLiteral("arw"), QStringLiteral("orf"),
        QStringLiteral("rw2"), QStringLiteral("psd"), QStringLiteral("jxl")
    };
    static const QSet<QString> audioSuffixes = {
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("ogg"), QStringLiteral("oga"),
        QStringLiteral("m4a"), QStringLiteral("m4b"), QStringLiteral("wav"), QStringLiteral("wma"),
        QStringLiteral("aac"), QStringLiteral("opus"), QStringLiteral("aiff"), QStringLiteral("aif"),
        QStringLiteral("mid"), QStringLiteral("midi"), QStringLiteral("alac"), QStringLiteral("ape"),
        QStringLiteral("mka")
    };
    static const QSet<QString> videoSuffixes = {
        QStringLiteral("mp4"), QStringLiteral("avi"), QStringLiteral("mkv"), QStringLiteral("mov"),
        QStringLiteral("wmv"), QStringLiteral("webm"), QStringLiteral("flv"), QStringLiteral("m4v"),
        QStringLiteral("mpg"), QStringLiteral("mpeg"), QStringLiteral("3gp"), QStringLiteral("ts"),
        QStringLiteral("mts"), QStringLiteral("m2ts"), QStringLiteral("ogv"), QStringLiteral("vob")
    };
    static const QSet<QString> archiveSuffixes = {
        QStringLiteral("zip"), QStringLiteral("rar"), QStringLiteral("7z"), QStringLiteral("tar"),
        QStringLiteral("gz"), QStringLiteral("tgz"), QStringLiteral("bz2"), QStringLiteral("xz"),
        QStringLiteral("cab"), QStringLiteral("iso"), QStringLiteral("img"), QStringLiteral("vhd"),
        QStringLiteral("vhdx"), QStringLiteral("wim"), QStringLiteral("zst"), QStringLiteral("txz"),
        QStringLiteral("tbz"), QStringLiteral("tbz2"), QStringLiteral("tlz"), QStringLiteral("lz")
    };
    static const QSet<QString> textSuffixes = {
        QStringLiteral("txt"), QStringLiteral("text"), QStringLiteral("log"), QStringLiteral("md"),
        QStringLiteral("markdown"), QStringLiteral("rst"), QStringLiteral("nfo"), QStringLiteral("diz")
    };
    static const QSet<QString> documentSuffixes = {
        QStringLiteral("doc"), QStringLiteral("docx"), QStringLiteral("docm"), QStringLiteral("dot"),
        QStringLiteral("dotx"), QStringLiteral("odt"), QStringLiteral("ott"), QStringLiteral("rtf"),
        QStringLiteral("pages"), QStringLiteral("tex")
    };
    static const QSet<QString> spreadsheetSuffixes = {
        QStringLiteral("xls"), QStringLiteral("xlsx"), QStringLiteral("xlsm"), QStringLiteral("csv"),
        QStringLiteral("xlsb"), QStringLiteral("xlt"), QStringLiteral("xltx"), QStringLiteral("ods"),
        QStringLiteral("ots"), QStringLiteral("tsv"), QStringLiteral("numbers")
    };
    static const QSet<QString> presentationSuffixes = {
        QStringLiteral("ppt"), QStringLiteral("pptx"), QStringLiteral("pps"), QStringLiteral("ppsx"),
        QStringLiteral("pptm"), QStringLiteral("pot"), QStringLiteral("potx"), QStringLiteral("odp"),
        QStringLiteral("otp"), QStringLiteral("key")
    };
    static const QSet<QString> codeSuffixes = {
        QStringLiteral("js"), QStringLiteral("mjs"), QStringLiteral("cjs"), QStringLiteral("ts"),
        QStringLiteral("tsx"), QStringLiteral("jsx"), QStringLiteral("html"), QStringLiteral("htm"),
        QStringLiteral("css"), QStringLiteral("scss"), QStringLiteral("sass"), QStringLiteral("less"),
        QStringLiteral("json"), QStringLiteral("xml"), QStringLiteral("yaml"), QStringLiteral("yml"),
        QStringLiteral("toml"), QStringLiteral("ini"), QStringLiteral("conf"), QStringLiteral("cfg"),
        QStringLiteral("qml"), QStringLiteral("py"), QStringLiteral("cpp"), QStringLiteral("cxx"),
        QStringLiteral("cc"), QStringLiteral("c"), QStringLiteral("h"),
        QStringLiteral("hpp"), QStringLiteral("cs"), QStringLiteral("java"), QStringLiteral("go"),
        QStringLiteral("rs"), QStringLiteral("php"), QStringLiteral("rb"), QStringLiteral("sh"),
        QStringLiteral("sql"), QStringLiteral("swift"), QStringLiteral("kt"), QStringLiteral("kts"),
        QStringLiteral("dart"), QStringLiteral("lua"), QStringLiteral("pl"), QStringLiteral("r"),
        QStringLiteral("vue"), QStringLiteral("svelte")
    };
    static const QSet<QString> fontSuffixes = {
        QStringLiteral("ttf"), QStringLiteral("otf"), QStringLiteral("woff"), QStringLiteral("woff2"),
        QStringLiteral("fon"), QStringLiteral("ttc"), QStringLiteral("otc"), QStringLiteral("eot")
    };
    static const QSet<QString> executableSuffixes = {
        QStringLiteral("exe"), QStringLiteral("bat"), QStringLiteral("cmd"), QStringLiteral("ps1"),
        QStringLiteral("com"), QStringLiteral("msi"), QStringLiteral("dll"), QStringLiteral("sys"),
        QStringLiteral("appx"), QStringLiteral("msix"), QStringLiteral("scr"), QStringLiteral("cpl"),
        QStringLiteral("jar")
    };
    static const QSet<QString> shortcutSuffixes = {
        QStringLiteral("lnk"), QStringLiteral("url"), QStringLiteral("shortcut")
    };

    if (hasSuffix(s, imageSuffixes)) return fileTypeIconPath(QStringLiteral("image"));
    if (hasSuffix(s, audioSuffixes)) return fileTypeIconPath(QStringLiteral("music"));
    if (hasSuffix(s, videoSuffixes)) return fileTypeIconPath(QStringLiteral("video"));
    if (hasSuffix(s, archiveSuffixes)) return fileTypeIconPath(QStringLiteral("archive"));
    if (s == QStringLiteral("pdf")) return fileTypeIconPath(QStringLiteral("pdf"));
    if (hasSuffix(s, textSuffixes)) return fileTypeIconPath(QStringLiteral("text"));
    if (hasSuffix(s, documentSuffixes)) return fileTypeIconPath(QStringLiteral("document"));
    if (hasSuffix(s, spreadsheetSuffixes)) return fileTypeIconPath(QStringLiteral("spreadsheet"));
    if (hasSuffix(s, presentationSuffixes)) return fileTypeIconPath(QStringLiteral("presentation"));
    if (hasSuffix(s, codeSuffixes)) return fileTypeIconPath(QStringLiteral("code"));
    if (hasSuffix(s, fontSuffixes)) return fileTypeIconPath(QStringLiteral("font"));
    if (hasSuffix(s, shortcutSuffixes)) return fileTypeIconPath(QStringLiteral("shortcut"));
    if (hasSuffix(s, executableSuffixes)) return fileTypeIconPath(QStringLiteral("executable"));
    return fileTypeIconPath(QStringLiteral("document"));
}

QString FileTypeIconResolver::iconForPath(const QString &path) const
{
    if (const QString virtualIcon = virtualFolderIconNameForPathHint(path); !virtualIcon.isEmpty()) {
        return fileTypeIconPath(virtualIcon);
    }

    const QFileInfo info(path);
    const QString explicitIcon = info.isDir()
        ? QString{}
        : matchingRuleIconForFileName(info.fileName(), nativeIconOverrideRules());
    if (!explicitIcon.isEmpty()) {
        return fileTypeIconPath(explicitIcon);
    }
    return iconForSuffix(info.suffix(), info.isDir());
}

QString FileTypeIconResolver::iconForPathHint(const QString &path, bool isDirectory) const
{
    if (isDirectory) {
        if (const QString virtualIcon = virtualFolderIconNameForPathHint(path); !virtualIcon.isEmpty()) {
            return fileTypeIconPath(virtualIcon);
        }
    }

    const QString fileName = fileNameFromPathHint(path);
    const QString explicitIcon = isDirectory
        ? QString{}
        : matchingRuleIconForFileName(fileName, nativeIconOverrideRules());
    if (!explicitIcon.isEmpty()) {
        return fileTypeIconPath(explicitIcon);
    }
    return iconForSuffix(QFileInfo(fileName).suffix(), isDirectory);
}

QString FileTypeIconResolver::nativeIconOverrideForPathHint(const QString &path, bool isDirectory) const
{
    if (isDirectory) {
        return {};
    }

    const QString iconName = matchingRuleIconForFileName(fileNameFromPathHint(path), nativeIconOverrideRules());
    return iconName.isEmpty() ? QString{} : fileTypeIconPath(iconName);
}
