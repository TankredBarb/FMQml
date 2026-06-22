#include "MegaClient.h"
#include "MegaCache.h"
#include "MegaPath.h"

using namespace mega;

#include <QDebug>
#include <QDir>
#include <QEventLoop>

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
        // Initialize MEGA API with a generic app key, one per linkId
        api = new MegaApi("FMQml");
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

    qDebug() << "[MegaClient] Requesting public node for linkId:" << linkId << "isFolder:" << isFolder;

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

void MegaClient::startDownload(const QString &path, const QString &localPath)
{
    QString megaHandleStr = MegaCache::getMegaHandle(path).value_or(QString{});
    if (megaHandleStr.isEmpty()) {
        qWarning() << "[MegaClient] Mega handle not found in cache for download path:" << path;
        emit downloadFinished(path, false, QStringLiteral("Node handle not found in cache"));
        return;
    }

    bool ok = false;
    uint64_t handle = megaHandleStr.toULongLong(&ok);
    if (!ok) {
        qWarning() << "[MegaClient] Invalid handle format for download path:" << path;
        emit downloadFinished(path, false, QStringLiteral("Invalid node handle format"));
        return;
    }

    QString linkId = MegaPath::linkIdForPath(path);
    if (linkId.isEmpty()) {
        qWarning() << "[MegaClient] Could not determine linkId from path:" << path;
        emit downloadFinished(path, false, QStringLiteral("Invalid path: no linkId"));
        return;
    }

    MegaApi *api = sessionForLink(linkId);
    if (!api) {
        qWarning() << "[MegaClient] Session not found for linkId:" << linkId;
        emit downloadFinished(path, false, QStringLiteral("No session for linkId"));
        return;
    }

    MegaNode *node = api->getNodeByHandle(handle);
    if (!node) {
        qWarning() << "[MegaClient] MegaNode not found for handle:" << handle << "in session:" << linkId;
        emit downloadFinished(path, false, QStringLiteral("Node not found in SDK database"));
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_activeDownloads.insert(handle, path);
    }

    qDebug() << "[MegaClient] Starting download of" << path << "to" << localPath;

    api->startDownload(node, localPath.toUtf8().constData(), nullptr, nullptr, false, nullptr,
                       MegaTransfer::COLLISION_CHECK_ASSUMEDIFFERENT,
                       MegaTransfer::COLLISION_RESOLUTION_OVERWRITE, false, this);

    delete node; // SDK returned a copy of node, we must delete it
}

void MegaClient::cancelAll()
{
    QMutexLocker locker(&m_mutex);
    for (MegaApi *api : m_sessions) {
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

        qDebug() << "[MegaClient] getPublicNode finished for linkId:" << linkId << "success:" << success;

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

        qDebug() << "[MegaClient] loginToFolder finished for linkId:" << linkId << "success:" << success;

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

        qDebug() << "[MegaClient] fetchNodes finished for linkId:" << linkId << "success:" << success;

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
    qDebug() << "[MegaClient] Transfer start for handle:" << transfer->getNodeHandle() << "localPath:" << transfer->getPath();
}

void MegaClient::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    Q_UNUSED(api)
    uint64_t handle = transfer->getNodeHandle();

    QString virtualPath;
    {
        QMutexLocker locker(&m_mutex);
        virtualPath = m_activeDownloads.take(handle);
    }

    if (virtualPath.isEmpty()) {
        return;
    }

    bool success = (error->getErrorCode() == MegaError::API_OK);
    QString errorString = QString::fromUtf8(error->getErrorString());

    qDebug() << "[MegaClient] Transfer finished for" << virtualPath << "success:" << success << "error:" << errorString;
    emit downloadFinished(virtualPath, success, errorString);
}

void MegaClient::onTransferUpdate(MegaApi *api, MegaTransfer *transfer)
{
    Q_UNUSED(api)
    uint64_t handle = transfer->getNodeHandle();

    QString virtualPath;
    {
        QMutexLocker locker(&m_mutex);
        virtualPath = m_activeDownloads.value(handle);
    }

    if (virtualPath.isEmpty()) {
        return;
    }

    qint64 processed = transfer->getTransferredBytes();
    qint64 total = transfer->getTotalBytes();

    emit downloadProgress(virtualPath, processed, total);
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
