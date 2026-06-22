#include "MegaClient.h"
#include "MegaCache.h"
#include "MegaPath.h"

using namespace mega;

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QList>
#include <QMetaObject>
#include <QSet>
#include <QStandardPaths>

namespace {

QString megaSdkStateRoot()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.isEmpty()) {
        base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        if (!base.isEmpty()) {
            base = QDir(base).filePath(QStringLiteral("FMQml"));
        }
    }
    if (base.isEmpty()) {
        base = QDir::tempPath();
    }

    const QString root = QDir(base).filePath(QStringLiteral("mega-sdk"));
    QDir().mkpath(root);
    return QDir::fromNativeSeparators(root);
}

} // namespace

MegaClient &MegaClient::instance()
{
    static MegaClient client;
    return client;
}

MegaClient::MegaClient(QObject *parent)
    : QObject(parent)
{
}

MegaClient::~MegaClient()
{
    QMutexLocker locker(&m_mutex);
    for (MegaApi *api : m_sessions) {
        api->removeListener(this);
        delete api;
    }
    m_sessions.clear();
}

MegaApi *MegaClient::sessionForLink(const QString &linkId)
{
    QMutexLocker locker(&m_mutex);
    MegaApi *api = m_sessions.value(linkId);
    if (!api) {
        // Initialize MEGA API with a generic app key, one per linkId.  Pass an
        // explicit cache/state directory so the SDK does not create
        // megaclient_state_cache* files in the process working directory.
        const QString stateRoot = megaSdkStateRoot();
        const QByteArray stateRootBytes = stateRoot.toUtf8();
        api = new MegaApi("FMQml", stateRootBytes.constData());
        api->addListener(this);
        m_sessions.insert(linkId, api);
    }
    return api;
}

int MegaClient::getPublicNode(const QString &linkId)
{
    bool isFolder = false;
    QString key = MegaCache::retrieveKey(linkId, &isFolder);
    if (key.isEmpty()) {
        qWarning() << "[MegaClient] Decryption key not found in cache for link:" << linkId;
        return -1;
    }

    QString url;
    if (isFolder) {
        url = QStringLiteral("https://mega.nz/folder/%1#%2").arg(linkId, key);
    } else {
        url = QStringLiteral("https://mega.nz/file/%1#%2").arg(linkId, key);
    }


    MegaApi *api = sessionForLink(linkId);
    if (!api) {
        return -1;
    }

    MegaCache::markLinkLoading(linkId);
    if (isFolder) {
        api->loginToFolder(url.toUtf8().constData());
    } else {
        api->getPublicNode(url.toUtf8().constData());
    }

    return 0; // Request initiated
}

qint64 MegaClient::startDownload(const QString &path, const QString &localPath)
{
    qint64 requestId = 0;
    {
        QMutexLocker locker(&m_mutex);
        requestId = ++m_nextDownloadRequestId;
    }

    auto finishFailed = [this, requestId, path](const QString &error) {
        qWarning() << "[MegaClient] Download request failed before SDK start"
                   << "request:" << requestId
                   << "path:" << path
                   << "error:" << error;
        QMetaObject::invokeMethod(this, [this, requestId, path, error]() {
            emit downloadFinished(requestId, path, false, error);
        }, Qt::QueuedConnection);
    };

    QString megaHandleStr = MegaCache::getMegaHandle(path).value_or(QString{});
    if (megaHandleStr.isEmpty()) {
        qWarning() << "[MegaClient] Mega handle not found in cache for download path:" << path;
        finishFailed(QStringLiteral("Node handle not found in cache"));
        return requestId;
    }

    bool ok = false;
    uint64_t handle = megaHandleStr.toULongLong(&ok);
    if (!ok) {
        qWarning() << "[MegaClient] Invalid handle format for download path:" << path;
        finishFailed(QStringLiteral("Invalid node handle format"));
        return requestId;
    }

    QString linkId = MegaPath::linkIdForPath(path);
    if (linkId.isEmpty()) {
        qWarning() << "[MegaClient] Could not determine linkId from path:" << path;
        finishFailed(QStringLiteral("Invalid path: no linkId"));
        return requestId;
    }

    MegaApi *api = sessionForLink(linkId);
    if (!api) {
        qWarning() << "[MegaClient] Session not found for linkId:" << linkId;
        finishFailed(QStringLiteral("No session for linkId"));
        return requestId;
    }

    MegaNode *node = api->getNodeByHandle(handle);
    if (!node) {
        qWarning() << "[MegaClient] MegaNode not found for handle:" << handle << "in session:" << linkId;
        finishFailed(QStringLiteral("Node not found in SDK database"));
        return requestId;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_pendingDownloadsByLocalPath.insert(localPath, DownloadRequest{requestId, path});
        m_cancelledDownloads.remove(requestId);
    }


    // Do not pass this as an extra per-transfer listener: this object is already
    // registered as the session listener, and double registration produces
    // duplicate callbacks for the same SDK transfer.
    api->startDownload(node, localPath.toUtf8().constData(), nullptr, nullptr, false, nullptr,
                       MegaTransfer::COLLISION_CHECK_ASSUMEDIFFERENT,
                       MegaTransfer::COLLISION_RESOLUTION_OVERWRITE, false, nullptr);

    delete node; // SDK returned a copy of node, we must delete it
    return requestId;
}

void MegaClient::cancelAll()
{
    QList<MegaApi *> sessions;
    {
        QMutexLocker locker(&m_mutex);
        const auto activeRequests = m_activeDownloads;
        const auto pendingRequests = m_pendingDownloadsByLocalPath;
        for (const DownloadRequest &request : activeRequests) {
            m_cancelledDownloads.insert(request.id);
        }
        for (const DownloadRequest &request : pendingRequests) {
            m_cancelledDownloads.insert(request.id);
        }
        sessions = m_sessions.values();
        qWarning() << "[MegaClient] cancelAll marked downloads"
                   << "active:" << activeRequests.size()
                   << "pending:" << pendingRequests.size()
                   << "cancelledSet:" << m_cancelledDownloads.size()
                   << "sessions:" << sessions.size();
    }

    for (MegaApi *api : sessions) {
        api->cancelTransfers(MegaTransfer::TYPE_DOWNLOAD);
    }
}

// MegaListener callbacks
void MegaClient::onRequestStart(MegaApi *api, MegaRequest *request)
{
    Q_UNUSED(api)
    Q_UNUSED(request)
}

void MegaClient::onRequestFinish(MegaApi *api, MegaRequest *request, MegaError *e)
{
    // Find the linkId corresponding to this api pointer
    QString linkId;
    {
        QMutexLocker locker(&m_mutex);
        for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
            if (it.value() == api) {
                linkId = it.key();
                break;
            }
        }
    }

    // If not found in sessions, try to parse from the link URL if available
    if (linkId.isEmpty() && request->getLink() != nullptr) {
        QString url = QString::fromUtf8(request->getLink());
        QString linkKey;
        bool isFolder = false;
        MegaPath::fromUserInput(url, linkId, linkKey, isFolder);
    }

    if (linkId.isEmpty()) {
        return;
    }

    if (request->getType() == MegaRequest::TYPE_GET_PUBLIC_NODE) {
        bool success = (e->getErrorCode() == MegaError::API_OK);
        QString errorString = QString::fromUtf8(e->getErrorString());


        if (success) {
            MegaNode *rootNode = request->getPublicMegaNode();

            if (rootNode) {
                // Clear existing cache for this link path
                QString rootVirtualPath = QStringLiteral("mega://link/") + linkId;
                MegaCache::removeSubtree(rootVirtualPath);

                // Recursively traverse and cache all nodes in this public folder/file
                traverseAndCache(api, rootNode, QString{}, linkId);

                delete rootNode;
            } else {
                success = false;
                errorString = QStringLiteral("Failed to retrieve public node tree");
            }
        }

        MegaCache::markLinkLoaded(linkId, success, errorString);
        emit publicLinkLoaded(linkId, success, errorString);
    }
    else if (request->getType() == MegaRequest::TYPE_LOGIN) {
        // loginToFolder completed
        bool success = (e->getErrorCode() == MegaError::API_OK);
        QString errorString = QString::fromUtf8(e->getErrorString());


        if (success) {
            api->fetchNodes();
        } else {
            MegaCache::markLinkLoaded(linkId, false, errorString);
            emit publicLinkLoaded(linkId, false, errorString);
        }
    }
    else if (request->getType() == MegaRequest::TYPE_FETCH_NODES) {
        bool success = (e->getErrorCode() == MegaError::API_OK);
        QString errorString = QString::fromUtf8(e->getErrorString());


        if (success) {
            MegaNode *rootNode = api->getRootNode();

            if (rootNode) {
                // Clear existing cache for this link path
                QString rootVirtualPath = QStringLiteral("mega://link/") + linkId;
                MegaCache::removeSubtree(rootVirtualPath);

                // Recursively traverse and cache all nodes in this public folder
                traverseAndCache(api, rootNode, QString{}, linkId);

                delete rootNode;
            } else {
                success = false;
                errorString = QStringLiteral("Failed to retrieve public folder root node");
            }
        }

        MegaCache::markLinkLoaded(linkId, success, errorString);
        emit publicLinkLoaded(linkId, success, errorString);
    }
}

void MegaClient::onRequestUpdate(MegaApi *api, MegaRequest *request)
{
    Q_UNUSED(api)
    Q_UNUSED(request)
}

void MegaClient::onRequestTemporaryError(MegaApi *api, MegaRequest *request, MegaError *e)
{
    Q_UNUSED(api)
    Q_UNUSED(request)
    Q_UNUSED(e)
}

// MegaTransferListener callbacks
void MegaClient::onTransferStart(MegaApi *api, MegaTransfer *transfer)
{
    Q_UNUSED(api)

    const QString localPath = QString::fromUtf8(transfer->getPath());
    DownloadRequest request;
    {
        QMutexLocker locker(&m_mutex);
        request = m_pendingDownloadsByLocalPath.take(localPath);
        if (request.id != 0) {
            m_activeDownloads.insert(transfer->getTag(), request);
        }
    }

    if (request.id == 0) {
        qWarning() << "[MegaClient] Transfer start without pending request"
                   << "tag:" << transfer->getTag()
                   << "handle:" << transfer->getNodeHandle()
                   << "localPath:" << transfer->getPath();
    }

}

void MegaClient::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    Q_UNUSED(api)
    DownloadRequest request;
    const QString localPath = QString::fromUtf8(transfer->getPath());
    bool wasCancelled = false;
    {
        QMutexLocker locker(&m_mutex);
        request = m_activeDownloads.take(transfer->getTag());
        if (request.id == 0) {
            request = m_pendingDownloadsByLocalPath.take(localPath);
        }
        wasCancelled = request.id != 0 && m_cancelledDownloads.remove(request.id);
    }

    if (request.id == 0 || request.path.isEmpty()) {
        qWarning() << "[MegaClient] Transfer finish without tracked request"
                   << "tag:" << transfer->getTag()
                   << "handle:" << transfer->getNodeHandle()
                   << "localPath:" << transfer->getPath()
                   << "error:" << error->getErrorString();
        return;
    }

    bool success = (error->getErrorCode() == MegaError::API_OK) && !wasCancelled;
    QString errorString = QString::fromUtf8(error->getErrorString());

    emit downloadFinished(request.id, request.path, success, errorString);
}

void MegaClient::onTransferUpdate(MegaApi *api, MegaTransfer *transfer)
{
    Q_UNUSED(api)
    DownloadRequest request;
    {
        QMutexLocker locker(&m_mutex);
        request = m_activeDownloads.value(transfer->getTag());
    }

    if (request.id == 0 || request.path.isEmpty()) {
        qWarning() << "[MegaClient] Transfer update without tracked request"
                   << "tag:" << transfer->getTag()
                   << "handle:" << transfer->getNodeHandle()
                   << "localPath:" << transfer->getPath();
        return;
    }

    qint64 processed = transfer->getTransferredBytes();
    qint64 total = transfer->getTotalBytes();


    emit downloadProgress(request.id, request.path, processed, total);
}

void MegaClient::onTransferTemporaryError(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    Q_UNUSED(api)
    qWarning() << "[MegaClient] Transfer temporary error for handle:" << transfer->getNodeHandle()
               << "localPath:" << transfer->getPath() << "error:" << error->getErrorString();
}

void MegaClient::traverseAndCache(MegaApi *api, MegaNode *node, const QString &parentVirtualPath, const QString &linkId)
{
    if (!node) {
        return;
    }

    FileEntry entry;
    entry.name = QString::fromUtf8(node->getName());

    // For the root node of the link, its name might be empty, use linkId or fallback
    if (entry.name.isEmpty()) {
        if (parentVirtualPath.isEmpty()) {
            entry.name = linkId;
        } else {
            entry.name = QStringLiteral("unnamed");
        }
    }

    entry.isDirectory = (node->getType() != MegaNode::TYPE_FILE);
    entry.size = node->getSize();
    const int suffixIndex = entry.name.lastIndexOf(QLatin1Char('.'));
    entry.suffix = (!entry.isDirectory && suffixIndex >= 0) ? entry.name.mid(suffixIndex + 1).toLower() : QString{};
    entry.isReadOnly = true;
    static const QSet<QString> imageSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"),
        QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("heic"), QStringLiteral("heif")
    };
    static const QSet<QString> previewSuffixes = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"), QStringLiteral("gif"),
        QStringLiteral("bmp"), QStringLiteral("webp"), QStringLiteral("tif"), QStringLiteral("tiff"),
        QStringLiteral("heic"), QStringLiteral("heif"), QStringLiteral("svg"), QStringLiteral("mp4"),
        QStringLiteral("mov"), QStringLiteral("m4v"), QStringLiteral("mkv"), QStringLiteral("webm"),
        QStringLiteral("avi")
    };
    entry.isImage = !entry.isDirectory && imageSuffixes.contains(entry.suffix);
    entry.hasThumbnail = !entry.isDirectory && previewSuffixes.contains(entry.suffix);
    entry.modified = QDateTime::fromSecsSinceEpoch(node->getModificationTime());
    entry.iconName = entry.isDirectory ? QStringLiteral("folder") : QString{};

    QString virtualPath;
    if (parentVirtualPath.isEmpty()) {
        virtualPath = QStringLiteral("mega://link/") + linkId;
    } else {
        virtualPath = parentVirtualPath + QLatin1Char('/') + entry.name;
    }
    entry.path = virtualPath;

    // Store in cache
    QString handleStr = QString::number(node->getHandle());
    MegaCache::cacheEntry(virtualPath, entry, handleStr);
    if (entry.isDirectory) {
        MegaNodeList *childrenList = api->getChildren(node);

        if (childrenList) {
            QStringList childPaths;
            childPaths.reserve(childrenList->size());

            for (int i = 0; i < childrenList->size(); ++i) {
                MegaNode *child = childrenList->get(i);
                if (child) {
                    QString childName = QString::fromUtf8(child->getName());
                    if (childName.isEmpty()) {
                        childName = QStringLiteral("unnamed");
                    }
                    childPaths.append(virtualPath + QLatin1Char('/') + childName);

                    // Recursive call to cache grandchildren
                    traverseAndCache(api, child, virtualPath, linkId);
                }
            }

            MegaCache::cacheChildren(virtualPath, childPaths);
            delete childrenList; // deletes list but not nodes
        }
    }
}
