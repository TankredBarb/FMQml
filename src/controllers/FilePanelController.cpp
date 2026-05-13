#include "FilePanelController.h"

#include <QDir>
#include <QFileInfo>

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

