#pragma once

#include <QVariantList>
#include <QVariantMap>

#include <functional>

#include "../models/DirectoryModel.h"
#include "../core/LaunchService.h"
#include "../core/LinuxAdminBroker.h"
#include "../core/OpenWithService.h"

class FileProvider;

namespace FilePanelControllerInternal {

struct NavigationResolution {
    enum class Type { OpenPath, MountIso, Invalid };

    Type type = Type::Invalid;
    QString path;
    QString error;
    QString traceType;
};

using SuggestionCancelCheck = std::function<bool()>;

QString materializeAdminReadOnlyLaunchFile(const QString &sourcePath, QString *error);
bool filePanelNavTraceEnabled();
void traceFilePanelNav(const char *stage, const QString &path = {}, const QString &detail = {});
bool isLoadMorePathForCurrentProviderPath(const QString &currentPath, const QString &targetPath);
QVariantMap launchErrorInfo(const LaunchService::LaunchResult &result, const QString &path);
QVariantMap launchResultMap(const LaunchService::LaunchResult &result, const QString &path);
OpenWithService &openWithService();
QVariantMap openWithCandidateMap(const OpenWithCandidate &candidate);
QVariantMap openWithErrorInfo(const OpenWithResult &result, const QString &path);
bool samePanelFilesystemPath(const QString &left, const QString &right);
QString uriSchemeForPath(const QString &path);
bool isProviderUriPath(const QString &path);
bool isPortableUriPath(const QString &path);
bool portableFailureIndicatesRemoval(const QString &error);
QString expandHomeShortcutPath(const QString &path);
bool localAutocompleteAllowedFor(const QString &inputPath, const QString &currentPath);
bool providerNavigationSuggestionsAllowedFor(const QString &inputPath);
LinuxAdminBroker::Result submitLinuxAdminRequest(LinuxAdminBroker::Request request);
QString uniqueCreationName(FileProvider *provider, const QString &parentPath, const QString &name, bool file);
QString normalizedVirtualRoot(const QString &path);
NavigationResolution resolveNavigationPath(QString path);
QString fallbackPathForMissing(QString path);
QString nestedArchiveApprovalTarget(QString path);
QString nestedArchiveScopeKeyForPath(const QString &path);
QString outerArchiveSessionKeyForPath(const QString &path);
QString nestedArchiveDisplayNameForPath(const QString &path);
QString nestedArchiveSizeTextForPath(const QString &path);
QString nestedArchivePreparationStatusForPath(const QString &path);
QString nestedArchivePreparedStatusForPath(const QString &path);
QString failedNavigationRevealPath(const QString &path);
bool navigationFailureIndicatesMissingPath(const QString &error);
QVariantList directorySuggestionEntriesForInput(const QString &inputPath,
                                                const QString &currentPath,
                                                qsizetype maxSuggestions,
                                                const SuggestionCancelCheck &shouldCancel = {});
bool sameOrChildPath(const QString &path, const QString &scope);
QString categoryFilterSummaryText(DirectoryModel::CategoryFilter filter);

} // namespace FilePanelControllerInternal
