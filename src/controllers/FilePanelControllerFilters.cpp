#include "FilePanelController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QProcess>
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QUrl>
#include <QUuid>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <functional>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <winioctl.h>
#endif

#ifdef Q_OS_LINUX
#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#include "../core/ArchiveSupport.h"
#include "../core/ArchiveFileProvider.h"
#include "../core/FileAccessResolver.h"
#include "../core/IsoSupport.h"
#include "../core/LaunchService.h"
#include "../core/OpenWithService.h"
#include "../core/LinuxAdminBroker.h"
#include "../core/LocalFileProvider.h"
#include "../core/MetadataExtractor.h"
#include "../core/TerminalLauncher.h"
#include "../core/WallpaperSetter.h"
#include "../core/DriveUtils.h"
#include "../core/CleanupSubsystem.h"
#include "../core/FileProviderFactory.h"
#include "../core/FileError.h"
#include "../core/VolumeMonitor.h"
#include "FavoritesController.h"
#include "../platform/openwith/LinuxOpenWithBackend.h"

#include "FilePanelControllerInternal.h"

using namespace FilePanelControllerInternal;

void FilePanelController::setCategoryFilter(int filter)
{
    if (filter < DirectoryModel::FilterAll || filter > DirectoryModel::FilterDocuments) {
        filter = DirectoryModel::FilterAll;
    }

    const auto category = static_cast<DirectoryModel::CategoryFilter>(filter);
    if (category == DirectoryModel::FilterAll) {
        clearCategoryFilterScope();
        return;
    }

    const bool stateChanged = m_categoryFilter != category
        || m_categoryFilterScopePath != filterScopeForPath(currentPath())
        || m_categoryFilterContext != filterContextForPath(currentPath());
    m_categoryFilter = category;
    m_categoryFilterScopePath = filterScopeForPath(currentPath());
    m_categoryFilterContext = filterContextForPath(currentPath());
    updateCategoryFilterForPath(currentPath());
    if (stateChanged) {
        emit categoryFilterStateChanged();
    }
}

QString FilePanelController::filterScopeForPath(const QString &path) const
{
    if (path.isEmpty()) {
        return {};
    }
    if (ArchiveSupport::isArchivePath(path)) {
        return normalizedScopePath(ArchiveSupport::normalizeArchivePath(path));
    }
    return normalizedScopePath(m_fileProvider->normalizedPath(path));
}

QString FilePanelController::comparisonPathForFilterScope(const QString &path) const
{
    if (path.isEmpty()) {
        return {};
    }
    if (ArchiveSupport::isArchivePath(m_categoryFilterScopePath)) {
        return ArchiveSupport::isArchivePath(path)
            ? normalizedScopePath(ArchiveSupport::normalizeArchivePath(path))
            : normalizedScopePath(m_fileProvider->normalizedPath(path));
    }
    if (ArchiveSupport::isArchivePath(path)) {
        return normalizedScopePath(ArchiveSupport::physicalArchivePath(path));
    }
    return normalizedScopePath(m_fileProvider->normalizedPath(path));
}

QString FilePanelController::filterContextForPath(const QString &path) const
{
    const QString trimmed = path.trimmed();
    if (ArchiveSupport::isArchivePath(trimmed)) {
        return QStringLiteral("archive");
    }
    if (normalizedVirtualRoot(trimmed) == DEVICE_ROOT) {
        return QStringLiteral("devices");
    }
    if (normalizedVirtualRoot(trimmed) == FAVORITES_ROOT) {
        return QStringLiteral("favorites");
    }

    const int schemeIndex = trimmed.indexOf(QStringLiteral("://"));
    if (schemeIndex > 0) {
        return trimmed.left(schemeIndex).toLower();
    }
    return QStringLiteral("filesystem");
}

bool FilePanelController::isPathInsideCategoryFilterScope(const QString &path) const
{
    if (m_categoryFilterScopePath.isEmpty()) {
        return true;
    }
    return sameOrChildPath(comparisonPathForFilterScope(path), m_categoryFilterScopePath);
}

void FilePanelController::clearCategoryFilterScope()
{
    const bool stateChanged = categoryFilterActive() || categoryFilterSuspended();
    m_categoryFilter = DirectoryModel::FilterAll;
    m_categoryFilterScopePath.clear();
    m_categoryFilterContext.clear();
    m_directoryModel.setCategoryFilter(DirectoryModel::FilterAll);
    if (stateChanged) {
        emit categoryFilterStateChanged();
    }
}

void FilePanelController::updateCategoryFilterForPath(const QString &path)
{
    if (m_categoryFilter == DirectoryModel::FilterAll) {
        m_categoryFilterScopePath.clear();
        m_categoryFilterContext.clear();
        m_directoryModel.setCategoryFilter(DirectoryModel::FilterAll);
        return;
    }

    if (!isPathInsideCategoryFilterScope(path)) {
        clearCategoryFilterScope();
        return;
    }

    const DirectoryModel::CategoryFilter displayFilter =
        filterContextForPath(path) == m_categoryFilterContext
            ? m_categoryFilter
            : DirectoryModel::FilterAll;
    const bool wasSuspended = categoryFilterSuspended();
    m_directoryModel.setCategoryFilter(displayFilter);
    if (wasSuspended != categoryFilterSuspended()) {
        emit categoryFilterStateChanged();
    }
}

