#pragma once

#include "SftpClient.h"
#include <QByteArray>
#include <QDebug>
#include <QFileInfo>
#include <QProcess>
#include <QElapsedTimer>
#include <QStandardPaths>
#include <QtEndian>
#include <mutex>
#include <libssh/callbacks.h>
#include <libssh/server.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>

namespace {
// Real loopback SSH/SFTP; all host-key writes use a temporary known_hosts.
int initializeServer(sftp_session session)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return sftp_server_init(session);
#pragma GCC diagnostic pop
}

int generateKey(ssh_key *key)
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    return ssh_pki_generate(SSH_KEYTYPE_ED25519, 0, key);
#pragma GCC diagnostic pop
}

class TestServer {
public:
    explicit TestServer(bool offerSftp = true, bool stallList = false, bool fileBackend = false,
                        int dropAfterWrites = 0, bool hideRename = false, bool readOnly = false,
                        int firstWriteReplyDelayMs = 0, int firstReadReplyDelayMs = 0,
                        int dropAfterReads = 0, int mutationReplyDelayMs = 0, bool shortFirstRead = false)
        : m_offerSftp(offerSftp), m_stallList(stallList), m_fileBackend(fileBackend),
          m_dropAfterWrites(dropAfterWrites), m_hideRename(hideRename), m_readOnly(readOnly),
          m_firstWriteReplyDelayMs(firstWriteReplyDelayMs), m_firstReadReplyDelayMs(firstReadReplyDelayMs),
          m_dropAfterReads(dropAfterReads), m_mutationReplyDelayMs(mutationReplyDelayMs), m_shortFirstRead(shortFirstRead)
    {
        m_bind = ssh_bind_new();
        ssh_key key = nullptr;
        generateKey(&key);
        ssh_bind_options_set(m_bind, SSH_BIND_OPTIONS_IMPORT_KEY, key);
        const unsigned int port = 0;
        const bool config = false;
        ssh_bind_options_set(m_bind, SSH_BIND_OPTIONS_PROCESS_CONFIG, &config);
        ssh_bind_options_set(m_bind, SSH_BIND_OPTIONS_BINDADDR, "127.0.0.1");
        ssh_bind_options_set(m_bind, SSH_BIND_OPTIONS_BINDPORT, &port);
        if (ssh_bind_listen(m_bind) != SSH_OK) {
            qWarning() << "Loopback SSH fixture:" << ssh_get_error(m_bind);
            return;
        }
        sockaddr_in address{};
        socklen_t length = sizeof(address);
        if (getsockname(ssh_bind_get_fd(m_bind), reinterpret_cast<sockaddr *>(&address), &length) < 0) return;
        m_port = ntohs(address.sin_port);
        m_worker = std::jthread([this](std::stop_token stop) {
            while (!stop.stop_requested()) {
                pollfd descriptor{ssh_bind_get_fd(m_bind), POLLIN, 0};
                if (::poll(&descriptor, 1, 50) <= 0) continue;
                ssh_session session = ssh_new();
                if (ssh_bind_accept(m_bind, session) != SSH_OK) { ssh_free(session); continue; }
                {
                    std::lock_guard lock(m_socketMutex);
                    m_socket = ssh_get_fd(session);
                }
                serve(session);
                {
                    std::lock_guard lock(m_socketMutex);
                    m_socket = SSH_INVALID_SOCKET;
                    ssh_disconnect(session);
                    ssh_free(session);
                }
            }
        });
    }
    ~TestServer()
    {
        if (m_worker.joinable()) {
            m_worker.request_stop();
            {
                std::lock_guard lock(m_socketMutex);
                if (m_socket != SSH_INVALID_SOCKET) ::shutdown(m_socket, SHUT_RDWR);
            }
            m_worker.join();
        }
        ssh_bind_free(m_bind);
    }
    int port() const { return m_port; }
    std::atomic_bool stallHandshake{false};
    std::atomic_bool handshakeStarted{false};
    std::atomic_int passwordRequests{0};
    std::atomic_bool listingStarted{false};
    std::atomic_int writesBeforeFirstReply{0};
    std::atomic_int readsBeforeFirstReply{0};
    std::atomic_bool mutationStarted{false};

private:
    struct Session {
        TestServer *server;
        ssh_channel channel = nullptr;
        bool subsystem = false;
        ssh_channel_callbacks_struct channelCallbacks{};
    };
    static int password(ssh_session, const char *user, const char *value, void *data)
    {
        ++static_cast<Session *>(data)->server->passwordRequests;
        return QByteArray(user) == "tester" && QByteArray(value) == "test-password"
            ? SSH_AUTH_SUCCESS : SSH_AUTH_DENIED;
    }
    static int subsystem(ssh_session, ssh_channel, const char *name, void *data)
    {
        auto *state = static_cast<Session *>(data);
        if (!state->server->m_offerSftp || QByteArray(name) != "sftp") return SSH_ERROR;
        state->subsystem = true;
        return SSH_OK;
    }
    static ssh_channel openChannel(ssh_session session, void *data)
    {
        auto *state = static_cast<Session *>(data);
        state->channel = ssh_channel_new(session);
        ssh_callbacks_init(&state->channelCallbacks);
        state->channelCallbacks.userdata = state;
        state->channelCallbacks.channel_subsystem_request_function = subsystem;
        ssh_set_channel_callbacks(state->channel, &state->channelCallbacks);
        return state->channel;
    }
    void serve(ssh_session session)
    {
        handshakeStarted.store(true);
        while (stallHandshake.load() && !m_worker.get_stop_token().stop_requested())
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (m_worker.get_stop_token().stop_requested()) return;
        const long timeout = 3;
        ssh_options_set(session, SSH_OPTIONS_TIMEOUT, &timeout);
        Session state{this};
        ssh_server_callbacks_struct callbacks{};
        ssh_callbacks_init(&callbacks);
        callbacks.userdata = &state;
        callbacks.auth_password_function = password;
        callbacks.channel_open_request_session_function = openChannel;
        ssh_set_server_callbacks(session, &callbacks);
        ssh_set_auth_methods(session, SSH_AUTH_METHOD_PASSWORD);
        if (ssh_handle_key_exchange(session) != SSH_OK) return;
        ssh_event event = ssh_event_new();
        ssh_event_add_session(event, session);
        while (ssh_is_connected(session) && !state.subsystem) {
            if (ssh_event_dopoll(event, 100) == SSH_ERROR) break;
        }
        ssh_event_remove_session(event, session);
        ssh_event_free(event);
        if (state.subsystem) {
            if (m_fileBackend) serveProcess(session, state.channel);
            else {
                sftp_session sftp = sftp_server_new(session, state.channel);
                if (initializeServer(sftp) == SSH_OK) serveFiles(sftp);
                sftp_server_free(sftp);
            }
        }
        if (state.channel) ssh_channel_free(state.channel);
    }
    void serveProcess(ssh_session session, ssh_channel channel)
    {
        QString executable;
        for (const QString &candidate : {QStringLiteral("/usr/lib/ssh/sftp-server"), QStringLiteral("/usr/lib/openssh/sftp-server")}) {
            if (QFileInfo::exists(candidate)) { executable = candidate; break; }
        }
        if (executable.isEmpty()) executable = QStandardPaths::findExecutable(QStringLiteral("sftp-server"));
        QProcess process;
        QStringList arguments{QStringLiteral("-e")};
        if (m_readOnly) arguments.append(QStringLiteral("-R"));
        process.start(executable, arguments);
        if (!process.waitForStarted(1000)) return;
        QByteArray requests;
        QByteArray responses;
        bool initialVersion = true;
        int writes = 0;
        int reads = 0;
        QElapsedTimer firstWriteWait;
        QElapsedTimer firstReadWait;
        QElapsedTimer mutationWait;
        bool firstWriteReplySent = false;
        bool firstReadReplySent = false;
        bool shortReadSent = false;
        QByteArray buffer(65536, Qt::Uninitialized);
        while (ssh_is_connected(session) && !ssh_channel_is_eof(channel)) {
            const int count = ssh_channel_read_timeout(channel, buffer.data(), buffer.size(), 0, 10);
            if (count < 0 && count != SSH_AGAIN) break;
            if (count > 0) {
                if (m_dropAfterWrites > 0 || m_firstWriteReplyDelayMs > 0 || m_firstReadReplyDelayMs > 0
                    || m_dropAfterReads > 0 || m_mutationReplyDelayMs > 0) {
                    requests.append(buffer.constData(), count);
                    while (requests.size() >= 5) {
                        const quint32 length = qFromBigEndian<quint32>(requests.constData());
                        if (requests.size() < length + 4) break;
                        if (static_cast<unsigned char>(requests.at(4)) == SSH_FXP_WRITE) {
                            if (++writes == 1) firstWriteWait.start();
                            if (m_dropAfterWrites > 0 && writes >= m_dropAfterWrites) {
                                ::shutdown(ssh_get_fd(session), SHUT_RDWR);
                                process.kill();
                                process.waitForFinished(1000);
                                return;
                            }
                        }
                        const auto type = static_cast<unsigned char>(requests.at(4));
                        if (type == SSH_FXP_READ) {
                            if (++reads == 1) firstReadWait.start();
                            if (m_dropAfterReads > 0 && reads >= m_dropAfterReads) {
                                ::shutdown(ssh_get_fd(session), SHUT_RDWR);
                                process.kill();
                                process.waitForFinished(1000);
                                return;
                            }
                        }
                        if (type == SSH_FXP_MKDIR || type == SSH_FXP_RENAME || type == SSH_FXP_OPEN) {
                            mutationStarted.store(true);
                            mutationWait.start();
                        }
                        requests.remove(0, length + 4);
                    }
                }
                process.write(buffer.constData(), count);
                process.waitForBytesWritten(1000);
            }
            process.waitForReadyRead(10);
            responses += process.readAllStandardOutput();
            process.readAllStandardError();
            if (mutationWait.isValid() && mutationWait.elapsed() < m_mutationReplyDelayMs) continue;
            if (!firstReadReplySent && firstReadWait.isValid() && !responses.isEmpty()) {
                if (firstReadWait.elapsed() < m_firstReadReplyDelayMs) continue;
                readsBeforeFirstReply.store(reads);
                firstReadReplySent = true;
            }
            if (m_shortFirstRead && !shortReadSent) {
                qsizetype offset = 0;
                while (responses.size() >= offset + 5) {
                    const quint32 length = qFromBigEndian<quint32>(responses.constData() + offset);
                    if (responses.size() < offset + length + 4) break;
                    if (static_cast<unsigned char>(responses.at(offset + 4)) == SSH_FXP_DATA && length > 9 + 1024) {
                        QByteArray shortened = responses.mid(offset, 13 + 1024);
                        qToBigEndian<quint32>(9 + 1024, shortened.data());
                        qToBigEndian<quint32>(1024, shortened.data() + 9);
                        responses.replace(offset, length + 4, shortened);
                        shortReadSent = true;
                        break;
                    }
                    offset += length + 4;
                }
            }
            if (!firstWriteReplySent && firstWriteWait.isValid() && !responses.isEmpty()) {
                if (firstWriteWait.elapsed() < m_firstWriteReplyDelayMs) continue;
                writesBeforeFirstReply.store(writes);
                firstWriteReplySent = true;
            }
            if (initialVersion && m_hideRename) {
                if (responses.size() < 5) continue;
                const quint32 length = qFromBigEndian<quint32>(responses.constData());
                if (responses.size() < length + 4) continue;
                responses.replace(0, length + 4, QByteArray::fromHex("000000050200000003"));
            }
            if (!responses.isEmpty()) initialVersion = false;
            qsizetype sent = 0;
            while (sent < responses.size()) {
                const int count = ssh_channel_write(channel, responses.constData() + sent,
                                                     std::min<qsizetype>(32768, responses.size() - sent));
                if (count <= 0) break;
                sent += count;
            }
            responses.remove(0, sent);
        }
        process.kill();
        process.waitForFinished(1000);
    }
    void serveFiles(sftp_session sftp)
    {
        bool listed = false;
        while (sftp_client_message message = sftp_get_client_message(sftp)) {
            sftp_attributes_struct attrs{};
            attrs.flags = SSH_FILEXFER_ATTR_PERMISSIONS | SSH_FILEXFER_ATTR_SIZE | SSH_FILEXFER_ATTR_ACMODTIME;
            attrs.permissions = S_IFDIR | 0755;
            attrs.mtime = 1700000000;
            switch (sftp_client_message_get_type(message)) {
            case SSH_FXP_LSTAT:
                if (QByteArray(sftp_client_message_get_filename(message)) == "/missing")
                    sftp_reply_status(message, SSH_FX_NO_SUCH_FILE, "not found");
                else sftp_reply_attr(message, &attrs);
                break;
            case SSH_FXP_OPENDIR: {
                listed = false;
                ssh_string handle = ssh_string_from_char("folder");
                sftp_reply_handle(message, handle);
                ssh_string_free(handle);
                break;
            }
            case SSH_FXP_READDIR:
                listingStarted.store(true);
                if (m_stallList) {
                    char byte;
                    ::recv(ssh_get_fd(sftp->session), &byte, 1, 0);
                } else if (listed) sftp_reply_status(message, SSH_FX_EOF, "end");
                else {
                    sftp_reply_names_add(message, ".", ".", &attrs);
                    sftp_reply_names_add(message, "folder", "folder", &attrs);
                    attrs.permissions = S_IFREG | 0644;
                    attrs.size = 123;
                    sftp_reply_names_add(message, "données #?.txt", "file", &attrs);
                    sftp_reply_names_add(message, ".hidden", ".hidden", &attrs);
                    attrs.permissions = S_IFLNK | 0777;
                    sftp_reply_names_add(message, "link", "link", &attrs);
                    sftp_reply_names(message);
                    listed = true;
                }
                break;
            case SSH_FXP_CLOSE: sftp_reply_status(message, SSH_FX_OK, "closed"); break;
            default: sftp_reply_status(message, SSH_FX_OP_UNSUPPORTED, "unsupported"); break;
            }
            sftp_client_message_free(message);
        }
    }
    bool m_offerSftp;
    bool m_stallList;
    bool m_fileBackend;
    int m_dropAfterWrites;
    bool m_hideRename;
    bool m_readOnly;
    int m_firstWriteReplyDelayMs;
    int m_firstReadReplyDelayMs;
    int m_dropAfterReads;
    int m_mutationReplyDelayMs;
    bool m_shortFirstRead;
    ssh_bind m_bind = nullptr;
    int m_port = 0;
    std::jthread m_worker;
    std::mutex m_socketMutex;
    socket_t m_socket = SSH_INVALID_SOCKET;
};

SftpCredentials credentialsFor(const TestServer &server)
{
    return {SftpPath::fromFields(QStringLiteral("127.0.0.1"), server.port(), QStringLiteral("tester"), QStringLiteral("/")),
            QStringLiteral("test-password"), {}};
}
QString confirm(TestServer &server, const SftpCredentials &credentials, const QString &knownHosts)
{
    SftpClient client({}, knownHosts);
    if (client.connect(credentials) || !client.needsHostConfirmation() || server.passwordRequests.load() != 0) return {};
    return client.fingerprint();
}
}

