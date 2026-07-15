#include "AudioTagEditModel.h"

#include <QHash>

namespace {
const QStringList &editableFields()
{
    static const QStringList fields = {QStringLiteral("title"), QStringLiteral("artist"),
        QStringLiteral("album"), QStringLiteral("year"), QStringLiteral("track"),
        QStringLiteral("genre"), QStringLiteral("comment"), QStringLiteral("lyrics")};
    return fields;
}
}

AudioTagEditModel::AudioTagEditModel(QObject *parent) : QAbstractListModel(parent) {}
int AudioTagEditModel::rowCount(const QModelIndex &parent) const { return parent.isValid() ? 0 : m_rows.size(); }

QVariant AudioTagEditModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) return {};
    const Row &row = m_rows.at(index.row());
    switch (role) {
    case RecordRole: return record(index.row());
    case PathRole: return row.current.value(QStringLiteral("path"));
    case NameRole: return row.current.value(QStringLiteral("name"));
    case OkRole: return row.current.value(QStringLiteral("ok"));
    case DirtyRole: return isDirty(row);
    case ErrorRole: return row.current.value(QStringLiteral("error"));
    default: return {};
    }
}

QHash<int, QByteArray> AudioTagEditModel::roleNames() const
{
    return {{RecordRole, "record"}, {PathRole, "path"}, {NameRole, "name"},
            {OkRole, "ok"}, {DirtyRole, "dirty"}, {ErrorRole, "error"}};
}

void AudioTagEditModel::setRecords(const QVariantList &records)
{
    beginResetModel();
    m_rows.clear();
    m_rows.reserve(records.size());
    for (const QVariant &value : records) {
        QVariantMap item = value.toMap();
        item.insert(QStringLiteral("dirty"), false);
        item.insert(QStringLiteral("coverDirty"), false);
        item.insert(QStringLiteral("clearAllTags"), false);
        item.insert(QStringLiteral("removeCover"), false);
        m_rows.append({item, item});
    }
    endResetModel();
    emit countChanged();
    emit dirtyCountChanged();
}

QVariantMap AudioTagEditModel::record(int index) const
{
    if (index < 0 || index >= m_rows.size()) return {};
    QVariantMap result = m_rows.at(index).current;
    result.insert(QStringLiteral("dirty"), isDirty(m_rows.at(index)));
    return result;
}

bool AudioTagEditModel::updateField(int index, const QString &field, const QVariant &value)
{
    if (index < 0 || index >= m_rows.size() || !editableFields().contains(field)) return false;
    const int before = dirtyCount();
    Row &row = m_rows[index];
    row.current.insert(field, value);
    row.current.insert(QStringLiteral("error"), QString());
    emitRowChanged(index, before);
    return true;
}

bool AudioTagEditModel::setCover(int index, const QString &coverPath, const QString &previewSource, bool removeCover)
{
    if (index < 0 || index >= m_rows.size()) return false;
    Row &row = m_rows[index];
    if (!row.current.value(QStringLiteral("coverWriteSupported")).toBool()) return false;
    const int before = dirtyCount();
    row.current.insert(QStringLiteral("coverDirty"), true);
    row.current.insert(QStringLiteral("removeCover"), removeCover);
    row.current.insert(QStringLiteral("coverImagePath"), removeCover ? QString() : coverPath);
    row.current.insert(QStringLiteral("pendingCoverSource"), removeCover ? QString() : previewSource);
    row.current.insert(QStringLiteral("error"), QString());
    emitRowChanged(index, before);
    return true;
}

bool AudioTagEditModel::clearTags(int index)
{
    if (index < 0 || index >= m_rows.size()) return false;
    const int before = dirtyCount();
    Row &row = m_rows[index];
    for (const QString &field : editableFields()) row.current.insert(field, QString());
    row.current.insert(QStringLiteral("clearAllTags"), true);
    if (row.current.value(QStringLiteral("coverWriteSupported")).toBool()) {
        row.current.insert(QStringLiteral("coverDirty"), true);
        row.current.insert(QStringLiteral("removeCover"), true);
        row.current.insert(QStringLiteral("coverImagePath"), QString());
        row.current.insert(QStringLiteral("pendingCoverSource"), QString());
    }
    row.current.insert(QStringLiteral("error"), QString());
    emitRowChanged(index, before);
    return true;
}

int AudioTagEditModel::applyCoverToAll(int sourceIndex)
{
    const QVariantMap source = record(sourceIndex);
    if (!source.value(QStringLiteral("coverDirty")).toBool()
        || source.value(QStringLiteral("removeCover")).toBool()) return 0;
    const QString path = source.value(QStringLiteral("coverImagePath")).toString();
    const QString preview = source.value(QStringLiteral("pendingCoverSource")).toString();
    if (path.isEmpty() || preview.isEmpty()) return 0;
    const int before = dirtyCount();
    int changed = 0;
    for (int i = 0; i < m_rows.size(); ++i) {
        Row &row = m_rows[i];
        if (!row.current.value(QStringLiteral("coverWriteSupported")).toBool()) continue;
        row.current.insert(QStringLiteral("coverDirty"), true);
        row.current.insert(QStringLiteral("removeCover"), false);
        row.current.insert(QStringLiteral("coverImagePath"), path);
        row.current.insert(QStringLiteral("pendingCoverSource"), preview);
        row.current.insert(QStringLiteral("error"), QString());
        emit dataChanged(index(i), index(i));
        emit recordChanged(i);
        ++changed;
    }
    if (before != dirtyCount()) emit dirtyCountChanged();
    return changed;
}

int AudioTagEditModel::applyLookupFields(int index, const QVariantMap &fields)
{
    if (index < 0 || index >= m_rows.size()) return 0;
    static const QStringList keys = {QStringLiteral("title"), QStringLiteral("artist"),
        QStringLiteral("album"), QStringLiteral("year"), QStringLiteral("track"), QStringLiteral("genre")};
    const int before = dirtyCount();
    int count = 0;
    for (const QString &key : keys) {
        const QString value = fields.value(key).toString();
        if (!value.isEmpty()) { m_rows[index].current.insert(key, value); ++count; }
    }
    if (count > 0) {
        m_rows[index].current.insert(QStringLiteral("error"), QString());
        emitRowChanged(index, before);
    }
    return count;
}

QVariantList AudioTagEditModel::dirtyRecords(int onlyIndex) const
{
    QVariantList result;
    for (int i = 0; i < m_rows.size(); ++i) {
        if ((onlyIndex < 0 || onlyIndex == i) && isDirty(m_rows.at(i))) {
            QVariantMap item = m_rows.at(i).current;
            item.insert(QStringLiteral("dirty"), true);
            result.append(item);
        }
    }
    return result;
}

void AudioTagEditModel::reconcileApplyResults(const QVariantList &results)
{
    QHash<QString, QVariantMap> byPath;
    for (const QVariant &value : results) {
        const QVariantMap status = value.toMap();
        byPath.insert(status.value(QStringLiteral("path")).toString(), status);
    }
    const int before = dirtyCount();
    for (int i = 0; i < m_rows.size(); ++i) {
        Row &row = m_rows[i];
        const QVariantMap status = byPath.value(row.current.value(QStringLiteral("path")).toString());
        if (status.isEmpty()) continue;
        if (status.value(QStringLiteral("ok")).toBool()) {
            row.current.insert(QStringLiteral("coverDirty"), false);
            row.current.insert(QStringLiteral("clearAllTags"), false);
            row.current.insert(QStringLiteral("removeCover"), false);
            row.current.insert(QStringLiteral("coverImagePath"), QString());
            row.current.insert(QStringLiteral("error"), QString());
            row.original = row.current;
        } else {
            row.current.insert(QStringLiteral("error"), status.value(QStringLiteral("message"), QStringLiteral("Save failed.")).toString());
        }
        emit dataChanged(index(i), index(i));
        emit recordChanged(i);
    }
    if (before != dirtyCount()) emit dirtyCountChanged();
}

bool AudioTagEditModel::hasPendingCover() const
{
    for (const Row &row : m_rows) {
        if (row.current.value(QStringLiteral("coverDirty")).toBool()
            && !row.current.value(QStringLiteral("coverImagePath")).toString().isEmpty()) return true;
    }
    return false;
}

int AudioTagEditModel::dirtyCount() const
{
    int count = 0;
    for (const Row &row : m_rows) count += isDirty(row) ? 1 : 0;
    return count;
}

bool AudioTagEditModel::isDirty(const Row &row)
{
    if (row.current.value(QStringLiteral("coverDirty")).toBool()
        || row.current.value(QStringLiteral("clearAllTags")).toBool()) return true;
    for (const QString &field : editableFields()) {
        if (row.current.value(field) != row.original.value(field)) return true;
    }
    return false;
}

void AudioTagEditModel::emitRowChanged(int indexValue, int previousDirtyCount)
{
    m_rows[indexValue].current.insert(QStringLiteral("dirty"), isDirty(m_rows.at(indexValue)));
    emit dataChanged(index(indexValue), index(indexValue));
    emit recordChanged(indexValue);
    if (previousDirtyCount != dirtyCount()) emit dirtyCountChanged();
}
