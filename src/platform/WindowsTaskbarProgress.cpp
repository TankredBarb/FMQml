#include "WindowsTaskbarProgress.h"

#include "../core/IsoMountManager.h"
#include "../core/OperationQueue.h"

#include <QWindow>

#ifdef Q_OS_WIN
#include <algorithm>
#include <objbase.h>
#include <shobjidl.h>
#include <windows.h>
#endif

WindowsTaskbarProgress::WindowsTaskbarProgress(QObject *parent)
    : QObject(parent)
{
#ifdef Q_OS_WIN
    const HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    m_comInitialized = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        return;
    }

    ITaskbarList3 *taskbar = nullptr;
    const HRESULT createResult = CoCreateInstance(
        CLSID_TaskbarList,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&taskbar));
    if (FAILED(createResult) || !taskbar) {
        return;
    }

    const HRESULT initTaskbarResult = taskbar->HrInit();
    if (FAILED(initTaskbarResult)) {
        taskbar->Release();
        return;
    }

    m_taskbar = taskbar;
#endif
}

WindowsTaskbarProgress::~WindowsTaskbarProgress()
{
#ifdef Q_OS_WIN
    setNoProgress();
    if (m_taskbar) {
        m_taskbar->Release();
        m_taskbar = nullptr;
    }
    if (m_comInitialized) {
        CoUninitialize();
    }
#endif
}

void WindowsTaskbarProgress::attachWindow(QWindow *window)
{
#ifdef Q_OS_WIN
    m_hwnd = window ? reinterpret_cast<HWND>(window->winId()) : nullptr;
    refresh();
#else
    Q_UNUSED(window);
#endif
}

void WindowsTaskbarProgress::setOperationQueue(OperationQueue *queue)
{
    if (m_queue == queue) {
        return;
    }
    if (m_queue) {
        disconnect(m_queue, nullptr, this, nullptr);
    }

    m_queue = queue;
    if (m_queue) {
        connect(m_queue, &OperationQueue::busyChanged, this, &WindowsTaskbarProgress::refresh);
        connect(m_queue, &OperationQueue::progressChanged, this, &WindowsTaskbarProgress::refresh);
        connect(m_queue, &OperationQueue::errorChanged, this, &WindowsTaskbarProgress::refresh);
    }
    refresh();
}

void WindowsTaskbarProgress::setIsoMountManager(IsoMountManager *manager)
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

void WindowsTaskbarProgress::refresh()
{
#ifdef Q_OS_WIN
    if (!m_taskbar || !m_hwnd) {
        return;
    }

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
#endif
}

void WindowsTaskbarProgress::setNoProgress()
{
#ifdef Q_OS_WIN
    if (m_taskbar && m_hwnd) {
        m_taskbar->SetProgressState(static_cast<HWND>(m_hwnd), TBPF_NOPROGRESS);
    }
#endif
}

void WindowsTaskbarProgress::setIndeterminate()
{
#ifdef Q_OS_WIN
    if (m_taskbar && m_hwnd) {
        m_taskbar->SetProgressState(static_cast<HWND>(m_hwnd), TBPF_INDETERMINATE);
    }
#endif
}

void WindowsTaskbarProgress::setNormalProgress(double progress)
{
#ifdef Q_OS_WIN
    if (!m_taskbar || !m_hwnd) {
        return;
    }

    const double clamped = std::clamp(progress, 0.0, 1.0);
    constexpr ULONGLONG total = 1000;
    const auto completed = static_cast<ULONGLONG>(clamped * static_cast<double>(total));
    m_taskbar->SetProgressState(static_cast<HWND>(m_hwnd), TBPF_NORMAL);
    m_taskbar->SetProgressValue(static_cast<HWND>(m_hwnd), completed, total);
#else
    Q_UNUSED(progress);
#endif
}

void WindowsTaskbarProgress::setError()
{
#ifdef Q_OS_WIN
    if (m_taskbar && m_hwnd) {
        m_taskbar->SetProgressState(static_cast<HWND>(m_hwnd), TBPF_ERROR);
    }
#endif
}
