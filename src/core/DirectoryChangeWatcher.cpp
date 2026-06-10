#include "DirectoryChangeWatcher.h"

#ifdef Q_OS_WIN
#include "WinDirectoryChangeWatcher.h"
#elif defined(Q_OS_LINUX)
#include "LinuxDirectoryChangeWatcher.h"
#else
#include "QtDirectoryChangeWatcher.h"
#endif

#include <QMetaType>

DirectoryChangeWatcher::DirectoryChangeWatcher(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<DirectoryChangeEvent>("DirectoryChangeEvent");
    qRegisterMetaType<QList<DirectoryChangeEvent>>("QList<DirectoryChangeEvent>");
}

std::unique_ptr<DirectoryChangeWatcher> createDirectoryChangeWatcher(QObject *parent)
{
#ifdef Q_OS_WIN
    return std::make_unique<WinDirectoryChangeWatcher>(parent);
#elif defined(Q_OS_LINUX)
    return std::make_unique<LinuxDirectoryChangeWatcher>(parent);
#else
    return std::make_unique<QtDirectoryChangeWatcher>(parent);
#endif
}
