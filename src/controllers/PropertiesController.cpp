#include "PropertiesController.h"
#include "../core/FolderSizeCalculator.h"
#include "../core/DriveUtils.h"
#include "../core/FileAccessResolver.h"
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QLocale>
#include <QMimeDatabase>
#include <QImageReader>
#include <QStorageInfo>
#include <QSet>
#include <QProcess>
#include "../core/MetadataExtractor.h"
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>
#include <QMetaObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <QRegularExpression>

namespace {

QVariantMap makePropertyRow(const QString &key,
                            const QString &label,
                            const QString &value,
                            const QString &category,
                            bool copyable = true,
                            bool emphasize = false,
                            QVariantMap metadata = {})
{
    QVariantMap row;
    row.insert(QStringLiteral("key"), key);
    row.insert(QStringLiteral("label"), label);
    row.insert(QStringLiteral("value"), value);
    row.insert(QStringLiteral("category"), category);
    row.insert(QStringLiteral("copyable"), copyable);
    row.insert(QStringLiteral("emphasize"), emphasize);

    for (auto it = metadata.cbegin(); it != metadata.cend(); ++it) {
        row.insert(it.key(), it.value());
    }

    return row;
}

QVariantMap makePropertyGroup(const QString &key,
                              const QString &title,
                              const QString &category,
                              const QVariantList &rows)
{
    QVariantMap group;
    group.insert(QStringLiteral("key"), key);
    group.insert(QStringLiteral("title"), title);
    group.insert(QStringLiteral("category"), category);
    group.insert(QStringLiteral("rows"), rows);
    return group;
}

void appendGroup(QVariantList &groups,
                 const QString &key,
                 const QString &title,
                 const QString &category,
                 const QVariantList &rows)
{
    if (!rows.isEmpty()) {
        groups.append(makePropertyGroup(key, title, category, rows));
    }
}

QString stableKey(const QString &prefix, const QString &label)
{
    QString key = label.toLower();
    key.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("_"));
    key = key.trimmed();
    while (key.startsWith(QLatin1Char('_'))) {
        key.remove(0, 1);
    }
    while (key.endsWith(QLatin1Char('_'))) {
        key.chop(1);
    }
    return key.isEmpty() ? prefix : prefix + QLatin1Char('.') + key;
}

} // namespace

PropertiesController::PropertiesController(QObject *parent)
    : QObject(parent)
{
    m_threadPool.setMaxThreadCount(1);
}

QString PropertiesController::name() const { return m_name; }
QString PropertiesController::path() const { return m_path; }
QString PropertiesController::sizeText() const { return m_sizeText; }
QString PropertiesController::typeText() const { return m_typeText; }
QString PropertiesController::created() const { return m_created; }
QString PropertiesController::modified() const { return m_modified; }
QString PropertiesController::accessed() const { return m_accessed; }
bool PropertiesController::isDirectory() const { return m_isDirectory; }
bool PropertiesController::isDrive() const { return m_isDrive; }
QString PropertiesController::driveRootPath() const { return m_driveRootPath; }
QString PropertiesController::driveFileSystem() const { return m_driveFileSystem; }
QString PropertiesController::driveType() const { return m_driveType; }
QString PropertiesController::driveUsedText() const { return m_driveUsedText; }
QString PropertiesController::driveFreeText() const { return m_driveFreeText; }
QString PropertiesController::driveTotalText() const { return m_driveTotalText; }
double PropertiesController::driveUsagePercent() const { return m_driveUsagePercent; }
bool PropertiesController::driveReady() const { return m_driveReady; }
bool PropertiesController::driveCritical() const { return m_driveCritical; }
bool PropertiesController::isCalculating() const { return m_isCalculating; }
bool PropertiesController::visible() const { return m_visible; }
QVariantList PropertiesController::extraProperties() const { return m_extraProperties; }
QVariantList PropertiesController::accessProperties() const { return m_accessProperties; }
QVariantList PropertiesController::attributeProperties() const { return m_attributeProperties; }
bool PropertiesController::canEditAttributes() const { return m_canEditAttributes; }
bool PropertiesController::hiddenAttribute() const { return m_hiddenAttribute; }
bool PropertiesController::readOnlyAttribute() const { return m_readOnlyAttribute; }
int PropertiesController::fileCount() const { return m_fileCount; }
int PropertiesController::folderCount() const { return m_folderCount; }
int PropertiesController::selectedCount() const { return m_selectedCount; }
QStringList PropertiesController::selectedPaths() const { return m_selectedPaths; }

QVariantList PropertiesController::propertyGroups() const
{
    return m_propertyGroups;
}

void PropertiesController::rebuildPropertyGroups()
{
    QVariantList groups;
    if (m_selectedCount == 0) {
        m_propertyGroups.clear();
        return;
    }

    if (m_selectedCount == 1) {
        QVariantList generalRows;
        if (m_isDrive) {
            generalRows.append(makePropertyRow(QStringLiteral("general.name"), QStringLiteral("Name"), m_name, QStringLiteral("general"), true, true));
            generalRows.append(makePropertyRow(QStringLiteral("general.root"), QStringLiteral("Root"), m_driveRootPath, QStringLiteral("general")));
            generalRows.append(makePropertyRow(QStringLiteral("general.type"), QStringLiteral("Type"), m_typeText, QStringLiteral("general"), false));
            if (!m_driveFileSystem.isEmpty()) {
                generalRows.append(makePropertyRow(QStringLiteral("general.fileSystem"), QStringLiteral("File System"), m_driveFileSystem, QStringLiteral("general"), false));
            }
            generalRows.append(makePropertyRow(QStringLiteral("general.usedSpace"), QStringLiteral("Used Space"), m_driveUsedText, QStringLiteral("general"), false));
            generalRows.append(makePropertyRow(QStringLiteral("general.freeSpace"), QStringLiteral("Free Space"), m_driveFreeText, QStringLiteral("general"), false));
            generalRows.append(makePropertyRow(QStringLiteral("general.totalSpace"), QStringLiteral("Total Space"), m_driveTotalText, QStringLiteral("general"), false));
            generalRows.append(makePropertyRow(QStringLiteral("general.ready"), QStringLiteral("Ready"), m_driveReady ? QStringLiteral("Yes") : QStringLiteral("No"), QStringLiteral("general"), false, false,
                                               QVariantMap{{QStringLiteral("status"), m_driveReady ? QStringLiteral("ok") : QStringLiteral("warning")}}));
            if (m_driveCritical) {
                generalRows.append(makePropertyRow(QStringLiteral("general.spaceStatus"), QStringLiteral("Space Status"), QStringLiteral("Low free space"), QStringLiteral("general"), false, true,
                                                   QVariantMap{{QStringLiteral("status"), QStringLiteral("warning")}}));
            }
        } else {
            generalRows.append(makePropertyRow(QStringLiteral("general.name"), QStringLiteral("Name"), m_name, QStringLiteral("general"), true, true));
            generalRows.append(makePropertyRow(QStringLiteral("general.location"), QStringLiteral("Location"), QDir::toNativeSeparators(QFileInfo(m_path).absolutePath()), QStringLiteral("general")));
            generalRows.append(makePropertyRow(QStringLiteral("general.fullPath"), QStringLiteral("Full Path"), QDir::toNativeSeparators(m_path), QStringLiteral("general")));
            generalRows.append(makePropertyRow(QStringLiteral("general.type"), QStringLiteral("Type"), m_typeText, QStringLiteral("general"), false));
            generalRows.append(makePropertyRow(QStringLiteral("general.size"), QStringLiteral("Size"), m_sizeText, QStringLiteral("general"), false, false,
                                               QVariantMap{{QStringLiteral("busy"), m_isCalculating}}));
            if (m_isDirectory) {
                generalRows.append(makePropertyRow(QStringLiteral("general.contents"), QStringLiteral("Contents"),
                                                   QStringLiteral("%1 files, %2 folders").arg(m_fileCount).arg(m_folderCount),
                                                   QStringLiteral("general"), false));
            }
            if (!m_created.isEmpty()) {
                generalRows.append(makePropertyRow(QStringLiteral("general.created"), QStringLiteral("Created"), m_created, QStringLiteral("general"), false));
            }
            if (!m_modified.isEmpty()) {
                generalRows.append(makePropertyRow(QStringLiteral("general.modified"), QStringLiteral("Modified"), m_modified, QStringLiteral("general"), false));
            }
            if (!m_accessed.isEmpty()) {
                generalRows.append(makePropertyRow(QStringLiteral("general.accessed"), QStringLiteral("Accessed"), m_accessed, QStringLiteral("general"), false));
            }
        }
        appendGroup(groups, QStringLiteral("general"), QStringLiteral("General"), QStringLiteral("general"), generalRows);

        QVariantList detailsRows;
        for (const QVariant &propVal : m_extraProperties) {
            const QVariantMap prop = propVal.toMap();
            const QString label = prop.value(QStringLiteral("label")).toString();
            const QString value = prop.value(QStringLiteral("value")).toString();
            if (!label.isEmpty() || !value.isEmpty()) {
                detailsRows.append(makePropertyRow(stableKey(QStringLiteral("details"), label), label, value, QStringLiteral("details")));
            }
        }
        appendGroup(groups, QStringLiteral("details"), QStringLiteral("Details"), QStringLiteral("details"), detailsRows);

        QVariantList accessRows;
        for (const QVariant &propVal : m_accessProperties) {
            const QVariantMap prop = propVal.toMap();
            const QString label = prop.value(QStringLiteral("label")).toString();
            const QString value = prop.value(QStringLiteral("value")).toString();
            const bool allowed = prop.value(QStringLiteral("allowed")).toBool();
            accessRows.append(makePropertyRow(stableKey(QStringLiteral("access"), label), label, value, QStringLiteral("access"), false, false,
                                              QVariantMap{
                                                  {QStringLiteral("allowed"), allowed},
                                                  {QStringLiteral("status"), allowed ? QStringLiteral("ok") : QStringLiteral("blocked")}
                                              }));
        }
        appendGroup(groups, QStringLiteral("access.capabilities"), QStringLiteral("Capabilities"), QStringLiteral("access"), accessRows);

        QVariantList attributeRows;
        for (const QVariant &propVal : m_attributeProperties) {
            const QVariantMap prop = propVal.toMap();
            const QString label = prop.value(QStringLiteral("label")).toString();
            const QString value = prop.value(QStringLiteral("value")).toString();
            const bool enabled = prop.value(QStringLiteral("enabled")).toBool();
            attributeRows.append(makePropertyRow(stableKey(QStringLiteral("attributes"), label), label, value, QStringLiteral("attributes"), false, false,
                                                 QVariantMap{
                                                     {QStringLiteral("enabled"), enabled},
                                                     {QStringLiteral("editable"), m_canEditAttributes && (label == QLatin1String("Hidden") || label == QLatin1String("Read-only"))},
                                                     {QStringLiteral("status"), enabled ? QStringLiteral("enabled") : QStringLiteral("off")}
                                                 }));
        }
        appendGroup(groups, QStringLiteral("access.attributes"), QStringLiteral("Attributes"), QStringLiteral("access"), attributeRows);
    } else {
        QVariantList generalRows;
        generalRows.append(makePropertyRow(QStringLiteral("selection.count"), QStringLiteral("Selection"), m_name, QStringLiteral("general"), false, true));
        generalRows.append(makePropertyRow(QStringLiteral("selection.location"), QStringLiteral("Location"), m_path, QStringLiteral("general")));
        generalRows.append(makePropertyRow(QStringLiteral("selection.contains"), QStringLiteral("Contains"), m_typeText, QStringLiteral("general"), false));
        generalRows.append(makePropertyRow(QStringLiteral("selection.contents"), QStringLiteral("Files & Folders"),
                                           QStringLiteral("%1 files, %2 folders").arg(m_fileCount).arg(m_folderCount),
                                           QStringLiteral("general"), false));
        generalRows.append(makePropertyRow(QStringLiteral("selection.totalSize"), QStringLiteral("Total Size"), m_sizeText, QStringLiteral("general"), false, false,
                                           QVariantMap{{QStringLiteral("busy"), m_isCalculating}}));

        if (!m_created.isEmpty()) {
            generalRows.append(makePropertyRow(QStringLiteral("selection.createdRange"), QStringLiteral("Created Range"), m_created, QStringLiteral("general"), false));
        }
        if (!m_modified.isEmpty()) {
            generalRows.append(makePropertyRow(QStringLiteral("selection.modifiedRange"), QStringLiteral("Modified Range"), m_modified, QStringLiteral("general"), false));
        }
        if (!m_accessed.isEmpty()) {
            generalRows.append(makePropertyRow(QStringLiteral("selection.accessedRange"), QStringLiteral("Accessed Range"), m_accessed, QStringLiteral("general"), false));
        }
        appendGroup(groups, QStringLiteral("selection.summary"), QStringLiteral("General"), QStringLiteral("general"), generalRows);

        QVariantList selectedRows;
        for (int i = 0; i < m_selectedPaths.size(); ++i) {
            const QString selectedPath = m_selectedPaths.at(i);
            selectedRows.append(makePropertyRow(QStringLiteral("selection.item.%1").arg(i + 1),
                                                QFileInfo(selectedPath).fileName(),
                                                QDir::toNativeSeparators(selectedPath),
                                                QStringLiteral("selection")));
        }
        appendGroup(groups, QStringLiteral("selection.items"), QStringLiteral("Selected Items"), QStringLiteral("selection"), selectedRows);
    }

    m_propertyGroups = groups;
}

void PropertiesController::setVisible(bool visible)
{
    if (m_visible == visible) return;

    if (!visible && m_isCalculating) {
        cancelAllCalculators();
    }

    m_visible = visible;
    emit visibleChanged();
}

// === Single-item load ========================================================

void PropertiesController::load(const QString &path)
{
    cancelAllCalculators();
    m_checksumCalculator.abort();
    m_checksumCalculator.clear();

    ++m_calcGeneration;
    m_progressUpdateTimer.invalidate();
    m_selectedCount = 1;
    m_selectedPaths = { path };
    resetDriveProperties();

    QFileInfo info(path);
    if (!info.exists()) {
        m_name.clear();
        m_path.clear();
        m_sizeText.clear();
        m_typeText.clear();
        m_created.clear();
        m_modified.clear();
        m_accessed.clear();
        m_extraProperties.clear();
        m_accessProperties.clear();
        m_attributeProperties.clear();
        m_canEditAttributes = false;
        m_hiddenAttribute = false;
        m_readOnlyAttribute = false;
        m_fileCount = 0;
        m_folderCount = 0;
        m_isDirectory = false;
        resetDriveProperties();
        m_isCalculating = false;
        rebuildPropertyGroups();
        emit propertiesChanged();
        emit isCalculatingChanged();
        setVisible(false);
        return;
    }

    if (tryLoadDrive(path)) {
        rebuildPropertyGroups();
        emit propertiesChanged();
        emit isCalculatingChanged();
        setVisible(true);
        return;
    }

    m_path = path;
    m_name = info.fileName();
    m_isDirectory = info.isDir();
    m_extraProperties.clear();
    m_accessProperties.clear();
    m_attributeProperties.clear();
    m_fileCount = 0;
    m_folderCount = 0;

    const FileCapabilityInfo capabilities = FileAccessResolver::resolve(path);
    m_accessProperties = FileAccessResolver::accessProperties(capabilities);
    m_attributeProperties = FileAccessResolver::attributeProperties(capabilities);
    updateAttributeState(capabilities);

    QLocale locale;
    if (!m_isDirectory) {
        m_sizeText = locale.formattedDataSize(info.size());
        m_isCalculating = false;
    } else {
        m_sizeText = locale.formattedDataSize(0);
        m_isCalculating = true;

        m_currentCalculator = new FolderSizeCalculator(path, m_calcGeneration);
        connect(m_currentCalculator, &FolderSizeCalculator::resultReady,
                this, &PropertiesController::onSizeCalculated);
        connect(m_currentCalculator, &FolderSizeCalculator::progressUpdate,
                this, &PropertiesController::onSizeProgress);
        m_threadPool.start(m_currentCalculator);
    }

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(path);
    m_typeText = mime.comment();

    if (!m_isDirectory) {
        QPointer<PropertiesController> self(this);
        const int gen = m_calcGeneration;
        (void)QtConcurrent::run([self, path, gen]() {
            QVariantList props = MetadataExtractor::extract(path);
            if (!self) return;
            QMetaObject::invokeMethod(self.data(), [self, gen, props = std::move(props)]() {
                if (!self || gen != self->m_calcGeneration) {
                    return;
                }
                self->m_extraProperties = props;
                self->rebuildPropertyGroups();
                emit self->propertiesChanged();
            });
        });
    }

    m_created  = locale.toString(info.birthTime(),    QLocale::ShortFormat);
    m_modified = locale.toString(info.lastModified(), QLocale::ShortFormat);
    m_accessed = locale.toString(info.lastRead(),     QLocale::ShortFormat);

    rebuildPropertyGroups();
    emit propertiesChanged();
    emit isCalculatingChanged();
    setVisible(true);
}

// === Multi-item load =========================================================

void PropertiesController::loadMultiple(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    if (paths.size() == 1) {
        load(paths.first());
        return;
    }

    cancelAllCalculators();
    m_checksumCalculator.abort();
    m_checksumCalculator.clear();
    ++m_calcGeneration;
    m_progressUpdateTimer.invalidate();
    resetDriveProperties();

    m_selectedCount  = paths.size();
    m_selectedPaths  = paths;
    m_extraProperties.clear();
    m_accessProperties.clear();
    m_attributeProperties.clear();
    m_canEditAttributes = false;
    m_hiddenAttribute = false;
    m_readOnlyAttribute = false;

    // === Aggregate basic info ==================================================
    int  fileItems   = 0;
    int  folderItems = 0;
    qint64 knownSize = 0;
    QSet<QString> parentDirs;
    QDateTime earliestCreated, latestModified, latestAccessed;
    QStringList typeSet;

    QLocale locale;

    for (const QString &p : paths) {
        QFileInfo info(p);
        if (!info.exists()) continue;

        parentDirs.insert(QDir::toNativeSeparators(info.absolutePath()));

        if (info.isDir()) folderItems++;
        else              fileItems++;

        if (!info.isDir())
            knownSize += info.size();

        QDateTime ct = info.birthTime();
        QDateTime mt = info.lastModified();
        QDateTime at = info.lastRead();

        if (!earliestCreated.isValid() || ct < earliestCreated) earliestCreated = ct;
        if (!latestModified.isValid()  || mt > latestModified)  latestModified  = mt;
        if (!latestAccessed.isValid()  || at > latestAccessed)  latestAccessed  = at;

        QMimeDatabase db;
        QString comment = db.mimeTypeForFile(p).comment();
        if (!comment.isEmpty() && !typeSet.contains(comment))
            typeSet.append(comment);
    }

    // === Heading fields ========================================================
    m_name = QString("%1 items").arg(paths.size());

    // Common parent or "Multiple locations"
    if (parentDirs.size() == 1)
        m_path = *parentDirs.begin();
    else
        m_path = "Multiple locations";

    // Type summary
    QStringList typeParts;
    if (fileItems > 0)   typeParts << QString("%1 file%2").arg(fileItems).arg(fileItems > 1 ? "s" : "");
    if (folderItems > 0) typeParts << QString("%1 folder%2").arg(folderItems).arg(folderItems > 1 ? "s" : "");
    m_typeText = typeParts.join(", ");

    m_fileCount = fileItems;
    m_folderCount = folderItems;
    m_isDirectory = (folderItems > 0 && fileItems == 0);

    // === Timestamps ============================================================
    m_created  = earliestCreated.isValid()  ? locale.toString(earliestCreated,  QLocale::ShortFormat) : "";
    m_modified = latestModified.isValid()   ? locale.toString(latestModified,   QLocale::ShortFormat) : "";
    m_accessed = latestAccessed.isValid()   ? locale.toString(latestAccessed,   QLocale::ShortFormat) : "";

    // === Size: files are known; folders need async calculation =================
    m_multiBaseSize = knownSize;
    m_multiTotalSize = knownSize;
    m_multiBaseFileCount = fileItems;
    m_multiBaseFolderCount = folderItems;
    m_multiFileCount = fileItems;
    m_multiFolderCount = folderItems;
    m_multiPendingCalcs = 0;
    m_multiCalculators.clear();
    m_multiFolderSizes.clear();
    m_multiFolderFileCounts.clear();
    m_multiFolderFolderCounts.clear();

    // Start async size for each subfolder
    const int gen = m_calcGeneration;
    for (const QString &p : paths) {
        QFileInfo info(p);
        if (!info.exists() || !info.isDir()) continue;

        auto *calc = new FolderSizeCalculator(p, gen);
        connect(calc, &FolderSizeCalculator::progressUpdate,
                this, &PropertiesController::onMultiSizeProgress);
        connect(calc, &FolderSizeCalculator::resultReady,
                this, &PropertiesController::onMultiSizeCalculated);
        m_multiCalculators.append(calc);
        m_multiFolderSizes.insert(calc, 0);
        m_multiFolderFileCounts.insert(calc, 0);
        m_multiFolderFolderCounts.insert(calc, 0);
        m_multiPendingCalcs++;
        m_threadPool.start(calc);
    }

    m_isCalculating = (m_multiPendingCalcs > 0);
    m_sizeText = locale.formattedDataSize(m_multiTotalSize);

    rebuildPropertyGroups();
    emit propertiesChanged();
    emit isCalculatingChanged();
    setVisible(true);
}

void PropertiesController::resetDriveProperties()
{
    m_isDrive = false;
    m_driveRootPath.clear();
    m_driveFileSystem.clear();
    m_driveType.clear();
    m_driveUsedText.clear();
    m_driveFreeText.clear();
    m_driveTotalText.clear();
    m_driveUsagePercent = 0.0;
    m_driveReady = false;
    m_driveCritical = false;
    m_canEditAttributes = false;
    m_hiddenAttribute = false;
    m_readOnlyAttribute = false;
}

bool PropertiesController::tryLoadDrive(const QString &path)
{
    const QStorageInfo storage(path);
    if (!storage.isValid()) {
        return false;
    }

    const QString rootPath = QDir::cleanPath(storage.rootPath());
    const QString cleanPath = QDir::cleanPath(path);
    const QString rootComparable = rootPath.endsWith(QLatin1Char(':'))
        ? rootPath + QLatin1Char('/')
        : rootPath;
    const QString pathComparable = cleanPath.endsWith(QLatin1Char(':'))
        ? cleanPath + QLatin1Char('/')
        : cleanPath;

    if (QDir::fromNativeSeparators(rootComparable).compare(
            QDir::fromNativeSeparators(pathComparable),
            Qt::CaseInsensitive) != 0) {
        return false;
    }

    QLocale locale;
    const qint64 total = storage.bytesTotal();
    const qint64 free = storage.bytesFree();
    const qint64 used = total > 0 ? total - free : 0;

    m_isDrive = true;
    m_isDirectory = true;
    m_isCalculating = false;
    m_path = storage.rootPath();
    m_driveRootPath = storage.rootPath();
    m_name = storage.displayName().isEmpty() ? storage.rootPath() : storage.displayName();
    m_typeText = QStringLiteral("Drive");
    m_driveReady = storage.isReady();
    m_driveFileSystem = QString::fromLatin1(storage.fileSystemType());
    m_driveType = DriveUtils::detectDriveType(storage);
    m_driveUsedText = DriveUtils::formatSize(used);
    m_driveFreeText = DriveUtils::formatSize(free);
    m_driveTotalText = DriveUtils::formatSize(total);
    m_driveUsagePercent = total > 0
        ? static_cast<double>(used) / static_cast<double>(total)
        : 0.0;
    m_driveCritical = total > 0
        && (static_cast<double>(free) / static_cast<double>(total)) < 0.10;
    m_accessProperties.clear();
    m_attributeProperties.clear();
    m_canEditAttributes = false;
    m_hiddenAttribute = false;
    m_readOnlyAttribute = false;
    m_sizeText = m_driveTotalText;
    m_fileCount = 0;
    m_folderCount = 0;
    m_extraProperties = {
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Root")}, {QStringLiteral("value"), m_driveRootPath}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("File System")}, {QStringLiteral("value"), m_driveFileSystem.isEmpty() ? QStringLiteral("Unknown") : m_driveFileSystem}},
        QVariantMap{{QStringLiteral("label"), QStringLiteral("Device")}, {QStringLiteral("value"), QString::fromLocal8Bit(storage.device())}},
    };

    m_created.clear();
    m_modified = storage.isReady() ? locale.toString(QDateTime::currentDateTime(), QLocale::ShortFormat) : QString();
    m_accessed.clear();
    return true;
}

// === Cancel helpers ==========================================================

void PropertiesController::cancelAllCalculators()
{
    // Single
    if (m_currentCalculator) {
        m_currentCalculator->cancel();
        m_currentCalculator = nullptr;
    }
    // Multi
    for (auto *c : m_multiCalculators) {
        c->cancel();
    }
    m_multiCalculators.clear();
    m_multiFolderSizes.clear();
    m_multiFolderFileCounts.clear();
    m_multiFolderFolderCounts.clear();
    m_multiPendingCalcs = 0;

    ++m_calcGeneration;
    m_progressUpdateTimer.invalidate();
    if (m_isCalculating) {
        m_isCalculating = false;
        rebuildPropertyGroups();
        emit propertiesChanged();
        emit isCalculatingChanged();
    }
}

void PropertiesController::cancelCalculation()
{
    cancelAllCalculators();
    m_checksumCalculator.abort();
    m_checksumCalculator.clear();
}

bool PropertiesController::setHiddenAttribute(bool enabled)
{
    if (!m_canEditAttributes || m_path.isEmpty()) {
        return false;
    }
    QString error;
    if (!FileAccessResolver::setHidden(m_path, enabled, &error)) {
        return false;
    }
    load(m_path);
    return true;
}

bool PropertiesController::setReadOnlyAttribute(bool enabled)
{
    if (!m_canEditAttributes || m_path.isEmpty()) {
        return false;
    }
    QString error;
    if (!FileAccessResolver::setReadOnly(m_path, enabled, &error)) {
        return false;
    }
    load(m_path);
    return true;
}

void PropertiesController::updateAttributeState(const FileCapabilityInfo &capabilities)
{
#ifdef Q_OS_WIN
    const bool isEditableLocalPath = capabilities.exists
        && !capabilities.isArchiveLike
        && !m_isDrive
        && m_selectedCount <= 1
        && !m_path.isEmpty()
        && capabilities.access.canChangeAttributes;
    m_canEditAttributes = isEditableLocalPath;
#else
    m_canEditAttributes = false;
#endif
    m_hiddenAttribute = capabilities.attributes.hidden;
    m_readOnlyAttribute = capabilities.attributes.readOnly;
}

// === Single-item calc callbacks ===============================================

void PropertiesController::onSizeProgress(qint64 size, int files, int folders, int generation)
{
    if (generation != m_calcGeneration) return;
    if (!shouldEmitProgressUpdate()) return;

    QLocale locale;
    m_sizeText    = locale.formattedDataSize(size);
    m_fileCount   = files;
    m_folderCount = folders;
    rebuildPropertyGroups();
    emit propertiesChanged();
}

void PropertiesController::onSizeCalculated(qint64 size, int files, int folders, int generation)
{
    auto *calc = qobject_cast<FolderSizeCalculator *>(sender());

    if (generation == m_calcGeneration) {
        QLocale locale;
        m_sizeText    = locale.formattedDataSize(size);
        m_fileCount   = files;
        m_folderCount = folders;
        m_isCalculating = false;
        rebuildPropertyGroups();
        emit propertiesChanged();
        emit isCalculatingChanged();
    }

    if (calc) {
        if (m_currentCalculator == calc)
            m_currentCalculator = nullptr;
        calc->deleteLater();
    }
}

// === Multi-item calc callbacks ================================================

void PropertiesController::onMultiSizeProgress(qint64 size, int files, int folders, int generation)
{
    if (generation != m_calcGeneration) return;

    if (auto *calc = qobject_cast<FolderSizeCalculator *>(sender())) {
        m_multiFolderSizes[calc] = size;
        m_multiFolderFileCounts[calc] = files;
        m_multiFolderFolderCounts[calc] = folders;
    }

    if (!shouldEmitProgressUpdate()) return;
    emitProgressUpdate();
}

void PropertiesController::onMultiSizeCalculated(qint64 size, int files, int folders, int generation)
{
    auto *calc = qobject_cast<FolderSizeCalculator *>(sender());

    if (generation == m_calcGeneration) {
        if (calc) {
            m_multiFolderSizes[calc] = size;
            m_multiFolderFileCounts[calc] = files;
            m_multiFolderFolderCounts[calc] = folders;
        }

        if (m_multiPendingCalcs > 0)
            m_multiPendingCalcs--;

        emitProgressUpdate();

        if (m_multiPendingCalcs == 0) {
            m_isCalculating = false;
            rebuildPropertyGroups();
            emit propertiesChanged();
            emit isCalculatingChanged();
        }
    }

    if (calc) {
        m_multiCalculators.removeOne(calc);
        if (generation != m_calcGeneration) {
            m_multiFolderSizes.remove(calc);
            m_multiFolderFileCounts.remove(calc);
            m_multiFolderFolderCounts.remove(calc);
        }
        calc->deleteLater();
    }
}

void PropertiesController::emitProgressUpdate()
{
    QLocale locale;
    qint64 totalSize = m_multiBaseSize;
    int totalFiles = m_multiBaseFileCount;
    int totalFolders = m_multiBaseFolderCount;

    for (auto it = m_multiFolderSizes.cbegin(); it != m_multiFolderSizes.cend(); ++it) {
        totalSize += it.value();
    }
    for (auto it = m_multiFolderFileCounts.cbegin(); it != m_multiFolderFileCounts.cend(); ++it) {
        totalFiles += it.value();
    }
    for (auto it = m_multiFolderFolderCounts.cbegin(); it != m_multiFolderFolderCounts.cend(); ++it) {
        totalFolders += it.value();
    }

    m_multiTotalSize = totalSize;
    m_multiFileCount = totalFiles;
    m_multiFolderCount = totalFolders;
    m_sizeText = locale.formattedDataSize(m_multiTotalSize);
    m_fileCount = m_multiFileCount;
    m_folderCount = m_multiFolderCount;
    rebuildPropertyGroups();
    emit propertiesChanged();
}

bool PropertiesController::shouldEmitProgressUpdate()
{
    constexpr qint64 minProgressUpdateIntervalMs = 350;
    if (!m_progressUpdateTimer.isValid()) {
        m_progressUpdateTimer.start();
        return true;
    }
    if (m_progressUpdateTimer.elapsed() < minProgressUpdateIntervalMs) {
        return false;
    }
    m_progressUpdateTimer.restart();
    return true;
}

QString PropertiesController::exportableText() const
{
    QString result;
    QTextStream out(&result);

    for (const QVariant &groupVal : m_propertyGroups) {
        QVariantMap group = groupVal.toMap();
        QString title = group.value(QStringLiteral("title")).toString();
        out << "=== " << title << " ===\n";

        QVariantList rows = group.value(QStringLiteral("rows")).toList();
        for (const QVariant &rowVal : rows) {
            QVariantMap row = rowVal.toMap();
            QString label = row.value(QStringLiteral("label")).toString();
            QString value = row.value(QStringLiteral("value")).toString();
            if (value.isEmpty()) {
                out << label << "\n";
            } else {
                out << label << ": " << value << "\n";
            }
        }
        out << "\n";
    }

    return result.trimmed();
}

QString PropertiesController::exportableJson() const
{
    QJsonArray groupsArray;

    for (const QVariant &groupVal : m_propertyGroups) {
        const QVariantMap group = groupVal.toMap();
        QJsonObject groupObj;
        groupObj.insert(QStringLiteral("key"), group.value(QStringLiteral("key")).toString());
        groupObj.insert(QStringLiteral("title"), group.value(QStringLiteral("title")).toString());
        groupObj.insert(QStringLiteral("category"), group.value(QStringLiteral("category")).toString());

        QJsonArray rowsArray;
        const QVariantList rows = group.value(QStringLiteral("rows")).toList();
        for (const QVariant &rowVal : rows) {
            rowsArray.append(QJsonObject::fromVariantMap(rowVal.toMap()));
        }
        groupObj.insert(QStringLiteral("rows"), rowsArray);
        groupsArray.append(groupObj);
    }

    QJsonDocument doc(groupsArray);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}

bool PropertiesController::saveToFile(const QString &fileUrl, const QString &content)
{
    if (fileUrl.isEmpty()) {
        return false;
    }

    QUrl url(fileUrl);
    QString localPath = url.isLocalFile() ? url.toLocalFile() : fileUrl;

    QFile file(localPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream out(&file);
    out << content;
    return true;
}

bool PropertiesController::isPathDir(const QString &path) const
{
    return QFileInfo(path).isDir();
}

QString PropertiesController::getPathSuffix(const QString &path) const
{
    return QFileInfo(path).suffix();
}

QString PropertiesController::actionFolderPath() const
{
    if (m_isDrive && !m_driveRootPath.isEmpty()) {
        const QString rootPath = QDir::fromNativeSeparators(m_driveRootPath);
        if (QDir(rootPath).exists()) {
            return rootPath;
        }
    }
    if (m_path.isEmpty()) {
        return {};
    }
    const QFileInfo info(m_path);
    const QString folder = info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    if (folder.isEmpty() || !QFileInfo(folder).isDir()) {
        return {};
    }
    return folder;
}

bool PropertiesController::revealActionTarget() const
{
    const QString folder = actionFolderPath();
    if (folder.isEmpty()) {
        return false;
    }

#if defined(Q_OS_WIN)
    return QProcess::startDetached(QStringLiteral("explorer.exe"),
                                   {QDir::toNativeSeparators(folder)});
#elif defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"), {folder});
#else
    return QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
#endif
}

bool PropertiesController::openTerminalAtActionTarget() const
{
    const QString folder = actionFolderPath();
    if (folder.isEmpty()) {
        return false;
    }

#if defined(Q_OS_WIN)
    const QString nativePath = QDir::toNativeSeparators(folder);
    return QProcess::startDetached(QStringLiteral("wt.exe"),
        {QStringLiteral("-d"), nativePath, QStringLiteral("powershell.exe"),
         QStringLiteral("-NoExit"), QStringLiteral("-Command"),
         QStringLiteral("Set-Location '%1'").arg(nativePath)});
#elif defined(Q_OS_MACOS)
    return QProcess::startDetached(QStringLiteral("open"), {QStringLiteral("-a"), QStringLiteral("Terminal"), folder});
#else
    return QProcess::startDetached(QStringLiteral("xdg-terminal-exec"), {folder})
        || QProcess::startDetached(QStringLiteral("x-terminal-emulator"), {QStringLiteral("--working-directory"), folder});
#endif
}
