#include "ProviderPropertiesController.h"

#include "../core/DriveUtils.h"
#include "../core/FileProviderFactory.h"
#include "../core/ProviderFolderSizeCalculator.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QStringList>

#include <memory>
#include <optional>
#include <utility>

namespace {

QString providerDisplayName(const QString &scheme)
{
    if (scheme == QLatin1String("gdrive")) {
        return QStringLiteral("Google Drive");
    }
    if (scheme == QLatin1String("mega")) {
        return QStringLiteral("MEGA");
    }
    if (scheme == QLatin1String("ftp")) {
        return QStringLiteral("FTP");
    }
    if (scheme == QLatin1String("portable")) {
        return QStringLiteral("Portable Device");
    }
    return scheme.isEmpty() ? QStringLiteral("Provider") : scheme.toUpper();
}

QString explicitScheme(const QString &path)
{
    const QString trimmed = path.trimmed();
    const int separator = trimmed.indexOf(QStringLiteral("://"));
    if (separator <= 0) {
        return {};
    }

    const QString scheme = trimmed.left(separator).toLower();
    static const QRegularExpression schemePattern(QStringLiteral("^[a-z][a-z0-9+.-]*$"));
    return schemePattern.match(scheme).hasMatch() ? scheme : QString{};
}

bool isProviderScheme(const QString &scheme)
{
    return !scheme.isEmpty()
        && scheme != QLatin1String("file")
        && scheme != QLatin1String("archive")
        && scheme != QLatin1String("devices")
        && scheme != QLatin1String("favorites");
}

QVariantMap makeRow(const QString &key, const QString &label, const QString &value, bool copyable = true)
{
    QVariantMap row;
    row.insert(QStringLiteral("key"), key);
    row.insert(QStringLiteral("label"), label);
    row.insert(QStringLiteral("value"), value);
    row.insert(QStringLiteral("copyable"), copyable);
    return row;
}

QVariantMap makeGroup(const QString &key, const QString &title, const QVariantList &rows)
{
    QVariantMap group;
    group.insert(QStringLiteral("key"), key);
    group.insert(QStringLiteral("title"), title);
    group.insert(QStringLiteral("rows"), rows);
    return group;
}

void appendGroup(QVariantList &groups, const QString &key, const QString &title, const QVariantList &rows)
{
    if (!rows.isEmpty()) {
        groups.append(makeGroup(key, title, rows));
    }
}

QString typeTextForEntry(const FileEntry &entry)
{
    if (entry.isShortcut) {
        return entry.shortcutTargetIsDirectory ? QStringLiteral("Folder shortcut") : QStringLiteral("File shortcut");
    }
    if (entry.isDirectory) {
        return QStringLiteral("Folder");
    }
    if (!entry.mimeType.isEmpty()) {
        return entry.mimeType;
    }
    if (!entry.suffix.isEmpty()) {
        return entry.suffix.toUpper() + QStringLiteral(" file");
    }
    return QStringLiteral("File");
}

QString providerSpecificTypeText(const QString &scheme, const QString &path, const FileEntry &entry)
{
    const QString normalized = path.toLower();
    if (scheme == QLatin1String("gdrive")) {
        if (normalized == QLatin1String("gdrive://")) {
            return QStringLiteral("Google Drive root");
        }
        if (normalized == QLatin1String("gdrive://my-drive")) {
            return QStringLiteral("My Drive");
        }
        if (normalized == QLatin1String("gdrive://shared-with-me")) {
            return QStringLiteral("Shared with me");
        }
        if (normalized == QLatin1String("gdrive://shortcuts")) {
            return QStringLiteral("Shortcuts");
        }
        if (normalized == QLatin1String("gdrive://trash")) {
            return QStringLiteral("Trash");
        }
    } else if (scheme == QLatin1String("mega")) {
        if (normalized == QLatin1String("mega://") || normalized == QLatin1String("mega:///")) {
            return QStringLiteral("MEGA root");
        }
        if (normalized == QLatin1String("mega://cloud drive") || normalized == QLatin1String("mega:///cloud drive")) {
            return QStringLiteral("MEGA Cloud Drive");
        }
        if (normalized.startsWith(QLatin1String("mega://link/"))) {
            return entry.isDirectory ? QStringLiteral("MEGA public folder") : QStringLiteral("MEGA public file");
        }
    }
    return typeTextForEntry(entry);
}

QString dateText(const QString &providerText, const QDateTime &date)
{
    if (!providerText.trimmed().isEmpty()) {
        return providerText;
    }
    return date.isValid() ? QLocale().toString(date, QLocale::ShortFormat) : QString{};
}

QString quotaValue(const QVariantMap &quota, const QString &textKey, const QString &numberKey)
{
    const QString text = quota.value(textKey).toString();
    if (!text.isEmpty()) {
        return text;
    }
    const qint64 bytes = quota.value(numberKey, -1).toLongLong();
    return bytes >= 0 ? DriveUtils::formatSize(bytes) : QString{};
}

} // namespace

ProviderPropertiesController::ProviderPropertiesController(QObject *parent)
    : QObject(parent)
{
    m_threadPool.setMaxThreadCount(1);
}

bool ProviderPropertiesController::visible() const { return m_visible; }
bool ProviderPropertiesController::loading() const { return m_loading; }
bool ProviderPropertiesController::calculatingSize() const { return m_calculatingSize; }
bool ProviderPropertiesController::sizeExact() const { return m_sizeExact; }
QString ProviderPropertiesController::providerName() const { return m_providerName; }
QString ProviderPropertiesController::path() const { return m_path; }
QString ProviderPropertiesController::name() const { return m_name; }
QString ProviderPropertiesController::typeText() const { return m_typeText; }
QString ProviderPropertiesController::sizeText() const { return m_sizeText; }
QString ProviderPropertiesController::itemCountText() const { return m_itemCountText; }
QString ProviderPropertiesController::modifiedText() const { return m_modifiedText; }
QString ProviderPropertiesController::createdText() const { return m_createdText; }
QString ProviderPropertiesController::statusText() const { return m_statusText; }
QVariantList ProviderPropertiesController::propertyGroups() const { return m_propertyGroups; }
QVariantList ProviderPropertiesController::quotaProperties() const { return m_quotaProperties; }
QString ProviderPropertiesController::errorText() const { return m_errorText; }

void ProviderPropertiesController::setVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    if (!m_visible) {
        cancel();
    }
    emit visibleChanged();
}

void ProviderPropertiesController::reset()
{
    cancelCalculation();
    m_loading = false;
    m_calculatingSize = false;
    m_sizeExact = false;
    m_fileCount = 0;
    m_folderCount = 0;
    m_providerName.clear();
    m_path.clear();
    m_name.clear();
    m_typeText.clear();
    m_sizeText.clear();
    m_itemCountText.clear();
    m_modifiedText.clear();
    m_createdText.clear();
    m_statusText.clear();
    m_errorText.clear();
    m_propertyGroups.clear();
    m_detailGroups.clear();
    m_quotaProperties.clear();
}

void ProviderPropertiesController::load(const QString &path)
{
    ++m_generation;
    reset();
    m_loading = true;
    emit stateChanged();

    const QString scheme = explicitScheme(path);
    if (!isProviderScheme(scheme)) {
        m_loading = false;
        m_errorText = QStringLiteral("Provider properties are available for provider paths only.");
        m_statusText = QStringLiteral("Unavailable");
        emit propertiesChanged();
        emit stateChanged();
        return;
    }

    std::unique_ptr<FileProvider> provider = FileProviderFactory::createProvider(path);
    if (!provider || provider->scheme() == QLatin1String("file")) {
        m_loading = false;
        m_providerName = providerDisplayName(scheme);
        m_errorText = QStringLiteral("Provider unavailable.");
        m_statusText = QStringLiteral("Unavailable");
        emit propertiesChanged();
        emit stateChanged();
        return;
    }

    m_providerName = providerDisplayName(provider->scheme());
    m_path = provider->normalizedPath(path);
    const std::optional<FileEntry> entry = provider->entryInfo(m_path);
    if (!entry) {
        m_loading = false;
        m_errorText = provider->lastErrorString().isEmpty()
            ? QStringLiteral("Item metadata is unavailable. Refresh the folder and try again.")
            : provider->lastErrorString();
        m_statusText = QStringLiteral("Unavailable");
        rebuildPropertyGroups();
        emit propertiesChanged();
        emit stateChanged();
        return;
    }

    m_name = entry->name.isEmpty() ? provider->fileName(m_path) : entry->name;
    if (m_name.isEmpty()) {
        m_name = m_providerName;
    }
    m_typeText = providerSpecificTypeText(provider->scheme(), m_path, *entry);
    m_modifiedText = dateText(entry->modifiedText, entry->modified);
    m_createdText = dateText(entry->createdText, entry->created);
    const bool folderShortcutTarget = entry->isShortcut && entry->shortcutTargetIsDirectory;
    const bool shouldCalculateSize = entry->isDirectory && !folderShortcutTarget;
    if (shouldCalculateSize) {
        m_sizeText = QStringLiteral("Calculating...");
    } else if (folderShortcutTarget) {
        m_sizeText = QStringLiteral("Target folder not counted");
    } else {
        m_sizeText = !entry->sizeText.isEmpty() ? entry->sizeText : DriveUtils::formatSize(entry->size);
    }
    m_itemCountText = shouldCalculateSize ? QStringLiteral("Calculating...") : QString{};
    if (shouldCalculateSize) {
        m_statusText = QStringLiteral("Calculating folder size...");
    } else if (folderShortcutTarget) {
        m_statusText = QStringLiteral("Folder shortcut target metadata loaded; target contents are not counted automatically");
    } else {
        m_statusText = QStringLiteral("Provider metadata loaded");
    }

    const QVariantMap quota = provider->storageInfo(m_path);
    if (quota.value(QStringLiteral("valid"), false).toBool()) {
        const QString used = quotaValue(quota, QStringLiteral("usedStr"), QStringLiteral("used"));
        const QString free = quotaValue(quota, QStringLiteral("freeStr"), QStringLiteral("free"));
        const QString total = quotaValue(quota, QStringLiteral("totalStr"), QStringLiteral("total"));
        if (!used.isEmpty()) {
            m_quotaProperties.append(makeRow(QStringLiteral("quota.used"), QStringLiteral("Used"), used, false));
        }
        if (!free.isEmpty()) {
            m_quotaProperties.append(makeRow(QStringLiteral("quota.free"), QStringLiteral("Free"), free, false));
        }
        if (!total.isEmpty()) {
            m_quotaProperties.append(makeRow(QStringLiteral("quota.total"), QStringLiteral("Total"), total, false));
        }
    }

    QVariantList detailsRows;
    if (!entry->mimeType.isEmpty()) {
        detailsRows.append(makeRow(QStringLiteral("details.mimeType"), QStringLiteral("MIME Type"), entry->mimeType));
    }
    if (!entry->suffix.isEmpty()) {
        detailsRows.append(makeRow(QStringLiteral("details.extension"), QStringLiteral("Extension"), entry->suffix));
    }
    if (entry->isShortcut) {
        detailsRows.append(makeRow(QStringLiteral("details.shortcut"), QStringLiteral("Shortcut"), QStringLiteral("Yes"), false));
        if (!entry->shortcutTargetMimeType.isEmpty()) {
            detailsRows.append(makeRow(QStringLiteral("details.shortcutTargetType"), QStringLiteral("Target MIME Type"), entry->shortcutTargetMimeType, false));
        }
        if (!entry->shortcutTargetPath.isEmpty()) {
            detailsRows.append(makeRow(QStringLiteral("details.shortcutTarget"), QStringLiteral("Target"), entry->shortcutTargetPath));
        }
        if (!entry->shortcutTargetResourceKey.isEmpty()) {
            detailsRows.append(makeRow(QStringLiteral("details.shortcutResourceKey"), QStringLiteral("Target Resource Key"), entry->shortcutTargetResourceKey));
        }
    }
    if (entry->isReadOnly) {
        detailsRows.append(makeRow(QStringLiteral("details.access"), QStringLiteral("Access"), QStringLiteral("Read-only"), false));
    }
    if (!entry->providerCapabilitiesText.isEmpty()) {
        detailsRows.append(makeRow(QStringLiteral("details.capabilities"), QStringLiteral("Capabilities"), entry->providerCapabilitiesText, false));
    }

    appendGroup(m_detailGroups, QStringLiteral("details"), QStringLiteral("Provider Details"), detailsRows);
    rebuildPropertyGroups();
    m_loading = false;
    if (shouldCalculateSize) {
        startFolderSizeCalculation();
    }
    emit propertiesChanged();
    emit stateChanged();
}

void ProviderPropertiesController::refresh()
{
    const QString currentPath = m_path;
    if (!currentPath.isEmpty()) {
        load(currentPath);
    }
}

void ProviderPropertiesController::cancel()
{
    ++m_generation;
    cancelCalculation();
    if (m_loading || m_calculatingSize) {
        m_loading = false;
        m_calculatingSize = false;
        emit stateChanged();
    }
}

void ProviderPropertiesController::cancelCalculation()
{
    if (m_currentCalculator) {
        m_currentCalculator->cancel();
        m_currentCalculator = nullptr;
    }
}

void ProviderPropertiesController::startFolderSizeCalculation()
{
    cancelCalculation();
    m_calculatingSize = true;
    m_sizeExact = false;
    const int generation = m_generation;
    m_currentCalculator = new ProviderFolderSizeCalculator(m_path, generation);
    connect(m_currentCalculator, &ProviderFolderSizeCalculator::progressUpdate,
            this, &ProviderPropertiesController::onSizeProgress);
    connect(m_currentCalculator, &ProviderFolderSizeCalculator::resultReady,
            this, &ProviderPropertiesController::onSizeCalculated);
    m_threadPool.start(m_currentCalculator);
    emit stateChanged();
}

void ProviderPropertiesController::onSizeProgress(qint64 bytes, int files, int folders, bool exact, int generation)
{
    if (generation != m_generation) {
        return;
    }

    m_sizeExact = exact;
    m_fileCount = files;
    m_folderCount = folders;
    m_sizeText = DriveUtils::formatSize(bytes);
    m_itemCountText = QStringLiteral("%1 files, %2 folders").arg(files).arg(folders);
    m_statusText = exact ? QStringLiteral("Calculating folder size...") : QStringLiteral("Calculating with partial metadata...");
    rebuildPropertyGroups();
    emit propertiesChanged();
    emit stateChanged();
}

void ProviderPropertiesController::onSizeCalculated(qint64 bytes,
                                                   int files,
                                                   int folders,
                                                   bool exact,
                                                   bool cancelled,
                                                   const QString &error,
                                                   int generation)
{
    auto *calculator = qobject_cast<ProviderFolderSizeCalculator *>(sender());

    if (generation == m_generation) {
        m_sizeExact = exact && !cancelled;
        m_fileCount = files;
        m_folderCount = folders;
        m_sizeText = DriveUtils::formatSize(bytes);
        m_itemCountText = QStringLiteral("%1 files, %2 folders").arg(files).arg(folders);
        m_calculatingSize = false;
        if (cancelled) {
            m_statusText = QStringLiteral("Cancelled");
        } else if (!error.isEmpty()) {
            m_statusText = exact ? QStringLiteral("Exact size calculated") : QStringLiteral("Partial result: %1").arg(error);
        } else {
            m_statusText = exact ? QStringLiteral("Exact size calculated") : QStringLiteral("Partial result");
        }
        rebuildPropertyGroups();
        emit propertiesChanged();
        emit stateChanged();
    }

    if (calculator) {
        if (m_currentCalculator == calculator) {
            m_currentCalculator = nullptr;
        }
        calculator->deleteLater();
    }
}

QString ProviderPropertiesController::exportableText() const
{
    QStringList lines;
    for (const QVariant &groupValue : m_propertyGroups) {
        const QVariantMap group = groupValue.toMap();
        const QString title = group.value(QStringLiteral("title")).toString();
        if (!title.isEmpty()) {
            if (!lines.isEmpty()) {
                lines << QString();
            }
            lines << title;
        }
        const QVariantList rows = group.value(QStringLiteral("rows")).toList();
        for (const QVariant &rowValue : rows) {
            const QVariantMap row = rowValue.toMap();
            const QString label = row.value(QStringLiteral("label")).toString();
            const QString value = row.value(QStringLiteral("value")).toString();
            if (!label.isEmpty() || !value.isEmpty()) {
                lines << QStringLiteral("%1: %2").arg(label, value);
            }
        }
    }
    if (!m_statusText.isEmpty()) {
        if (!lines.isEmpty()) {
            lines << QString();
        }
        lines << QStringLiteral("Status: %1").arg(m_statusText);
    }
    return lines.join(QLatin1Char('\n'));
}

QString ProviderPropertiesController::exportableJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("name"), m_name);
    root.insert(QStringLiteral("provider"), m_providerName);
    root.insert(QStringLiteral("path"), m_path);
    root.insert(QStringLiteral("type"), m_typeText);
    root.insert(QStringLiteral("size"), m_sizeText);
    root.insert(QStringLiteral("items"), m_itemCountText);
    root.insert(QStringLiteral("modified"), m_modifiedText);
    root.insert(QStringLiteral("created"), m_createdText);
    root.insert(QStringLiteral("status"), m_statusText);
    root.insert(QStringLiteral("exact"), m_sizeExact);
    root.insert(QStringLiteral("calculatingSize"), m_calculatingSize);
    root.insert(QStringLiteral("propertyGroups"), QJsonArray::fromVariantList(m_propertyGroups));
    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

void ProviderPropertiesController::rebuildPropertyGroups()
{
    QVariantList groups;

    QVariantList generalRows;
    if (!m_name.isEmpty()) {
        generalRows.append(makeRow(QStringLiteral("general.name"), QStringLiteral("Name"), m_name));
    }
    if (!m_providerName.isEmpty()) {
        generalRows.append(makeRow(QStringLiteral("general.provider"), QStringLiteral("Provider"), m_providerName, false));
    }
    if (!m_path.isEmpty()) {
        generalRows.append(makeRow(QStringLiteral("general.path"), QStringLiteral("Provider Path"), m_path));
    }
    if (!m_typeText.isEmpty()) {
        generalRows.append(makeRow(QStringLiteral("general.type"), QStringLiteral("Type"), m_typeText, false));
    }
    if (!m_sizeText.isEmpty()) {
        generalRows.append(makeRow(QStringLiteral("general.size"), QStringLiteral("Size"), m_sizeText, false));
    }
    if (!m_itemCountText.isEmpty()) {
        generalRows.append(makeRow(QStringLiteral("general.items"), QStringLiteral("Items"), m_itemCountText, false));
    }
    if (!m_modifiedText.isEmpty()) {
        generalRows.append(makeRow(QStringLiteral("general.modified"), QStringLiteral("Modified"), m_modifiedText, false));
    }
    if (!m_createdText.isEmpty()) {
        generalRows.append(makeRow(QStringLiteral("general.created"), QStringLiteral("Created"), m_createdText, false));
    }
    appendGroup(groups, QStringLiteral("general"), QStringLiteral("General"), generalRows);

    for (const QVariant &group : std::as_const(m_detailGroups)) {
        groups.append(group);
    }
    appendGroup(groups, QStringLiteral("quota"), QStringLiteral("Storage"), m_quotaProperties);

    m_propertyGroups = groups;
}
