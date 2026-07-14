#include "QuickLookController.h"
#include <QFileInfo>
#include <QFileDevice>
#include <QFile>
#include <QByteArray>
#include <QMimeDatabase>
#include <QMimeType>
#include <QDateTime>
#include <QLocale>
#include <QStringList>
#include <QMetaObject>
#include <QPointer>
#include <QImageReader>
#include <QImage>
#include <QPixelFormat>
#include <QRegularExpression>
#include <QUrl>
#include <QTimer>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrentRun>
#include <memory>
#include <utility>
#include "../core/ArchiveFileProvider.h"
#include "../core/ArchiveSupport.h"
#include "../core/FileAccessResolver.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileProviderPluginRegistry.h"
#include "../core/MetadataExtractor.h"
#include "../core/DriveUtils.h"
#include "../core/IsoMountManager.h"
#include "../core/LinuxAdminBroker.h"
#include "../core/CleanupSubsystem.h"
#include <QCoreApplication>
#include <QStorageInfo>
#include <QDir>
#include <QUuid>

#ifdef HAS_TAGLIB
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <taglib/flacfile.h>
#include <taglib/flacpicture.h>
#include <taglib/mp4file.h>
#include <taglib/mp4tag.h>
#include <taglib/mp4coverart.h>
#include <taglib/vorbisfile.h>
#include <taglib/taglib.h>
#endif

#include "PreviewInternal.h"

namespace PreviewInternal {
bool quickLookPreviewTraceEnabled()
{
    return qEnvironmentVariableIsSet("FM_QUICKLOOK_PREVIEW_TRACE");
}

bool isDjvuDocument(const QString &suffix, const QString &mimeName)
{
    const QString lowerSuffix = suffix.toLower();
    const QString lowerMime = mimeName.toLower();
    return lowerSuffix == QLatin1String("djvu")
        || lowerSuffix == QLatin1String("djv")
        || lowerMime == QLatin1String("image/vnd.djvu")
        || lowerMime == QLatin1String("image/vnd.djvu+multipage")
        || lowerMime == QLatin1String("image/x-djvu")
        || lowerMime == QLatin1String("application/x-djvu");
}

bool isPreviewableRasterImage(const QString &suffix, const QString &mimeName)
{
    return mimeName.startsWith(QStringLiteral("image/"))
        && mimeName != QStringLiteral("image/svg+xml")
        && !isDjvuDocument(suffix, mimeName);
}

QVariant prop(const QString &label, const QString &value)
{
    QVariantMap m;
    m.insert(QStringLiteral("label"), label);
    m.insert(QStringLiteral("value"), value);
    return QVariant::fromValue(m);
}

QString propertyValue(const QVariantList &properties, const QString &label)
{
    for (const QVariant &property : properties) {
        const QVariantMap map = property.toMap();
        if (map.value(QStringLiteral("label")).toString() == label) {
            return map.value(QStringLiteral("value")).toString();
        }
    }
    return {};
}

QSize dimensionsFromText(const QString &text)
{
    static const QRegularExpression numberPattern(QStringLiteral(R"((\d+(?:\.\d+)?))"));
    QRegularExpressionMatchIterator it = numberPattern.globalMatch(text);

    QList<double> values;
    while (it.hasNext() && values.size() < 2) {
        bool ok = false;
        const double value = it.next().captured(1).toDouble(&ok);
        if (ok && value > 0.0) {
            values.append(value);
        }
    }

    if (values.size() < 2) {
        return {};
    }

    return QSize(qRound(values.at(0)), qRound(values.at(1)));
}

void setPropertyValue(QVariantList &properties, const QString &label, const QString &value)
{
    for (QVariant &property : properties) {
        QVariantMap map = property.toMap();
        if (map.value(QStringLiteral("label")).toString() == label) {
            map.insert(QStringLiteral("value"), value);
            property = map;
            return;
        }
    }
    properties.append(prop(label, value));
}

void removePropertyValue(QVariantList &properties, const QString &label)
{
    for (qsizetype i = properties.size() - 1; i >= 0; --i) {
        const QVariantMap map = properties.at(i).toMap();
        if (map.value(QStringLiteral("label")).toString() == label) {
            properties.removeAt(i);
        }
    }
}


QString remotePreviewRoot(bool create)
{
    const QString base = StagingLocationPolicy::defaultCleanupRoot();
    if (base.isEmpty()) {
        return {};
    }
    const QString previewRoot = QDir(base).filePath(QStringLiteral("remote-preview"));
    if (create && !QDir().mkpath(previewRoot)) {
        return {};
    }
    return QDir::fromNativeSeparators(previewRoot);
}

bool isInsideDirectory(const QString &rootPath, const QString &candidatePath)
{
    const QString root = QDir::cleanPath(QFileInfo(rootPath).absoluteFilePath());
    const QString candidate = QDir::cleanPath(QFileInfo(candidatePath).absoluteFilePath());
    if (root.isEmpty() || candidate.isEmpty() || candidate == root) {
        return false;
    }
    const QString prefix = root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/');
    return candidate.startsWith(prefix, Qt::CaseInsensitive);
}

void removeRemotePreviewDir(const QString &path)
{
    if (path.trimmed().isEmpty()) {
        return;
    }
    const QString root = remotePreviewRoot(false);
    if (root.isEmpty() || !isInsideDirectory(root, path)) {
        return;
    }
    QDir(path).removeRecursively();
}

QString redactedPreviewPathForLog(const QString &path)
{
    if (FileProviderFactory::hasPluginProviderForPath(path)) {
        const int schemeEnd = path.indexOf(QStringLiteral("://"));
        const QString scheme = schemeEnd > 0 ? path.left(schemeEnd) : QStringLiteral("provider");
        return scheme + QStringLiteral("://<redacted>");
    }
    return path;
}

QString safePreviewFileName(QString name)
{
    name = name.trimmed();
    if (name.isEmpty()) {
        name = QStringLiteral("preview.bin");
    }
    static const QRegularExpression invalid(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    name.replace(invalid, QStringLiteral("_"));
    while (name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1Char(' '))) {
        name.chop(1);
    }
    return name.isEmpty() ? QStringLiteral("preview.bin") : name.left(180);
}

QString remotePreviewTooLargeText(const FileEntry &entry)
{
    const QString size = entry.size > 0 ? DriveUtils::formatSize(entry.size) : QStringLiteral("unknown size");
    return QStringLiteral("Remote preview is limited to %1.\nCopy the file locally to open the full content.\n\nName: %2\nSize: %3")
        .arg(DriveUtils::formatSize(kRemotePreviewMaterializeLimit), entry.name, size);
}

bool hasDriveCapability(const FileEntry &entry, QLatin1StringView capability)
{
    const QString token = QStringLiteral("%1: true").arg(QString(capability));
    return entry.providerCapabilitiesText.contains(token, Qt::CaseInsensitive);
}

QString googleDriveAccessSummary(const FileEntry &entry)
{
    QStringList items;
    if (entry.isDirectory) {
        if (hasDriveCapability(entry, QLatin1StringView("canListChildren"))) {
            items.append(QStringLiteral("Browse"));
            items.append(QStringLiteral("Traverse"));
        }
        if (hasDriveCapability(entry, QLatin1StringView("canAddChildren"))) {
            items.append(QStringLiteral("Create"));
        }
    } else {
        if (hasDriveCapability(entry, QLatin1StringView("canDownload"))) {
            items.append(QStringLiteral("Read"));
        }
    }

    if (hasDriveCapability(entry, QLatin1StringView("canTrash"))
        || hasDriveCapability(entry, QLatin1StringView("canDelete"))) {
        items.append(QStringLiteral("Delete"));
    }

    return items.isEmpty() ? QStringLiteral("Access unknown") : items.join(QStringLiteral(", "));
}

QString googleDriveAccountLabel()
{
    const QVariantMap status = FileProviderPluginRegistry::instance().triggerAction(
        QStringLiteral("fm.gdrive-provider::authStatus"),
        {});
    if (!status.value(QStringLiteral("signedIn")).toBool()) {
        return {};
    }
    return status.value(QStringLiteral("accountLabel")).toString().trimmed();
}

bool isTextSuffix(const QString &suffix)
{
    static const QStringList textSuffixes = {
        QStringLiteral("txt"),
        QStringLiteral("log"),
        QStringLiteral("md"),
        QStringLiteral("json"),
        QStringLiteral("xml"),
        QStringLiteral("fb2"),
        QStringLiteral("csv"),
        QStringLiteral("ini"),
        QStringLiteral("conf"),
        QStringLiteral("cfg"),
        QStringLiteral("yaml"),
        QStringLiteral("yml"),
        QStringLiteral("toml"),
        QStringLiteral("js"),
        QStringLiteral("ts"),
        QStringLiteral("css"),
        QStringLiteral("html"),
        QStringLiteral("qml"),
        QStringLiteral("cpp"),
        QStringLiteral("c"),
        QStringLiteral("h"),
        QStringLiteral("hpp"),
        QStringLiteral("py"),
        QStringLiteral("java"),
        QStringLiteral("cs"),
        QStringLiteral("sh"),
        QStringLiteral("ps1"),
        QStringLiteral("svg")
    };
    return textSuffixes.contains(suffix.toLower());
}

bool isOfficeDocumentSuffix(const QString &suffix)
{
    static const QStringList officeSuffixes = {
        QStringLiteral("doc"),
        QStringLiteral("docx"),
        QStringLiteral("docm"),
        QStringLiteral("dot"),
        QStringLiteral("dotx"),
        QStringLiteral("odt"),
        QStringLiteral("ott"),
        QStringLiteral("rtf"),
        QStringLiteral("pages"),
        QStringLiteral("xls"),
        QStringLiteral("xlsx"),
        QStringLiteral("xlsm"),
        QStringLiteral("xlsb"),
        QStringLiteral("xlt"),
        QStringLiteral("xltx"),
        QStringLiteral("ods"),
        QStringLiteral("ots"),
        QStringLiteral("numbers"),
        QStringLiteral("ppt"),
        QStringLiteral("pptx"),
        QStringLiteral("pptm"),
        QStringLiteral("pps"),
        QStringLiteral("ppsx"),
        QStringLiteral("pot"),
        QStringLiteral("potx"),
        QStringLiteral("odp"),
        QStringLiteral("otp"),
        QStringLiteral("key")
    };
    return officeSuffixes.contains(suffix.toLower());
}

QString officeDocumentMimeLabel(const QString &suffix)
{
    const QString normalized = suffix.toLower();
    if (normalized.startsWith(QStringLiteral("xls"))
        || normalized == QStringLiteral("ods")
        || normalized == QStringLiteral("ots")
        || normalized == QStringLiteral("numbers")) {
        return QStringLiteral("Spreadsheet");
    }
    if (normalized.startsWith(QStringLiteral("ppt"))
        || normalized.startsWith(QStringLiteral("pps"))
        || normalized.startsWith(QStringLiteral("pot"))
        || normalized == QStringLiteral("odp")
        || normalized == QStringLiteral("otp")
        || normalized == QStringLiteral("key")) {
        return QStringLiteral("Presentation");
    }
    return QStringLiteral("Document");
}

bool isFb2Suffix(const QString &suffix)
{
    return suffix.compare(QStringLiteral("fb2"), Qt::CaseInsensitive) == 0;
}

bool isGoogleAppsMimeType(const QString &mimeType)
{
    return mimeType.startsWith(QStringLiteral("application/vnd.google-apps."), Qt::CaseInsensitive)
        && mimeType.compare(QStringLiteral("application/vnd.google-apps.folder"), Qt::CaseInsensitive) != 0;
}

QString materializedPreviewSuffix(const FileEntry &entry)
{
    const QString mimeType = entry.isShortcut && !entry.shortcutTargetMimeType.isEmpty()
        ? entry.shortcutTargetMimeType
        : entry.mimeType;
    if (isGoogleAppsMimeType(mimeType)) {
        return QStringLiteral("pdf");
    }
    if (entry.isShortcut && entry.suffix == QLatin1String("shortcut")) {
        const QString suffix = QMimeDatabase().mimeTypeForName(mimeType).preferredSuffix();
        if (!suffix.isEmpty()) {
            return suffix;
        }
    }
    return entry.suffix;
}

bool isVideoPreviewEntry(const FileEntry &entry)
{
    const QString suffix = materializedPreviewSuffix(entry).toLower();
    const QString mimeType = entry.isShortcut && !entry.shortcutTargetMimeType.isEmpty()
        ? entry.shortcutTargetMimeType
        : entry.mimeType;
    return mimeType.startsWith(QStringLiteral("video/"), Qt::CaseInsensitive)
        || suffix == QLatin1String("mp4")
        || suffix == QLatin1String("m4v")
        || suffix == QLatin1String("mov")
        || suffix == QLatin1String("webm")
        || suffix == QLatin1String("mkv")
        || suffix == QLatin1String("avi")
        || suffix == QLatin1String("wmv");
}

bool materializedVideoLooksUsable(const QString &path, const QString &suffix)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    const QByteArray header = file.read(512).trimmed();
    if (header.isEmpty()) {
        return false;
    }

    const QByteArray lower = header.left(64).toLower();
    if (lower.startsWith("<!doctype html")
        || lower.startsWith("<html")
        || lower.startsWith("{")
        || lower.startsWith("for (;;);")) {
        return false;
    }

    const QString contentMime = QMimeDatabase().mimeTypeForFile(path, QMimeDatabase::MatchContent).name();
    if (contentMime.startsWith(QStringLiteral("video/"), Qt::CaseInsensitive)) {
        return true;
    }

    const QString normalizedSuffix = suffix.toLower();
    if ((normalizedSuffix == QLatin1String("mp4")
         || normalizedSuffix == QLatin1String("m4v")
         || normalizedSuffix == QLatin1String("mov"))
        && header.size() >= 12
        && header.mid(4, 4) == QByteArrayLiteral("ftyp")) {
        return true;
    }
    if (normalizedSuffix == QLatin1String("webm")
        && header.size() >= 4
        && static_cast<unsigned char>(header.at(0)) == 0x1A
        && static_cast<unsigned char>(header.at(1)) == 0x45
        && static_cast<unsigned char>(header.at(2)) == 0xDF
        && static_cast<unsigned char>(header.at(3)) == 0xA3) {
        return true;
    }

    return false;
}


} // namespace PreviewInternal
