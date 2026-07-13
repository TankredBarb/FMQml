#include "DirectoryWatchPolicy.h"

#include "DirectoryModelAlgorithms.h"

#include <QDir>

namespace {
bool watchSourcePathEquals(const QString &left, const QString &right)
{
    const QString normalizedLeft = QDir::cleanPath(QDir::fromNativeSeparators(left));
    const QString normalizedRight = QDir::cleanPath(QDir::fromNativeSeparators(right));
#ifdef Q_OS_WIN
    return normalizedLeft.compare(normalizedRight, Qt::CaseInsensitive) == 0;
#else
    return normalizedLeft == normalizedRight;
#endif
}
}

namespace DirectoryWatchPolicy {

bool sourceMatches(const DirectoryChangeEvent &event, const QString &watchPath)
{
    return event.sourcePath.isEmpty()
        || watchSourcePathEquals(event.sourcePath, watchPath);
}

bool isTransientPartWrite(const DirectoryChangeEvent &event)
{
    return event.type == DirectoryChangeEvent::Type::Modified
        && !event.path.isEmpty()
        && DirectoryModelAlgorithms::pathKey(event.path).endsWith(QStringLiteral(".part"));
}

QString coalescingKey(const DirectoryChangeEvent &event)
{
    switch (event.type) {
    case DirectoryChangeEvent::Type::Added:
    case DirectoryChangeEvent::Type::Modified:
    case DirectoryChangeEvent::Type::Removed:
        return event.path.isEmpty() ? QString{}
            : QStringLiteral("path:") + DirectoryModelAlgorithms::pathKey(event.path);
    case DirectoryChangeEvent::Type::Renamed:
        return QStringLiteral("rename:")
            + DirectoryModelAlgorithms::pathKey(event.oldPath)
            + QStringLiteral("->")
            + DirectoryModelAlgorithms::pathKey(event.newPath);
    case DirectoryChangeEvent::Type::Overflow:
        return QStringLiteral("overflow:") + DirectoryModelAlgorithms::pathKey(event.path);
    }
    return {};
}

void appendCoalesced(QList<DirectoryChangeEvent> &pending,
                     const DirectoryChangeEvent &event)
{
    if (event.type == DirectoryChangeEvent::Type::Overflow) {
        pending = {event};
        return;
    }
    if (event.type == DirectoryChangeEvent::Type::Renamed) {
        pending.append(event);
        return;
    }

    const QString key = coalescingKey(event);
    if (key.isEmpty()) {
        pending.append(event);
        return;
    }
    for (int index = pending.size() - 1; index >= 0; --index) {
        const DirectoryChangeEvent &existing = pending.at(index);
        if (existing.type != DirectoryChangeEvent::Type::Renamed
            && existing.type != DirectoryChangeEvent::Type::Overflow
            && coalescingKey(existing) == key) {
            pending[index] = event;
            return;
        }
    }
    pending.append(event);
}

}
