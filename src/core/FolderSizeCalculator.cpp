#include "FolderSizeCalculator.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QStringList>

void FolderSizeCalculator::run() {
    setAutoDelete(false);
    qint64 totalSize = 0;
    QElapsedTimer timer;
    timer.start();

    QStringList dirs;
    dirs.append(m_path);

    while (!dirs.isEmpty()) {
        QString current = dirs.takeLast();

        QDirIterator files(current, QDir::Files | QDir::NoDotAndDotDot);
        while (files.hasNext()) {
            files.next();
            totalSize += files.fileInfo().size();
            if (timer.elapsed() > 500) {
                emit progressUpdate(totalSize, m_generation);
                timer.restart();
            }
        }

        QDirIterator subdirs(current, QDir::Dirs | QDir::NoDotAndDotDot);
        while (subdirs.hasNext()) {
            subdirs.next();
            QFileInfo fi = subdirs.fileInfo();
            if (fi.isSymLink())
                continue;
            dirs.append(fi.absoluteFilePath());
        }
    }

    emit resultReady(totalSize, m_generation);
}
