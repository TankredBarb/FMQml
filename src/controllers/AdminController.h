#pragma once

#include "LinuxAdminBroker.h"
#include "LinuxAdminSession.h"

#include <QObject>
#include <QTimer>

class AdminController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool isElevated READ isElevated NOTIFY isElevatedChanged)
    Q_PROPERTY(bool canRelaunchAsAdmin READ canRelaunchAsAdmin CONSTANT)
    Q_PROPERTY(bool linuxAdminModeSupported READ linuxAdminModeSupported CONSTANT)
    Q_PROPERTY(bool adminModeAvailable READ adminModeAvailable NOTIFY adminModeAvailabilityChanged)
    Q_PROPERTY(bool adminModeActive READ adminModeActive NOTIFY adminModeStateChanged)
    Q_PROPERTY(AdminModeState adminModeState READ adminModeState NOTIFY adminModeStateChanged)
    Q_PROPERTY(QString adminModeStateName READ adminModeStateName NOTIFY adminModeStateChanged)
    Q_PROPERTY(QString adminModeBackendName READ adminModeBackendName NOTIFY adminModeAvailabilityChanged)
    Q_PROPERTY(QString adminModeUnavailableReason READ adminModeUnavailableReason NOTIFY adminModeAvailabilityChanged)
    Q_PROPERTY(int adminModeRemainingSeconds READ adminModeRemainingSeconds NOTIFY adminModeRemainingSecondsChanged)
    Q_PROPERTY(int adminModeTimeoutMinutes READ adminModeTimeoutMinutes WRITE setAdminModeTimeoutMinutes NOTIFY adminModeTimeoutMinutesChanged)
    Q_PROPERTY(bool shouldShowAdminSafetyWarning READ shouldShowAdminSafetyWarning NOTIFY shouldShowAdminSafetyWarningChanged)

public:
    enum class AdminModeState {
        Unavailable,
        Locked,
        Unlocking,
        Active,
        ExpiringSoon,
        Expired,
        Revoking,
        Error
    };
    Q_ENUM(AdminModeState)

    explicit AdminController(QObject *parent = nullptr);

    bool isElevated() const;
    bool canRelaunchAsAdmin() const;
    bool linuxAdminModeSupported() const;
    bool adminModeAvailable() const;
    bool adminModeActive() const;
    AdminModeState adminModeState() const;
    QString adminModeStateName() const;
    QString adminModeBackendName() const;
    QString adminModeUnavailableReason() const;
    int adminModeRemainingSeconds() const;
    int adminModeTimeoutMinutes() const;
    void setAdminModeTimeoutMinutes(int minutes);
    bool shouldShowAdminSafetyWarning() const;

    Q_INVOKABLE bool relaunchAsAdmin();
    Q_INVOKABLE bool unlockAdminMode();
    Q_INVOKABLE void lockAdminMode();
    Q_INVOKABLE void acknowledgeAdminSafetyWarning();
    void refreshAdminModeAfterOperation();
    Q_INVOKABLE void refresh();

signals:
    void isElevatedChanged();
    void adminModeAvailabilityChanged();
    void adminModeStateChanged();
    void adminModeRemainingSecondsChanged();
    void adminModeTimeoutMinutesChanged();
    void shouldShowAdminSafetyWarningChanged();

private:
    bool detectElevated() const;
    void refreshAdminBackendAvailability();
    void setAdminModeState(AdminModeState state);
    void setAdminModeAvailability(bool available, const QString &backendName, const QString &unavailableReason);
    void syncAdminModeStateFromSession();
    void updateAdminModeTimer();
    static QString adminModeStateToString(AdminModeState state);
    static AdminModeState adminModeStateFromSession(LinuxAdminSession::State state);

    bool m_isElevated = false;
    bool m_adminModeAvailable = false;
    bool m_adminSafetyWarningAcknowledged = false;
    AdminModeState m_adminModeState = AdminModeState::Unavailable;
    QString m_adminModeBackendName;
    QString m_adminModeUnavailableReason;
    LinuxAdminBroker m_adminBroker;
    LinuxAdminSession m_adminSession;
    QTimer m_adminModeTimer;
    qint64 m_lastObservedBrokerActivityMs = 0;
};
