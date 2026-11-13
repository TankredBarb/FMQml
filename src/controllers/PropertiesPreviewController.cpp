#include "PropertiesPreviewController.h"

#include <QUrl>

void PropertiesPreviewController::setPath(const QString &path)
{
    const QUrl url(path);
    const QString normalized = url.isLocalFile() ? url.toLocalFile() : path;
    if (m_path == normalized)
        return;
    m_path = normalized;
    refresh();
    emit pathChanged();
}

void PropertiesPreviewController::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    refresh();
    emit activeChanged();
}

void PropertiesPreviewController::refresh()
{
    ++m_generation;
    if (m_available) {
        m_available = false;
        emit availableChanged();
    }
    m_request.clear();
    // MIME/content detection happens on the dedicated image-provider workers.
    if (m_active && !m_path.isEmpty() && !m_path.contains(QStringLiteral("://"))) {
        m_request = {{QStringLiteral("id"), QString::number(m_generation)},
                     {QStringLiteral("source"), QStringLiteral("image://propertiespreview/")
                      + QString::fromLatin1(QUrl::toPercentEncoding(m_path))}};
    }
    emit requestChanged();
}

void PropertiesPreviewController::complete(const QString &id, bool success, int width, int height)
{
    if (!m_active || m_request.isEmpty() || id != m_request.value(QStringLiteral("id")).toString())
        return;
    // The dedicated provider reserves 1x1 for misses and enlarges genuine one-pixel images.
    const bool available = success && width > 0 && height > 0 && (width > 1 || height > 1);
    if (m_available == available)
        return;
    m_available = available;
    emit availableChanged();
}
