#include "FileTypeIconResolver.h"

#include <QFileInfo>
#include <QJsonDocument>
#include <QList>
#include <QSettings>
#include <QSet>
#include <QStringList>
#include <QUrl>

#include <algorithm>

namespace {
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

bool hasSuffix(const QString &suffix, const QSet<QString> &suffixes)
{
    return suffixes.contains(suffix.toLower());
}

bool isBundledIconName(const QString &name)
{
    static const QSet<QString> names = {
        QStringLiteral("archive"), QStringLiteral("code"), QStringLiteral("document"), QStringLiteral("epub"),
        QStringLiteral("executable"), QStringLiteral("fb2"), QStringLiteral("folder"), QStringLiteral("font"),
        QStringLiteral("gdrive"), QStringLiteral("gdrive-badge-shared"), QStringLiteral("gdrive-badge-shortcut"),
        QStringLiteral("gdrive-badge-trash"), QStringLiteral("gdrive-file-shortcut"), QStringLiteral("image"),
        QStringLiteral("instagram"), QStringLiteral("instagram-badge-load-more"), QStringLiteral("instagram-badge-stories"),
        QStringLiteral("instagram-load-more"), QStringLiteral("instagram-stories"), QStringLiteral("mega"),
        QStringLiteral("music"), QStringLiteral("pdf"), QStringLiteral("presentation"), QStringLiteral("shortcut"),
        QStringLiteral("spreadsheet"), QStringLiteral("telegram"), QStringLiteral("telegram-badge-channel"),
        QStringLiteral("telegram-badge-chat"), QStringLiteral("telegram-badge-downloads"),
        QStringLiteral("telegram-badge-load-more"), QStringLiteral("telegram-chats"),
        QStringLiteral("telegram-downloads"), QStringLiteral("telegram-saved"), QStringLiteral("text"), QStringLiteral("video")
    };
    return names.contains(name);
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
    loadIconOverrides();
}

QString FileTypeIconResolver::iconForSuffix(const QString &suffix, bool isDirectory) const
{
    if (isDirectory) {
        return fileTypeIconPath(QStringLiteral("folder"));
    }

    const QString s = suffix.toLower();
    if (s == QLatin1String("epub")) return fileTypeIconPath(QStringLiteral("epub"));
    if (s == QLatin1String("fb2")) return fileTypeIconPath(QStringLiteral("fb2"));

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
        QStringLiteral("tbz"), QStringLiteral("tbz2"), QStringLiteral("tlz"), QStringLiteral("lz"), QStringLiteral("apk")
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
    if (!info.isDir() && info.fileName().endsWith(QStringLiteral(".fb2.zip"), Qt::CaseInsensitive)) return fileTypeIconPath(QStringLiteral("fb2"));
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
    if (!isDirectory && fileName.endsWith(QStringLiteral(".fb2.zip"), Qt::CaseInsensitive)) return fileTypeIconPath(QStringLiteral("fb2"));
    return iconForSuffix(QFileInfo(fileName).suffix(), isDirectory);
}

QString FileTypeIconResolver::nativeIconOverrideForPathHint(const QString &path, bool isDirectory) const
{
    if (isDirectory) {
        return {};
    }

    QReadLocker locker(&m_iconOverridesLock);
    const IconOverride *rule = matchingOverride(fileNameFromPathHint(path));
    if (!rule) return {};
    if (rule->sourceType == QLatin1String("bundled") && isBundledIconName(rule->sourceValue)) return fileTypeIconPath(rule->sourceValue);
    if (rule->sourceType == QLatin1String("theme")) return QStringLiteral("image://icon/theme/") + QUrl::toPercentEncoding(rule->sourceValue);
    if (rule->sourceType == QLatin1String("file")) return QUrl::fromLocalFile(rule->sourceValue).toString();
    return {};
}

QVariantList FileTypeIconResolver::iconOverrides() const
{
    QReadLocker locker(&m_iconOverridesLock);
    QVariantList rows;
    for (const IconOverride &rule : m_iconOverrides) {
        const bool available = rule.sourceType == QLatin1String("bundled")
            ? isBundledIconName(rule.sourceValue)
            : rule.sourceType != QLatin1String("file")
                || (QFileInfo(rule.sourceValue).isAbsolute() && QFileInfo::exists(rule.sourceValue));
        rows.append(QVariantMap{{QStringLiteral("suffix"), rule.suffix}, {QStringLiteral("sourceType"), rule.sourceType}, {QStringLiteral("sourceValue"), rule.sourceValue}, {QStringLiteral("available"), available}});
    }
    return rows;
}

int FileTypeIconResolver::iconOverrideRevision() const
{
    QReadLocker locker(&m_iconOverridesLock);
    return m_iconOverrideRevision;
}

QString FileTypeIconResolver::normalizedSuffix(QString suffix) const
{
    suffix = suffix.trimmed().toLower();
    while (suffix.startsWith(QLatin1Char('.'))) suffix.remove(0, 1);
    return suffix;
}

namespace {
bool isValidOverrideSuffix(const QString &suffix)
{
    if (suffix.isEmpty() || suffix.startsWith(QLatin1Char('.')) || suffix.endsWith(QLatin1Char('.'))) return false;
    for (const QChar character : suffix) {
        if (character.isSpace() || character == QLatin1Char('/') || character == QLatin1Char('\\')) return false;
    }
    return !suffix.contains(QStringLiteral(".."));
}
}

const FileTypeIconResolver::IconOverride *FileTypeIconResolver::matchingOverride(const QString &path) const
{
    const QString name = fileNameFromPathHint(path).toLower();
    const IconOverride *best = nullptr;
    for (const IconOverride &rule : m_iconOverrides)
        if (name.endsWith(QLatin1Char('.') + rule.suffix) && (!best || rule.suffix.size() > best->suffix.size())) best = &rule;
    return best;
}

void FileTypeIconResolver::loadIconOverrides()
{
    QWriteLocker locker(&m_iconOverridesLock);
    QSettings settings;
    settings.beginGroup(QStringLiteral("appearance"));
    const QJsonDocument document = QJsonDocument::fromJson(settings.value(QStringLiteral("iconOverridesRules")).toByteArray());
    const QVariantList values = document.toVariant().toList();
    settings.endGroup();
    bool migratedFb2ZipRule = false;
    for (const QVariant &value : values) {
        const QVariantMap map = value.toMap();
        const QString suffix = normalizedSuffix(map.value(QStringLiteral("suffix")).toString());
        const QString type = map.value(QStringLiteral("sourceType")).toString().trimmed().toLower();
        QString source = map.value(QStringLiteral("sourceValue")).toString().trimmed();
        if (suffix == QLatin1String("fb2.zip") && type == QLatin1String("bundled") && source == QLatin1String("fb2.zip")) {
            source = QStringLiteral("fb2");
            migratedFb2ZipRule = true;
        }
        if (isValidOverrideSuffix(suffix) && !source.isEmpty()
            && (type == QLatin1String("bundled") || type == QLatin1String("theme") || type == QLatin1String("file"))) {
            auto existing = std::find_if(m_iconOverrides.begin(), m_iconOverrides.end(),
                                         [&suffix](const IconOverride &rule) { return rule.suffix == suffix; });
            if (existing == m_iconOverrides.end()) m_iconOverrides.append({suffix, type, source});
            else *existing = {suffix, type, source};
        }
    }
    if (migratedFb2ZipRule) saveIconOverrides();
}

void FileTypeIconResolver::saveIconOverrides() const
{
    QSettings settings;
    QVariantList rows;
    for (const IconOverride &rule : m_iconOverrides) {
        rows.append(QVariantMap{{QStringLiteral("suffix"), rule.suffix}, {QStringLiteral("sourceType"), rule.sourceType}, {QStringLiteral("sourceValue"), rule.sourceValue}});
    }
    settings.beginGroup(QStringLiteral("appearance"));
    settings.setValue(QStringLiteral("iconOverridesRules"), QJsonDocument::fromVariant(rows).toJson(QJsonDocument::Compact));
    settings.endGroup();
    settings.sync();
}

bool FileTypeIconResolver::addOrUpdateIconOverride(const QString &suffix, const QString &sourceType, const QString &sourceValue)
{
    const QString key = normalizedSuffix(suffix), type = sourceType.trimmed().toLower(), source = sourceValue.trimmed();
    if (!isValidOverrideSuffix(key) || source.isEmpty() || (type != QLatin1String("bundled") && type != QLatin1String("theme") && type != QLatin1String("file"))
        || (type == QLatin1String("bundled") && !isBundledIconName(source))) return false;
    {
        QWriteLocker locker(&m_iconOverridesLock);
        for (IconOverride &rule : m_iconOverrides) if (rule.suffix == key) { rule = {key, type, source}; saveIconOverrides(); ++m_iconOverrideRevision; locker.unlock(); emit iconOverridesChanged(); return true; }
        m_iconOverrides.append({key, type, source}); saveIconOverrides(); ++m_iconOverrideRevision;
    }
    emit iconOverridesChanged(); return true;
}

bool FileTypeIconResolver::removeIconOverride(const QString &suffix)
{
    const QString key = normalizedSuffix(suffix);
    {
        QWriteLocker locker(&m_iconOverridesLock);
        for (qsizetype i = 0; i < m_iconOverrides.size(); ++i) if (m_iconOverrides.at(i).suffix == key) { m_iconOverrides.removeAt(i); saveIconOverrides(); ++m_iconOverrideRevision; locker.unlock(); emit iconOverridesChanged(); return true; }
    }
    return false;
}

void FileTypeIconResolver::clearIconOverrides()
{
    {
        QWriteLocker locker(&m_iconOverridesLock);
        if (m_iconOverrides.isEmpty()) return;
        m_iconOverrides.clear(); saveIconOverrides(); ++m_iconOverrideRevision;
    }
    emit iconOverridesChanged();
}

QStringList FileTypeIconResolver::availableBundledIconNames() const
{
    return {QStringLiteral("archive"), QStringLiteral("code"), QStringLiteral("document"),
            QStringLiteral("epub"), QStringLiteral("executable"), QStringLiteral("fb2"),
            QStringLiteral("folder"), QStringLiteral("font"), QStringLiteral("image"),
            QStringLiteral("music"), QStringLiteral("pdf"), QStringLiteral("presentation"),
            QStringLiteral("shortcut"), QStringLiteral("spreadsheet"), QStringLiteral("text"),
            QStringLiteral("video")};
}

void FileTypeIconResolver::reloadIconOverrides()
{
    {
        QWriteLocker locker(&m_iconOverridesLock);
        m_iconOverrides.clear();
    }
    loadIconOverrides();
    {
        QWriteLocker locker(&m_iconOverridesLock);
        ++m_iconOverrideRevision;
    }
    emit iconOverridesChanged();
}
