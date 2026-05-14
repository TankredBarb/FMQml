#include "DirectoryScanner.h"
#include "../models/DirectoryModel.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>
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
                emit finished(path, false, myGen, QStringLiteral("Folder does not exist"));
            return;
        }

        const QString canonicalPath = info.canonicalFilePath();
        QDir dir(canonicalPath);
        if (!dir.isReadable()) {
            if (myGen == m_scanGeneration.load())
                emit finished(path, false, myGen, QStringLiteral("Folder is not readable"));
            return;
        }

        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::System;
        if (m_showHidden) {
            filters |= QDir::Hidden;
        }

        QList<FileEntry> batch;
        batch.reserve(100);
        QLocale loc;
        QDirIterator it(dir.absolutePath(), filters);

        while (it.hasNext()) {
            it.next();
            if (myGen != m_scanGeneration.load()) {
                return;
            }

            QFileInfo fileInfo = it.fileInfo();

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
            entry.sizeText = entry.isDirectory ? QStringLiteral("Folder") : loc.formattedDataSize(entry.size, 1, QLocale::DataSizeTraditionalFormat);
            entry.modifiedText = loc.toString(entry.modified, QLocale::ShortFormat);
            static const QStringList imageSuffixes = {QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"), QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("ico")};
            entry.isImage = !entry.isDirectory && imageSuffixes.contains(entry.suffix.toLower());
            batch.append(entry);

            // Send in batches of 100 or when finished
            if (batch.size() >= 100) {
                emit batchReady(batch, myGen);
                batch.clear();
            }
        }

        if (!batch.isEmpty()) {
            emit batchReady(batch, myGen);
        }

        emit finished(canonicalPath, true, myGen);
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
