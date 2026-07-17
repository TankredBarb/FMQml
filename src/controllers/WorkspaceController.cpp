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
#include "../core/PathSemantics.h"
#include <QSysInfo>
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#else
#include <unistd.h>
#include <fstream>
#include <string>
#endif
#include "WorkspaceControllerInternal.h"

namespace WorkspaceControllerInternal {
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
    return PathSemantics::explicitScheme(path);
}

bool isProviderUriPath(const QString &path)
{
    return PathSemantics::isProviderPath(path);
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

} // namespace WorkspaceControllerInternal

using namespace WorkspaceControllerInternal;

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

    connect(&m_operationQueue, &OperationQueue::operationCompleted, this,
        [this](const QVariantMap &completion) {
            const auto type = static_cast<OperationQueue::Type>(completion.value(QStringLiteral("type")).toInt());
            const QStringList sources = completion.value(QStringLiteral("sources")).toStringList();
            const QString destination = completion.value(QStringLiteral("requestedDestinationDirectory")).toString();
            const QStringList resultPaths = completion.value(QStringLiteral("resultPaths")).toStringList();
            const int succeededCount = completion.value(QStringLiteral("succeededCount")).toInt();
            QHash<QString, QString> finalPathBySource;
            const QVariantList outcomes = completion.value(QStringLiteral("itemOutcomes")).toList();
            for (const QVariant &value : outcomes) {
                const QVariantMap outcome = value.toMap();
                const QString finalPath = outcome.value(QStringLiteral("finalPath")).toString();
                if (!finalPath.isEmpty()) {
                    finalPathBySource.insert(outcome.value(QStringLiteral("sourcePath")).toString(), finalPath);
                }
            }
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

            if (!m_operationQueue.error().isEmpty() && succeededCount <= 0) {
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
                        const bool alreadyPresent = panel->directoryModel()->indexOfPath(destination) >= 0;
                        const bool inserted = !alreadyPresent
                            && panel->directoryModel()->insertPath(destination);
                        if (!alreadyPresent && !inserted) {
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
                    FilePanelController *sourcePanel = panelForPath(source);
                    const QString destPath = finalPathBySource.value(source);
                    const QString sourceParent = sourcePanel->parentPathForPath(source);
                    addTreeRefreshPath(sourceParent);
                    addTreeRefreshPath(destination);

                    for (FilePanelController *panel : panels) {
                        const QString panelPath = panel->directoryModel()->currentPath();
                        const QString destParent = destination;

                        if (type == OperationQueue::Type::Move && !destPath.isEmpty() && panelPath == sourceParent) {
                            const bool removed = panel->directoryModel()->removePath(source);
                            if (!removed) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                            } else {
                                panel->directoryModel()->noteLocalMutation();
                            }
                        }

                        if (panelPath == destParent) {
                            if (!destPath.isEmpty()) {
                                panel->directoryModel()->removePath(destPath + QStringLiteral(".part"));
                            }
                            const bool alreadyPresent = !destPath.isEmpty()
                                && panel->directoryModel()->indexOfPath(destPath) >= 0;
                            const bool inserted = !alreadyPresent && !destPath.isEmpty()
                                && panel->directoryModel()->insertPath(destPath);
                            if (!destPath.isEmpty() && !alreadyPresent && !inserted) {
                                if (panel == &m_leftPanel) needsLeftRefresh = true;
                                if (panel == &m_rightPanel) needsRightRefresh = true;
                            } else if (alreadyPresent || inserted) {
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
            recordOperationHistory(type, sources, destination, resultPaths);
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
