#pragma once

#include "MegaClientInterface.h"
#include <QHash>
#include <QMutex>
#include <QString>
#include <QDateTime>
#include <QSet>

#include <megaapi.h>

using namespace mega;

class MegaClient final : public MegaClientInterface, public MegaListener, public MegaRequestListener, public MegaTransferListener
{
    Q_OBJECT

public:
    static MegaClient &instance();

    MegaApi *sessionForLink(const QString &linkId);

    // Asynchronous request to get a public node (file or folder). Returns 0 on success.
    int getPublicNode(const QString &linkId) override;

    int loginToAccount(const QString &email, const QString &password) override;
    int resumeAccountSession(const QString &session) override;
    bool logoutAccount(QString *errorString = nullptr) override;
    bool isAccountAuthenticated() const override;
    QString accountEmail() const override;
    QString accountSessionToken() const override;
    int loadAccountRoot() override;

    // Asynchronous transfer to download a file. Returns a stable request id used
    // to match progress/finish callbacks when the same virtual path is downloaded
    // concurrently by thumbnails, previews, and Quick Look.
    qint64 startDownload(const QString &path, const QString &localPath) override;

    // Cancel transfers
    void cancelAll() override;

private:
    explicit MegaClient(QObject *parent = nullptr);
    ~MegaClient() override;

    MegaClient(const MegaClient &) = delete;
    MegaClient &operator=(const MegaClient &) = delete;

    // MegaListener callbacks
    void onRequestStart(MegaApi *api, MegaRequest *request) override;
    void onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e) override;
    void onRequestUpdate(MegaApi *api, MegaRequest *request) override;
    void onRequestTemporaryError(MegaApi *api, MegaRequest *request, MegaError *e) override;

    // MegaTransferListener callbacks
    void onTransferStart(MegaApi *api, MegaTransfer *transfer) override;
    void onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error) override;
    void onTransferUpdate(MegaApi *api, MegaTransfer *transfer) override;
    void onTransferTemporaryError(MegaApi *api, MegaTransfer *transfer, MegaError *error) override;
    bool onTransferData(MegaApi *api, MegaTransfer *transfer, char *buffer, size_t size) override { return true; }

    MegaApi *accountApiSession();
    void traverseAndCache(MegaApi *api, MegaNode *node, const QString &parentVirtualPath, const QString &linkId);
    void traverseAndCacheAccount(MegaApi *api, MegaNode *node, const QString &virtualPath);
    void clearAccountCache();

    struct DownloadRequest {
        qint64 id = 0;
        QString path;
    };

    mutable QMutex m_mutex;
    // Map of linkId -> MegaApi session
    QHash<QString, MegaApi*> m_sessions;
    MegaApi *m_accountSession = nullptr;
    bool m_accountAuthenticated = false;
    bool m_accountNodesLoaded = false;
    QString m_accountEmail;
    QString m_accountSessionToken;
    // MEGA can run several transfers for the same node handle at once (thumbnail,
    // preview, Quick Look). Track them by SDK transfer tag and stage new transfers
    // by their local .part path until onTransferStart gives us the tag.
    qint64 m_nextDownloadRequestId = 0;
    QHash<int, DownloadRequest> m_activeDownloads;
    QHash<QString, DownloadRequest> m_pendingDownloadsByLocalPath;
    QSet<qint64> m_cancelledDownloads;
};
