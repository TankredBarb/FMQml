#include "GDriveExportPolicy.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>

namespace GDriveExportPolicy {
namespace {

constexpr QLatin1StringView GoogleDriveFolderMime{"application/vnd.google-apps.folder"};
constexpr QLatin1StringView GoogleDriveShortcutMime{"application/vnd.google-apps.shortcut"};
constexpr QLatin1StringView GoogleDriveAppsMimePrefix{"application/vnd.google-apps."};

ExportFormat pdfExportFormat()
{
    return {QStringLiteral("application/pdf"), QStringLiteral("pdf")};
}

QString exportSuffixForDestinationFile(QString destinationFilePath)
{
    destinationFilePath = destinationFilePath.trimmed();
    if (destinationFilePath.endsWith(QStringLiteral(".part"), Qt::CaseInsensitive)) {
        destinationFilePath.chop(QStringLiteral(".part").size());
    }
    return QFileInfo(destinationFilePath).suffix().toLower();
}

} // namespace

bool isGoogleAppsMimeType(const QString &mimeType)
{
    return mimeType.startsWith(GoogleDriveAppsMimePrefix, Qt::CaseInsensitive)
        && mimeType != GoogleDriveFolderMime
        && mimeType != GoogleDriveShortcutMime;
}

ExportFormat defaultExportFormatForGoogleAppsMimeType(QString mimeType)
{
    mimeType = mimeType.trimmed().toLower();
    if (mimeType == QLatin1String("application/vnd.google-apps.document")) {
        return {
            QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document"),
            QStringLiteral("docx")
        };
    }
    if (mimeType == QLatin1String("application/vnd.google-apps.spreadsheet")) {
        return {
            QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"),
            QStringLiteral("xlsx")
        };
    }
    if (mimeType == QLatin1String("application/vnd.google-apps.presentation")) {
        return {
            QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation"),
            QStringLiteral("pptx")
        };
    }
    if (mimeType == QLatin1String("application/vnd.google-apps.drawing")) {
        return {QStringLiteral("image/png"), QStringLiteral("png")};
    }
    return pdfExportFormat();
}

ExportFormat exportFormatForGoogleAppsDownload(const QString &mimeType, const QString &destinationFilePath)
{
    const ExportFormat defaultFormat = defaultExportFormatForGoogleAppsMimeType(mimeType);
    const QString requestedSuffix = exportSuffixForDestinationFile(destinationFilePath);
    if (requestedSuffix == QLatin1String("pdf")) {
        return pdfExportFormat();
    }
    if (!requestedSuffix.isEmpty() && requestedSuffix == defaultFormat.suffix) {
        return defaultFormat;
    }
    return defaultFormat;
}

QString withExportSuffix(QString name, const QString &suffix)
{
    name = name.trimmed();
    if (name.isEmpty() || suffix.isEmpty()) {
        return name;
    }
    const QString dottedSuffix = QLatin1Char('.') + suffix;
    return name.endsWith(dottedSuffix, Qt::CaseInsensitive) ? name : name + dottedSuffix;
}

QString safeLocalExportFileName(QString name)
{
    name = name.trimmed();
    static const QString invalidCharacters = QStringLiteral("<>:\"/\\|?*");
    for (QChar &ch : name) {
        const ushort code = ch.unicode();
        if (code < 0x20 || code == 0x7f || invalidCharacters.contains(ch)) {
            ch = QLatin1Char('_');
        }
    }
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) {
        name.chop(1);
    }
    return name.isEmpty() ? QStringLiteral("download") : name;
}

QString uniqueLocalFilePath(const QString &path)
{
    if (!QFileInfo::exists(path)) {
        return path;
    }

    const QFileInfo info(path);
    const QDir dir = info.dir();
    const QString baseName = info.completeBaseName().isEmpty()
        ? info.fileName()
        : info.completeBaseName();
    const QString suffix = info.suffix();
    for (int i = 1; i < 10000; ++i) {
        const QString candidateName = suffix.isEmpty()
            ? QStringLiteral("%1 copy %2").arg(baseName).arg(i)
            : QStringLiteral("%1 copy %2.%3").arg(baseName).arg(i).arg(suffix);
        const QString candidate = dir.filePath(candidateName);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return path;
}

QString iconSuffixForMimeType(QString mimeType)
{
    mimeType = mimeType.trimmed().toLower();
    if (mimeType.isEmpty()) {
        return {};
    }

    static const QHash<QString, QString> exactSuffixes = {
        {QStringLiteral("application/vnd.google-apps.document"), QStringLiteral("docx")},
        {QStringLiteral("application/vnd.google-apps.spreadsheet"), QStringLiteral("xlsx")},
        {QStringLiteral("application/vnd.google-apps.presentation"), QStringLiteral("pptx")},
        {QStringLiteral("application/vnd.google-apps.drawing"), QStringLiteral("png")},
        {QStringLiteral("application/vnd.google-apps.script"), QStringLiteral("js")},
        {QStringLiteral("application/vnd.google-apps.form"), QStringLiteral("docx")},
        {QStringLiteral("application/vnd.google-apps.site"), QStringLiteral("html")},
        {QStringLiteral("application/vnd.google-apps.fusiontable"), QStringLiteral("xlsx")},
        {QStringLiteral("application/pdf"), QStringLiteral("pdf")},
        {QStringLiteral("text/plain"), QStringLiteral("txt")},
        {QStringLiteral("text/markdown"), QStringLiteral("md")},
        {QStringLiteral("text/csv"), QStringLiteral("csv")},
        {QStringLiteral("text/tab-separated-values"), QStringLiteral("tsv")},
        {QStringLiteral("text/html"), QStringLiteral("html")},
        {QStringLiteral("text/css"), QStringLiteral("css")},
        {QStringLiteral("application/json"), QStringLiteral("json")},
        {QStringLiteral("application/ld+json"), QStringLiteral("json")},
        {QStringLiteral("application/javascript"), QStringLiteral("js")},
        {QStringLiteral("text/javascript"), QStringLiteral("js")},
        {QStringLiteral("application/xml"), QStringLiteral("xml")},
        {QStringLiteral("text/xml"), QStringLiteral("xml")},
        {QStringLiteral("application/x-yaml"), QStringLiteral("yaml")},
        {QStringLiteral("application/yaml"), QStringLiteral("yaml")},
        {QStringLiteral("application/zip"), QStringLiteral("zip")},
        {QStringLiteral("application/x-zip-compressed"), QStringLiteral("zip")},
        {QStringLiteral("application/x-rar-compressed"), QStringLiteral("rar")},
        {QStringLiteral("application/vnd.rar"), QStringLiteral("rar")},
        {QStringLiteral("application/x-7z-compressed"), QStringLiteral("7z")},
        {QStringLiteral("application/x-tar"), QStringLiteral("tar")},
        {QStringLiteral("application/gzip"), QStringLiteral("gz")},
        {QStringLiteral("application/x-gzip"), QStringLiteral("gz")},
        {QStringLiteral("application/x-xz"), QStringLiteral("xz")},
        {QStringLiteral("application/x-bzip2"), QStringLiteral("bz2")},
        {QStringLiteral("application/vnd.android.package-archive"), QStringLiteral("apk")},
        {QStringLiteral("application/msword"), QStringLiteral("doc")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.wordprocessingml.document"), QStringLiteral("docx")},
        {QStringLiteral("application/vnd.oasis.opendocument.text"), QStringLiteral("odt")},
        {QStringLiteral("application/vnd.ms-excel"), QStringLiteral("xls")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"), QStringLiteral("xlsx")},
        {QStringLiteral("application/vnd.oasis.opendocument.spreadsheet"), QStringLiteral("ods")},
        {QStringLiteral("application/vnd.ms-powerpoint"), QStringLiteral("ppt")},
        {QStringLiteral("application/vnd.openxmlformats-officedocument.presentationml.presentation"), QStringLiteral("pptx")},
        {QStringLiteral("application/vnd.oasis.opendocument.presentation"), QStringLiteral("odp")},
        {QStringLiteral("font/ttf"), QStringLiteral("ttf")},
        {QStringLiteral("font/otf"), QStringLiteral("otf")},
        {QStringLiteral("font/woff"), QStringLiteral("woff")},
        {QStringLiteral("font/woff2"), QStringLiteral("woff2")},
        {QStringLiteral("application/font-woff"), QStringLiteral("woff")},
        {QStringLiteral("application/x-font-ttf"), QStringLiteral("ttf")},
        {QStringLiteral("application/x-font-otf"), QStringLiteral("otf")},
        {QStringLiteral("application/vnd.ms-fontobject"), QStringLiteral("eot")},
        {QStringLiteral("application/x-msdownload"), QStringLiteral("exe")},
        {QStringLiteral("application/x-msi"), QStringLiteral("msi")},
        {QStringLiteral("application/java-archive"), QStringLiteral("jar")},
    };

    const QString exact = exactSuffixes.value(mimeType);
    if (!exact.isEmpty()) {
        return exact;
    }
    if (mimeType.startsWith(QStringLiteral("image/"))) {
        return QStringLiteral("png");
    }
    if (mimeType.startsWith(QStringLiteral("audio/"))) {
        return QStringLiteral("mp3");
    }
    if (mimeType.startsWith(QStringLiteral("video/"))) {
        return QStringLiteral("mp4");
    }
    if (mimeType.startsWith(GoogleDriveAppsMimePrefix, Qt::CaseInsensitive)) {
        return QStringLiteral("docx");
    }
    if (mimeType.startsWith(QStringLiteral("text/"))) {
        return QStringLiteral("txt");
    }
    return {};
}

} // namespace GDriveExportPolicy
