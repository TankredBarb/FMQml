#pragma once

#include "SftpClient.h"
#include "SftpProfiles.h"

#include <QFutureWatcher>
#include <QObject>

class SftpConnectionController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool hasError READ hasError NOTIFY changed)
    Q_PROPERTY(QString fingerprint READ fingerprint NOTIFY changed)
    Q_PROPERTY(QString openPath READ openPath NOTIFY connected)
    Q_PROPERTY(QVariantList connections READ connections NOTIFY changed)

public:
    explicit SftpConnectionController(QObject *parent = nullptr, const QString &knownHostsFile = {});
    ~SftpConnectionController() override;
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    bool hasError() const { return m_hasError; }
    QString fingerprint() const { return m_fingerprint; }
    QString openPath() const { return m_openPath; }
    QVariantList connections() const { return SftpProfiles::connections(); }
    Q_INVOKABLE void signIn(const QString &host, int port, const QString &user,
                           const QString &password, const QString &path,
                           const QString &name = {}, bool saveConnection = false, bool rememberPassword = false);
    Q_INVOKABLE void connectSaved(const QString &url);
    Q_INVOKABLE void disconnectFrom(const QString &url);
    Q_INVOKABLE void forget(const QString &url);
    Q_INVOKABLE void refresh() { emit changed(); }
    Q_INVOKABLE QVariantMap describe(const QString &url) const;
    Q_INVOKABLE void confirmHost();
    Q_INVOKABLE void cancel();

signals:
    void changed();
    void connected();

private:
    struct Result {
        bool ok = false;
        bool needsConfirmation = false;
        QString fingerprint;
        QString error;
        QString password;
        QString warning;
    };
    void start(const QString &acceptedFingerprint);
    bool m_busy = false;
    bool m_hasError = false;
    QString m_status;
    QString m_fingerprint;
    QString m_openPath;
    QString m_knownHostsFile;
    SftpCredentials m_pending;
    QString m_name;
    bool m_saveConnection = false;
    bool m_rememberPassword = false;
    bool m_readSavedPassword = false;
    bool m_removing = false;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    QFutureWatcher<Result> m_watcher;
};
