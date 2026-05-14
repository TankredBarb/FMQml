#include "FolderSizeCalculator.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QElapsedTimer>

void FolderSizeCalculator::run() {
    qint64 totalSize = 0;
    QElapsedTimer timer;
    timer.start();
    
    QDirIterator it(m_path, QDir::Files | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    
    while (it.hasNext()) {
        it.next();
        totalSize += it.fileInfo().size();
        
        if (timer.elapsed() > 500) {
            emit progressUpdate(totalSize);
            timer.restart();
        }
    }
    emit resultReady(totalSize);
}
