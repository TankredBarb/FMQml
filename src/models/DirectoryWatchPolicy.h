#pragma once

#include "../core/DirectoryChangeWatcher.h"

namespace DirectoryWatchPolicy {

bool sourceMatches(const DirectoryChangeEvent &event, const QString &watchPath);
bool isTransientPartWrite(const DirectoryChangeEvent &event);
QString coalescingKey(const DirectoryChangeEvent &event);
void appendCoalesced(QList<DirectoryChangeEvent> &pending,
                     const DirectoryChangeEvent &event);

}
