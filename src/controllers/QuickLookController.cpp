#include "QuickLookController.h"
#include <QFileInfo>
#include <QFile>
#include <QMimeDatabase>
#include <QMimeType>
#include <QDebug>
#include <QDateTime>

QuickLookController::QuickLookController(QObject *parent)
    : QObject(parent)
{
}

QString QuickLookController::path() const
{
    return m_path;
}

QString QuickLookController::content() const
{
    return m_content;
}

QString QuickLookController::type() const
{
    return m_type;
}

QString QuickLookController::extension() const
{
    return m_extension;
}

int QuickLookController::lines() const
{
    return m_lines;
}

bool QuickLookController::visible() const
{
    return m_visible;
}

void QuickLookController::preview(const QString &path)
{
    qDebug() << "QuickLook preview requested for:" << path;
    if (m_path == path && m_visible) {
        setVisible(false);
        return;
    }

    m_path = path;
    QFileInfo info(path);
    m_extension = info.suffix().toLower();
    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(path);
    qDebug() << "Detected MIME type:" << mime.name();

    if (mime.name().startsWith("image/")) {
        m_type = "image";
        m_content = path;
        m_lines = 0;
    } else if (mime.name().startsWith("text/") || mime.inherits("application/json") || mime.inherits("application/javascript") || mime.inherits("text/plain")) {
        m_type = "text";
        QFile file(path);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray data = file.read(20000); // Read first 20k
            m_content = QString::fromUtf8(data);
            m_lines = m_content.count('\n') + 1;
            if (file.size() > 20000) m_content += "\n...";
        } else {
            m_content = "Cannot read file.";
            m_lines = 0;
        }
    } else {
        m_type = "info";
        m_content = QString("Name: %1\nSize: %2 bytes\nModified: %3")
                        .arg(info.fileName())
                        .arg(info.size())
                        .arg(info.lastModified().toString());
        m_lines = 0;
    }

    setVisible(true);
    emit extensionChanged();
    emit linesChanged();
    emit typeChanged();
    emit pathChanged();
    emit contentChanged();
}

void QuickLookController::setVisible(bool visible)
{
    if (m_visible == visible) return;
    m_visible = visible;
    emit visibleChanged();
}
