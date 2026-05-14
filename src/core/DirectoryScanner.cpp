#include "DirectoryScanner.h"
#include "../models/DirectoryModel.h"

#include <QDir>
#include <QFileInfo>
#include <QtConcurrent>

DirectoryScanner::DirectoryScanner(QObject *parent)
    : QObject(parent)
{
}

DirectoryScanner::~DirectoryScanner()
{
    cancel();
    m_watcher.waitForFinished();
}

void DirectoryScanner::setShowHidden(bool show)
{
    m_showHidden = show;
}

void DirectoryScanner::scan(const QString &path)
{
    cancel();

    int myGen = ++m_scanGeneration;
    m_currentPath = path;

    emit started();

    m_watcher.setFuture(QtConcurrent::run([this, path, myGen]() {
        QFileInfo info(path);
        if (!info.exists() || !info.isDir()) {
            if (myGen == m_scanGeneration.load())
                emit finished(path, false, QStringLiteral("Folder does not exist"));
            return;
        }

        const QString canonicalPath = info.canonicalFilePath();
        QDir dir(canonicalPath);
        if (!dir.isReadable()) {
            if (myGen == m_scanGeneration.load())
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
            if (myGen != m_scanGeneration.load()) {
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

        if (myGen != m_scanGeneration.load())
            return;

        if (!batch.isEmpty()) {
            emit batchReady(batch);
        }

        emit finished(canonicalPath, true);
    }));
}

void DirectoryScanner::cancel()
{
    ++m_scanGeneration;
}

bool DirectoryScanner::isRunning() const
{
    return m_watcher.isRunning();
}

QString DirectoryScanner::currentPath() const
{
    return m_currentPath;
}
