#include "FavoritesStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

namespace {
constexpr int SchemaVersion = 1;
constexpr qsizetype MaxUsageEntries = 300;

QString nowIso()
{
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString displayLabelForPath(const QString &path)
{
    const QFileInfo info(path);
    QString name = info.fileName();
    if (!name.isEmpty()) {
        return name;
    }

    QString normalized = QDir::fromNativeSeparators(path);
    if (normalized.endsWith(QLatin1Char('/'))) {
        normalized.chop(1);
    }
    return normalized.isEmpty() ? path : normalized;
}

double usageScore(int visitCount, const QString &lastVisitedAt)
{
    QDateTime visited = QDateTime::fromString(lastVisitedAt, Qt::ISODateWithMs);
    if (!visited.isValid()) {
        visited = QDateTime::fromString(lastVisitedAt, Qt::ISODate);
    }
    if (!visited.isValid()) {
        return double(visitCount);
    }

    const qint64 ageSeconds = visited.secsTo(QDateTime::currentDateTimeUtc());
    const double ageDays = std::max(0.0, double(ageSeconds) / 86400.0);
    const double recencyBoost = 3.0 / (1.0 + ageDays);
    return double(visitCount) + recencyBoost;
}

void sortUsageEntries(QList<FavoriteUsageEntry> &entries)
{
    std::sort(entries.begin(), entries.end(), [](const auto &left, const auto &right) {
        if (left.score != right.score) {
            return left.score > right.score;
        }
        return left.lastVisitedAt > right.lastVisitedAt;
    });
}

void updatePinnedOrder(QList<FavoritePinnedEntry> &entries)
{
    for (qsizetype i = 0; i < entries.size(); ++i) {
        entries[i].order = int(i);
    }
}

QStringList normalizedTags(const QStringList &tags)
{
    QStringList normalized;
    for (const QString &tag : tags) {
        QString value = tag.trimmed();
        if (value.startsWith(QLatin1Char('#'))) {
            value = value.mid(1).trimmed();
        }
        value.replace(QLatin1Char(','), QLatin1Char(' '));
        value = value.simplified().left(32);
        if (!value.isEmpty() && !normalized.contains(value, Qt::CaseInsensitive)) {
            normalized.append(value);
        }
        if (normalized.size() >= 8) {
            break;
        }
    }
    normalized.sort(Qt::CaseInsensitive);
    return normalized;
}
}

FavoritesStore::FavoritesStore()
{
    load();
}

const QList<FavoritePinnedEntry> &FavoritesStore::pinnedEntries() const
{
    return m_pinnedEntries;
}

const QList<FavoriteUsageEntry> &FavoritesStore::usageEntries() const
{
    return m_usageEntries;
}

QString FavoritesStore::normalizedPathKey(const QString &path)
{
    const QString trimmed = path.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }
    QString normalized = QDir::cleanPath(QDir::fromNativeSeparators(trimmed));
#ifdef Q_OS_WIN
    normalized = normalized.toCaseFolded();
#endif
    return normalized;
}

QString FavoritesStore::storageFilePath() const
{
    const QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dataRoot).filePath(QStringLiteral("favorites.json"));
}

bool FavoritesStore::load()
{
    m_pinnedEntries.clear();
    m_usageEntries.clear();

    QFile file(storageFilePath());
    if (!file.exists()) {
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        return false;
    }

    const QJsonArray pinned = doc.object().value(QStringLiteral("pinned")).toArray();
    for (const QJsonValue &value : pinned) {
        const QJsonObject object = value.toObject();
        FavoritePinnedEntry entry;
        entry.id = object.value(QStringLiteral("id")).toString();
        entry.targetPath = QDir::fromNativeSeparators(object.value(QStringLiteral("targetPath")).toString().trimmed());
        entry.label = object.value(QStringLiteral("label")).toString();
        entry.createdAt = object.value(QStringLiteral("createdAt")).toString();
        entry.lastUsedAt = object.value(QStringLiteral("lastUsedAt")).toString();
        entry.order = object.value(QStringLiteral("order")).toInt(m_pinnedEntries.size());
        const QJsonArray tags = object.value(QStringLiteral("tags")).toArray();
        QStringList tagList;
        for (const QJsonValue &tag : tags) {
            tagList.append(tag.toString());
        }
        entry.tags = normalizedTags(tagList);

        if (entry.id.isEmpty() || entry.targetPath.isEmpty()) {
            continue;
        }
        if (entry.label.isEmpty()) {
            entry.label = displayLabelForPath(entry.targetPath);
        }
        m_pinnedEntries.append(entry);
    }

    std::sort(m_pinnedEntries.begin(), m_pinnedEntries.end(), [](const auto &left, const auto &right) {
        return left.order < right.order;
    });

    const QJsonArray usage = doc.object().value(QStringLiteral("usage")).toArray();
    for (const QJsonValue &value : usage) {
        const QJsonObject object = value.toObject();
        FavoriteUsageEntry entry;
        entry.targetPath = QDir::fromNativeSeparators(object.value(QStringLiteral("targetPath")).toString().trimmed());
        entry.label = object.value(QStringLiteral("label")).toString();
        entry.lastVisitedAt = object.value(QStringLiteral("lastVisitedAt")).toString();
        entry.visitCount = object.value(QStringLiteral("visitCount")).toInt();

        if (entry.targetPath.isEmpty() || entry.visitCount <= 0) {
            continue;
        }
        if (entry.label.isEmpty()) {
            entry.label = displayLabelForPath(entry.targetPath);
        }
        entry.score = usageScore(entry.visitCount, entry.lastVisitedAt);
        m_usageEntries.append(entry);
    }

    sortUsageEntries(m_usageEntries);
    return true;
}

bool FavoritesStore::save() const
{
    const QString filePath = storageFilePath();
    QDir dir(QFileInfo(filePath).absolutePath());
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        return false;
    }

    QJsonArray pinned;
    for (const FavoritePinnedEntry &entry : m_pinnedEntries) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), entry.id);
        object.insert(QStringLiteral("targetPath"), entry.targetPath);
        object.insert(QStringLiteral("label"), entry.label);
        object.insert(QStringLiteral("createdAt"), entry.createdAt);
        object.insert(QStringLiteral("lastUsedAt"), entry.lastUsedAt);
        object.insert(QStringLiteral("order"), entry.order);
        QJsonArray tags;
        for (const QString &tag : entry.tags) {
            tags.append(tag);
        }
        object.insert(QStringLiteral("tags"), tags);
        pinned.append(object);
    }

    QJsonArray usage;
    for (const FavoriteUsageEntry &entry : m_usageEntries) {
        QJsonObject object;
        object.insert(QStringLiteral("targetPath"), entry.targetPath);
        object.insert(QStringLiteral("label"), entry.label);
        object.insert(QStringLiteral("lastVisitedAt"), entry.lastVisitedAt);
        object.insert(QStringLiteral("visitCount"), entry.visitCount);
        object.insert(QStringLiteral("score"), entry.score);
        usage.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("schemaVersion"), SchemaVersion);
    root.insert(QStringLiteral("pinned"), pinned);
    root.insert(QStringLiteral("usage"), usage);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    return true;
}

bool FavoritesStore::pinPath(const QString &path)
{
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    const QString key = normalizedPathKey(normalized);
    if (key.isEmpty() || isPinned(normalized)) {
        return false;
    }

    FavoritePinnedEntry entry;
    entry.id = nextId();
    entry.targetPath = normalized;
    entry.label = displayLabelForPath(normalized);
    entry.createdAt = nowIso();
    entry.lastUsedAt = entry.createdAt;
    entry.order = m_pinnedEntries.size();
    m_pinnedEntries.append(entry);
    if (!save()) {
        m_pinnedEntries.removeLast();
        return false;
    }
    return true;
}

bool FavoritesStore::unpinPath(const QString &path)
{
    const QString key = normalizedPathKey(path);
    for (qsizetype i = 0; i < m_pinnedEntries.size(); ++i) {
        if (normalizedPathKey(m_pinnedEntries.at(i).targetPath) == key) {
            const FavoritePinnedEntry removed = m_pinnedEntries.at(i);
            m_pinnedEntries.removeAt(i);
            updatePinnedOrder(m_pinnedEntries);
            if (!save()) {
                m_pinnedEntries.insert(i, removed);
                updatePinnedOrder(m_pinnedEntries);
                return false;
            }
            return true;
        }
    }
    return false;
}

bool FavoritesStore::movePinnedPath(const QString &path, int offset)
{
    if (offset == 0 || m_pinnedEntries.size() < 2) {
        return false;
    }

    const QString key = normalizedPathKey(path);
    if (key.isEmpty()) {
        return false;
    }

    for (qsizetype i = 0; i < m_pinnedEntries.size(); ++i) {
        if (normalizedPathKey(m_pinnedEntries.at(i).targetPath) != key) {
            continue;
        }

        const qsizetype targetIndex = std::clamp(i + qsizetype(offset), qsizetype(0), m_pinnedEntries.size() - 1);
        if (targetIndex == i) {
            return false;
        }

        const QList<FavoritePinnedEntry> previous = m_pinnedEntries;
        m_pinnedEntries.move(int(i), int(targetIndex));
        updatePinnedOrder(m_pinnedEntries);
        if (!save()) {
            m_pinnedEntries = previous;
            return false;
        }
        return true;
    }

    return false;
}

bool FavoritesStore::setPinnedLabel(const QString &path, const QString &label)
{
    const QString key = normalizedPathKey(path);
    if (key.isEmpty()) {
        return false;
    }

    for (FavoritePinnedEntry &entry : m_pinnedEntries) {
        if (normalizedPathKey(entry.targetPath) != key) {
            continue;
        }

        const QString newLabel = label.trimmed().isEmpty()
            ? displayLabelForPath(entry.targetPath)
            : label.trimmed().left(160);
        if (entry.label == newLabel) {
            return false;
        }

        const QString previous = entry.label;
        entry.label = newLabel;
        if (!save()) {
            entry.label = previous;
            return false;
        }
        return true;
    }

    return false;
}

bool FavoritesStore::setPinnedTags(const QString &path, const QStringList &tags)
{
    const QString key = normalizedPathKey(path);
    if (key.isEmpty()) {
        return false;
    }

    const QStringList newTags = normalizedTags(tags);
    for (FavoritePinnedEntry &entry : m_pinnedEntries) {
        if (normalizedPathKey(entry.targetPath) != key) {
            continue;
        }

        if (entry.tags == newTags) {
            return false;
        }

        const QStringList previous = entry.tags;
        entry.tags = newTags;
        if (!save()) {
            entry.tags = previous;
            return false;
        }
        return true;
    }

    return false;
}

QStringList FavoritesStore::tagsForPath(const QString &path) const
{
    const QString key = normalizedPathKey(path);
    if (key.isEmpty()) {
        return {};
    }

    for (const FavoritePinnedEntry &entry : m_pinnedEntries) {
        if (normalizedPathKey(entry.targetPath) == key) {
            return entry.tags;
        }
    }

    return {};
}

bool FavoritesStore::isPinned(const QString &path) const
{
    const QString key = normalizedPathKey(path);
    if (key.isEmpty()) {
        return false;
    }
    for (const FavoritePinnedEntry &entry : m_pinnedEntries) {
        if (normalizedPathKey(entry.targetPath) == key) {
            return true;
        }
    }
    return false;
}

bool FavoritesStore::recordVisit(const QString &path)
{
    const QString normalized = QDir::fromNativeSeparators(path.trimmed());
    const QString key = normalizedPathKey(normalized);
    if (key.isEmpty()) {
        return false;
    }

    const QString visitedAt = nowIso();
    for (FavoriteUsageEntry &entry : m_usageEntries) {
        if (normalizedPathKey(entry.targetPath) == key) {
            ++entry.visitCount;
            entry.lastVisitedAt = visitedAt;
            entry.score = usageScore(entry.visitCount, entry.lastVisitedAt);
            sortUsageEntries(m_usageEntries);
            save();
            return true;
        }
    }

    FavoriteUsageEntry entry;
    entry.targetPath = normalized;
    entry.label = displayLabelForPath(normalized);
    entry.lastVisitedAt = visitedAt;
    entry.visitCount = 1;
    entry.score = usageScore(entry.visitCount, entry.lastVisitedAt);
    m_usageEntries.prepend(entry);
    sortUsageEntries(m_usageEntries);
    while (m_usageEntries.size() > MaxUsageEntries) {
        m_usageEntries.removeLast();
    }
    save();
    return true;
}

bool FavoritesStore::forgetUsagePath(const QString &path)
{
    const QString key = normalizedPathKey(path);
    if (key.isEmpty()) {
        return false;
    }
    for (qsizetype i = 0; i < m_usageEntries.size(); ++i) {
        if (normalizedPathKey(m_usageEntries.at(i).targetPath) == key) {
            const FavoriteUsageEntry removed = m_usageEntries.at(i);
            m_usageEntries.removeAt(i);
            if (!save()) {
                m_usageEntries.insert(i, removed);
                return false;
            }
            return true;
        }
    }
    return false;
}

bool FavoritesStore::clearUsage()
{
    if (m_usageEntries.isEmpty()) {
        return false;
    }

    const QList<FavoriteUsageEntry> removed = m_usageEntries;
    m_usageEntries.clear();
    if (!save()) {
        m_usageEntries = removed;
        return false;
    }
    return true;
}

QString FavoritesStore::nextId() const
{
    return QStringLiteral("pin-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}
