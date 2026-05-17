#include "FilePanelController.h"

#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QUrl>

#include "../core/LocalFileProvider.h"

FilePanelController::FilePanelController(QObject *parent)
    : QObject(parent)
    , m_fileProvider(std::make_unique<LocalFileProvider>())
{
    connect(&m_directoryModel, &DirectoryModel::currentPathChanged, this, &FilePanelController::currentPathChanged);
}

DirectoryModel *FilePanelController::directoryModel()
{
    return &m_directoryModel;
}

QString FilePanelController::currentPath() const
{
    return m_directoryModel.currentPath();
}

bool FilePanelController::canGoBack() const
{
    return !m_backStack.isEmpty();
}

bool FilePanelController::canGoForward() const
{
    return !m_forwardStack.isEmpty();
}

QString FilePanelController::hoveredPath() const
{
    return m_hoveredPath;
}

void FilePanelController::setHoveredPath(const QString &path)
{
    if (m_hoveredPath == path) {
        return;
    }
    m_hoveredPath = path;
    emit hoveredPathChanged();
}

bool FilePanelController::openPath(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }

    if (!m_fileProvider->pathExists(path)) {
        return false;
    }

    return openPathInternal(path, true);
}

void FilePanelController::openRow(int row)
{
    if (!m_directoryModel.isDirectoryAt(row)) {
        return;
    }
    openPath(m_directoryModel.pathAt(row));
}

void FilePanelController::openItem(int row)
{
    if (m_directoryModel.isDirectoryAt(row)) {
        openPath(m_directoryModel.pathAt(row));
        return;
    }
    const QString path = m_directoryModel.pathAt(row);
    if (!path.isEmpty()) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
}

void FilePanelController::revealInFileManager(int row)
{
    const QString path = m_directoryModel.pathAt(row);
    if (path.isEmpty()) {
        return;
    }

    const QString nativePath = QDir::toNativeSeparators(path);

#if defined(Q_OS_WIN)
    const QString arg = QStringLiteral("/select,\"%1\"").arg(nativePath);
    QProcess::startDetached(QStringLiteral("explorer.exe"), {arg});
#elif defined(Q_OS_MACOS)
    QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-R"), path});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_fileProvider->parentPath(path)));
#endif
}

void FilePanelController::openInTerminal()
{
#if defined(Q_OS_WIN)
    const QString path = QDir::toNativeSeparators(currentPath());
    QProcess::startDetached(QStringLiteral("wt.exe"),
        {QStringLiteral("-d"), path, QStringLiteral("powershell.exe"),
         QStringLiteral("-NoExit"), QStringLiteral("-Command"),
         QStringLiteral("Set-Location '%1'").arg(path)});
#endif
}

void FilePanelController::goBack()
{
    if (m_backStack.isEmpty()) {
        return;
    }

    const QString previous = m_backStack.takeLast();
    if (!currentPath().isEmpty()) {
        m_forwardStack.append(currentPath());
    }
    openPathInternal(previous, false);
    emit historyChanged();
}

void FilePanelController::goForward()
{
    if (m_forwardStack.isEmpty()) {
        return;
    }

    const QString next = m_forwardStack.takeLast();
    if (!currentPath().isEmpty()) {
        m_backStack.append(currentPath());
    }
    openPathInternal(next, false);
    emit historyChanged();
}

void FilePanelController::goUp()
{
    QDir dir(currentPath());
    if (dir.cdUp()) {
        openPath(dir.absolutePath());
    }
}

bool FilePanelController::rename(int row, const QString &newName)
{
    const QString oldPath = m_directoryModel.pathAt(row);
    if (oldPath.isEmpty()) {
        return false;
    }

    return renamePath(oldPath, newName);
}

bool FilePanelController::renamePath(const QString &oldPath, const QString &newName)
{
    if (oldPath.isEmpty()) {
        return false;
    }

    if (m_fileProvider->renamePath(oldPath, newName)) {
        const QString trimmedName = newName.trimmed();
        const QString newPath = m_fileProvider->childPath(m_fileProvider->parentPath(oldPath), trimmedName);
        if (!m_directoryModel.renamePath(oldPath, newPath)) {
            refresh();
        }
        emit entryRenamed(oldPath, newPath);
        return true;
    }

    return false;
}

bool FilePanelController::createFolder(const QString &name)
{
    QString path;
    if (m_fileProvider->createFolder(currentPath(), name, &path)) {
        if (!m_directoryModel.insertPath(path)) {
            refresh();
        }
        return true;
    }
    return false;
}

bool FilePanelController::createFile(const QString &name)
{
    QString path;
    if (m_fileProvider->createFile(currentPath(), name, &path)) {
        if (!m_directoryModel.insertPath(path)) {
            refresh();
        }
        return true;
    }
    return false;
}

QString FilePanelController::fileNameForPath(const QString &path) const
{
    return m_fileProvider->fileName(path);
}

QString FilePanelController::parentPathForPath(const QString &path) const
{
    return m_fileProvider->parentPath(path);
}

QString FilePanelController::childPathForCurrent(const QString &name) const
{
    return m_fileProvider->childPath(currentPath(), name);
}

QString FilePanelController::childPathForPath(const QString &parentPath, const QString &name) const
{
    return m_fileProvider->childPath(parentPath, name);
}

void FilePanelController::showProperties(int row)
{
    const QString path = m_directoryModel.pathAt(row);
    if (!path.isEmpty()) {
        emit revealProperties(path);
    }
}

void FilePanelController::refresh()
{
    m_directoryModel.refresh();
}

QStringList FilePanelController::selectedPaths() const
{
    return m_directoryModel.selectedPaths();
}

bool FilePanelController::openPathInternal(const QString &path, bool addToHistory)
{
    const QString newPath = m_fileProvider->normalizedPath(path);
    const QString oldPath = m_fileProvider->normalizedPath(currentPath());

    if (!newPath.isEmpty() && newPath == oldPath) {
        return true;
    }

    if (m_directoryModel.openPath(newPath)) {
        m_directoryModel.setFilterText({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        emit historyChanged();
        return true;
    }

    return false;
}

void FilePanelController::pushHistory(const QString &path)
{
    m_backStack.append(path);
    constexpr qsizetype maxHistory = 64;
    while (m_backStack.size() > maxHistory) {
        m_backStack.removeFirst();
    }
}

int FilePanelController::viewMode() const
{
    return m_viewMode;
}

void FilePanelController::setViewMode(int mode)
{
    if (m_viewMode == mode) return;
    m_viewMode = mode;
    emit viewModeChanged();
}

