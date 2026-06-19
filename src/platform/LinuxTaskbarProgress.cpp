#include "LinuxTaskbarProgress.h"

#include "../core/IsoMountManager.h"
#include "../core/OperationQueue.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariantMap>
#include <QWindow>

#include <algorithm>
#include <cmath>

namespace {
constexpr auto LauncherEntryPath = "/com/canonical/Unity/LauncherEntry";
constexpr auto LauncherEntryInterface = "com.canonical.Unity.LauncherEntry";
constexpr auto LauncherEntrySignal = "Update";
constexpr auto DesktopApplicationUri = "application://fm.desktop";
constexpr double ProgressEpsilon = 0.001;
}

LinuxTaskbarProgress::LinuxTaskbarProgress(QObject *parent)
    : QObject(parent)
{
}

void LinuxTaskbarProgress::attachWindow(QWindow *window)
{
    Q_UNUSED(window);
    refresh();
}

void LinuxTaskbarProgress::setOperationQueue(OperationQueue *queue)
{
    if (m_queue == queue) {
        return;
    }
    if (m_queue) {
        disconnect(m_queue, nullptr, this, nullptr);
    }

    m_queue = queue;
    if (m_queue) {
        connect(m_queue, &OperationQueue::busyChanged, this, &LinuxTaskbarProgress::refresh);
        connect(m_queue, &OperationQueue::progressChanged, this, &LinuxTaskbarProgress::refresh);
        connect(m_queue, &OperationQueue::errorChanged, this, &LinuxTaskbarProgress::refresh);
    }
    refresh();
}

void LinuxTaskbarProgress::setIsoMountManager(IsoMountManager *manager)
{
    if (m_isoMountManager == manager) {
        return;
    }
    if (m_isoMountManager) {
        disconnect(m_isoMountManager, nullptr, this, nullptr);
    }

    m_isoMountManager = manager;
    if (m_isoMountManager) {
        connect(m_isoMountManager, &IsoMountManager::mountStarted, this, [this]() {
            m_isoActivity = true;
            refresh();
        });
        connect(m_isoMountManager, &IsoMountManager::unmountStarted, this, [this]() {
            m_isoActivity = true;
            refresh();
        });
        connect(m_isoMountManager, &IsoMountManager::mountFinished, this,
            [this](const QString &, const QString &, bool success, const QString &) {
                m_isoActivity = false;
                success ? refresh() : setError();
            });
        connect(m_isoMountManager, &IsoMountManager::unmountFinished, this,
            [this](const QString &, bool success, const QString &) {
                m_isoActivity = false;
                success ? refresh() : setError();
            });
    }
    refresh();
}

void LinuxTaskbarProgress::refresh()
{
    if (m_queue && !m_queue->error().isEmpty()) {
        setError();
        return;
    }

    if (m_queue && m_queue->busy()) {
        const double progress = m_queue->progress();
        if (progress > 0.0) {
            setNormalProgress(progress);
        } else {
            setIndeterminate();
        }
        return;
    }

    if (m_isoActivity) {
        setIndeterminate();
        return;
    }

    setNoProgress();
}

void LinuxTaskbarProgress::setNoProgress()
{
    sendProgress(false, 0.0, false);
}

void LinuxTaskbarProgress::setIndeterminate()
{
    sendProgress(true, 0.0, false);
}

void LinuxTaskbarProgress::setNormalProgress(double progress)
{
    sendProgress(true, std::clamp(progress, 0.0, 1.0), false);
}

void LinuxTaskbarProgress::setError()
{
    sendProgress(false, 0.0, true);
}

void LinuxTaskbarProgress::sendProgress(bool visible, double progress, bool urgent)
{
    const State nextState = urgent
        ? State::Error
        : (visible ? (progress > 0.0 ? State::Normal : State::Indeterminate) : State::NoProgress);
    const double clamped = std::clamp(progress, 0.0, 1.0);
    if (m_state == nextState
            && m_urgent == urgent
            && std::abs(m_progress - clamped) < ProgressEpsilon) {
        return;
    }

    m_state = nextState;
    m_progress = clamped;
    m_urgent = urgent;

    QVariantMap properties;
    properties.insert(QStringLiteral("progress-visible"), visible);
    properties.insert(QStringLiteral("progress"), clamped);
    properties.insert(QStringLiteral("urgent"), urgent);

    QDBusMessage message = QDBusMessage::createSignal(
        QString::fromLatin1(LauncherEntryPath),
        QString::fromLatin1(LauncherEntryInterface),
        QString::fromLatin1(LauncherEntrySignal));
    message << QString::fromLatin1(DesktopApplicationUri) << properties;
    QDBusConnection::sessionBus().send(message);
}
