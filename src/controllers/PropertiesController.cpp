#include "PropertiesController.h"
#include "../core/FolderSizeCalculator.h"
#include "../core/DriveUtils.h"
#include "../core/FileAccessResolver.h"
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QMimeDatabase>
#include <QImageReader>
#include <QStorageInfo>
#include <QSet>
#include "../core/MetadataExtractor.h"
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>
#include <QMetaObject>

PropertiesController::PropertiesController(QObject *parent)
    : QObject(parent)
{
    m_threadPool.setMaxThreadCount(4);
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

void PropertiesController::setVisible(bool visible)
{
    if (m_visible == visible) return;

    if (!visible && m_isCalculating) {
        cancelAllCalculators();
    }

    m_visible = visible;
    emit visibleChanged();
}

// ─── Single-item load ────────────────────────────────────────────────────────

void PropertiesController::load(const QString &path)
{
    cancelAllCalculators();

    ++m_calcGeneration;
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
        emit propertiesChanged();
        emit isCalculatingChanged();
        setVisible(false);
        return;
    }

    if (tryLoadDrive(path)) {
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
                emit self->propertiesChanged();
            });
        });
    }

    m_created  = locale.toString(info.birthTime(),    QLocale::ShortFormat);
    m_modified = locale.toString(info.lastModified(), QLocale::ShortFormat);
    m_accessed = locale.toString(info.lastRead(),     QLocale::ShortFormat);

    emit propertiesChanged();
    emit isCalculatingChanged();
    setVisible(true);
}

// ─── Multi-item load ─────────────────────────────────────────────────────────

void PropertiesController::loadMultiple(const QStringList &paths)
{
    if (paths.isEmpty()) return;
    if (paths.size() == 1) {
        load(paths.first());
        return;
    }

    cancelAllCalculators();
    ++m_calcGeneration;
    resetDriveProperties();

    m_selectedCount  = paths.size();
    m_selectedPaths  = paths;
    m_extraProperties.clear();
    m_accessProperties.clear();
    m_attributeProperties.clear();
    m_canEditAttributes = false;
    m_hiddenAttribute = false;
    m_readOnlyAttribute = false;

    // ── Aggregate basic info ──────────────────────────────────────────────────
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

    // ── Heading fields ────────────────────────────────────────────────────────
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

    // ── Timestamps ────────────────────────────────────────────────────────────
    m_created  = earliestCreated.isValid()  ? locale.toString(earliestCreated,  QLocale::ShortFormat) : "";
    m_modified = latestModified.isValid()   ? locale.toString(latestModified,   QLocale::ShortFormat) : "";
    m_accessed = latestAccessed.isValid()   ? locale.toString(latestAccessed,   QLocale::ShortFormat) : "";

    // ── Size: files are known; folders need async calculation ─────────────────
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

// ─── Cancel helpers ──────────────────────────────────────────────────────────

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
    if (m_isCalculating) {
        m_isCalculating = false;
        emit propertiesChanged();
        emit isCalculatingChanged();
    }
}

void PropertiesController::cancelCalculation()
{
    cancelAllCalculators();
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
        && !m_path.isEmpty();
    m_canEditAttributes = isEditableLocalPath;
#else
    m_canEditAttributes = false;
#endif
    m_hiddenAttribute = capabilities.attributes.hidden;
    m_readOnlyAttribute = capabilities.attributes.readOnly;
}

// ─── Single-item calc callbacks ───────────────────────────────────────────────

void PropertiesController::onSizeProgress(qint64 size, int files, int folders, int generation)
{
    if (generation != m_calcGeneration) return;

    QLocale locale;
    m_sizeText    = locale.formattedDataSize(size);
    m_fileCount   = files;
    m_folderCount = folders;
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
        emit propertiesChanged();
        emit isCalculatingChanged();
    }

    if (calc) {
        if (m_currentCalculator == calc)
            m_currentCalculator = nullptr;
        calc->deleteLater();
    }
}

// ─── Multi-item calc callbacks ────────────────────────────────────────────────

void PropertiesController::onMultiSizeProgress(qint64 size, int files, int folders, int generation)
{
    if (generation != m_calcGeneration) return;

    if (auto *calc = qobject_cast<FolderSizeCalculator *>(sender())) {
        m_multiFolderSizes[calc] = size;
        m_multiFolderFileCounts[calc] = files;
        m_multiFolderFolderCounts[calc] = folders;
    }

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
    emit propertiesChanged();
}
