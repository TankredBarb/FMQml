#include "SftpProfiles.h"
#include "SftpClient.h"

#include <QCryptographicHash>
#include <QMutex>
#include <QSettings>
#include <QSet>
#include <algorithm>
#include <chrono>

#ifndef FM_SFTP_TEST_CREDENTIALS
#pragma push_macro("signals")
#undef signals
#include <libsecret/secret.h>
#pragma pop_macro("signals")
#endif

namespace {
QMutex profilesMutex;
QMutex secretsMutex;
QMutex mutationsMutex;
#ifdef FM_SFTP_TEST_CREDENTIALS
QHash<QString, QString> testPasswords;
#else
const SecretSchema schema = [] {
    SecretSchema value{};
    value.name = "org.fmqml.SFTP";
    value.flags = SECRET_SCHEMA_NONE;
    value.attributes[0] = {"connection", SECRET_SCHEMA_ATTRIBUTE_STRING};
    return value;
}();

struct SecretRequest {
    GCancellable *handle = g_cancellable_new();
    std::jthread watchdog;
    explicit SecretRequest(const std::function<bool()> &cancelled)
        : watchdog([this, cancelled](std::stop_token stop) {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
            while (!stop.stop_requested()) {
                if ((cancelled && cancelled()) || std::chrono::steady_clock::now() >= deadline) {
                    g_cancellable_cancel(handle);
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }) {}
    ~SecretRequest() {
        watchdog.request_stop();
        watchdog.join();
        g_object_unref(handle);
    }
};
#endif

QString key(const SftpPath::Address &address)
{
    return QString::fromLatin1(QCryptographicHash::hash(address.identity().toUtf8(), QCryptographicHash::Sha256).toHex());
}

bool changePassword(const SftpPath::Address &address, const QString *password, QString *error,
                    const std::function<bool()> &cancelled)
{
    QMutexLocker lock(&secretsMutex);
    if (cancelled && cancelled()) { if (error) *error = QStringLiteral("Cancelled."); return false; }
#ifdef FM_SFTP_TEST_CREDENTIALS
    Q_UNUSED(error);
    if (password) testPasswords.insert(key(address), *password);
    else testPasswords.remove(key(address));
    return true;
#else
    const QByteArray identity = address.identity().toUtf8();
    GError *failure = nullptr;
    SecretRequest request(cancelled);
    bool ok;
    if (password) {
        const QByteArray bytes = password->toUtf8();
        const QByteArray label = (QStringLiteral("FM SFTP · ") + address.identity()).toUtf8();
        ok = secret_password_store_sync(&schema, SECRET_COLLECTION_DEFAULT, label.constData(), bytes.constData(),
                                        request.handle, &failure, "connection", identity.constData(), nullptr);
    } else {
        secret_password_clear_sync(&schema, request.handle, &failure, "connection", identity.constData(), nullptr);
        ok = !failure;
    }
    if (failure) {
        if (error) *error = QStringLiteral("System password storage: %1").arg(QString::fromUtf8(failure->message));
        g_error_free(failure);
    }
    return ok;
#endif
}

bool remembersPassword(const SftpPath::Address &address)
{
    QMutexLocker lock(&profilesMutex);
    QSettings settings;
    return settings.value(QStringLiteral("sftp/connections/") + key(address)
                          + QStringLiteral("/rememberPassword"), false).toBool();
}
}

QVariantList SftpProfiles::connections()
{
    QMutexLocker lock(&profilesMutex);
    QSettings settings;
    settings.beginGroup(QStringLiteral("sftp/connections"));
    QVariantList result;
    QSet<QString> identities;
    for (const auto &id : settings.childGroups()) {
        settings.beginGroup(id);
        const auto address = SftpPath::parse(settings.value(QStringLiteral("url")).toString());
        if (address.valid()) {
            const QString name = settings.value(QStringLiteral("name")).toString();
            result.append(QVariantMap{{"url", address.encoded()}, {"name", name.isEmpty() ? address.identity() : name},
                {"saved", true}, {"rememberPassword", settings.value(QStringLiteral("rememberPassword"), false)},
                {"connected", SftpSessions::credentials(address).has_value()}});
            identities.insert(address.identity());
        }
        settings.endGroup();
    }
    for (const auto &address : SftpSessions::addresses()) {
        if (identities.contains(address.identity())) continue;
        result.append(QVariantMap{{"url", address.encoded()}, {"name", address.identity()},
            {"saved", false}, {"rememberPassword", false}, {"connected", true}});
    }
    std::sort(result.begin(), result.end(), [](const QVariant &a, const QVariant &b) {
        return a.toMap().value("name").toString().localeAwareCompare(b.toMap().value("name").toString()) < 0;
    });
    return result;
}

bool SftpProfiles::save(const SftpPath::Address &address, const QString &name, const QString &password,
                        bool rememberPassword, QString *error, const std::function<bool()> &cancelled)
{
    if (!address.valid()) { if (error) *error = address.error; return false; }
    QMutexLocker mutation(&mutationsMutex);
    if (cancelled && cancelled()) { if (error) *error = QStringLiteral("Cancelled."); return false; }
    if ((rememberPassword || remembersPassword(address))
        && !changePassword(address, rememberPassword ? &password : nullptr, error, cancelled)) return false;
    QMutexLocker lock(&profilesMutex);
    QSettings settings;
    settings.beginGroup(QStringLiteral("sftp/connections/") + key(address));
    settings.setValue(QStringLiteral("url"), address.encoded());
    settings.setValue(QStringLiteral("name"), name.trimmed());
    settings.setValue(QStringLiteral("rememberPassword"), rememberPassword);
    settings.sync();
    if (settings.status() == QSettings::NoError) return true;
    if (error) *error = QStringLiteral("Could not save the connection settings.");
    return false;
}

bool SftpProfiles::remove(const SftpPath::Address &address, QString *error, const std::function<bool()> &cancelled)
{
    if (!address.valid()) { if (error) *error = address.error; return false; }
    QMutexLocker mutation(&mutationsMutex);
    if (cancelled && cancelled()) { if (error) *error = QStringLiteral("Cancelled."); return false; }
    if (remembersPassword(address) && !changePassword(address, nullptr, error, cancelled)) return false;
    QMutexLocker lock(&profilesMutex);
    QSettings settings;
    settings.remove(QStringLiteral("sftp/connections/") + key(address));
    settings.sync();
    if (settings.status() == QSettings::NoError) return true;
    if (error) *error = QStringLiteral("Could not remove the connection settings.");
    return false;
}

std::optional<QString> SftpProfiles::password(const SftpPath::Address &address, QString *error,
                                            const std::function<bool()> &cancelled)
{
    if (!address.valid()) { if (error) *error = address.error; return std::nullopt; }
    QMutexLocker lock(&secretsMutex);
    if (cancelled && cancelled()) { if (error) *error = QStringLiteral("Cancelled."); return std::nullopt; }
#ifdef FM_SFTP_TEST_CREDENTIALS
    const auto found = testPasswords.constFind(key(address));
    if (found != testPasswords.cend()) return *found;
#else
    const QByteArray identity = address.identity().toUtf8();
    GError *failure = nullptr;
    SecretRequest request(cancelled);
    gchar *stored = secret_password_lookup_sync(&schema, request.handle, &failure, "connection", identity.constData(), nullptr);
    if (failure) {
        if (error) *error = QStringLiteral("System password storage: %1").arg(QString::fromUtf8(failure->message));
        g_error_free(failure);
        return std::nullopt;
    }
    if (stored) {
        const QString value = QString::fromUtf8(stored);
        secret_password_free(stored);
        return value;
    }
#endif
    if (error) *error = QStringLiteral("Enter the password for this connection.");
    return std::nullopt;
}
