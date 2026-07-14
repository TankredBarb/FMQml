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

void FilePanelController::showProperties(int row)
{
    if (isVirtualRoot()) return;
    QStringList selected = m_directoryModel.selectedPaths();
    if (selected.isEmpty()) {
        // Fallback: use the path at the given row
        const QString path = m_directoryModel.pathAt(row);
        if (!path.isEmpty()) {
            selected = { path };
        }
    }
    if (!selected.isEmpty()) {
        emit revealProperties(selected);
    }
}

void FilePanelController::showPropertiesForPath(const QString &path)
{
    if (isVirtualRoot() || path.isEmpty()) {
        return;
    }
    emit revealProperties({path});
}

void FilePanelController::showAccessOwnershipAsAdministrator(int row)
{
    if (isVirtualRoot()) {
        return;
    }

    const QString path = m_directoryModel.pathAt(row);
    if (path.isEmpty() || isProviderUriPath(path) || ArchiveSupport::isArchivePath(path)) {
        return;
    }
    emit revealAccessOwnershipAsAdministrator(path);
}

void FilePanelController::fetchMetadataAsync(const QString &path)
{
    if (isVirtualRoot()) return;
    // Run extraction on a worker thread; marshal result back to GUI thread via signal.
    QThreadPool::globalInstance()->start([this, path]() {
        const QVariantList props = MetadataExtractor::extract(path);
        // Convert the label/value list into a flat map for efficient QML access
        QVariantMap meta;
        for (const QVariant &v : props) {
            const QVariantMap pair = v.toMap();
            const QString label = pair.value(QStringLiteral("label")).toString();
            const QString value = pair.value(QStringLiteral("value")).toString();
            // Normalize keys to camelCase for QML
            if (label == QLatin1String("Dimensions")) {
                meta[QStringLiteral("dimensions")] = value;
                meta[QStringLiteral("resolution")] = value;
            }
            if (label == QLatin1String("Duration"))    meta[QStringLiteral("duration")]   = value;
            if (label == QLatin1String("Artist"))      meta[QStringLiteral("artist")]     = value;
            if (label == QLatin1String("Album"))       meta[QStringLiteral("album")]      = value;
            if (label == QLatin1String("Bitrate"))     meta[QStringLiteral("bitrate")]    = value;
        }
        // Always emit even if empty so delegate knows loading is done
        QMetaObject::invokeMethod(this, [this, path, meta]() {
            emit metadataReady(path, meta);
        }, Qt::QueuedConnection);
    });
}

void FilePanelController::refresh()
{
    clearError();
    if (isVirtualRoot()) {
        emit contentsChanged(currentPath());
        return;
    }
    m_directoryModel.refresh();
    emit contentsChanged(currentPath());
}

void FilePanelController::clearError()
{
    setStatusMessage({});
    setLastError({});
    m_directoryModel.clearError();
}

QStringList FilePanelController::selectedPaths() const
{
    return m_directoryModel.selectedPaths();
}

QVariantList FilePanelController::selectedItems() const
{
    QVariantList items;
    const QStringList paths = m_directoryModel.selectedPaths();
    items.reserve(paths.size());
    for (const QString &path : paths) {
        QString name;
        const int row = m_directoryModel.indexOfPath(path);
        if (row >= 0) {
            name = m_directoryModel.data(m_directoryModel.index(row, 0), DirectoryModel::NameRole).toString();
        }
        if (name.isEmpty()) {
            name = fileNameForPath(path);
        }

        QVariantMap item;
        item.insert(QStringLiteral("path"), path);
        item.insert(QStringLiteral("name"), name);
        items.append(item);
    }
    return items;
}

QVariantMap FilePanelController::storageInfoForPath(const QString &rootPath) const
{
    if (isProviderUriPath(rootPath)) {
        std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(rootPath);
        return provider ? provider->storageInfo(rootPath) : QVariantMap{};
    }

    const QStorageInfo storage(rootPath);
    if (!storage.isValid() || !storage.isReady()) {
        return {};
    }
    const qint64 total = storage.bytesTotal();
    const qint64 free  = storage.bytesFree();
    const qint64 used  = total - free;
    const double pct   = total > 0 ? static_cast<double>(used) / static_cast<double>(total) : 0.0;
    return {
        {QStringLiteral("total"),      total},
        {QStringLiteral("free"),       free},
        {QStringLiteral("used"),       used},
        {QStringLiteral("percent"),    pct},
        {QStringLiteral("totalStr"),   DriveUtils::formatSize(total)},
        {QStringLiteral("freeStr"),    DriveUtils::formatSize(free)},
        {QStringLiteral("fs"),         QString::fromLatin1(storage.fileSystemType())},
        {QStringLiteral("isCritical"), total > 0 && (static_cast<double>(free) / static_cast<double>(total)) < 0.10},
    };
}

