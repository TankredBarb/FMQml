#include "DirectoryScanner.h"
#include "../models/DirectoryModel.h"

#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>

DirectoryScanner::DirectoryScanner(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<void>::finished, this, &DirectoryScanner::onWatcherFinished);
}

DirectoryScanner::~DirectoryScanner()
{
    cancel();
}

void DirectoryScanner::setShowHidden(bool show)
{
    m_showHidden = show;
}

void DirectoryScanner::scan(const QString &path)
{
    cancel();

    m_currentPath = path;
    m_canceled = false;

    emit started();

    m_watcher.setFuture(QtConcurrent::run([this, path]() {
        QFileInfo info(path);
        if (!info.exists() || !info.isDir()) {
            emit finished(path, false, QStringLiteral("Folder does not exist"));
            return;
        }

        const QString canonicalPath = info.canonicalFilePath();
        QDir dir(canonicalPath);
        if (!dir.isReadable()) {
            emit finished(path, false, QStringLiteral("Folder is not readable"));
            return;
        }

        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
        if (m_showHidden) {
            filters |= QDir::Hidden;
        }

        const QFileInfoList infos = dir.entryInfoList(
            filters,
            QDir::DirsFirst | QDir::Name | QDir::IgnoreCase);

        QList<FileEntry> batch;
        batch.reserve(infos.size());

        for (const QFileInfo &fileInfo : infos) {
            if (m_canceled) {
                return;
            }

            // Explicitly hide dot-files if showHidden is false
            if (!m_showHidden && fileInfo.fileName().startsWith('.')) {
                continue;
            }

            FileEntry entry;
            entry.name = fileInfo.fileName();
            entry.path = fileInfo.absoluteFilePath();
            entry.suffix = fileInfo.suffix();
            entry.size = fileInfo.size();
            entry.modified = fileInfo.lastModified();
            entry.isDirectory = fileInfo.isDir();
            entry.isHidden = fileInfo.isHidden();
            batch.append(entry);

            // Send in batches of 100 or when finished
            if (batch.size() >= 100) {
                emit batchReady(batch);
                batch.clear();
            }
        }

        if (!batch.isEmpty()) {
            emit batchReady(batch);
        }

        emit finished(canonicalPath, true);
    }));
}

void DirectoryScanner::cancel()
{
    m_canceled = true;
    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

bool DirectoryScanner::isRunning() const
{
    return m_watcher.isRunning();
}

QString DirectoryScanner::currentPath() const
{
    return m_currentPath;
}

void DirectoryScanner::onWatcherFinished()
{
    // Finished signal is already emitted from the thread
}
