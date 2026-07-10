#pragma once

#include <QtGlobal>

#ifdef Q_OS_LINUX

#include "FileProvider.h"

#include <QDateTime>
#include <QList>
#include <QLocale>
#include <QString>

#include <optional>
#include <sys/types.h>

namespace LinuxFileEnumerator {

struct Entry {
    QString path;
    QString name;
    QString parentPath;
    qint64 size = 0;
    qint64 apparentSize = 0;
    quint64 device = 0;
    quint64 inode = 0;
    quint64 hardLinkCount = 0;
    QDateTime modified;
    QDateTime created;
    bool isDirectory = false;
    bool isHidden = false;
    bool isReadOnly = false;
    bool isLocked = false;
    bool isSymlink = false;
    bool isBrokenSymlink = false;
    bool isMountBoundary = false;
};

struct Options {
    bool includeHidden = true;
    bool stayOnRootDevice = false;
    dev_t rootDevice = 0;
};

std::optional<dev_t> deviceForPath(const QString &path);
qint64 apparentSizeForPath(const QString &path);
bool enumerateChildren(const QString &path, const Options &options, QList<Entry> *entries, QString *error);
FileEntry toFileEntry(const Entry &entry, const QLocale &locale);

} // namespace LinuxFileEnumerator

#endif
