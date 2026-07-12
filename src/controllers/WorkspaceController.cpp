#include "WorkspaceController.h"
#include "../core/ArchiveSupport.h"
#include "../core/ArchiveFileProvider.h"
#include "../core/DriveUtils.h"
#include "../core/FileAccessResolver.h"
#include <QClipboard>
#include <QCoreApplication>
#include <QCursor>
#include <QDir>
#include <QDebug>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include "../core/FileProviderPluginRegistry.h"
#include <QSysInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <fstream>
#include <string>
#endif

namespace {
QString normalizedLocalPath(const QString &path)
{
    QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(path));
#ifdef Q_OS_WIN
    normalized = normalized.toLower();
#endif
    return normalized;
}
#ifdef Q_OS_WIN
bool deletePolicyPathEquals(const QString &lhs, const QString &rhs)
{
    return !lhs.isEmpty() && !rhs.isEmpty() && normalizedLocalPath(lhs) == normalizedLocalPath(rhs);
}

bool deletePolicyIsChildOfPath(const QString &path, const QString &ancestor)
{
    const QString normalizedPathValue = normalizedLocalPath(path);
    const QString normalizedAncestor = normalizedLocalPath(ancestor);
    if (normalizedPathValue.isEmpty() || normalizedAncestor.isEmpty()
        || normalizedPathValue == normalizedAncestor
        || !normalizedPathValue.startsWith(normalizedAncestor)) {
        return false;
    }

    return normalizedAncestor.endsWith(QLatin1Char('/'))
        || normalizedPathValue.at(normalizedAncestor.size()) == QLatin1Char('/');
}
#endif

QString nativeDisplayPath(const QString &path)
{
    if (path.contains(QStringLiteral("://"))) {
        return path;
    }
    return QDir::toNativeSeparators(path);
}

QString uriSchemeForPath(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separatorIndex = trimmed.indexOf(QStringLiteral("://"));
    if (separatorIndex <= 0) {
        return {};
    }

    const QString scheme = trimmed.left(separatorIndex);
    if (!scheme.at(0).isLetter()) {
        return {};
    }
    for (const QChar ch : scheme) {
        if (!ch.isLetterOrNumber() && ch != QLatin1Char('+') && ch != QLatin1Char('.') && ch != QLatin1Char('-')) {
            return {};
        }
    }
    return scheme.toLower();
}

bool isProviderUriPath(const QString &path)
{
    const QString scheme = uriSchemeForPath(path);
    return !scheme.isEmpty()
        && scheme != QStringLiteral("file")
        && scheme != QStringLiteral("archive")
        && scheme != QStringLiteral("devices")
        && scheme != QStringLiteral("favorites");
}

bool isLocalFilesystemPath(const QString &path)
{
    const QString scheme = uriSchemeForPath(path);
    return scheme.isEmpty() || scheme == QStringLiteral("file");
}

QString localPathFromUrlVariant(const QVariant &value)
{
    QUrl url;
    if (value.metaType() == QMetaType::fromType<QUrl>()) {
        url = value.toUrl();
    } else {
        const QString text = value.toString().trimmed();
        if (text.isEmpty()) {
            return {};
        }
        url = QUrl(text);
        if (!url.isValid() || url.scheme().isEmpty()) {
            return QDir::cleanPath(QDir::fromNativeSeparators(text));
        }
    }

    if (!url.isLocalFile()) {
        return {};
    }
    return QDir::cleanPath(QDir::fromNativeSeparators(url.toLocalFile()));
}

QString externalDropStatusMessage(int acceptedCount, int conflictCount, int invalidCount)
{
    QStringList parts;
    if (acceptedCount > 0) {
        parts.append(QStringLiteral("Copied %1 %2.")
                         .arg(acceptedCount)
                         .arg(acceptedCount == 1 ? QStringLiteral("item") : QStringLiteral("items")));
    }
    const int skippedCount = conflictCount + invalidCount;
    if (skippedCount > 0) {
        parts.append(QStringLiteral("Skipped %1 %2.")
                         .arg(skippedCount)
                         .arg(skippedCount == 1 ? QStringLiteral("item") : QStringLiteral("items")));
    }
    return parts.join(QLatin1Char(' '));
}

bool pathsReferToSameDropDestination(const QString &lhs, const QString &rhs)
{
    if (!uriSchemeForPath(lhs).isEmpty() || !uriSchemeForPath(rhs).isEmpty()) {
        return lhs.trimmed().compare(rhs.trimmed(), Qt::CaseInsensitive) == 0;
    }
    return normalizedLocalPath(lhs) == normalizedLocalPath(rhs);
}

bool isPortablePlaceRoot(const QString &path)
{
    return path.trimmed().startsWith(QStringLiteral("portable://device/"), Qt::CaseInsensitive);
}

bool pathBelongsToProviderPlaceRoot(const QString &path, const QString &rootPath)
{
    QString normalizedPath = path.trimmed();
    QString normalizedRoot = rootPath.trimmed();
    if (normalizedPath.isEmpty() || normalizedRoot.isEmpty()) {
        return false;
    }
    if (normalizedPath.compare(normalizedRoot, Qt::CaseInsensitive) == 0) {
        return true;
    }
    if (!normalizedRoot.endsWith(QLatin1Char('/'))) {
        normalizedRoot += QLatin1Char('/');
    }
    return normalizedPath.startsWith(normalizedRoot, Qt::CaseInsensitive);
}

QString normalizedArchiveFormat(QString format)
{
    format = format.trimmed().toLower();
    if (format == QLatin1String("7zip") || format == QLatin1String("7-zip")) {
        return QStringLiteral("7z");
    }
    if (format == QLatin1String("gzip")) {
        return QStringLiteral("gz");
    }
    if (format == QLatin1String("bzip2")) {
        return QStringLiteral("bz2");
    }
    if (format == QLatin1String("zx")) {
        return QStringLiteral("xz");
    }
    if (format == QLatin1String("zip")
        || format == QLatin1String("gz")
        || format == QLatin1String("bz2")
        || format == QLatin1String("xz")) {
        return format;
    }
    return QStringLiteral("7z");
}

QString archiveExtensionForFormat(const QString &format)
{
    return format == QLatin1String("7z") ? QStringLiteral(".7z") : QStringLiteral(".%1").arg(format);
}

QString archiveExtractionBaseName(const QString &fileName)
{
    const QString lower = fileName.toLower();
    const QStringList compoundSuffixes = {
        QStringLiteral(".tar.gz"),
        QStringLiteral(".tgz"),
        QStringLiteral(".tar.xz"),
        QStringLiteral(".txz"),
        QStringLiteral(".tar.bz2"),
        QStringLiteral(".tbz"),
        QStringLiteral(".tbz2"),
        QStringLiteral(".tar.zst"),
        QStringLiteral(".tzst"),
    };
    for (const QString &suffix : compoundSuffixes) {
        if (lower.endsWith(suffix) && fileName.size() > suffix.size()) {
            return fileName.left(fileName.size() - suffix.size());
        }
    }

    const QString baseName = QFileInfo(fileName).completeBaseName();
    return baseName.isEmpty() ? fileName : baseName;
}

bool archiveFormatRequiresSingleFile(const QString &format)
{
    return format == QLatin1String("gz")
        || format == QLatin1String("bz2")
        || format == QLatin1String("xz");
}

QString uniqueArchivePath(const QString &folderPath, const QStringList &sources, const QString &format)
{
    QDir dir(folderPath);
    const QString extension = archiveExtensionForFormat(format);
    QString baseName = QStringLiteral("Archive");
    if (sources.size() == 1) {
        const QFileInfo info(sources.constFirst());
        baseName = info.isDir() || info.completeBaseName().isEmpty()
            ? info.fileName()
            : info.completeBaseName();
        if (baseName.isEmpty()) {
            baseName = QStringLiteral("Archive");
        }
    }

    QString candidate = dir.filePath(baseName + extension);
    if (!QFileInfo::exists(candidate)) {
        return candidate;
    }
    for (int i = 1; i < 10000; ++i) {
        candidate = dir.filePath(QStringLiteral("%1 copy %2%3").arg(baseName).arg(i).arg(extension));
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }
    return dir.filePath(baseName + extension);
}

QVariantMap makeDeleteDetails(bool blocked,
                              bool warning,
                              bool explicitConfirmation,
                              const QString &title,
                              const QString &subtitle,
                              const QString &details,
                              const QString &confirmPhrase,
                              const QString &buttonText)
{
    QVariantMap map;
    map.insert(QStringLiteral("blocked"), blocked);
    map.insert(QStringLiteral("warning"), warning);
    map.insert(QStringLiteral("requiresExplicitConfirmation"), explicitConfirmation);
    map.insert(QStringLiteral("title"), title);
    map.insert(QStringLiteral("subtitle"), subtitle);
    map.insert(QStringLiteral("details"), details);
    map.insert(QStringLiteral("confirmPhrase"), confirmPhrase);
    map.insert(QStringLiteral("buttonText"), buttonText);
    return map;
}

}

WorkspaceController::WorkspaceController(QObject *parent)
    : QObject(parent)
{
    m_placesModel.setIsoMountManager(&m_isoMountManager);
    m_placesModel.setVolumeMonitor(&m_volumeMonitor);
    m_treeModel.setIsoMountManager(&m_isoMountManager);
    m_treeModel.setVolumeMonitor(&m_volumeMonitor);
    m_leftPanel.setVolumeMonitor(&m_volumeMonitor);
    m_rightPanel.setVolumeMonitor(&m_volumeMonitor);

    connect(&m_leftPanel, &FilePanelController::contentsChanged, this,
        [this](const QString &path) {
            m_treeModel.refreshPath(path);
        });
    connect(&m_rightPanel, &FilePanelController::contentsChanged, this,
        [this](const QString &path) {
            m_treeModel.refreshPath(path);
        });

    connect(&m_leftPanel, &FilePanelController::isoMountRequested, this, &WorkspaceController::requestMountIso);
    connect(&m_rightPanel, &FilePanelController::isoMountRequested, this, &WorkspaceController::requestMountIso);
    connect(&m_isoMountManager, &IsoMountManager::mountFinished, this,
        [this](const QString &, const QString &rootPath, bool success, const QString &) {
            m_placesModel.refresh();
            m_treeModel.refresh();
            QTimer::singleShot(1000, this, [this]() {
                m_placesModel.refresh();
                m_treeModel.refresh();
            });
            if (success && !rootPath.isEmpty()) {
                (m_activePanel == 0 ? &m_leftPanel : &m_rightPanel)->openPath(rootPath);
            }
        });
    connect(&m_isoMountManager, &IsoMountManager::unmountFinished, this,
        [this](const QString &rootPath, bool success, const QString &) {
            m_placesModel.refresh();
            m_treeModel.refresh();
            QTimer::singleShot(1000, this, [this]() {
                m_placesModel.refresh();
                m_treeModel.refresh();
            });
            if (!success) {
                return;
            }
            for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
                const QString current = panel->currentPath();
                if (!current.isEmpty() && current.startsWith(rootPath, Qt::CaseInsensitive)) {
                    panel->openPath(QStringLiteral("devices://"));
                }
            }
        });
    connect(&m_isoMountManager, &IsoMountManager::statusMessage, &m_operationQueue, &OperationQueue::setStatusMessage);
    connect(&m_volumeMonitor, &VolumeMonitor::volumeRemoved,
            this, &WorkspaceController::handleVolumeRemoved);
    connect(&m_placesModel, &PlacesModel::providerPlaceRemoved,
            this, &WorkspaceController::handleProviderPlaceRemoved);
    connect(&m_volumeMonitor, &VolumeMonitor::deviceTopologyChanged, this, [this]() {
        m_placesModel.refreshProviderPlacesAsync();
        QTimer::singleShot(800, &m_placesModel, &PlacesModel::refreshProviderPlacesAsync);
        QTimer::singleShot(1800, &m_placesModel, &PlacesModel::refreshProviderPlacesAsync);
    });
    connect(&m_volumeMonitor, &VolumeMonitor::ejectFinished,
            this, &WorkspaceController::handleVolumeEjectFinished);
    connect(&m_volumeMonitor, &VolumeMonitor::mountFinished, this,
            [this](const QString &, const QString &, bool success, const QString &message) {
                m_operationQueue.setStatusMessage(success
                    ? QStringLiteral("Device mounted")
                    : (message.isEmpty() ? QStringLiteral("Cannot mount device.") : message));
                m_placesModel.refresh();
                m_treeModel.refresh();
            });

#ifdef Q_OS_LINUX
    connect(&m_operationQueue, &OperationQueue::operationStarted, this,
        [this](auto type, const auto &, const auto &destination) {
            if (type != OperationQueue::Type::Extract
                || destination.isEmpty()
                || isProviderUriPath(destination)
                || ArchiveSupport::isArchivePath(destination)) {
                return;
            }

            const QString destinationParent = m_leftPanel.parentPathForPath(destination);
            const auto panels = {&m_leftPanel, &m_rightPanel};
            for (FilePanelController *panel : panels) {
                const QString panelPath = panel->directoryModel()->currentPath();
                if (panelPath == destination || panelPath == destinationParent) {
                    panel->directoryModel()->beginBulkWatchSuppression(panelPath);
                }
            }
        });
#endif

    connect(&m_operationQueue, &OperationQueue::operationFinished, this,
        [this](auto type, const auto &sources, const auto &destination) {
#ifdef Q_OS_LINUX
            if (type == OperationQueue::Type::Extract
                && !destination.isEmpty()
                && !isProviderUriPath(destination)
                && !ArchiveSupport::isArchivePath(destination)) {
                const QString destinationParent = m_leftPanel.parentPathForPath(destination);
                const auto panels = {&m_leftPanel, &m_rightPanel};
                for (FilePanelController *panel : panels) {
                    panel->directoryModel()->endBulkWatchSuppression(destination);
                    panel->directoryModel()->endBulkWatchSuppression(destinationParent);
                }
            }
#endif

            for (const QString &source : sources) {
                FileAccessResolver::invalidate(source);
                if (!ArchiveSupport::isArchivePath(source)) {
                    FileAccessResolver::invalidate(QFileInfo(source).absolutePath());
                }
            }
            if (!destination.isEmpty()) {
                FileAccessResolver::invalidate(destination);
            }

            m_placesModel.refreshDriveInfo();
            m_volumeMonitor.scheduleRefresh();
            QTimer::singleShot(1200, this, [this]() {
                m_placesModel.refreshDriveInfo();
            });

            if (!m_operationQueue.error().isEmpty()) {
                if (type == OperationQueue::Type::Extract
                    && ArchiveFileProvider::errorNeedsPassword(m_operationQueue.error())
                    && !sources.isEmpty()) {
                    const QString source = sources.constFirst();
                    ArchiveFileProvider::clearPasswordForPath(source);
                    m_pendingPasswordArchivePath = source;
                    m_pendingPasswordExtractDestination = destination;
                    emit archivePasswordRequested(
                        source,
                        ArchiveSupport::isArchivePath(source)
                            ? ArchiveSupport::archiveFileName(source)
                            : QFileInfo(source).fileName(),
                        QStringLiteral("Archive password required"));
                    return;
                }

                const auto refreshIfShowing = [this](const QString &path) {
                    if (path.isEmpty()) {
                        return;
                    }
                    if (m_leftPanel.directoryModel()->currentPath() == path) {
                        m_leftPanel.refresh();
                    }
                    if (m_rightPanel.directoryModel()->currentPath() == path) {
                        m_rightPanel.refresh();
                    }
                    m_treeModel.refreshPath(path);
                };

                for (const QString &source : sources) {
                    refreshIfShowing(m_leftPanel.parentPathForPath(source));
                }
                if (!destination.isEmpty()) {
                    refreshIfShowing(destination);
                }
                return;
            }
            const auto panels = {&m_leftPanel, &m_rightPanel};
            bool needsLeftRefresh = false;
            bool needsRightRefresh = false;
            QStringList treeRefreshPaths;

            const auto addTreeRefreshPath = [&treeRefreshPaths](const QString &path) {
                if (path.isEmpty() || treeRefreshPaths.contains(path)) {
                    return;
                }
                treeRefreshPaths.append(path);
            };

            if (type == OperationQueue::Type::Compress) {
                const QString archiveParent = m_leftPanel.parentPathForPath(destination);
                addTreeRefreshPath(archiveParent);
                for (FilePanelController *panel : panels) {
                    if (panel->directoryModel()->currentPath() == archiveParent) {
                        const bool inserted = panel->directoryModel()->insertPath(destination);
                        if (!inserted) {
                            if (panel == &m_leftPanel) needsLeftRefresh = true;
                            if (panel == &m_rightPanel) needsRightRefresh = true;
                        } else {
                            panel->directoryModel()->noteLocalMutation();
                        }
                    }
                }
            } else if (type == OperationQueue::Type::Extract) {
                const QString destinationParent = m_leftPanel.parentPathForPath(destination);
                addTreeRefreshPath(destination);
                addTreeRefreshPath(destinationParent);

                for (FilePanelController *panel : panels) {
                    const QString panelPath = panel->directoryModel()->currentPath();
                    if (panelPath == destination || panelPath == destinationParent) {
                        if (panel == &m_leftPanel) needsLeftRefresh = true;
                        if (panel == &m_rightPanel) needsRightRefresh = true;
                    }
                }
            } else if (type == OperationQueue::Type::Delete) {
                for (const QString &source : sources) {
                    const bool providerSource = isProviderUriPath(source);
                    const QString sourceParent = m_leftPanel.parentPathForPath(source);
                    addTreeRefreshPath(sourceParent);
                    for (FilePanelController *panel : panels) {
                        if (providerSource) {
                            const bool removed = panel->directoryModel()->removePath(source);
                            if (removed) {
                                panel->directoryModel()->noteLocalMutation();
                                continue;
                            }
                        }

                        const QString panelPath = panel->directoryModel()->currentPath();
                        const bool rawMatch = panelPath == sourceParent;
                        if (rawMatch) {
                            const bool removed = panel->directoryModel()->removePath(source);
                            if (!removed) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                            } else {
                                panel->directoryModel()->noteLocalMutation();
                            }
                        }
                    }
                }
            } else if (type == OperationQueue::Type::Duplicate) {
                addTreeRefreshPath(destination);
                for (FilePanelController *panel : panels) {
                    if (panel->directoryModel()->currentPath() == destination) {
                        if (panel == &m_leftPanel) needsLeftRefresh = true;
                        if (panel == &m_rightPanel) needsRightRefresh = true;
                    }
                }
            } else if (type == OperationQueue::Type::CreateFolder) {
                const QString createdPath = destination.isEmpty() || sources.isEmpty()
                    ? QString()
                    : m_leftPanel.childPathForPath(destination, sources.constFirst());
                addTreeRefreshPath(destination);
                if (!createdPath.isEmpty()) {
                    addTreeRefreshPath(createdPath);
                }
                for (FilePanelController *panel : panels) {
                    if (panel->directoryModel()->currentPath() == destination) {
                        const bool inserted = !createdPath.isEmpty() && panel->directoryModel()->insertPath(createdPath);
                        if (!inserted) {
                            if (panel == &m_leftPanel) needsLeftRefresh = true;
                            if (panel == &m_rightPanel) needsRightRefresh = true;
                        } else {
                            panel->directoryModel()->noteLocalMutation();
                        }
                    }
                }
            } else {
                for (const QString &source : sources) {
                    const bool sourceIsArchiveEntry = ArchiveSupport::isArchivePath(source);
                    FilePanelController *sourcePanel = panelForPath(source);
                    const QString destPath = destination.isEmpty()
                        ? QString()
                        : sourcePanel->childPathForPath(destination, sourcePanel->fileNameForPath(source));
                    const QString sourceParent = sourcePanel->parentPathForPath(source);
                    addTreeRefreshPath(sourceParent);
                    addTreeRefreshPath(destination);

                    for (FilePanelController *panel : panels) {
                        const QString panelPath = panel->directoryModel()->currentPath();
                        const QString destParent = destination;

                        if (type == OperationQueue::Type::Move && panelPath == sourceParent) {
                            const bool removed = panel->directoryModel()->removePath(source);
                            if (!removed) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                            } else {
                                panel->directoryModel()->noteLocalMutation();
                            }
                        }

                        if (panelPath == destParent) {
                            if (sourceIsArchiveEntry) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                                continue;
                            }
                            if (!destPath.isEmpty()) {
                                panel->directoryModel()->removePath(destPath + QStringLiteral(".part"));
                            }
                            const bool inserted = !destPath.isEmpty() && panel->directoryModel()->insertPath(destPath);
                            if (!destPath.isEmpty() && !inserted) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                            } else if (inserted) {
                                panel->directoryModel()->noteLocalMutation();
                            }
                        }
                    }
                }
            }

            if (needsLeftRefresh) {
                m_leftPanel.refresh();
            }
            if (needsRightRefresh) {
                m_rightPanel.refresh();
            }

            for (const QString &path : treeRefreshPaths) {
                m_treeModel.refreshPath(path);
            }

            if (m_replayingHistory) {
                m_replayingHistory = false;
                return;
            }
            recordOperationHistory(type, sources, destination);
        });

    connect(&m_leftPanel, &FilePanelController::entryRenamed, this,
        [this](const QString &oldPath, const QString &newPath) {
            if (m_replayingHistory) {
                return;
            }
            recordRenameHistory(oldPath, newPath);
        });
    connect(&m_rightPanel, &FilePanelController::entryRenamed, this,
        [this](const QString &oldPath, const QString &newPath) {
            if (m_replayingHistory) {
                return;
            }
            recordRenameHistory(oldPath, newPath);
        });
}

FilePanelController *WorkspaceController::leftPanel()
{
    return &m_leftPanel;
}

FilePanelController *WorkspaceController::rightPanel()
{
    return &m_rightPanel;
}

PlacesModel *WorkspaceController::placesModel()
{
    return &m_placesModel;
}

TreeModel *WorkspaceController::treeModel()
{
    return &m_treeModel;
}

OperationQueue *WorkspaceController::operationQueue()
{
    return &m_operationQueue;
}

WorkspaceController::~WorkspaceController()
{
    clearDragCursorShape();
}

FilePanelController *WorkspaceController::panelBySide(int side)
{
    if (side == 0) {
        return &m_leftPanel;
    }
    if (side == 1) {
        return &m_rightPanel;
    }
    return nullptr;
}

HistoryManager *WorkspaceController::historyManager()
{
    return &m_historyManager;
}

IsoMountManager *WorkspaceController::isoMountManager()
{
    return &m_isoMountManager;
}

VolumeMonitor *WorkspaceController::volumeMonitor()
{
    return &m_volumeMonitor;
}

bool WorkspaceController::splitEnabled() const
{
    return m_splitEnabled;
}

void WorkspaceController::setSplitEnabled(bool enabled)
{
    if (m_splitEnabled == enabled) {
        return;
    }

    if (enabled) {
        FilePanelController *source = m_activePanel == 1 ? &m_rightPanel : &m_leftPanel;
        FilePanelController *target = m_activePanel == 1 ? &m_leftPanel : &m_rightPanel;
        target->syncStateFrom(source);
    } else if (m_activePanel == 1) {
        m_leftPanel.syncStateFrom(&m_rightPanel);
    }

    m_splitEnabled = enabled;
    if (!m_splitEnabled && m_activePanel == 1) {
        setActivePanel(0);
    }
    emit splitEnabledChanged();
}

int WorkspaceController::activePanel() const
{
    return m_activePanel;
}

void WorkspaceController::setActivePanel(int panel)
{
    const int normalizedPanel = panel == 1 ? 1 : 0;
    if (m_activePanel == normalizedPanel) {
        return;
    }
    m_activePanel = normalizedPanel;
    emit activePanelChanged();
}

void WorkspaceController::toggleSplit()
{
    setSplitEnabled(!m_splitEnabled);
}

void WorkspaceController::activateLeft()
{
    setActivePanel(0);
}

void WorkspaceController::activateRight()
{
    if (m_splitEnabled) {
        setActivePanel(1);
    }
}

void WorkspaceController::focusActivePanel()
{
    emit focusActivePanelRequested();
}

void WorkspaceController::setDragCursorShape(int shape)
{
    if (m_dragCursorOverridden && m_dragCursorShape == shape) {
        return;
    }

    if (m_dragCursorOverridden) {
        QGuiApplication::changeOverrideCursor(QCursor(static_cast<Qt::CursorShape>(shape)));
    } else {
        QGuiApplication::setOverrideCursor(QCursor(static_cast<Qt::CursorShape>(shape)));
        m_dragCursorOverridden = true;
    }
    m_dragCursorShape = shape;
}

void WorkspaceController::clearDragCursorShape()
{
    if (!m_dragCursorOverridden) {
        return;
    }
    QGuiApplication::restoreOverrideCursor();
    m_dragCursorOverridden = false;
    m_dragCursorShape = -1;
}

void WorkspaceController::mirrorActivePanelToOpposite()
{
    FilePanelController *source = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    if (!source || !destination) {
        return;
    }

    if (!m_splitEnabled) {
        setSplitEnabled(true);
        destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    }

    destination->syncStateFrom(source);
    focusActivePanel();
}

FilePanelController *WorkspaceController::panelForPath(const QString &path)
{
    const QString parentPath = m_leftPanel.parentPathForPath(path);
    if (m_leftPanel.currentPath() == parentPath) {
        return &m_leftPanel;
    }
    if (m_rightPanel.currentPath() == parentPath) {
        return &m_rightPanel;
    }
    return m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
}

void WorkspaceController::recordOperationHistory(OperationQueue::Type type, const QStringList &sources, const QString &destination)
{
    HistoryAction::Type historyType;
    switch (type) {
    case OperationQueue::Type::Copy:
        historyType = HistoryAction::Type::Copy;
        break;
    case OperationQueue::Type::Duplicate:
        return;
    case OperationQueue::Type::Move:
        historyType = HistoryAction::Type::Move;
        break;
    case OperationQueue::Type::Extract:
        return;
    case OperationQueue::Type::Compress:
        return;
    case OperationQueue::Type::Delete:
        return;
    case OperationQueue::Type::CreateFolder:
        return;
    default:
        return;
    }

    m_historyManager.recordAction({historyType, sources, destination, {}});
}

void WorkspaceController::recordRenameHistory(const QString &oldPath, const QString &newPath)
{
    if (oldPath.isEmpty() || newPath.isEmpty()) {
        return;
    }
    m_historyManager.recordAction({HistoryAction::Type::Rename, {oldPath}, newPath, {oldPath}});
}

void WorkspaceController::finishHistoryReplay()
{
    m_replayingHistory = false;
}

void WorkspaceController::copyActiveSelectionToOpposite()
{
    if (!m_splitEnabled) {
        return;
    }
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    copyDroppedSelectionToPanel(m_activePanel, panelBySide(m_activePanel)->selectedPaths(),
                                m_activePanel == 0 ? 1 : 0, destination->currentPath());
}

QVariantMap WorkspaceController::oppositePanelDropCapabilities(int sourcePanel,
                                                               const QStringList &sources,
                                                               int destinationPanel)
{
    QVariantMap result;
    result.insert(QStringLiteral("canCopy"), false);
    result.insert(QStringLiteral("canMove"), false);
    result.insert(QStringLiteral("reason"), QString());
    result.insert(QStringLiteral("copyReason"), QString());
    result.insert(QStringLiteral("moveReason"), QString());
    result.insert(QStringLiteral("destinationPath"), QString());

    FilePanelController *source = panelBySide(sourcePanel);
    FilePanelController *destination = panelBySide(destinationPanel);
    if (!m_splitEnabled) {
        result[QStringLiteral("reason")] = QStringLiteral("Split view is required.");
        return result;
    }
    if (!source || !destination || source == destination || (sourcePanel + destinationPanel) != 1) {
        result[QStringLiteral("reason")] = QStringLiteral("Drop target must be the opposite panel.");
        return result;
    }
    result[QStringLiteral("destinationPath")] = destination->currentPath();
    if (sources.isEmpty()) {
        result[QStringLiteral("reason")] = QStringLiteral("No selected items to drop.");
        return result;
    }
    if (m_operationQueue.busy()) {
        result[QStringLiteral("reason")] = QStringLiteral("Another file operation is already running.");
        return result;
    }
    if (source->isVirtualRoot() || destination->isVirtualRoot()) {
        result[QStringLiteral("reason")] = QStringLiteral("Cannot drop from or to a virtual root.");
        return result;
    }

    QString copyReason;
    if (!source->canCopyPaths(sources)) {
        copyReason = QStringLiteral("One or more selected items cannot be copied from this location.");
    } else if (!destination->canCreateInCurrentPath()) {
        copyReason = QStringLiteral("You do not have permission to write items to this location.");
    } else {
        bool allSourcesInDestination = true;
        for (const QString &sourcePath : sources) {
            if (ArchiveSupport::isArchivePath(sourcePath)) {
                allSourcesInDestination = false;
                break;
            }
            const QString sourceParent = destination->parentPathForPath(sourcePath);
            if (normalizedLocalPath(sourceParent) != normalizedLocalPath(destination->currentPath())) {
                allSourcesInDestination = false;
                break;
            }
        }
        if (allSourcesInDestination) {
            copyReason = QStringLiteral("Source and destination are the same folder.");
        }
    }

    const bool canCopy = copyReason.isEmpty();
    result[QStringLiteral("canCopy")] = canCopy;
    result[QStringLiteral("copyReason")] = copyReason;

    QString moveReason;
    if (!canCopy) {
        moveReason = copyReason;
    } else if (isProviderUriPath(source->currentPath()) || isProviderUriPath(destination->currentPath())) {
        moveReason = QStringLiteral("Move is not supported for remote providers. Use copy instead.");
    } else if (!source->canDeletePaths(sources)) {
        moveReason = QStringLiteral("You do not have permission to move the selected items from this location.");
    }
    result[QStringLiteral("canMove")] = moveReason.isEmpty();
    result[QStringLiteral("moveReason")] = moveReason;
    if (!canCopy) {
        result[QStringLiteral("reason")] = copyReason;
    }
    return result;
}

QVariantMap WorkspaceController::externalDropCapabilities(const QVariantList &urls,
                                                          int destinationPanel,
                                                          const QString &destinationPath)
{
    QVariantMap result;
    result.insert(QStringLiteral("canCopy"), false);
    result.insert(QStringLiteral("reason"), QString());
    result.insert(QStringLiteral("destinationPath"), QString());
    result.insert(QStringLiteral("acceptedPaths"), QStringList());
    result.insert(QStringLiteral("rejectedPaths"), QStringList());
    result.insert(QStringLiteral("conflictCount"), 0);
    result.insert(QStringLiteral("invalidCount"), 0);

    FilePanelController *destination = panelBySide(destinationPanel);
    if (!destination) {
        result[QStringLiteral("reason")] = QStringLiteral("Invalid drop destination.");
        return result;
    }

    const QString currentDestinationPath = destination->currentPath();
    result[QStringLiteral("destinationPath")] = currentDestinationPath;
    if (currentDestinationPath.isEmpty()
        || !pathsReferToSameDropDestination(destinationPath, currentDestinationPath)) {
        result[QStringLiteral("reason")] = QStringLiteral("Destination changed before drop completed.");
        return result;
    }
    if (m_operationQueue.busy()) {
        result[QStringLiteral("reason")] = QStringLiteral("Another file operation is already running.");
        return result;
    }
    if (destination->isVirtualRoot()) {
        result[QStringLiteral("reason")] = QStringLiteral("Cannot drop files into a virtual root.");
        return result;
    }
    if (!isLocalFilesystemPath(currentDestinationPath)
        || ArchiveSupport::isArchivePath(currentDestinationPath)
        || m_isoMountManager.isInsideManagedMount(currentDestinationPath)
        || isProviderUriPath(currentDestinationPath)) {
        result[QStringLiteral("reason")] = QStringLiteral("External drops are supported for local folders only.");
        return result;
    }
    if (!destination->canCreateInCurrentPath()) {
        result[QStringLiteral("reason")] = QStringLiteral("You do not have permission to write items to this location.");
        return result;
    }
    if (urls.isEmpty()) {
        result[QStringLiteral("reason")] = QStringLiteral("Drop local files only.");
        return result;
    }

    QStringList acceptedPaths;
    QStringList rejectedPaths;
    QStringList acceptedNames;
    int conflictCount = 0;
    int invalidCount = 0;
    const QDir destinationDir(currentDestinationPath);

    for (const QVariant &urlValue : urls) {
        const QString sourcePath = localPathFromUrlVariant(urlValue);
        const QFileInfo sourceInfo(sourcePath);
        bool valid = !sourcePath.isEmpty()
            && !ArchiveSupport::isArchivePath(sourcePath)
            && isLocalFilesystemPath(sourcePath)
            && sourceInfo.exists();
        if (valid) {
            FileAccessResolver::invalidate(sourcePath);
            const FileCapabilityInfo sourceCapabilities = FileAccessResolver::resolve(sourcePath);
            valid = sourceCapabilities.exists
                && (sourceCapabilities.isDirectory
                    ? sourceCapabilities.access.canBrowse
                    : sourceCapabilities.access.canRead);
        }
        if (!valid) {
            if (!sourcePath.isEmpty()) {
                rejectedPaths.append(sourcePath);
            }
            ++invalidCount;
            continue;
        }

        const QString fileName = sourceInfo.fileName();
        const QString destinationChildPath = destinationDir.filePath(fileName);
        const QString normalizedName = normalizedLocalPath(fileName);
        if (fileName.isEmpty()
            || QFileInfo::exists(destinationChildPath)
            || acceptedNames.contains(normalizedName)) {
            rejectedPaths.append(sourcePath);
            ++conflictCount;
            continue;
        }

        acceptedNames.append(normalizedName);
        acceptedPaths.append(sourcePath);
    }

    result[QStringLiteral("acceptedPaths")] = acceptedPaths;
    result[QStringLiteral("rejectedPaths")] = rejectedPaths;
    result[QStringLiteral("conflictCount")] = conflictCount;
    result[QStringLiteral("invalidCount")] = invalidCount;

    if (acceptedPaths.isEmpty()) {
        if (conflictCount > 0 && invalidCount == 0) {
            result[QStringLiteral("reason")] = QStringLiteral("All dropped items already exist in the destination.");
        } else if (conflictCount > 0) {
            result[QStringLiteral("reason")] = QStringLiteral("No dropped local files can be copied; some already exist.");
        } else {
            result[QStringLiteral("reason")] = QStringLiteral("Drop local files only.");
        }
        return result;
    }

    result[QStringLiteral("canCopy")] = true;
    if (conflictCount > 0 || invalidCount > 0) {
        result[QStringLiteral("reason")] = externalDropStatusMessage(acceptedPaths.size(), conflictCount, invalidCount);
    }
    return result;
}

bool WorkspaceController::copyDroppedSelectionToPanel(int sourcePanel,
                                                      const QStringList &sources,
                                                      int destinationPanel,
                                                      const QString &destinationPath)
{
    const QVariantMap capabilities = oppositePanelDropCapabilities(sourcePanel, sources, destinationPanel);
    if (!capabilities.value(QStringLiteral("canCopy")).toBool()) {
        QString reason = capabilities.value(QStringLiteral("copyReason")).toString();
        if (reason.isEmpty()) {
            reason = capabilities.value(QStringLiteral("reason")).toString();
        }
        m_operationQueue.setStatusMessage(reason);
        return false;
    }
    FilePanelController *destination = panelBySide(destinationPanel);
    if (!destination || !pathsReferToSameDropDestination(destinationPath, destination->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Destination changed before drop completed."));
        return false;
    }
    return copyPathsToPanel(sources, destination);
}

bool WorkspaceController::copyExternalUrlsToPanel(const QVariantList &urls,
                                                  int destinationPanel,
                                                  const QString &destinationPath)
{
    const QVariantMap capabilities = externalDropCapabilities(urls, destinationPanel, destinationPath);
    if (!capabilities.value(QStringLiteral("canCopy")).toBool()) {
        const QString reason = capabilities.value(QStringLiteral("reason")).toString();
        m_operationQueue.setStatusMessage(reason.isEmpty()
                                              ? QStringLiteral("Drop local files only.")
                                              : reason);
        return false;
    }

    FilePanelController *destination = panelBySide(destinationPanel);
    if (!destination || !pathsReferToSameDropDestination(destinationPath, destination->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Destination changed before drop completed."));
        return false;
    }

    const QStringList acceptedPaths = capabilities.value(QStringLiteral("acceptedPaths")).toStringList();
    if (acceptedPaths.isEmpty()) {
        m_operationQueue.setStatusMessage(QStringLiteral("Drop local files only."));
        return false;
    }

    m_operationQueue.copyTo(acceptedPaths, destination->currentPath());
    const int conflictCount = capabilities.value(QStringLiteral("conflictCount")).toInt();
    const int invalidCount = capabilities.value(QStringLiteral("invalidCount")).toInt();
    if (conflictCount > 0 || invalidCount > 0) {
        m_operationQueue.setStatusMessage(externalDropStatusMessage(acceptedPaths.size(), conflictCount, invalidCount));
    }
    return true;
}

void WorkspaceController::duplicateActiveSelection()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canDuplicateSelection()) {
        m_operationQueue.reportError(QStringLiteral("You do not have permission to write items to this location."),
                                     active->currentPath(),
                                     QStringLiteral("copy"));
        return;
    }

    const QStringList selected = active->selectedPaths();
    if (selected.size() != 1) {
        return;
    }
    const QString path = selected.constFirst();
    if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path)) {
        m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
        return;
    }

    m_operationQueue.duplicateInPlace(selected, active->currentPath());
}

void WorkspaceController::compressActiveSelection(const QString &format)
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canCompressSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("Cannot create a 7z archive in this location."));
        return;
    }
    if (m_isoMountManager.isInsideManagedMount(active->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
        return;
    }

    const QStringList selected = active->selectedPaths();
    if (selected.isEmpty()) {
        return;
    }
    const QString normalizedFormat = normalizedArchiveFormat(format);
    if (archiveFormatRequiresSingleFile(normalizedFormat)) {
        if (selected.size() != 1 || !QFileInfo(selected.constFirst()).isFile()) {
            m_operationQueue.setStatusMessage(QStringLiteral("This format can compress one file only."));
            return;
        }
    }

    const QString archivePath = uniqueArchivePath(active->currentPath(), selected, normalizedFormat);
    m_operationQueue.compressToArchive(selected, archivePath);
}

void WorkspaceController::moveActiveSelectionToOpposite()
{
    if (!m_splitEnabled) {
        return;
    }
    FilePanelController *destination = m_activePanel == 0 ? &m_rightPanel : &m_leftPanel;
    moveDroppedSelectionToPanel(m_activePanel, panelBySide(m_activePanel)->selectedPaths(),
                                m_activePanel == 0 ? 1 : 0, destination->currentPath());
}

bool WorkspaceController::moveDroppedSelectionToPanel(int sourcePanel,
                                                      const QStringList &sources,
                                                      int destinationPanel,
                                                      const QString &destinationPath)
{
    const QVariantMap capabilities = oppositePanelDropCapabilities(sourcePanel, sources, destinationPanel);
    if (!capabilities.value(QStringLiteral("canMove")).toBool()) {
        QString reason = capabilities.value(QStringLiteral("moveReason")).toString();
        if (reason.isEmpty()) {
            reason = capabilities.value(QStringLiteral("reason")).toString();
        }
        m_operationQueue.setStatusMessage(reason);
        return false;
    }
    FilePanelController *destination = panelBySide(destinationPanel);
    if (!destination || !pathsReferToSameDropDestination(destinationPath, destination->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Destination changed before drop completed."));
        return false;
    }
    m_operationQueue.moveTo(sources, destination->currentPath());
    return true;
}

void WorkspaceController::deleteActiveSelection()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canDeleteSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be deleted from this location."));
        return;
    }
    requestDelete(active->selectedPaths(), active->currentPath(), active->selectedItems());
}

void WorkspaceController::requestDelete(const QStringList &paths, const QString &label, const QVariantList &items)
{
    if (paths.isEmpty()) {
        return;
    }
    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path)) {
            m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
            return;
        }
        if (isProviderUriPath(path)) {
            continue;
        }
        FileAccessResolver::invalidate(path);
        const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
        if (!capabilities.exists || !capabilities.access.canDelete) {
            m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be deleted from this location."));
            return;
        }
    }
    const QVariantMap details = deleteRequestDetails(paths, label);
    if (details.value(QStringLiteral("blocked")).toBool()) {
        const QString message = details.value(QStringLiteral("subtitle")).toString();
        m_operationQueue.setStatusMessage(message.isEmpty()
                                              ? QStringLiteral("Deletion is blocked for this protected location.")
                                              : message);
        return;
    }
    emit deleteRequested(paths, label, items);
}

void WorkspaceController::requestDeleteAsAdministrator(const QStringList &paths, const QString &label, const QVariantList &items)
{
#ifdef Q_OS_LINUX
    if (paths.size() != 1) {
        m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator supports one item at a time."));
        return;
    }
    const QString path = paths.constFirst();
    if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path) || isProviderUriPath(path)) {
        m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator is available for local items only."));
        return;
    }

    const QVariantMap details = deleteRequestDetails(paths, label);
    if (details.value(QStringLiteral("blocked")).toBool()) {
        const QString message = details.value(QStringLiteral("subtitle")).toString();
        m_operationQueue.setStatusMessage(message.isEmpty()
                                              ? QStringLiteral("Deletion is blocked for this protected location.")
                                              : message);
        return;
    }
    emit deleteAsAdministratorRequested(paths, label, items);
#else
    Q_UNUSED(paths)
    Q_UNUSED(label)
    Q_UNUSED(items)
    m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator is available on Linux only."));
#endif
}

bool WorkspaceController::confirmDelete(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return false;
    }

    for (const QString &path : paths) {
        if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path)) {
            m_operationQueue.setStatusMessage(QStringLiteral("This location is read-only"));
            return false;
        }
        if (isProviderUriPath(path)) {
            continue;
        }
        FileAccessResolver::invalidate(path);
        const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
        if (!capabilities.exists || !capabilities.access.canDelete) {
            m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be deleted from this location."));
            return false;
        }
    }

    const QVariantMap details = deleteRequestDetails(paths, {});
    if (details.value(QStringLiteral("blocked")).toBool()) {
        const QString message = details.value(QStringLiteral("subtitle")).toString();
        m_operationQueue.setStatusMessage(message.isEmpty()
                                              ? QStringLiteral("Deletion is blocked for this protected location.")
                                              : message);
        return false;
    }

    m_operationQueue.deletePaths(paths);
    return true;
}

bool WorkspaceController::confirmDeleteAsAdministrator(const QStringList &paths)
{
#ifdef Q_OS_LINUX
    if (paths.size() != 1) {
        m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator supports one item at a time."));
        return false;
    }
    const QString path = paths.constFirst();
    if (ArchiveSupport::isArchivePath(path) || m_isoMountManager.isInsideManagedMount(path) || isProviderUriPath(path)) {
        m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator is available for local items only."));
        return false;
    }
    const QVariantMap details = deleteRequestDetails(paths, {});
    if (details.value(QStringLiteral("blocked")).toBool()) {
        const QString message = details.value(QStringLiteral("subtitle")).toString();
        m_operationQueue.setStatusMessage(message.isEmpty()
                                              ? QStringLiteral("Deletion is blocked for this protected location.")
                                              : message);
        return false;
    }

    m_operationQueue.deletePathsAsAdministrator(paths);
    return true;
#else
    Q_UNUSED(paths)
    m_operationQueue.setStatusMessage(QStringLiteral("Delete as Administrator is available on Linux only."));
    return false;
#endif
}

QVariantMap WorkspaceController::deleteRequestDetails(const QStringList &paths, const QString &label) const
{
    Q_UNUSED(label)

    const int itemCount = paths.size();
    bool allProviderPaths = itemCount > 0;
    QString providerScheme;
    for (const QString &path : paths) {
        if (!isProviderUriPath(path)) {
            allProviderPaths = false;
            break;
        }
        const QString scheme = uriSchemeForPath(path);
        if (providerScheme.isEmpty()) {
            providerScheme = scheme;
        } else if (providerScheme != scheme) {
            allProviderPaths = false;
            break;
        }
    }

    if (allProviderPaths) {
        const bool googleDrive = providerScheme == QLatin1String("gdrive");
        return makeDeleteDetails(false,
                                 false,
                                 false,
                                 itemCount == 1
                                     ? (googleDrive ? QStringLiteral("Move item to Trash?")
                                                    : QStringLiteral("Delete remote item?"))
                                     : (googleDrive ? QStringLiteral("Move %1 items to Trash?").arg(itemCount)
                                                    : QStringLiteral("Delete %1 remote items?").arg(itemCount)),
                                 googleDrive
                                     ? QStringLiteral("Google Drive items will be moved to Trash.")
                                     : QStringLiteral("The remote provider will handle deletion."),
                                 {},
                                 {},
                                 googleDrive ? QStringLiteral("Move to Trash") : QStringLiteral("Delete"));
    }

    int protectedWarningCount = 0;
    int readOnlyWarningCount = 0;
    int systemWarningCount = 0;
    QString firstProtectedWarningPath;
    QString firstBlockedPath;

    for (const QString &path : paths) {
        if (!path.isEmpty() && !ArchiveSupport::isArchivePath(path) && !isProviderUriPath(path)) {
            FileAccessResolver::invalidate(path);
        }
    }

#ifdef Q_OS_WIN
    const QString homePath = QDir::cleanPath(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    const QString windowsPath = QDir::cleanPath(qEnvironmentVariable("SystemRoot"));
    const QString programFilesPath = QDir::cleanPath(qEnvironmentVariable("ProgramFiles"));
    const QString programFilesX86Path = QDir::cleanPath(qEnvironmentVariable("ProgramFiles(x86)"));
#endif

    for (const QString &path : paths) {
        if (path.isEmpty() || ArchiveSupport::isArchivePath(path)
            || path.startsWith(QStringLiteral("devices://"), Qt::CaseInsensitive)
            || path.startsWith(QStringLiteral("favorites://"), Qt::CaseInsensitive)) {
            continue;
        }

        const QFileInfo info(path);
#ifdef Q_OS_WIN
        if (info.exists() && info.isRoot()) {
            firstBlockedPath = nativeDisplayPath(path);
            break;
        }

        if (deletePolicyPathEquals(path, windowsPath)
            || deletePolicyPathEquals(path, programFilesPath)
            || deletePolicyPathEquals(path, programFilesX86Path)
            || deletePolicyPathEquals(path, homePath)) {
            firstBlockedPath = nativeDisplayPath(path);
            break;
        }

        if (deletePolicyIsChildOfPath(path, windowsPath)
            || deletePolicyIsChildOfPath(path, programFilesPath)
            || deletePolicyIsChildOfPath(path, programFilesX86Path)) {
            ++protectedWarningCount;
            if (firstProtectedWarningPath.isEmpty()) {
                firstProtectedWarningPath = nativeDisplayPath(path);
            }
        }
#endif

        const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
        if (capabilities.attributes.readOnly) {
            ++readOnlyWarningCount;
        }
        if (capabilities.attributes.system) {
            ++systemWarningCount;
            if (firstProtectedWarningPath.isEmpty()) {
                firstProtectedWarningPath = nativeDisplayPath(path);
            }
        }
    }

    if (!firstBlockedPath.isEmpty()) {
        const QString title = itemCount == 1
            ? QStringLiteral("Deletion blocked")
            : QStringLiteral("Deletion blocked for protected items");
        const QString subtitle = QStringLiteral("This protected location cannot be permanently deleted from FM.");
        const QString details = QStringLiteral("Blocked path: %1").arg(firstBlockedPath);
        return makeDeleteDetails(true,
                                 false,
                                 false,
                                 title,
                                 subtitle,
                                 details,
                                 {},
                                 QStringLiteral("Close"));
    }

    const bool protectedWarning = protectedWarningCount > 0 || systemWarningCount > 0;
    const bool bulkWarning = itemCount >= 20;
    const bool readOnlyAttributeWarning = readOnlyWarningCount > 0;
    const bool requiresExplicitConfirmation = protectedWarning || bulkWarning;

    if (protectedWarning) {
        const QString title = itemCount == 1
            ? QStringLiteral("Delete from a protected location?")
            : QStringLiteral("Delete protected items?");
        const QString subtitle = QStringLiteral("These items are in a sensitive location and will be deleted permanently.");
        const QString details = firstProtectedWarningPath.isEmpty()
            ? QStringLiteral("Review this selection carefully before continuing.")
            : QStringLiteral("Protected location detected: %1").arg(firstProtectedWarningPath);
        return makeDeleteDetails(false,
                                 true,
                                 requiresExplicitConfirmation,
                                 title,
                                 subtitle,
                                 details,
                                 QStringLiteral("DELETE"),
                                 QStringLiteral("Delete Forever"));
    }

    if (bulkWarning || readOnlyAttributeWarning) {
        QString title = itemCount == 1 ? QStringLiteral("Delete item?") : QStringLiteral("Delete %1 items?").arg(itemCount);
        QString subtitle = QStringLiteral("This action cannot be undone.");
        QString details;
        if (bulkWarning) {
            details = QStringLiteral("This selection contains %1 items. Permanent deletion will start immediately.").arg(itemCount);
        }
        if (readOnlyAttributeWarning) {
            if (!details.isEmpty()) {
                details += QLatin1Char(' ');
            }
            details += readOnlyWarningCount == 1
                ? QStringLiteral("One selected item is marked read-only.")
                : QStringLiteral("%1 selected items are marked read-only.").arg(readOnlyWarningCount);
        }
        return makeDeleteDetails(false,
                                 bulkWarning || readOnlyAttributeWarning,
                                 requiresExplicitConfirmation,
                                 title,
                                 subtitle,
                                 details,
                                 requiresExplicitConfirmation ? QStringLiteral("DELETE") : QString(),
                                 QStringLiteral("Delete Forever"));
    }

    return makeDeleteDetails(false,
                             false,
                             false,
                             itemCount == 1 ? QStringLiteral("Delete item?") : QStringLiteral("Delete %1 items?").arg(itemCount),
                             QStringLiteral("This action cannot be undone."),
                             {},
                             {},
                             QStringLiteral("Delete Forever"));
}

void WorkspaceController::triggerRename()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (!active->canRenameSelection()) {
#ifdef Q_OS_LINUX
        const QStringList selected = active->selectedPaths();
        if (selected.size() == 1
            && !active->isVirtualRoot()
            && !ArchiveSupport::isArchivePath(selected.constFirst())
            && !isProviderUriPath(selected.constFirst())) {
            emit renameRequested();
            return;
        }
#endif
        m_operationQueue.setStatusMessage(QStringLiteral("The current item cannot be renamed with the available permissions."));
        return;
    }
    emit renameRequested();
}

bool WorkspaceController::hasClipboard() const
{
    return !m_clipboard.isEmpty();
}

int WorkspaceController::clipboardCount() const
{
    return m_clipboard.size();
}

bool WorkspaceController::clipboardCut() const
{
    return m_isCut;
}

QString WorkspaceController::clipboardSummary() const
{
    if (m_clipboard.isEmpty()) {
        return {};
    }

    return QStringLiteral("Clipboard: %1 %2 %3")
        .arg(m_clipboard.size())
        .arg(m_clipboard.size() == 1 ? "file" : "files")
        .arg(m_isCut ? "cut" : "copied");
}

void WorkspaceController::copyToClipboard()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canCopySelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be copied from this location."));
        return;
    }
    m_clipboard = active->selectedPaths();
    m_isCut = false;
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        clipboardSummary());
    focusActivePanel();
}

void WorkspaceController::cutToClipboard()
{
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canDeleteSelection()) {
        m_operationQueue.setStatusMessage(QStringLiteral("One or more selected items cannot be moved from this location."));
        return;
    }
    m_clipboard = active->selectedPaths();
    m_isCut = true;
    emit clipboardChanged();
    m_operationQueue.setStatusMessage(
        clipboardSummary());
    focusActivePanel();
}

void WorkspaceController::pasteFromClipboard()
{
    if (m_clipboard.isEmpty()) {
        return;
    }
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()) {
        return;
    }
    if (!active->canPasteIntoCurrentPath()) {
        m_operationQueue.reportError(QStringLiteral("You do not have permission to write items to this location."),
                                     active->currentPath(),
                                     m_isCut ? QStringLiteral("move") : QStringLiteral("copy"));
        return;
    }
    if (m_isCut) {
        m_operationQueue.moveTo(m_clipboard, active->currentPath());
        m_clipboard.clear();
        m_isCut = false;
        emit clipboardChanged();
    } else {
        copyPathsToPanel(m_clipboard, active);
    }
}

void WorkspaceController::pasteFromClipboardAsAdministrator()
{
#ifdef Q_OS_LINUX
    if (m_clipboard.isEmpty()) {
        return;
    }
    if (m_isCut) {
        m_operationQueue.setStatusMessage(QStringLiteral("Paste as Administrator currently supports copied items only."));
        return;
    }
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()
        || isProviderUriPath(active->currentPath())
        || ArchiveSupport::isArchivePath(active->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Paste as Administrator is available for local folders only."));
        return;
    }
    m_operationQueue.copyToAsAdministrator(m_clipboard, active->currentPath());
#else
    m_operationQueue.setStatusMessage(QStringLiteral("Paste as Administrator is available on Linux only."));
#endif
}

void WorkspaceController::createFolderInActivePanelAsAdministrator()
{
#ifdef Q_OS_LINUX
    FilePanelController *active = m_activePanel == 0 ? &m_leftPanel : &m_rightPanel;
    if (active->isVirtualRoot()
        || isProviderUriPath(active->currentPath())
        || ArchiveSupport::isArchivePath(active->currentPath())) {
        m_operationQueue.setStatusMessage(QStringLiteral("Create Folder as Administrator is available for local folders only."));
        return;
    }
    m_operationQueue.createFolderAsAdministrator(active->currentPath(), QStringLiteral("New Folder"));
#else
    m_operationQueue.setStatusMessage(QStringLiteral("Create Folder as Administrator is available on Linux only."));
#endif
}

bool WorkspaceController::copyPathsToPanel(const QStringList &sources, FilePanelController *destination)
{
    if (sources.isEmpty() || !destination) {
        return false;
    }
    if (!destination->canCreateInCurrentPath()) {
        m_operationQueue.reportError(QStringLiteral("You do not have permission to write items to this location."),
                                     destination->currentPath(),
                                     QStringLiteral("copy"));
        return false;
    }

    bool allSourcesInDestination = true;
    for (const QString &source : sources) {
        if (ArchiveSupport::isArchivePath(source)) {
            allSourcesInDestination = false;
            break;
        }
        const QString sourceParent = destination->parentPathForPath(source);
        if (normalizedLocalPath(sourceParent) != normalizedLocalPath(destination->currentPath())) {
            allSourcesInDestination = false;
            break;
        }
    }
    if (allSourcesInDestination) {
        m_operationQueue.setStatusMessage(QStringLiteral("Source and destination are the same folder."));
        return false;
    }

    m_operationQueue.copyTo(sources, destination->currentPath());
    return true;
}

bool WorkspaceController::requestArchivePasswordForExtractIfNeeded(const QString &archivePath, const QString &destination)
{
    if (archivePath.isEmpty() || destination.isEmpty() || m_operationQueue.busy()) {
        return false;
    }
    if (!ArchiveFileProvider::needsPasswordForPath(archivePath)) {
        return false;
    }

    m_pendingPasswordArchivePath = archivePath;
    m_pendingPasswordExtractDestination = destination;
    emit archivePasswordRequested(
        archivePath,
        QFileInfo(archivePath).fileName(),
        QStringLiteral("Archive password required"));
    return true;
}

void WorkspaceController::extractArchiveTo(const QString &archivePath, const QString &destination)
{
    if (archivePath.isEmpty() || destination.isEmpty()) {
        return;
    }
    if (requestArchivePasswordForExtractIfNeeded(archivePath, destination)) {
        return;
    }
    m_operationQueue.extractTo(QStringList{archivePath}, destination);
}

bool WorkspaceController::canExtractArchivePath(const QString &archivePath) const
{
    return !archivePath.isEmpty() && m_leftPanel.isArchiveFilePath(archivePath);
}

void WorkspaceController::extractArchiveHerePath(const QString &archivePath, const QString &currentFolder)
{
    if (!canExtractArchivePath(archivePath) || currentFolder.isEmpty()) {
        return;
    }
    extractArchiveTo(archivePath, currentFolder);
}

void WorkspaceController::extractArchiveToNamedFolderPath(const QString &archivePath, const QString &currentFolder)
{
    if (!canExtractArchivePath(archivePath) || currentFolder.isEmpty()) {
        return;
    }

    const QFileInfo info(archivePath);
    const QString folderName = archiveExtractionBaseName(info.fileName());
    if (folderName.isEmpty()) {
        return;
    }

    QDir currentDir(currentFolder);
    QString destination = currentDir.filePath(folderName);
    if (QFileInfo::exists(destination)) {
        for (int i = 1; i < 10000; ++i) {
            const QString candidate = currentDir.filePath(QStringLiteral("%1 copy %2").arg(folderName).arg(i));
            if (!QFileInfo::exists(candidate)) {
                destination = candidate;
                break;
            }
        }
    }

    extractArchiveTo(archivePath, destination);
}

void WorkspaceController::submitArchivePassword(const QString &path, const QString &password)
{
    if (path.isEmpty() || password.isEmpty()) {
        return;
    }

    ArchiveFileProvider::setPasswordForPath(path, password);
    if (m_pendingPasswordArchivePath != path || m_pendingPasswordExtractDestination.isEmpty()) {
        return;
    }

    const QString archivePath = m_pendingPasswordArchivePath;
    const QString destination = m_pendingPasswordExtractDestination;
    m_pendingPasswordArchivePath.clear();
    m_pendingPasswordExtractDestination.clear();
    m_operationQueue.extractTo(QStringList{archivePath}, destination);
}

void WorkspaceController::cancelArchivePassword(const QString &path)
{
    if (!path.isEmpty()) {
        ArchiveFileProvider::clearPasswordForPath(path);
    }
    if (m_pendingPasswordArchivePath == path) {
        m_pendingPasswordArchivePath.clear();
        m_pendingPasswordExtractDestination.clear();
    }
    m_operationQueue.reportError(QStringLiteral("Archive password required"),
                                 path,
                                 QStringLiteral("extract"));
}

bool WorkspaceController::canMountIsoPath(const QString &path) const
{
    return m_isoMountManager.canMountIsoPath(path);
}

void WorkspaceController::requestMountIso(const QString &path)
{
    if (!canMountIsoPath(path)) {
        return;
    }
    const QString mountedRoot = m_isoMountManager.mountedRootForImage(path);
    if (!mountedRoot.isEmpty()) {
        (m_activePanel == 0 ? &m_leftPanel : &m_rightPanel)->openPath(mountedRoot);
        return;
    }
    emit mountIsoRequested(path);
}

void WorkspaceController::mountIsoToLetter(const QString &path, const QString &letter)
{
    m_isoMountManager.mountIsoToLetter(path, letter);
}

void WorkspaceController::mountIsoAutomatically(const QString &path)
{
    m_isoMountManager.mountIsoToLetter(path, {});
}

bool WorkspaceController::isManagedIsoMountRoot(const QString &rootPath) const
{
    return m_isoMountManager.isManagedMountRoot(rootPath);
}

bool WorkspaceController::isInsideManagedIsoMount(const QString &path) const
{
    return m_isoMountManager.isInsideManagedMount(path);
}

void WorkspaceController::unmountIsoRoot(const QString &rootPath)
{
    m_isoMountManager.unmountIsoRoot(rootPath);
}

void WorkspaceController::requestEjectVolume(const QString &rootPath)
{
    const QString root = QDir::cleanPath(QDir::fromNativeSeparators(rootPath.trimmed()));
    if (root.isEmpty()) {
        emit deviceEjectFailed(rootPath, {}, QStringLiteral("Invalid device path."));
        return;
    }

    const QString managedIsoRoot = m_isoMountManager.managedMountRootForPath(root);
    if (qEnvironmentVariableIntValue("FM_ISO_TRACE") > 0) {
        qInfo().noquote() << "[IsoTrace] eject-route"
                          << "inputRoot=" << rootPath
                          << "normalizedRoot=" << root
                          << "managedRoot=" << managedIsoRoot;
    }
    if (!managedIsoRoot.isEmpty()) {
        unmountIsoRoot(managedIsoRoot);
        return;
    }

    const QString displayName = m_volumeMonitor.displayNameForRoot(root);
    if (!m_volumeMonitor.isKnownUnmountableRoot(root)) {
        const QString message = QStringLiteral("This device cannot be unmounted from FM.");
        m_operationQueue.setStatusMessage(message);
        emit deviceEjectFailed(root, displayName, message);
        return;
    }

    if (m_operationQueue.busy()) {
        const QString message = QStringLiteral("Wait for the current file operation to finish before unmounting this device.");
        m_operationQueue.setStatusMessage(message);
        emit deviceEjectFailed(root, displayName, message);
        return;
    }

    const QStringList affectedRoots = m_volumeMonitor.relatedMountedRoots(root);
    for (const QString &affectedRoot : affectedRoots) {
        for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
            if (m_volumeMonitor.pathBelongsToRoot(panel->currentPath(), affectedRoot)
                || m_volumeMonitor.pathBelongsToRoot(panel->directoryModel()->currentPath(), affectedRoot)) {
                panel->handleDeviceRemoved(affectedRoot, displayName);
            }
        }
        emit deviceEjectStarted(affectedRoot, displayName);
    }
    m_volumeMonitor.requestEject(root);
}

void WorkspaceController::requestMountVolume(const QString &stableDeviceId)
{
    if (m_operationQueue.busy()) {
        m_operationQueue.setStatusMessage(QStringLiteral("Wait for the current file operation to finish before mounting this device."));
        return;
    }
    m_volumeMonitor.requestMount(stableDeviceId);
}

void WorkspaceController::handleVolumeRemoved(const QString &rootPath, const QString &displayName)
{
    bool affectedPanel = false;
    for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
        if (m_volumeMonitor.pathBelongsToRoot(panel->currentPath(), rootPath)
            || m_volumeMonitor.pathBelongsToRoot(panel->directoryModel()->currentPath(), rootPath)) {
            panel->handleDeviceRemoved(rootPath, displayName);
            affectedPanel = true;
        }
    }

    if (affectedPanel) {
        emit deviceRemoved(rootPath, displayName);
    }
}

void WorkspaceController::handleProviderPlaceRemoved(const QString &rootPath,
                                                     const QString &displayName,
                                                     const QString &section)
{
    if (section != QLatin1String("portable") || !isPortablePlaceRoot(rootPath)) {
        return;
    }

    bool affectedPanel = false;
    for (FilePanelController *panel : {&m_leftPanel, &m_rightPanel}) {
        if (pathBelongsToProviderPlaceRoot(panel->currentPath(), rootPath)
            || pathBelongsToProviderPlaceRoot(panel->directoryModel()->currentPath(), rootPath)) {
            panel->handleDeviceRemoved(rootPath, displayName);
            affectedPanel = true;
        }
    }

    if (affectedPanel) {
        emit deviceRemoved(rootPath, displayName);
    }
}

void WorkspaceController::handleVolumeEjectFinished(const QString &rootPath, bool success, const QString &message)
{
    const QString displayName = m_volumeMonitor.displayNameForRoot(rootPath);
    if (success) {
        m_operationQueue.setStatusMessage(QStringLiteral("Device disconnected safely"));
        emit deviceEjectSucceeded(rootPath, displayName);
    } else {
        const QString failure = message.isEmpty()
            ? QStringLiteral("Cannot eject device.")
            : QStringLiteral("Cannot eject device: %1").arg(message);
        m_operationQueue.setStatusMessage(failure);
        emit deviceEjectFailed(rootPath, displayName, failure);
    }
    m_placesModel.refresh();
    m_treeModel.refresh();
}

bool WorkspaceController::pathBelongsToVolumeRoot(const QString &path, const QString &rootPath) const
{
    return m_volumeMonitor.pathBelongsToRoot(path, rootPath);
}

void WorkspaceController::copyTextToClipboard(const QString &text)
{
    if (auto *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(text);
    }
}

QString WorkspaceController::applicationDirectory() const
{
    return QCoreApplication::applicationDirPath();
}

QString WorkspaceController::displayPath(const QString &path) const
{
    return DriveUtils::displayPath(path);
}

void WorkspaceController::undo()
{
    if (!m_historyManager.canUndo()) return;

    HistoryAction action = m_historyManager.takeUndo();

    switch (action.type) {
    case HistoryAction::Type::Move: {
        if (action.sources.isEmpty()) {
            break;
        }
        QStringList currentPaths;
        FilePanelController *sourcePanel = panelForPath(action.sources.first());
        for (const QString &src : action.sources) {
            currentPaths.append(sourcePanel->childPathForPath(action.destination, sourcePanel->fileNameForPath(src)));
        }
        m_replayingHistory = true;
        m_operationQueue.moveTo(currentPaths, sourcePanel->parentPathForPath(action.sources.first()));
        break;
    }
    case HistoryAction::Type::Copy: {
        if (action.sources.isEmpty()) {
            break;
        }
        QStringList copiedPaths;
        FilePanelController *sourcePanel = panelForPath(action.sources.first());
        for (const QString &src : action.sources) {
            copiedPaths.append(sourcePanel->childPathForPath(action.destination, sourcePanel->fileNameForPath(src)));
        }
        m_replayingHistory = true;
        m_operationQueue.deletePaths(copiedPaths);
        break;
    }
    case HistoryAction::Type::Rename: {
        if (action.sources.isEmpty() || action.destination.isEmpty()) {
            break;
        }
        const QString oldPath = action.sources.first();
        const QString newPath = action.destination;
        FilePanelController *panel = panelForPath(oldPath);
        const QString oldName = panel->fileNameForPath(oldPath);
        m_replayingHistory = true;
        if (!panel->renamePath(newPath, oldName)) {
            finishHistoryReplay();
        }
        break;
    }
    default:
        break;
    }
}

void WorkspaceController::redo()
{
    if (!m_historyManager.canRedo()) return;

    HistoryAction action = m_historyManager.takeRedo();
    switch (action.type) {
    case HistoryAction::Type::Copy:
        m_replayingHistory = true;
        m_operationQueue.copyTo(action.sources, action.destination);
        break;
    case HistoryAction::Type::Move:
        m_replayingHistory = true;
        m_operationQueue.moveTo(action.sources, action.destination);
        break;
    case HistoryAction::Type::Rename: {
        if (action.sources.isEmpty() || action.destination.isEmpty()) {
            break;
        }
        const QString oldPath = action.sources.first();
        const QString newPath = action.destination;
        FilePanelController *panel = panelForPath(oldPath);
        const QString newName = panel->fileNameForPath(newPath);
        m_replayingHistory = true;
        if (!panel->renamePath(oldPath, newName)) {
            finishHistoryReplay();
        }
        break;
    }
    default:
        break;
    }
}

QStringList WorkspaceController::clipboardPaths() const
{
    return m_clipboard;
}

QVariantList WorkspaceController::loadedPlugins() const
{
    QVariantList list;
    const auto infos = FileProviderPluginRegistry::instance().pluginInfos();
    for (const auto &info : infos) {
        if (info.loaded) {
            QVariantMap map;
            map.insert(QStringLiteral("pluginId"), info.pluginId);
            map.insert(QStringLiteral("displayName"), info.displayName);
            map.insert(QStringLiteral("filePath"), info.filePath);
            map.insert(QStringLiteral("schemes"), info.schemes);
            list.append(map);
        }
    }
    return list;
}

qint64 WorkspaceController::processMemoryUsage() const
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return static_cast<qint64>(pmc.WorkingSetSize);
    }
    return 0;
#else
    std::ifstream statm_stream("/proc/self/statm", std::ios_base::in);
    if (!statm_stream) return 0;
    long size = 0, resident = 0;
    statm_stream >> size >> resident;
    statm_stream.close();
    long page_size = sysconf(_SC_PAGE_SIZE);
    return static_cast<qint64>(resident * page_size);
#endif
}

QString WorkspaceController::qtVersion() const
{
    return QString::fromLatin1(qVersion());
}
