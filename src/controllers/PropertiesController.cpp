#include "PropertiesController.h"
#include <QFileInfo>
#include <QDir>
#include <QLocale>
#include <QMimeDatabase>

PropertiesController::PropertiesController(QObject *parent)
    : QObject(parent)
{
}

QString PropertiesController::name() const { return m_name; }
QString PropertiesController::path() const { return m_path; }
QString PropertiesController::sizeText() const { return m_sizeText; }
QString PropertiesController::typeText() const { return m_typeText; }
QString PropertiesController::created() const { return m_created; }
QString PropertiesController::modified() const { return m_modified; }
QString PropertiesController::accessed() const { return m_accessed; }
bool PropertiesController::isDirectory() const { return m_isDirectory; }
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
    m_sizeText = m_isDirectory ? "" : locale.formattedDataSize(info.size());
    
    QMimeDatabase db;
    m_typeText = db.mimeTypeForFile(path).comment();
    
    m_created = locale.toString(info.birthTime(), QLocale::LongFormat);
    m_modified = locale.toString(info.lastModified(), QLocale::LongFormat);
    m_accessed = locale.toString(info.lastRead(), QLocale::LongFormat);

    emit propertiesChanged();
    setVisible(true);
}
