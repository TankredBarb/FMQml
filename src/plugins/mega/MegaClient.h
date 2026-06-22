#pragma once

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QDateTime>
#include <QSet>

#include <megaapi.h>

using namespace mega;

class MegaClient final : public QObject, public MegaListener, public MegaRequestListener, public MegaTransferListener
{
    Q_OBJECT

public:
    static MegaClient &instance();

    MegaApi *sessionForLink(const QString &linkId);

    // Asynchronous request to get a public node (file or folder). Returns 0 on success.
    int getPublicNode(const QString &linkId);

    // Asynchronous transfer to download a file. Returns a stable request id used
    // to match progress/finish callbacks when the same virtual path is downloaded
    // concurrently by thumbnails, previews, and Quick Look.
    qint64 startDownload(const QString &path, const QString &localPath);

    // Cancel transfers
    void cancelAll();

signals:
    // Marshaled to the main thread
    void publicLinkLoaded(const QString &linkId, bool success, const QString &errorString);
    void downloadProgress(qint64 requestId, const QString &path, qint64 processedBytes, qint64 totalBytes);
    void downloadFinished(qint64 requestId, const QString &path, bool success, const QString &errorString);

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

    void traverseAndCache(MegaApi *api, MegaNode *node, const QString &parentVirtualPath, const QString &linkId);

    struct DownloadRequest {
        qint64 id = 0;
        QString path;
    };

    QMutex m_mutex;
    // Map of linkId -> MegaApi session
    QHash<QString, MegaApi*> m_sessions;
    // MEGA can run several transfers for the same node handle at once (thumbnail,
    // preview, Quick Look). Track them by SDK transfer tag and stage new transfers
    // by their local .part path until onTransferStart gives us the tag.
    qint64 m_nextDownloadRequestId = 0;
    QHash<int, DownloadRequest> m_activeDownloads;
    QHash<QString, DownloadRequest> m_pendingDownloadsByLocalPath;
    QSet<qint64> m_cancelledDownloads;
};
