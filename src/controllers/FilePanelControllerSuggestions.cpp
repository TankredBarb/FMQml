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

void FilePanelController::requestDirectorySuggestionEntries(const QString &inputPath, int requestId, int maxSuggestions) const
{
    const QString basePath = currentPath();
    const qsizetype boundedMax = maxSuggestions <= 0 ? 0 : qBound(1, maxSuggestions, 512);
    const bool showHidden = m_directoryModel.showHidden();
    const int generation = ++m_directorySuggestionGeneration;
    QPointer<FilePanelController> self(const_cast<FilePanelController *>(this));
    traceFilePanelNav("suggestion-entries-request", inputPath,
                      QStringLiteral("requestId=%1 base=%2 max=%3")
                          .arg(requestId)
                          .arg(QDir::toNativeSeparators(basePath))
                          .arg(boundedMax));

    if (!localAutocompleteAllowedFor(inputPath, basePath)
        && !providerNavigationSuggestionsAllowedFor(inputPath)) {
        QMetaObject::invokeMethod(self.data(), [self, requestId, generation]() {
            if (!self || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation) {
                return;
            }
            emit self->directorySuggestionEntriesReady(requestId, {});
        }, Qt::QueuedConnection);
        return;
    }

    (void)QtConcurrent::run([self, inputPath, requestId, basePath, boundedMax, showHidden, generation]() {
        const auto shouldCancel = [self, generation]() {
            return !self
                || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation;
        };

        QElapsedTimer timer;
        timer.start();
        const QVariantList suggestions = directorySuggestionEntriesForInput(inputPath, basePath, boundedMax,
                                                                             showHidden, shouldCancel);
        if (shouldCancel()) {
            return;
        }
        traceFilePanelNav("suggestion-entries-worker-finished", inputPath,
                          QStringLiteral("requestId=%1 count=%2 elapsedMs=%3")
                              .arg(requestId)
                              .arg(suggestions.size())
                              .arg(timer.elapsed()));
        if (!self) {
            return;
        }
        QMetaObject::invokeMethod(self.data(), [self, requestId, generation, suggestions]() {
            if (!self || self->m_directorySuggestionGeneration.load(std::memory_order_relaxed) != generation) {
                return;
            }
            emit self->directorySuggestionEntriesReady(requestId, suggestions);
        }, Qt::QueuedConnection);
    });
}

void FilePanelController::cancelDirectorySuggestions() const
{
    ++m_directorySuggestionGeneration;
}
