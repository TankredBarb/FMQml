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
    m_visible = visible;
    emit visibleChanged();
}

void PropertiesController::load(const QString &path)
{
    QFileInfo info(path);
    if (!info.exists()) return;

    m_path = path;
    m_name = info.fileName();
    m_isDirectory = info.isDir();

    QLocale locale;
    if (!m_isDirectory) {
        m_sizeText = locale.formattedDataSize(info.size());
        m_isCalculating = false;
    } else {
        ++m_calcGeneration;

        m_sizeText = "Calculating...";
        m_isCalculating = true;

        auto *calc = new FolderSizeCalculator(path, m_calcGeneration);
        connect(calc, &FolderSizeCalculator::resultReady, this, &PropertiesController::onSizeCalculated);
        connect(calc, &FolderSizeCalculator::progressUpdate, this, &PropertiesController::onSizeProgress);
        m_threadPool.start(calc);
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

    if (calc)
        calc->deleteLater();
}
