#include "DebugInformationController.h"

#include "AdminController.h"
#include "AppSettingsController.h"
#include "DiskUsageController.h"
#include "FilePanelController.h"
#include "FileSearchController.h"
#include "FolderCompareController.h"
#include "QuickLookController.h"
#include "PluginActionController.h"
#include "ThemeController.h"
#include "WorkspaceController.h"
#include "../core/OperationQueue.h"
#include "../core/DebugReportSanitizer.h"
#include "../core/DebugReportFormatter.h"
#include "../models/DirectoryModel.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QSysInfo>
#include <QWindow>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QUrl>

namespace {
QVariantMap row(const QString &label, const QVariant &value, bool path = false)
{
    return {{QStringLiteral("label"), label},
            {QStringLiteral("value"), value.toString().isEmpty() ? QStringLiteral("Unavailable") : value},
            {QStringLiteral("path"), path}};
}

QVariantMap section(const QString &title, const QVariantList &rows)
{
    return {{QStringLiteral("title"), title}, {QStringLiteral("rows"), rows}};
}

QString yesNo(bool value)
{
    return value ? QStringLiteral("Yes") : QStringLiteral("No");
}

QString viewModeName(int mode)
{
    switch (mode) {
    case 0: return QStringLiteral("Details");
    case 1: return QStringLiteral("Grid");
    case 2: return QStringLiteral("Brief");
    default: return QString::number(mode);
    }
}

QString sortRoleName(DirectoryModel::SortRole role)
{
    switch (role) {
    case DirectoryModel::SortByName: return QStringLiteral("Name");
    case DirectoryModel::SortBySize: return QStringLiteral("Size");
    case DirectoryModel::SortByType: return QStringLiteral("Type");
    case DirectoryModel::SortByDate: return QStringLiteral("Date modified");
    case DirectoryModel::SortByDateCreated: return QStringLiteral("Date created");
    case DirectoryModel::SortByExtension: return QStringLiteral("Extension");
    }
    return QStringLiteral("Unknown");
}

QString sortOrderName(Qt::SortOrder order)
{
    return order == Qt::DescendingOrder ? QStringLiteral("Descending")
                                        : QStringLiteral("Ascending");
}

QString panelScheme(const QString &path)
{
    const QString scheme = QUrl(path).scheme().toLower();
    return scheme.isEmpty() || scheme.size() == 1 ? QStringLiteral("file") : scheme;
}

QString graphicsApiName(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Software: return QStringLiteral("Software");
    case QSGRendererInterface::OpenGL: return QStringLiteral("OpenGL");
    case QSGRendererInterface::Direct3D11: return QStringLiteral("Direct3D 11");
    case QSGRendererInterface::Direct3D12: return QStringLiteral("Direct3D 12");
    case QSGRendererInterface::Vulkan: return QStringLiteral("Vulkan");
    case QSGRendererInterface::Metal: return QStringLiteral("Metal");
    case QSGRendererInterface::Null: return QStringLiteral("Null");
    case QSGRendererInterface::Unknown: return QStringLiteral("Unavailable");
    default: return QStringLiteral("Unavailable");
    }
}

QString errorSummary(const QVariantMap &error)
{
    if (error.isEmpty()) {
        return {};
    }
    const QString title = error.value(QStringLiteral("title")).toString();
    const QString message = error.value(QStringLiteral("message"), error.value(QStringLiteral("error"))).toString();
    if (title.isEmpty()) {
        return message;
    }
    return message.isEmpty() ? title : title + QStringLiteral(": ") + message;
}
}

DebugInformationController::DebugInformationController(QObject *parent)
    : QObject(parent)
{
}

void DebugInformationController::setSources(WorkspaceController *workspace,
                                            ThemeController *theme,
                                            AppSettingsController *settings,
                                            AdminController *admin,
                                            QuickLookController *quickLook,
                                            FileSearchController *fileSearch,
                                            DiskUsageController *diskUsage,
                                            FolderCompareController *folderCompare,
                                            PluginActionController *pluginActions)
{
    m_workspace = workspace;
    m_theme = theme;
    m_settings = settings;
    m_admin = admin;
    m_quickLook = quickLook;
    m_fileSearch = fileSearch;
    m_diskUsage = diskUsage;
    m_folderCompare = folderCompare;
    m_pluginActions = pluginActions;

    if (m_fileSearch) {
        connect(m_fileSearch, &FileSearchController::stateChanged, this, [this] {
            const bool active = m_fileSearch->state() == FileSearchController::State::Searching;
            if (active && !m_fileSearchActive) {
                ++m_fileSearchCount;
            }
            m_fileSearchActive = active;
        });
    }
    if (m_diskUsage) {
        connect(m_diskUsage, &DiskUsageController::stateChanged, this, [this] {
            const bool active = m_diskUsage->state() == DiskUsageController::State::Scanning;
            if (active && !m_diskAnalysisActive) {
                ++m_diskAnalysisCount;
            }
            m_diskAnalysisActive = active;
        });
    }
    if (m_folderCompare) {
        connect(m_folderCompare, &FolderCompareController::stateChanged, this, [this] {
            const bool active = m_folderCompare->busy();
            if (active && !m_folderComparisonActive) {
                ++m_folderComparisonCount;
            }
            m_folderComparisonActive = active;
        });
    }
    if (m_workspace && m_workspace->operationQueue()) {
        connect(m_workspace->operationQueue(), &OperationQueue::operationStarted,
                this, [this](OperationQueue::Type, const QStringList &, const QString &) {
            ++m_fileOperationCount;
        });
    }
}

QVariantMap DebugInformationController::snapshot() const
{
    return m_snapshot;
}

QString DebugInformationController::generatedAtText() const
{
    return m_generatedAtText;
}

QVariantMap DebugInformationController::panelSnapshot(const QString &name, QObject *object, bool active) const
{
    auto *panel = qobject_cast<FilePanelController *>(object);
    if (!panel) {
        return section(name, {row(QStringLiteral("State"), QStringLiteral("Unavailable"))});
    }
    DirectoryModel *model = panel->directoryModel();
    QVariantList rows{
        row(QStringLiteral("Active"), yesNo(active)),
        row(QStringLiteral("Scheme / provider"), panelScheme(panel->currentPath())),
        row(QStringLiteral("Path"), panel->currentPath(), true),
        row(QStringLiteral("View mode"), viewModeName(panel->viewMode())),
        row(QStringLiteral("Sort by"), sortRoleName(panel->panelSortRole())),
        row(QStringLiteral("Sort order"), sortOrderName(panel->panelSortOrder())),
        row(QStringLiteral("Visible items"), model ? model->rowCount() : 0),
        row(QStringLiteral("Selection"), model ? model->selectedCount() : 0),
        row(QStringLiteral("History"), QStringLiteral("%1 back, %2 forward")
            .arg(panel->backStackCount()).arg(panel->forwardStackCount())),
        row(QStringLiteral("Show hidden"), yesNo(model && model->showHidden())),
        row(QStringLiteral("Text filter"), model && !model->searchText().isEmpty()
            ? QStringLiteral("Active") : QStringLiteral("Inactive")),
        row(QStringLiteral("Category filter"), panel->categoryFilterActive()
            ? panel->categoryFilterSummary() : QStringLiteral("Inactive"))
    };
    return section(name, rows);
}

void DebugInformationController::refresh()
{
    const QDateTime now = QDateTime::currentDateTime();
    m_generatedAtText = now.toString(Qt::ISODateWithMs);
    QVariantList sections;

    QString windowSize = QStringLiteral("Unavailable");
    QString dpr = QStringLiteral("Unavailable");
    if (QWindow *window = QGuiApplication::focusWindow()) {
        windowSize = QStringLiteral("%1 x %2").arg(window->width()).arg(window->height());
        dpr = QString::number(window->devicePixelRatio(), 'f', 2);
    }
    const QString graphicsApi = graphicsApiName(QQuickWindow::graphicsApi());

    sections.append(section(QStringLiteral("BUILD & RUNTIME"), {
        row(QStringLiteral("Application version"), QCoreApplication::applicationVersion()),
        row(QStringLiteral("Build configuration"), QStringLiteral(FM_BUILD_CONFIGURATION)),
        row(QStringLiteral("Git revision"), QStringLiteral(FM_GIT_REVISION)),
        row(QStringLiteral("Qt runtime"), QString::fromLatin1(qVersion())),
        row(QStringLiteral("Operating system"), QSysInfo::prettyProductName()),
        row(QStringLiteral("Kernel"), QSysInfo::kernelType() + QLatin1Char(' ') + QSysInfo::kernelVersion()),
        row(QStringLiteral("Architecture"), QSysInfo::currentCpuArchitecture()),
        row(QStringLiteral("Qt platform"), QGuiApplication::platformName()),
        row(QStringLiteral("Graphics API"), graphicsApi),
        row(QStringLiteral("Process memory RSS"), m_workspace
            ? QStringLiteral("%1 MB").arg(m_workspace->processMemoryUsage() / (1024.0 * 1024.0), 0, 'f', 1)
            : QStringLiteral("Unavailable")),
        row(QStringLiteral("Snapshot timestamp"), m_generatedAtText)
    }));

    sections.append(section(QStringLiteral("DISPLAY & APPEARANCE"), {
        row(QStringLiteral("Window size"), windowSize),
        row(QStringLiteral("Device pixel ratio"), dpr),
        row(QStringLiteral("Theme"), m_theme ? m_theme->schemeName() : QStringLiteral("Unavailable")),
        row(QStringLiteral("Dark theme"), m_theme ? yesNo(m_theme->isDark()) : QStringLiteral("Unavailable")),
        row(QStringLiteral("Font family"), m_settings ? m_settings->resolvedFontFamily() : QStringLiteral("Unavailable")),
        row(QStringLiteral("Font scale"), m_settings ? QStringLiteral("%1%").arg(m_settings->fontScale()) : QStringLiteral("Unavailable"))
    }));

    sections.append(section(QStringLiteral("STORAGE PATHS"), {
        row(QStringLiteral("Executable directory"), QCoreApplication::applicationDirPath(), true),
        row(QStringLiteral("Configuration directory"), QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation), true),
        row(QStringLiteral("Cache directory"), QStandardPaths::writableLocation(QStandardPaths::CacheLocation), true),
        row(QStringLiteral("Temporary directory"), QStandardPaths::writableLocation(QStandardPaths::TempLocation), true)
    }));

    if (m_workspace) {
        sections.append(panelSnapshot(QStringLiteral("LEFT PANEL"), m_workspace->leftPanel(), m_workspace->activePanel() == 0));
        sections.append(panelSnapshot(QStringLiteral("RIGHT PANEL"), m_workspace->rightPanel(), m_workspace->activePanel() == 1));
        sections.append(section(QStringLiteral("CLIPBOARD & OPERATIONS"), {
            row(QStringLiteral("Clipboard"), m_workspace->clipboardSummary()),
            row(QStringLiteral("Clipboard item count"), m_workspace->clipboardPaths().size()),
            row(QStringLiteral("Operation active"), yesNo(m_workspace->operationQueue()->busy())),
            row(QStringLiteral("Operation label"), m_workspace->operationQueue()->currentLabel(), true),
            row(QStringLiteral("Operation progress"), QStringLiteral("%1%").arg(qRound(m_workspace->operationQueue()->progress() * 100.0))),
            row(QStringLiteral("Operation items"), QStringLiteral("%1 of %2")
                .arg(m_workspace->operationQueue()->property("completedItems").toInt())
                .arg(m_workspace->operationQueue()->property("totalItems").toInt())),
            row(QStringLiteral("Operation speed"), m_workspace->operationQueue()->speedText()),
            row(QStringLiteral("Operation remaining"), m_workspace->operationQueue()->remainingTimeText()),
            row(QStringLiteral("Operation error"), m_workspace->operationQueue()->error()),
            row(QStringLiteral("Administrator mode"), m_admin ? m_admin->adminModeStateName() : QStringLiteral("Unavailable"))
        }));

        QVariantList pluginRows;
        const QVariantList plugins = m_pluginActions ? m_pluginActions->plugins()
                                                     : m_workspace->loadedPlugins();
        int loadedPluginCount = 0;
        for (const QVariant &pluginValue : plugins) {
            const QVariantMap plugin = pluginValue.toMap();
            if (!plugin.contains(QStringLiteral("loaded")) || plugin.value(QStringLiteral("loaded")).toBool()) {
                ++loadedPluginCount;
            }
        }
        pluginRows.append(row(QStringLiteral("Loaded plugin count"), loadedPluginCount));
        for (const QVariant &pluginValue : plugins) {
            const QVariantMap plugin = pluginValue.toMap();
            if (plugin.contains(QStringLiteral("loaded")) && !plugin.value(QStringLiteral("loaded")).toBool()) {
                continue;
            }
            const QString name = plugin.value(QStringLiteral("displayName"), plugin.value(QStringLiteral("pluginId"))).toString();
            pluginRows.append(row(name, QStringLiteral("%1 | schemes: %2")
                .arg(plugin.value(QStringLiteral("pluginId")).toString(),
                     plugin.value(QStringLiteral("schemes")).toStringList().join(QStringLiteral(", ")))));
            pluginRows.append(row(name + QStringLiteral(" capabilities"),
                                  plugin.value(QStringLiteral("capabilitiesText"))));
            pluginRows.append(row(name + QStringLiteral(" API versions"),
                                  plugin.value(QStringLiteral("apiVersionsText"))));
            pluginRows.append(row(name + QStringLiteral(" load state"), QStringLiteral("Loaded")));
            pluginRows.append(row(name + QStringLiteral(" binary"), plugin.value(QStringLiteral("filePath")), true));
        }
        sections.append(section(QStringLiteral("PLUGINS & PROVIDERS"), pluginRows));

        QVariantList errorRows;
        const QString leftError = errorSummary(m_workspace->leftPanel()->lastError());
        const QString rightError = errorSummary(m_workspace->rightPanel()->lastError());
        const QString operationError = errorSummary(m_workspace->operationQueue()->lastError());
        if (!leftError.isEmpty()) errorRows.append(row(QStringLiteral("Left panel"), leftError));
        if (!rightError.isEmpty()) errorRows.append(row(QStringLiteral("Right panel"), rightError));
        if (!operationError.isEmpty()) errorRows.append(row(QStringLiteral("Operation Queue"), operationError));
        if (m_fileSearch && !m_fileSearch->error().isEmpty()) errorRows.append(row(QStringLiteral("File Search"), m_fileSearch->error()));
        if (m_diskUsage && !m_diskUsage->error().isEmpty()) errorRows.append(row(QStringLiteral("Disk Usage"), m_diskUsage->error()));
        if (m_folderCompare && !m_folderCompare->error().isEmpty()) errorRows.append(row(QStringLiteral("Folder Compare"), m_folderCompare->error()));
        if (!errorRows.isEmpty()) {
            sections.append(section(QStringLiteral("RECENT ERRORS"), errorRows));
        }
    }

    QVariantList backgroundRows;
    if (m_quickLook) {
        backgroundRows.append(row(QStringLiteral("Selection preview state"), m_quickLook->loading()
            ? QStringLiteral("Loading") : (m_quickLook->type().isEmpty() ? QStringLiteral("No preview")
                                                                         : QStringLiteral("Ready"))));
        backgroundRows.append(row(QStringLiteral("Selection preview type"), m_quickLook->type()));
    }
    backgroundRows.append(row(QStringLiteral("File searches this session"), m_fileSearchCount));
    backgroundRows.append(row(QStringLiteral("Disk analyses this session"), m_diskAnalysisCount));
    backgroundRows.append(row(QStringLiteral("Folder comparisons this session"), m_folderComparisonCount));
    backgroundRows.append(row(QStringLiteral("File operations this session"), m_fileOperationCount));
    if (m_fileSearch && (m_fileSearch->busy() || m_fileSearch->resultsModel()->count() > 0
                         || m_fileSearch->scannedFiles() > 0 || !m_fileSearch->error().isEmpty())) {
        backgroundRows.append(row(QStringLiteral("File Search state"), m_fileSearch->busy()
            ? QStringLiteral("Running") : (!m_fileSearch->error().isEmpty() ? QStringLiteral("Failed") : QStringLiteral("Finished"))));
        backgroundRows.append(row(QStringLiteral("File Search results"), m_fileSearch->resultsModel()->count()));
        backgroundRows.append(row(QStringLiteral("File Search scanned"), QStringLiteral("%1 files, %2 folders")
            .arg(m_fileSearch->scannedFiles()).arg(m_fileSearch->scannedFolders())));
        backgroundRows.append(row(QStringLiteral("File Search inaccessible"), m_fileSearch->inaccessiblePaths()));
        if (!m_fileSearch->error().isEmpty()) {
            backgroundRows.append(row(QStringLiteral("File Search error"), m_fileSearch->error()));
        }
    }
    if (m_diskUsage && (m_diskUsage->busy() || m_diskUsage->scannedFiles() > 0
                        || !m_diskUsage->rootPath().isEmpty() || !m_diskUsage->error().isEmpty())) {
        backgroundRows.append(row(QStringLiteral("Disk Usage state"), m_diskUsage->busy()
            ? QStringLiteral("Running") : (!m_diskUsage->error().isEmpty() ? QStringLiteral("Failed") : QStringLiteral("Finished"))));
        backgroundRows.append(row(QStringLiteral("Disk Usage root"), m_diskUsage->rootPath(), true));
        backgroundRows.append(row(QStringLiteral("Disk Usage total"), m_diskUsage->totalBytesText()));
        backgroundRows.append(row(QStringLiteral("Disk Usage scanned"), QStringLiteral("%1 files, %2 folders")
            .arg(m_diskUsage->scannedFiles()).arg(m_diskUsage->scannedFolders())));
        backgroundRows.append(row(QStringLiteral("Disk Usage inaccessible"), m_diskUsage->inaccessiblePaths()));
        if (!m_diskUsage->error().isEmpty()) {
            backgroundRows.append(row(QStringLiteral("Disk Usage error"), m_diskUsage->error()));
        }
    }
    if (m_folderCompare && (m_folderCompare->busy() || m_folderCompare->executing()
                            || m_folderCompare->resultsModel()->count() > 0
                            || !m_folderCompare->error().isEmpty()
                            || !m_folderCompare->executionSummary().isEmpty())) {
        backgroundRows.append(row(QStringLiteral("Folder Compare state"), m_folderCompare->executing()
            ? QStringLiteral("Synchronizing") : (m_folderCompare->busy() ? QStringLiteral("Comparing")
               : (!m_folderCompare->error().isEmpty() ? QStringLiteral("Failed") : QStringLiteral("Finished")))));
        backgroundRows.append(row(QStringLiteral("Folder Compare results"), m_folderCompare->resultsModel()->count()));
        backgroundRows.append(row(QStringLiteral("Folder Compare plan ready"), yesNo(m_folderCompare->planReady())));
        if (!m_folderCompare->error().isEmpty()) {
            backgroundRows.append(row(QStringLiteral("Folder Compare error"), m_folderCompare->error()));
        }
        if (!m_folderCompare->executionSummary().isEmpty()) {
            backgroundRows.append(row(QStringLiteral("Folder Compare last execution"), m_folderCompare->executionSummary()));
        }
    }
    sections.append(section(QStringLiteral("PREVIEW & BACKGROUND SERVICES"), backgroundRows));

    m_snapshot = {{QStringLiteral("generatedAt"), m_generatedAtText},
                  {QStringLiteral("sections"), sections}};
    emit snapshotChanged();
}

QString DebugInformationController::reportText(bool includePaths) const
{
    return DebugReportFormatter::format(m_snapshot, m_generatedAtText, includePaths);
}

void DebugInformationController::copyReport(bool includePaths) const
{
    if (QClipboard *clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(reportText(includePaths));
    }
}

QString DebugInformationController::sanitizeText(QString text)
{
    return DebugReportSanitizer::sanitize(text);
}
