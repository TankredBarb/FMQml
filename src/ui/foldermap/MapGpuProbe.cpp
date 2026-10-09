#include "MapGpuProbe.h"
#include "PathSemantics.h"
#include "ArchiveSupport.h"
#include <QDir>

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <rhi/qrhi.h>

bool MapGpuProbe::supportsPath(const QString &path)
{
    const auto kind = PathSemantics::describe(path).kind;
    if (kind == PathKind::Archive) {
        const QString container = ArchiveSupport::splitArchiveTokens(path).value(0);
        return PathSemantics::describe(container).kind == PathKind::Local
            && QDir::isAbsolutePath(container) && ArchiveSupport::archiveBackendAvailable();
    }
    return kind == PathKind::Local && QDir::isAbsolutePath(path);
}

MapGpuProbe::MapGpuProbe(QQuickItem *parent) : QQuickItem(parent)
{
    const auto attach = [this](QQuickWindow *window) {
        disconnect(m_initialized);
        disconnect(m_invalidated);
        disconnect(m_synchronizing);
        publish(false, false, {});
        if (!window) return;
        m_initialized = connect(window, &QQuickWindow::sceneGraphInitialized, this,
                                [this, window] { inspect(window); }, Qt::DirectConnection);
        m_invalidated = connect(window, &QQuickWindow::sceneGraphInvalidated, this,
                                [this, window] {
            QMetaObject::invokeMethod(this, [this, window] {
                if (this->window() == window) publish(false, false, {});
            }, Qt::QueuedConnection);
        }, Qt::DirectConnection);
        // A probe can also be created after the scene graph has initialized.
        m_synchronizing = connect(window, &QQuickWindow::beforeSynchronizing, this,
                [this, window] { inspect(window); }, Qt::DirectConnection);
        window->update();
    };
    connect(this, &QQuickItem::windowChanged, this, attach);
    attach(window());
}

void MapGpuProbe::inspect(QQuickWindow *window)
{
    disconnect(m_synchronizing);
    auto *renderer = window->rendererInterface();
    const auto api = renderer->graphicsApi();
    auto *rhi = static_cast<QRhi *>(renderer->getResource(window, QSGRendererInterface::RhiResource));
    bool available = false;
    QString name;
    if (rhi && api != QSGRendererInterface::Software && api != QSGRendererInterface::Null
        && api != QSGRendererInterface::Unknown) {
        const auto info = rhi->driverInfo();
        name = QString::fromUtf8(info.deviceName);
        available = info.deviceType != QRhiDriverInfo::CpuDevice;
        const QString lower = name.toLower();
        for (const auto *software : {"llvmpipe", "softpipe", "swrast", "swiftshader", "software", "basic render"}) {
            if (lower.contains(QLatin1String(software))) available = false;
        }
    }
    QMetaObject::invokeMethod(this, [this, window, available, name] {
        if (this->window() == window) publish(available, true, name);
    }, Qt::QueuedConnection);
}

void MapGpuProbe::publish(bool available, bool ready, const QString &name)
{
    if (m_available == available && m_ready == ready && m_deviceName == name) return;
    m_available = available;
    m_ready = ready;
    m_deviceName = name;
    emit capabilityChanged();
}
