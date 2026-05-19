#include "PropertiesController.h"
#include "../core/FolderSizeCalculator.h"
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QMimeDatabase>
#include <QImageReader>
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
bool PropertiesController::isCalculating() const { return m_isCalculating; }
bool PropertiesController::visible() const { return m_visible; }
QVariantList PropertiesController::extraProperties() const { return m_extraProperties; }
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
        m_fileCount = 0;
        m_folderCount = 0;
        m_isDirectory = false;
        m_isCalculating = false;
        emit propertiesChanged();
        emit isCalculatingChanged();
        setVisible(false);
        return;
    }

    m_path = path;
    m_name = info.fileName();
    m_isDirectory = info.isDir();
    m_extraProperties.clear();
    m_fileCount = 0;
    m_folderCount = 0;

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

    m_selectedCount  = paths.size();
    m_selectedPaths  = paths;
    m_extraProperties.clear();

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

    m_fileCount   = 0;   // folder contents, not top-level files
    m_folderCount = 0;
    m_isDirectory = (folderItems > 0 && fileItems == 0);

    // ── Timestamps ────────────────────────────────────────────────────────────
    m_created  = earliestCreated.isValid()  ? locale.toString(earliestCreated,  QLocale::ShortFormat) : "";
    m_modified = latestModified.isValid()   ? locale.toString(latestModified,   QLocale::ShortFormat) : "";
    m_accessed = latestAccessed.isValid()   ? locale.toString(latestAccessed,   QLocale::ShortFormat) : "";

    // ── Size: files are known; folders need async calculation ─────────────────
    m_multiTotalSize   = knownSize;
    m_multiFileCount   = 0;
    m_multiFolderCount = 0;
    m_multiPendingCalcs = 0;
    m_multiCalculators.clear();

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
        m_multiPendingCalcs++;
        m_threadPool.start(calc);
    }

    m_isCalculating = (m_multiPendingCalcs > 0);
    m_sizeText = locale.formattedDataSize(m_multiTotalSize);

    emit propertiesChanged();
    emit isCalculatingChanged();
    setVisible(true);
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

void PropertiesController::onMultiSizeProgress(qint64 size, int /*files*/, int /*folders*/, int generation)
{
    if (generation != m_calcGeneration) return;

    // We can't track which calculator sent this, so just update display size
    // by re-summing: use m_multiTotalSize as "base" (file sizes) and the sum
    // from all calculators via a separate accumulator.
    // Simpler: update m_sizeText optimistically (monotonically growing).
    Q_UNUSED(size)
    emitProgressUpdate();
}

void PropertiesController::onMultiSizeCalculated(qint64 size, int files, int folders, int generation)
{
    auto *calc = qobject_cast<FolderSizeCalculator *>(sender());

    if (generation == m_calcGeneration) {
        m_multiTotalSize   += size;
        m_multiFileCount   += files;
        m_multiFolderCount += folders;

        if (m_multiPendingCalcs > 0)
            m_multiPendingCalcs--;

        QLocale locale;
        m_sizeText    = locale.formattedDataSize(m_multiTotalSize);
        m_fileCount   = m_multiFileCount;
        m_folderCount = m_multiFolderCount;

        if (m_multiPendingCalcs == 0) {
            m_isCalculating = false;
            emit isCalculatingChanged();
        }
        emit propertiesChanged();
    }

    if (calc) {
        m_multiCalculators.removeOne(calc);
        calc->deleteLater();
    }
}

void PropertiesController::emitProgressUpdate()
{
    QLocale locale;
    m_sizeText = locale.formattedDataSize(m_multiTotalSize);
    emit propertiesChanged();
}
