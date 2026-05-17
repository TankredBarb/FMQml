#include "PropertiesController.h"
#include "../core/FolderSizeCalculator.h"
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QMimeDatabase>

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
bool PropertiesController::isCalculating() const { return m_isCalculating; }
bool PropertiesController::visible() const { return m_visible; }

void PropertiesController::setVisible(bool visible)
{
    if (m_visible == visible) return;
    
    if (!visible && m_isCalculating) {
        cancelCalculation();
    }
    
    m_visible = visible;
    emit visibleChanged();
}

void PropertiesController::load(const QString &path)
{
    // Cancel any ongoing calculation
    if (m_isCalculating) {
        cancelCalculation();
    }

    ++m_calcGeneration;
    QFileInfo info(path);
    if (!info.exists()) {
        m_name.clear();
        m_path.clear();
        m_sizeText.clear();
        m_typeText.clear();
        m_created.clear();
        m_modified.clear();
        m_accessed.clear();
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

    QLocale locale;
    if (!m_isDirectory) {
        m_sizeText = locale.formattedDataSize(info.size());
        m_isCalculating = false;
    } else {
        m_sizeText = locale.formattedDataSize(0);
        m_isCalculating = true;

        m_currentCalculator = new FolderSizeCalculator(path, m_calcGeneration);
        connect(m_currentCalculator, &FolderSizeCalculator::resultReady, this, &PropertiesController::onSizeCalculated);
        connect(m_currentCalculator, &FolderSizeCalculator::progressUpdate, this, &PropertiesController::onSizeProgress);
        m_threadPool.start(m_currentCalculator);
    }

    QMimeDatabase db;
    m_typeText = db.mimeTypeForFile(path).comment();

    m_created = locale.toString(info.birthTime(), QLocale::LongFormat);
    m_modified = locale.toString(info.lastModified(), QLocale::LongFormat);
    m_accessed = locale.toString(info.lastRead(), QLocale::LongFormat);

    emit propertiesChanged();
    emit isCalculatingChanged();
    setVisible(true);
}

void PropertiesController::cancelCalculation()
{
    if (m_currentCalculator) {
        m_currentCalculator->cancel();
        m_currentCalculator = nullptr;
    }
    ++m_calcGeneration;
    if (m_isCalculating) {
        m_isCalculating = false;
        emit propertiesChanged();
        emit isCalculatingChanged();
    }
}

void PropertiesController::onSizeProgress(qint64 size, int generation)
{
    if (generation != m_calcGeneration) return;

    QLocale locale;
    m_sizeText = locale.formattedDataSize(size);
    emit propertiesChanged();
}

void PropertiesController::onSizeCalculated(qint64 size, int generation)
{
    auto *calc = qobject_cast<FolderSizeCalculator *>(sender());

    if (generation == m_calcGeneration) {
        QLocale locale;
        m_sizeText = locale.formattedDataSize(size);
        m_isCalculating = false;
        emit propertiesChanged();
        emit isCalculatingChanged();
    }

    if (calc) {
        if (m_currentCalculator == calc) {
            m_currentCalculator = nullptr;
        }
        calc->deleteLater();
    }
}
