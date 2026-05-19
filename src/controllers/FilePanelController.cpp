#include "FilePanelController.h"

#include <QDesktopServices>
#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#include "../core/LocalFileProvider.h"

FilePanelController::FilePanelController(QObject *parent)
    : QObject(parent)
    , m_fileProvider(std::make_unique<LocalFileProvider>())
{
    connect(&m_directoryModel, &DirectoryModel::currentPathChanged, this, &FilePanelController::currentPathChanged);
    connect(&m_directoryModel, &DirectoryModel::directoryUnavailable, this, &FilePanelController::recoverFromMissingPath);
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

QString FilePanelController::statusMessage() const
{
    return m_statusMessage;
}

bool FilePanelController::scrolling() const
{
    return m_scrolling;
}

void FilePanelController::setHoveredPath(const QString &path)
{
    if (m_hoveredPath == path) {
        return;
    }
    m_hoveredPath = path;
    emit hoveredPathChanged();
}

void FilePanelController::setScrolling(bool scrolling)
{
    if (m_scrolling == scrolling) {
        return;
    }
    m_scrolling = scrolling;
    emit scrollingChanged();
}

void FilePanelController::setStatusMessage(const QString &message)
{
    if (m_statusMessage == message) {
        return;
    }
    m_statusMessage = message;
    emit statusMessageChanged();
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
    const QString parent = m_fileProvider->parentPath(currentPath());
    if (!parent.isEmpty() && parent != currentPath()) {
        openPath(parent);
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
        } else {
            m_directoryModel.noteLocalMutation();
        }
        emit entryRenamed(oldPath, newPath);
        emit contentsChanged(m_fileProvider->parentPath(oldPath));
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
        } else {
            m_directoryModel.noteLocalMutation();
        }
        emit entryCreated(path);
        emit contentsChanged(currentPath());
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
        } else {
            m_directoryModel.noteLocalMutation();
        }
        emit entryCreated(path);
        emit contentsChanged(currentPath());
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

void FilePanelController::refresh()
{
    setStatusMessage({});
    m_directoryModel.refresh();
    emit contentsChanged(currentPath());
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
        setStatusMessage({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        emit pathNavigated(newPath);
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

QString FilePanelController::fallbackPathForMissing(const QString &path) const
{
    QString candidate = m_fileProvider->normalizedPath(path);
    if (candidate.isEmpty()) {
        return {};
    }

    while (!candidate.isEmpty()) {
        if (m_fileProvider->pathExists(candidate) && m_fileProvider->isDirectory(candidate)) {
            return candidate;
        }

        const QString parent = m_fileProvider->parentPath(candidate);
        if (parent.isEmpty() || parent == candidate) {
            break;
        }
        candidate = parent;
    }

    const QString home = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (!home.isEmpty() && m_fileProvider->pathExists(home) && m_fileProvider->isDirectory(home)) {
        return m_fileProvider->normalizedPath(home);
    }

    return {};
}

void FilePanelController::recoverFromMissingPath(const QString &path, const QString &error)
{
    const QString normalizedCurrent = m_fileProvider->normalizedPath(currentPath());
    const QString normalizedMissing = m_fileProvider->normalizedPath(path);
    if (normalizedCurrent.isEmpty() || normalizedMissing.isEmpty()) {
        return;
    }

    if (normalizedCurrent != normalizedMissing) {
        return;
    }

    const QString fallback = fallbackPathForMissing(normalizedMissing);
    if (fallback.isEmpty() || fallback == normalizedCurrent) {
        setStatusMessage(QStringLiteral("Folder is no longer available"));
        return;
    }

    if (!openPathInternal(fallback, false)) {
        setStatusMessage(QStringLiteral("Folder is no longer available"));
        return;
    }

    setStatusMessage(QStringLiteral("Folder was removed externally. Moved up to %1")
                     .arg(m_fileProvider->fileName(fallback).isEmpty() ? fallback : m_fileProvider->fileName(fallback)));
    Q_UNUSED(error)
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

