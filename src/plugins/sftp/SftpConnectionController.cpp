#include "SftpConnectionController.h"

#include <QtConcurrent>

SftpConnectionController::SftpConnectionController(QObject *parent, const QString &knownHostsFile)
    : QObject(parent), m_knownHostsFile(knownHostsFile)
{
    connect(&m_watcher, &QFutureWatcher<Result>::finished, this, [this]() {
        m_busy = false;
        const Result result = m_watcher.result();
        if (m_removing) {
            m_removing = false;
            m_hasError = !result.ok;
            m_status = result.ok ? QStringLiteral("Saved connection removed.") : result.error;
            emit changed();
            return;
        }
        if (m_cancelled->load()) {
            m_hasError = false;
            m_status = QStringLiteral("Connection cancelled.");
            m_pending = {};
            m_fingerprint.clear();
        } else if (result.ok) {
            m_hasError = !result.warning.isEmpty();
            m_pending.password = result.password;
            SftpSessions::remember(m_pending);
            m_openPath = m_pending.address.encoded();
            m_status = result.warning.isEmpty() ? QStringLiteral("Connected.") : result.warning;
            m_pending = {};
            m_fingerprint.clear();
            emit changed();
            emit connected();
            return;
        } else {
            m_hasError = !result.needsConfirmation;
            m_status = result.error;
            m_fingerprint = result.needsConfirmation ? result.fingerprint : QString{};
            m_pending.password = result.password;
            m_readSavedPassword = false;
            if (!result.needsConfirmation) m_pending = {};
        }
        emit changed();
    });
}

SftpConnectionController::~SftpConnectionController()
{
    if (m_cancelled) m_cancelled->store(true);
    m_watcher.waitForFinished();
}

void SftpConnectionController::signIn(const QString &host, int port, const QString &user,
                                    const QString &password, const QString &path,
                                    const QString &name, bool saveConnection, bool rememberPassword)
{
    if (m_busy) return;
    m_fingerprint.clear();
    m_name = name;
    m_saveConnection = saveConnection;
    m_rememberPassword = saveConnection && rememberPassword;
    m_readSavedPassword = false;
    m_pending = {SftpPath::fromFields(host, port, user, path), password, {}};
    if (password.isEmpty() && m_rememberPassword && m_pending.address.valid()) {
        const auto profile = describe(m_pending.address.encoded());
        m_readSavedPassword = profile.value("rememberPassword").toBool();
    }
    if (!m_pending.address.valid() || password.contains(QChar::Null)) {
        m_hasError = true;
        m_status = m_pending.address.valid() ? QStringLiteral("The password contains an invalid character.")
                                             : m_pending.address.error;
        m_pending = {};
        emit changed();
        return;
    }
    start({});
}

void SftpConnectionController::confirmHost()
{
    if (!m_busy && !m_fingerprint.isEmpty()) start(m_fingerprint);
}

void SftpConnectionController::start(const QString &acceptedFingerprint)
{
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    m_busy = true;
    m_hasError = false;
    m_status = QStringLiteral("Connecting…");
    emit changed();
    SftpCredentials credentials = m_pending;
    const auto cancelled = m_cancelled;
    const QString knownHostsFile = m_knownHostsFile;
    const bool readSaved = m_readSavedPassword;
    const bool save = m_saveConnection;
    const bool remember = m_rememberPassword;
    const QString name = m_name;
    m_watcher.setFuture(QtConcurrent::run([credentials, acceptedFingerprint, cancelled, knownHostsFile, readSaved, save, remember, name]() mutable {
        SftpClient client([cancelled]() { return cancelled->load(); }, knownHostsFile);
        Result result;
        if (readSaved) {
            const auto password = SftpProfiles::password(credentials.address, &result.error, [cancelled] { return cancelled->load(); });
            if (!password || cancelled->load()) return result;
            credentials.password = *password;
        }
        result.password = credentials.password;
        result.ok = client.connect(credentials, acceptedFingerprint);
        if (result.ok) {
            const auto entry = client.stat(credentials.address);
            result.ok = entry && entry->isDirectory;
            if (!result.ok) result.error = entry ? QStringLiteral("The initial path is not a directory.") : client.error();
        } else {
            result.error = client.error();
        }
        result.needsConfirmation = client.needsHostConfirmation();
        result.fingerprint = client.fingerprint();
        if (result.ok && save && !cancelled->load()) {
            QString error;
            if (!SftpProfiles::save(credentials.address, name, credentials.password, remember, &error, [cancelled] { return cancelled->load(); }))
                result.warning = QStringLiteral("Connected, but settings were not saved: %1").arg(error);
        }
        return result;
    }));
}

void SftpConnectionController::connectSaved(const QString &url)
{
    if (m_busy) return;
    m_pending = {SftpPath::parse(url), {}, {}};
    if (!m_pending.address.valid()) { m_status = m_pending.address.error; m_hasError = true; emit changed(); return; }
    m_fingerprint.clear();
    m_readSavedPassword = true;
    m_saveConnection = false;
    start({});
}

QVariantMap SftpConnectionController::describe(const QString &url) const
{
    const auto address = SftpPath::parse(url);
    if (!address.valid()) return {};
    QVariantMap result{{"host", address.url.host()}, {"port", address.url.port(22)},
        {"user", address.url.userName(QUrl::FullyDecoded)}, {"path", address.url.path(QUrl::FullyDecoded)}};
    for (const auto &value : connections()) {
        const auto profile = value.toMap();
        if (SftpPath::parse(profile.value("url").toString()).identity() == address.identity()) {
            result.insert("name", profile.value("name"));
            result.insert("rememberPassword", profile.value("rememberPassword"));
            result.insert("saved", profile.value("saved"));
            break;
        }
    }
    return result;
}

void SftpConnectionController::disconnectFrom(const QString &url)
{
    if (m_busy) return;
    const auto address = SftpPath::parse(url);
    if (address.valid()) SftpSessions::forget(address.identity());
    m_status = QStringLiteral("Disconnected. Saved settings are kept.");
    m_hasError = false;
    emit changed();
}

void SftpConnectionController::forget(const QString &url)
{
    if (m_busy) return;
    const auto address = SftpPath::parse(url);
    if (!address.valid()) return;
    SftpSessions::forget(address.identity());
    m_busy = true;
    m_removing = true;
    m_hasError = false;
    m_status = QStringLiteral("Removing saved connection…");
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    emit changed();
    const auto cancelled = m_cancelled;
    m_watcher.setFuture(QtConcurrent::run([address, cancelled]() {
        Result result;
        result.ok = SftpProfiles::remove(address, &result.error, [cancelled] { return cancelled->load(); });
        return result;
    }));
}

void SftpConnectionController::cancel()
{
    m_hasError = false;
    if (m_cancelled) m_cancelled->store(true);
    m_pending = {};
    m_fingerprint.clear();
    if (!m_busy) m_status = QStringLiteral("Connection cancelled.");
    emit changed();
}

