#include "FilePanelController.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QUrl>

FilePanelController::FilePanelController(QObject *parent)
    : QObject(parent)
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

void FilePanelController::openPath(const QString &path)
{
    openPathInternal(path, true);
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
    const QFileInfo info(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
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

    QFileInfo oldInfo(oldPath);
    QString newPath = oldInfo.absoluteDir().filePath(newName);

    if (QFile::rename(oldPath, newPath)) {
        refresh();
        return true;
    }
    return false;
}

bool FilePanelController::createFolder(const QString &name)
{
    QDir dir(currentPath());
    QString folderName = name;
    
    if (dir.exists(folderName)) {
        for (int i = 1; i < 1000; ++i) {
            QString candidate = QStringLiteral("%1 (%2)").arg(name).arg(i);
            if (!dir.exists(candidate)) {
                folderName = candidate;
                break;
            }
        }
    }

    if (dir.mkdir(folderName)) {
        refresh();
        return true;
    }
    return false;
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

void FilePanelController::openPathInternal(const QString &path, bool addToHistory)
{
    const QString newPath = QDir::fromNativeSeparators(path);
    const QString oldPath = QDir::fromNativeSeparators(currentPath());

    if (!newPath.isEmpty() && newPath == oldPath) {
        return;
    }

    if (m_directoryModel.openPath(newPath)) {
        m_directoryModel.setFilterText({});
        if (addToHistory && !oldPath.isEmpty()) {
            pushHistory(oldPath);
            m_forwardStack.clear();
        }
        emit historyChanged();
    }
}

void FilePanelController::pushHistory(const QString &path)
{
    m_backStack.append(path);
    constexpr qsizetype maxHistory = 64;
    while (m_backStack.size() > maxHistory) {
        m_backStack.removeFirst();
    }
}

