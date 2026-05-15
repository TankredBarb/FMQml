#include "FolderSizeCalculator.h"
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>

void FolderSizeCalculator::run() {
    qint64 totalSize = 0;
    QElapsedTimer timer;
    timer.start();

    QDirIterator it(m_path, QDir::Files | QDir::NoDotAndDotDot | QDir::System | QDir::Hidden,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        totalSize += it.fileInfo().size();
        if (timer.elapsed() > 500) {
            emit progressUpdate(totalSize, m_generation);
            timer.restart();
        }
    }

    emit resultReady(totalSize, m_generation);
}
