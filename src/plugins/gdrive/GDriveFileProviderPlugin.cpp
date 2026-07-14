#include "GDriveFileProviderPlugin.h"

#include "GDriveAuth.h"
#include "GDriveCache.h"
#include "GDriveExportPolicy.h"
#include "GDriveFileProviderInternal.h"
#include "GDrivePath.h"
#include "GDriveTypes.h"

#include <optional>

#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QVariantList>

namespace {

constexpr QLatin1StringView GoogleDriveSignOutAction{"signOut"};
constexpr QLatin1StringView GoogleDriveAuthStatusAction{"authStatus"};
constexpr QLatin1StringView GoogleDriveRawCapabilitiesAction{"rawCapabilities"};
constexpr QLatin1StringView GoogleDriveDownloadPdfAction{"downloadAsPdf"};
constexpr QLatin1StringView GoogleDriveRestoreAction{"restore"};

using GDriveAuth::AccountInfo;
using GDriveAuth::accessTokenForBlockingRequest;
using GDriveAuth::clearSavedAuthorization;
using GDriveAuth::hasSavedAuthorization;
using GDriveAuth::savedAccountInfo;
using GDriveCache::clearSharedMetadata;
using GDriveCache::removeSharedPath;
using GDriveCache::sharedCapabilities;
using GDriveCache::sharedEntry;
using GDriveCache::sharedParent;
using GDriveExportPolicy::safeLocalExportFileName;
using GDriveExportPolicy::uniqueLocalFilePath;
using GDriveExportPolicy::withExportSuffix;

} // namespace

int GDriveFileProviderPlugin::apiVersion() const
{
    return FM_FILE_PROVIDER_PLUGIN_API_VERSION;
}
QString GDriveFileProviderPlugin::pluginId() const
{
    return QStringLiteral("fm.gdrive-provider");
}

QString GDriveFileProviderPlugin::displayName() const
{
    return QStringLiteral("Google Drive Provider");
}

QStringList GDriveFileProviderPlugin::schemes() const
{
    return {QStringLiteral("gdrive")};
}

bool GDriveFileProviderPlugin::canHandle(const QString &path) const
{
    return !GDrivePath::normalizedPath(path).isEmpty();
}

std::unique_ptr<FileProvider> GDriveFileProviderPlugin::createProvider()
{
    return GDriveFileProviderInternal::createProvider();
}

int GDriveFileProviderPlugin::actionApiVersion() const
{
    return FM_FILE_ACTION_PLUGIN_API_VERSION;
}

QString GDriveFileProviderPlugin::actionPluginId() const
{
    return pluginId();
}

QString GDriveFileProviderPlugin::actionDisplayName() const
{
    return displayName();
}

QList<FileActionDescriptor> GDriveFileProviderPlugin::actionsForContext(const FileActionContext &context) const
{
    const QString targetPath = GDrivePath::normalizedPath(context.targetPath);
    if (targetPath.isEmpty()) {
        return {};
    }

    QList<FileActionDescriptor> actions;
    const bool trashTarget = targetPath != GDrivePath::Trash && GDriveFileProviderInternal::isTrashViewPath(targetPath);
    if (trashTarget) {
        FileActionDescriptor restore;
        restore.id = QString(GoogleDriveRestoreAction);
        restore.text = QStringLiteral("Restore");
        restore.iconSource = QStringLiteral("../assets/icons/refresh.svg");
        restore.order = 80;
        actions.append(restore);
    }

    const bool hasDestinationPanel = !context.destinationPath.trimmed().isEmpty();
    if (!trashTarget && hasDestinationPanel && GDriveFileProviderInternal::exportTargetForPath(targetPath)) {
        FileActionDescriptor downloadPdf;
        downloadPdf.id = QString(GoogleDriveDownloadPdfAction);
        downloadPdf.text = QStringLiteral("Download as PDF");
        downloadPdf.iconSource = QStringLiteral("../assets/icons/download.svg");
        downloadPdf.order = 100;
        actions.append(downloadPdf);
    }

    FileActionDescriptor rawCapabilities;
    rawCapabilities.id = QString(GoogleDriveRawCapabilitiesAction);
    rawCapabilities.text = QStringLiteral("Raw capabilities");
    rawCapabilities.iconSource = QStringLiteral("../assets/icons/info.svg");
    rawCapabilities.order = 900;
    actions.append(rawCapabilities);
    return actions;
}

QVariantMap GDriveFileProviderPlugin::triggerAction(const QString &actionId, const FileActionContext &context)
{
    if (actionId == GoogleDriveSignOutAction) {
        const bool ok = clearSavedAuthorization();
        if (ok) {
            clearSharedMetadata();
        }
        return {
            {QStringLiteral("ok"), ok},
            {QStringLiteral("title"), QStringLiteral("Google Drive")},
            {QStringLiteral("message"),
             ok
                 ? QStringLiteral("Google Drive authorization was removed.")
                 : QStringLiteral("Google Drive authorization could not be removed.")},
        };
    }

    if (actionId == GoogleDriveAuthStatusAction) {
        const AccountInfo accountInfo = savedAccountInfo();
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Google Drive")},
            {QStringLiteral("signedIn"), hasSavedAuthorization()},
            {QStringLiteral("accountName"), accountInfo.displayName},
            {QStringLiteral("accountEmail"), accountInfo.email},
            {QStringLiteral("accountLabel"), accountInfo.label()},
        };
    }

    if (actionId == GoogleDriveDownloadPdfAction) {
        const QString targetPath = GDrivePath::normalizedPath(context.targetPath);
        const std::optional<GDriveFileProviderInternal::ExportTarget> exportTarget = GDriveFileProviderInternal::exportTargetForPath(targetPath);
        if (!exportTarget) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"), QStringLiteral("This action is available only for Google-native Drive files.")},
            };
        }

        const std::optional<GDriveItemCapabilities> downloadCapabilities =
            sharedCapabilities(exportTarget->sourcePath);
        if (downloadCapabilities && !downloadCapabilities->canDownload) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"), QStringLiteral("Google Drive does not allow downloading this file.")},
            };
        }

        const QString destinationFolder = QDir::fromNativeSeparators(context.destinationPath.trimmed());
        const QFileInfo destinationInfo(destinationFolder);
        if (destinationFolder.isEmpty()
            || destinationFolder.contains(QStringLiteral("://"))
            || !destinationInfo.exists()
            || !destinationInfo.isDir()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"),
                 QStringLiteral("Open a local folder in the opposite panel before downloading a Drive file as PDF.")},
            };
        }
        if (!destinationInfo.isWritable()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"), QStringLiteral("The opposite panel folder is not writable.")},
            };
        }

        const QString fileName = safeLocalExportFileName(withExportSuffix(exportTarget->displayName, QStringLiteral("pdf")));
        const QString destinationFilePath = uniqueLocalFilePath(QDir(destinationFolder).filePath(fileName));

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"), authError.isEmpty() ? QStringLiteral("Google Drive authorization failed.") : authError},
            };
        }

        QNetworkAccessManager network;
        QString downloadError;
        const bool ok = GDriveFileProviderInternal::downloadFileToLocalFile(network,
                                                     exportTarget->sourcePath,
                                                     exportTarget->mimeType,
                                                     destinationFilePath,
                                                     accessToken,
                                                     {},
                                                     &downloadError);
        if (!ok) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Download as PDF")},
                {QStringLiteral("message"),
                 downloadError.isEmpty() ? QStringLiteral("Google Drive PDF download failed.") : downloadError},
            };
        }

        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Download as PDF")},
            {QStringLiteral("subtitle"), QStringLiteral("Google Drive")},
            {QStringLiteral("message"), QStringLiteral("Saved PDF to the opposite panel folder.")},
            {QStringLiteral("statusMessage"), QStringLiteral("\"%1\" saved as PDF").arg(QFileInfo(destinationFilePath).fileName())},
            {QStringLiteral("properties"), QVariantList{
                 QVariantMap{
                     {QStringLiteral("label"), QStringLiteral("File")},
                     {QStringLiteral("value"), QDir::toNativeSeparators(destinationFilePath)},
                 },
             }},
        };
    }

    if (actionId == GoogleDriveRestoreAction) {
        const QString targetPath = GDrivePath::normalizedPath(context.targetPath);
        if (targetPath.isEmpty() || targetPath == GDrivePath::Trash || !GDriveFileProviderInternal::isTrashViewPath(targetPath)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Restore")},
                {QStringLiteral("message"), QStringLiteral("This action is available only for items in Google Drive Trash.")},
            };
        }

        const QString id = GDrivePath::idForItemPath(targetPath);
        if (id.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Restore")},
                {QStringLiteral("message"), QStringLiteral("Google Drive item path is invalid.")},
            };
        }

        QString authError;
        const QString accessToken = accessTokenForBlockingRequest(&authError);
        if (accessToken.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Restore")},
                {QStringLiteral("message"), authError.isEmpty() ? QStringLiteral("Google Drive authorization failed.") : authError},
            };
        }

        QNetworkAccessManager network;
        QJsonObject restoredObject;
        QString restoreError;
        if (!GDriveFileProviderInternal::restoreFileBlocking(network, id, accessToken, &restoredObject, &restoreError)) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Restore")},
                {QStringLiteral("message"),
                 restoreError.isEmpty() ? QStringLiteral("Google Drive restore failed.") : restoreError},
            };
        }

        const std::optional<FileEntry> restoredEntry = sharedEntry(targetPath);
        const QString restoredName = restoredEntry ? restoredEntry->name : GDrivePath::fallbackFileNameForPath(targetPath);
        const QString parent = sharedParent(targetPath);
        removeSharedPath(targetPath, parent);
        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Restore")},
            {QStringLiteral("subtitle"), QStringLiteral("Google Drive Trash")},
            {QStringLiteral("message"), QStringLiteral("Item was restored from Trash.")},
            {QStringLiteral("refreshCurrentPath"), true},
            {QStringLiteral("statusMessage"), QStringLiteral("\"%1\" restored").arg(restoredName)},
        };
    }

    if (actionId == GoogleDriveRawCapabilitiesAction) {
        const QString targetPath = GDrivePath::normalizedPath(context.targetPath);
        if (targetPath.isEmpty()) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Google Drive raw capabilities")},
                {QStringLiteral("message"), QStringLiteral("This action is available only for Google Drive paths.")},
            };
        }

        const std::optional<GDriveItemCapabilities> capabilities = sharedCapabilities(targetPath);
        if (!capabilities) {
            return {
                {QStringLiteral("ok"), false},
                {QStringLiteral("title"), QStringLiteral("Google Drive raw capabilities")},
                {QStringLiteral("subtitle"), QStringLiteral("Provider diagnostics")},
                {QStringLiteral("message"),
                 QStringLiteral("No cached capabilities for this item.\nOpen or refresh its parent folder first.\n\nPath: %1")
                     .arg(targetPath)},
            };
        }

        QVariantList properties;
        properties.append(QVariantMap{
            {QStringLiteral("label"), QStringLiteral("Path")},
            {QStringLiteral("value"), targetPath},
        });
        properties.append(GDriveFileProviderInternal::capabilitiesProperties(*capabilities));

        return {
            {QStringLiteral("ok"), true},
            {QStringLiteral("title"), QStringLiteral("Google Drive raw capabilities")},
            {QStringLiteral("subtitle"), QStringLiteral("Provider diagnostics")},
            {QStringLiteral("message"), QStringLiteral("Cached Google Drive capabilities reported by the API.")},
            {QStringLiteral("properties"), properties},
        };
    }

    return {
        {QStringLiteral("ok"), false},
        {QStringLiteral("title"), QStringLiteral("Google Drive")},
        {QStringLiteral("message"), QStringLiteral("Unknown Google Drive action.")},
    };
}
