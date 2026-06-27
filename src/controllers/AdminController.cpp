#include "AdminController.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QMetaObject>
#include <QStringList>

#ifdef Q_OS_WIN
#include <QDir>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
#ifdef Q_OS_WIN
QString quoteWindowsArgument(const QString &arg)
{
    if (arg.isEmpty()) {
        return QStringLiteral("\"\"");
    }

    QString escaped = arg;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    if (escaped.contains(QLatin1Char(' ')) || escaped.contains(QLatin1Char('\t'))) {
        return QStringLiteral("\"%1\"").arg(escaped);
    }
    return escaped;
}
#endif
}

AdminController::AdminController(QObject *parent)
    : QObject(parent)
    , m_isElevated(detectElevated())
{
    m_adminModeTimer.setInterval(1000);
    connect(&m_adminModeTimer, &QTimer::timeout, this, &AdminController::updateAdminModeTimer);
    refreshAdminBackendAvailability();
}

bool AdminController::isElevated() const
{
    return m_isElevated;
}

bool AdminController::canRelaunchAsAdmin() const
{
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

bool AdminController::linuxAdminModeSupported() const
{
#ifdef Q_OS_LINUX
    return true;
#else
    return false;
#endif
}

bool AdminController::adminModeAvailable() const
{
    return m_adminModeAvailable;
}

bool AdminController::adminModeActive() const
{
    return m_adminModeState == AdminModeState::Active
        || m_adminModeState == AdminModeState::ExpiringSoon;
}

AdminController::AdminModeState AdminController::adminModeState() const
{
    return m_adminModeState;
}

QString AdminController::adminModeStateName() const
{
    return adminModeStateToString(m_adminModeState);
}

QString AdminController::adminModeBackendName() const
{
    return m_adminModeBackendName;
}

QString AdminController::adminModeUnavailableReason() const
{
    return m_adminModeUnavailableReason;
}

int AdminController::adminModeRemainingSeconds() const
{
    return m_adminSession.remainingSeconds(QDateTime::currentMSecsSinceEpoch());
}

int AdminController::adminModeTimeoutMinutes() const
{
    return m_adminSession.timeoutMinutes();
}

void AdminController::setAdminModeTimeoutMinutes(int minutes)
{
    const int previous = m_adminSession.timeoutMinutes();
    m_adminSession.setTimeoutMinutes(minutes);
    if (m_adminSession.timeoutMinutes() == previous) {
        return;
    }
    emit adminModeTimeoutMinutesChanged();
}

bool AdminController::shouldShowAdminSafetyWarning() const
{
#ifdef Q_OS_LINUX
    return linuxAdminModeSupported() && !m_adminSafetyWarningAcknowledged;
#else
    return false;
#endif
}

bool AdminController::relaunchAsAdmin()
{
#ifdef Q_OS_WIN
    if (m_isElevated) {
        return true;
    }

    const QString executablePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    QStringList arguments = QCoreApplication::arguments();
    if (!arguments.isEmpty()) {
        arguments.removeFirst();
    }

    QStringList quotedArguments;
    quotedArguments.reserve(arguments.size());
    for (const QString &argument : std::as_const(arguments)) {
        quotedArguments.append(quoteWindowsArgument(argument));
    }
    const QString parameters = quotedArguments.join(QLatin1Char(' '));

    const std::wstring executable = executablePath.toStdWString();
    const std::wstring params = parameters.toStdWString();

    SHELLEXECUTEINFOW info{};
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOASYNC;
    info.hwnd = nullptr;
    info.lpVerb = L"runas";
    info.lpFile = executable.c_str();
    info.lpParameters = params.empty() ? nullptr : params.c_str();
    info.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&info)) {
        return false;
    }

    QMetaObject::invokeMethod(qApp, &QCoreApplication::quit, Qt::QueuedConnection);
    return true;
#else
    return false;
#endif
}

bool AdminController::unlockAdminMode()
{
#ifdef Q_OS_LINUX
    refreshAdminBackendAvailability();
    if (!m_adminModeAvailable) {
        setAdminModeState(AdminModeState::Unavailable);
        return false;
    }

    m_adminSession.beginUnlock();
    syncAdminModeStateFromSession();
    const LinuxAdminBroker::Result authResult = m_adminBroker.authenticateBlocking();
    if (!authResult.success) {
        LinuxAdminBroker::revokeSession();
        m_adminSession.lock();
        m_adminModeUnavailableReason = authResult.errorMessage.isEmpty() ? authResult.errorCode : authResult.errorMessage;
        syncAdminModeStateFromSession();
        emit adminModeAvailabilityChanged();
        emit adminModeRemainingSecondsChanged();
        return false;
    }
    if (!m_adminModeUnavailableReason.isEmpty()) {
        m_adminModeUnavailableReason.clear();
        emit adminModeAvailabilityChanged();
    }
    m_adminSession.activate(QDateTime::currentMSecsSinceEpoch());
    m_adminModeTimer.start();
    syncAdminModeStateFromSession();
    emit adminModeRemainingSecondsChanged();
    return true;
#else
    return false;
#endif
}

void AdminController::lockAdminMode()
{
#ifdef Q_OS_LINUX
    LinuxAdminBroker::revokeSession();
    m_adminSession.lock();
    m_adminModeTimer.stop();
    syncAdminModeStateFromSession();
    emit adminModeRemainingSecondsChanged();
#endif
}

void AdminController::acknowledgeAdminSafetyWarning()
{
    if (m_adminSafetyWarningAcknowledged) {
        return;
    }
    m_adminSafetyWarningAcknowledged = true;
    emit shouldShowAdminSafetyWarningChanged();
}

void AdminController::refreshAdminModeAfterOperation()
{
#ifdef Q_OS_LINUX
    m_adminSession.refreshAfterOperation(QDateTime::currentMSecsSinceEpoch());
    m_adminModeTimer.start();
    syncAdminModeStateFromSession();
    emit adminModeRemainingSecondsChanged();
#endif
}

void AdminController::refresh()
{
    const bool elevated = detectElevated();
    if (m_isElevated == elevated) {
        return;
    }
    m_isElevated = elevated;
    emit isElevatedChanged();
    refreshAdminBackendAvailability();
}

bool AdminController::detectElevated() const
{
#ifdef Q_OS_WIN
    BOOL isMember = FALSE;
    SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;
    if (!AllocateAndInitializeSid(&authority, 2,
                                  SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS,
                                  0, 0, 0, 0, 0, 0,
                                  &adminGroup)) {
        return false;
    }

    const BOOL ok = CheckTokenMembership(nullptr, adminGroup, &isMember);
    FreeSid(adminGroup);
    return ok && isMember;
#else
    return false;
#endif
}

void AdminController::refreshAdminBackendAvailability()
{
#ifdef Q_OS_LINUX
    if (m_adminBroker.available()) {
        setAdminModeAvailability(true, m_adminBroker.backendName(), {});
    } else {
        const QString reason = m_adminBroker.unavailableReason();
        setAdminModeAvailability(false, {}, reason.isEmpty() ? QStringLiteral("Linux admin helper is not installed") : reason);
    }
#else
    setAdminModeAvailability(false, {}, QStringLiteral("Linux admin mode is not supported on this platform"));
#endif
}

void AdminController::setAdminModeState(AdminModeState state)
{
    if (m_adminModeState == state) {
        return;
    }
    m_adminModeState = state;
    emit adminModeStateChanged();
}

void AdminController::setAdminModeAvailability(bool available,
                                               const QString &backendName,
                                               const QString &unavailableReason)
{
    const bool changed = m_adminModeAvailable != available
        || m_adminModeBackendName != backendName
        || m_adminModeUnavailableReason != unavailableReason;
    m_adminModeAvailable = available;
    m_adminModeBackendName = backendName;
    m_adminModeUnavailableReason = unavailableReason;
    m_adminSession.setBackendAvailable(available);

    if (!m_adminModeAvailable) {
        m_adminModeTimer.stop();
        emit adminModeRemainingSecondsChanged();
    }
    syncAdminModeStateFromSession();

    if (changed) {
        emit adminModeAvailabilityChanged();
    }
}

void AdminController::updateAdminModeTimer()
{
    m_adminSession.updateForNow(QDateTime::currentMSecsSinceEpoch());
    if (m_adminSession.state() == LinuxAdminSession::State::Expired) {
        LinuxAdminBroker::revokeSession();
    }
    const int remaining = adminModeRemainingSeconds();
    emit adminModeRemainingSecondsChanged();
    syncAdminModeStateFromSession();
    if (remaining <= 0) {
        m_adminModeTimer.stop();
    }
}

void AdminController::syncAdminModeStateFromSession()
{
    setAdminModeState(adminModeStateFromSession(m_adminSession.state()));
}

QString AdminController::adminModeStateToString(AdminModeState state)
{
    switch (state) {
    case AdminModeState::Unavailable: return QStringLiteral("Unavailable");
    case AdminModeState::Locked: return QStringLiteral("Locked");
    case AdminModeState::Unlocking: return QStringLiteral("Unlocking");
    case AdminModeState::Active: return QStringLiteral("Active");
    case AdminModeState::ExpiringSoon: return QStringLiteral("ExpiringSoon");
    case AdminModeState::Expired: return QStringLiteral("Expired");
    case AdminModeState::Revoking: return QStringLiteral("Revoking");
    case AdminModeState::Error: return QStringLiteral("Error");
    }
    return QStringLiteral("Unavailable");
}

AdminController::AdminModeState AdminController::adminModeStateFromSession(LinuxAdminSession::State state)
{
    switch (state) {
    case LinuxAdminSession::State::Unavailable: return AdminModeState::Unavailable;
    case LinuxAdminSession::State::Locked: return AdminModeState::Locked;
    case LinuxAdminSession::State::Unlocking: return AdminModeState::Unlocking;
    case LinuxAdminSession::State::Active: return AdminModeState::Active;
    case LinuxAdminSession::State::ExpiringSoon: return AdminModeState::ExpiringSoon;
    case LinuxAdminSession::State::Expired: return AdminModeState::Expired;
    case LinuxAdminSession::State::Revoking: return AdminModeState::Revoking;
    case LinuxAdminSession::State::Error: return AdminModeState::Error;
    }
    return AdminModeState::Unavailable;
}
