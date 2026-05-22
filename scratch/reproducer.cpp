#include <QCoreApplication>
#include <QFileSystemWatcher>
#include <iostream>
#include "../src/core/ArchiveFileProvider.h"
#include "../src/core/ArchiveSupport.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    std::cout << "Starting reproducer..." << std::endl;
    
    QString path = QStringLiteral("archive://C:/Users/tankr/cpu-z_2.19.zip|/");
    std::cout << "Path: " << path.toStdString() << std::endl;
    
    QFileSystemWatcher watcher;
    std::cout << "Adding path to watcher..." << std::endl;
    watcher.addPath(path);
    std::cout << "Path added to watcher (no crash so far)." << std::endl;
    
    ArchiveFileProvider provider;
    bool hasBackend = ArchiveSupport::archiveBackendAvailable();
    std::cout << "Created provider. Backend available (ArchiveSupport): " << (hasBackend ? "YES" : "NO") << std::endl;
    
    std::cout << "Calling pathExists..." << std::endl;
    bool exists = provider.pathExists(path);
    std::cout << "pathExists: " << (exists ? "YES" : "NO") << std::endl;
    
    std::cout << "Calling isDirectory..." << std::endl;
    bool isDir = provider.isDirectory(path);
    std::cout << "isDirectory: " << (isDir ? "YES" : "NO") << std::endl;
    
    std::cout << "Getting child paths..." << std::endl;
    QStringList children = provider.childPaths(path, true);
    std::cout << "Child paths count: " << children.size() << std::endl;
    for (const auto &child : children) {
        std::cout << " - " << child.toStdString() << std::endl;
    }
    
    std::cout << "Removing path from watcher..." << std::endl;
    watcher.removePath(path);
    std::cout << "Path removed from watcher." << std::endl;
    
    std::cout << "Reproducer finished successfully." << std::endl;
    return 0;
}
