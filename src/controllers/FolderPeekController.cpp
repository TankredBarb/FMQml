#include "FolderPeekController.h"

#include "FilePanelController.h"
#include "../core/FileEntrySortPolicy.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QUrl>

#include <algorithm>
#include <utility>

namespace {
constexpr int MaxPeekEntries = 1000;

bool localPath(const QString &path)
{
    const QUrl url(path);
    return !url.isValid() || url.scheme().isEmpty() || url.isLocalFile();
}

QVariantMap peekPresentationEntry(const FileEntry &entry)
{
    return {{QStringLiteral("name"), entry.name.left(1024)},
            {QStringLiteral("path"), entry.path.left(1024)},
            {QStringLiteral("isDirectory"), entry.isDirectory},
            {QStringLiteral("suffix"), entry.suffix.left(1024)},
            {QStringLiteral("mimeType"), entry.mimeType.left(1024)},
            {QStringLiteral("hasThumbnail"), entry.hasThumbnail},
            {QStringLiteral("iconName"), entry.iconName.left(1024)}};
}
}

FolderPeekController::FolderPeekController(QObject *parent) : QObject(parent)
{
    m_pool.setMaxThreadCount(1);
    m_pool.setExpiryTimeout(30000);
}

FolderPeekController::~FolderPeekController()
{
    ++m_generation;
    m_pool.clear();
    m_pool.waitForDone();
}

void FolderPeekController::setSourcePanel(FilePanelController *panel) { m_sourcePanel = panel; }
bool FolderPeekController::isOpen() const { return m_open; }
QString FolderPeekController::currentPath() const { return m_currentPath; }
QString FolderPeekController::state() const { return m_state; }
QVariantList FolderPeekController::entries() const { return m_entries; }
bool FolderPeekController::canGoBack() const { return !m_backStack.isEmpty(); }
QVariantList FolderPeekController::breadcrumbs() const
{
    return m_sourcePanel ? m_sourcePanel->breadcrumbEntriesForPath(m_currentPath) : QVariantList{};
}

void FolderPeekController::openPath(const QString &path, bool showHidden)
{
    ++m_openCount;
    emit statisticsChanged();
    m_showHidden = showHidden;
    m_backStack.clear();
    emit historyChanged();
    if (!m_open) {
        m_open = true;
        emit openChanged();
    }
    load(path, false);
}

void FolderPeekController::navigate(const QString &path) { load(path, true); }

void FolderPeekController::goBack()
{
    if (m_backStack.isEmpty()) return;
    const QString path = m_backStack.takeLast();
    emit historyChanged();
    load(path, false);
}

void FolderPeekController::goUp()
{
    if (!m_sourcePanel) return;
    const QString parent = m_sourcePanel->parentPathForPath(m_currentPath);
    if (!parent.isEmpty() && parent != m_currentPath) load(parent, true);
}

void FolderPeekController::close()
{
    if (m_state == QLatin1String("loading")) {
        ++m_cancellationCount;
        emit statisticsChanged();
    }
    ++m_generation;
    m_pool.clear();
    m_open = false;
    m_currentPath.clear();
    m_entries.clear();
    m_hasMore = false;
    m_backStack.clear();
    m_state = QStringLiteral("idle");
    emit openChanged();
    emit currentPathChanged();
    emit entriesChanged();
    emit stateChanged();
    emit historyChanged();
}

bool FolderPeekController::openInSourcePanel()
{
    if (!m_sourcePanel || m_currentPath.isEmpty()) return false;
    const bool opened = m_sourcePanel->openPath(m_currentPath);
    if (opened) close();
    return opened;
}

void FolderPeekController::openEntry(const QString &path, bool isDirectory)
{
    if (path.isEmpty()) return;
    if (isDirectory) {
        navigate(path);
        return;
    }
    if (!m_sourcePanel) return;
    m_sourcePanel->openFilePath(path);
}

void FolderPeekController::load(const QString &path, bool addToHistory)
{
    if (!m_open || path.isEmpty()) return;
    if (m_state == QLatin1String("loading")) {
        ++m_cancellationCount;
        emit statisticsChanged();
    }
    if (addToHistory && !m_currentPath.isEmpty() && m_currentPath != path) {
        m_backStack.push_back(m_currentPath);
        emit historyChanged();
    }
    m_currentPath = path;
    m_entries.clear();
    m_hasMore = false;
    m_state = localPath(path) ? QStringLiteral("loading") : QStringLiteral("unavailable");
    emit currentPathChanged();
    emit entriesChanged();
    emit stateChanged();
    const quint64 generation = ++m_generation;
    if (!localPath(path)) return;

    const QString resolvedPath = QUrl(path).isLocalFile() ? QUrl(path).toLocalFile() : path;
    const DirectoryModel *directoryModel = m_sourcePanel ? m_sourcePanel->directoryModel() : nullptr;
    const int sortRole = directoryModel ? int(directoryModel->sortRole()) : 0;
    const Qt::SortOrder sortOrder = directoryModel ? directoryModel->sortOrder() : Qt::AscendingOrder;
    const bool mixFilesAndFolders = directoryModel ? directoryModel->mixFilesAndFolders() : false;
    QPointer<FolderPeekController> self(this);
    m_pool.start([self, generation, path, resolvedPath, showHidden = m_showHidden,
                  sortRole, sortOrder, mixFilesAndFolders]() {
        QVariantList entries;
        bool hasMore = false;
        QString resultState;
        const QFileInfo root(resolvedPath);
        if (!root.exists() || !root.isDir() || !root.isReadable()) {
            resultState = QStringLiteral("error");
        } else {
            const QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot
                                          | (showHidden ? QDir::Hidden : QDir::Filter(0));
            QDirIterator iterator(resolvedPath, filters, QDirIterator::NoIteratorFlags);
            QMimeDatabase mimeDatabase;
            QList<FileEntry> scannedEntries;
            while (iterator.hasNext()) {
                if (!self || self->m_generation.load() != generation) return;
                iterator.next();
                if (scannedEntries.size() >= MaxPeekEntries) {
                    hasMore = true;
                    break;
                }
                const QFileInfo info = iterator.fileInfo();
                const bool directory = info.isDir();
                const QString mime = directory ? QStringLiteral("inode/directory")
                                               : mimeDatabase.mimeTypeForFile(info, QMimeDatabase::MatchExtension).name();
                FileEntry entry;
                entry.name = info.fileName();
                entry.path = info.absoluteFilePath();
                entry.suffix = info.suffix().toLower();
                entry.size = info.size();
                entry.modified = info.lastModified();
                entry.created = info.birthTime();
                entry.isDirectory = directory;
                entry.mimeType = mime;
                entry.hasThumbnail = !directory && mime.startsWith(QLatin1String("image/"));
                entry.iconName = directory ? QStringLiteral("folder.svg")
                                           : (entry.hasThumbnail ? QStringLiteral("image.svg")
                                                                 : QStringLiteral("document.svg"));
                scannedEntries.push_back(std::move(entry));
            }
            std::stable_sort(scannedEntries.begin(), scannedEntries.end(),
                             [mixFilesAndFolders, sortRole, sortOrder](const FileEntry &a, const FileEntry &b) {
                return FileEntrySortPolicy::lessThan(a, b, mixFilesAndFolders, sortRole, sortOrder);
            });
            for (const FileEntry &entry : std::as_const(scannedEntries)) {
                entries.push_back(peekPresentationEntry(entry));
            }
            resultState = scannedEntries.isEmpty() ? QStringLiteral("empty") : QStringLiteral("ready");
        }
        if (!self) return;
        QMetaObject::invokeMethod(self, [self, generation, path, resultState, entries, hasMore]() {
            if (self) self->publish(generation, path, resultState, entries, hasMore);
        }, Qt::QueuedConnection);
    });
}

void FolderPeekController::publish(quint64 generation, const QString &path,
                                   const QString &state, const QVariantList &entries, bool hasMore)
{
    if (m_generation.load() != generation || m_currentPath != path) return;
    m_state = state;
    m_entries = entries;
    m_hasMore = hasMore;
    if (state == QLatin1String("error")) {
        ++m_failureCount;
        emit statisticsChanged();
    }
    emit stateChanged();
    emit entriesChanged();
}
