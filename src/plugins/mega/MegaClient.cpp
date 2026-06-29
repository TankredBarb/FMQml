#include "MegaClient.h"
#include "MegaPresentation.h"
#include "MegaCache.h"
#include "MegaPath.h"
#include "MegaAuth.h"
#include "MegaDiagnostics.h"

using namespace mega;

#include <QDebug>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QList>
#include <QMetaObject>
#include <QSet>
#include <QStandardPaths>

#include <algorithm>

namespace {

constexpr int DefaultMegaUploadConnectionLimit = 4;
constexpr int MaxMegaUploadConnectionLimit = 4;

QString megaErrorMessage(MegaError *error, const QString &fallbackContext)
{
    if (!error) {
        return fallbackContext;
    }

    const int code = error->getErrorCode();
    if (code == MegaError::API_OK) {
        return {};
    }

    switch (code) {
    case MegaError::API_EARGS:
    case MegaError::API_EKEY:
        return QStringLiteral("Invalid or expired MEGA link");
    case MegaError::API_ENOENT:
        return QStringLiteral("MEGA item was not found");
    case MegaError::API_EACCESS:
    case MegaError::API_ESID:
        return QStringLiteral("MEGA access denied or session expired");
    case MegaError::API_EOVERQUOTA:
        return QStringLiteral("MEGA transfer or storage quota exceeded");
    case MegaError::API_ERATELIMIT:
    case MegaError::API_ETOOMANY:
        return QStringLiteral("MEGA request limit reached; try again later");
    case MegaError::API_EAGAIN:
    case MegaError::API_ETEMPUNAVAIL:
        return QStringLiteral("MEGA is temporarily unavailable; try again later");
    case MegaError::API_EBLOCKED:
        return QStringLiteral("MEGA account or link is blocked");
    case MegaError::API_EREAD:
    case MegaError::API_EWRITE:
        return QStringLiteral("MEGA transfer failed while reading or writing data");
    default:
        break;
    }

    const QString sdkMessage = QString::fromUtf8(error->getErrorString()).trimmed();
    if (!sdkMessage.isEmpty()) {
        return sdkMessage;
    }
    return fallbackContext;
}

QString megaDiagnosticText(const QString &text)
{
    return MegaDiagnostics::redactSensitiveText(text);
}

bool megaClientTimingEnabled()
{
    return qEnvironmentVariableIsSet("FM_MEGA_TIMING");
}

bool megaTransferTemporaryErrorLogEnabled()
{
    return qEnvironmentVariableIsSet("FM_MEGA_TRANSFER_TEMP_ERROR_LOG");
}

bool megaClientUploadItemTimingEnabled()
{
    return qEnvironmentVariableIsSet("FM_MEGA_UPLOAD_ITEM_TIMING");
}

bool megaClientTransferTraceEnabled()
{
    return qEnvironmentVariableIsSet("FM_MEGA_TRANSFER_TRACE");
}

bool megaTemporaryErrorShouldFailTransfer(MegaError *error)
{
    if (!error) {
        return false;
    }

    switch (error->getErrorCode()) {
    case MegaError::API_EOVERQUOTA:
    case MegaError::API_EREAD:
    case MegaError::API_EWRITE:
        return true;
    default:
        break;
    }

    const QString sdkMessage = QString::fromUtf8(error->getErrorString()).trimmed();
    return sdkMessage.compare(QStringLiteral("Failed permanently"), Qt::CaseInsensitive) == 0;
}

int megaUploadConnectionLimit()
{
    bool ok = false;
    const int requested = qEnvironmentVariableIntValue("FMQML_MEGA_UPLOAD_CONCURRENCY", &ok);
    if (!ok) {
        return DefaultMegaUploadConnectionLimit;
    }
    return std::clamp(requested, 1, MaxMegaUploadConnectionLimit);
}

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

QString megaRuntimeStateRoot(const QString &scope, const QString &id)
{
    static const QString runtimeId = QStringLiteral("%1-%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QDateTime::currentMSecsSinceEpoch());
    const QString root = QDir(megaSdkStateRoot()).filePath(
        QStringLiteral("runtime/%1/%2/%3").arg(runtimeId, scope, id));
    QDir().mkpath(root);
    return QDir::fromNativeSeparators(root);
}

QString sdkLocalTransferPath(const QString &path)
{
    // The MEGA SDK performs its own filesystem I/O for transfers.  On Windows it
    // is sensitive to the platform path spelling it receives, so always hand it
    // a native-separator path and use the same spelling for transfer callback
    // correlation.
    return QDir::toNativeSeparators(path);
}

QString sdkTransferCallbackPath(MegaTransfer *transfer)
{
    return sdkLocalTransferPath(QString::fromUtf8(transfer ? transfer->getPath() : ""));
}

} // namespace

MegaClient &MegaClient::instance()
{
    static MegaClient client;
    return client;
}

MegaClientInterface &defaultMegaClient()
{
    return MegaClient::instance();
}

MegaClient::MegaClient(QObject *parent)
    : MegaClientInterface(parent)
    , m_accountStorageUsed(-1)
    , m_accountStorageMax(-1)
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
    if (m_accountSession) {
        m_accountSession->removeListener(this);
        delete m_accountSession;
        m_accountSession = nullptr;
    }
}


MegaApi *MegaClient::accountApiSession()
{
    MegaApi *api = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_accountSession) {
            const QString stateRoot = megaRuntimeStateRoot(QStringLiteral("account"), QStringLiteral("default"));
            const QByteArray stateRootBytes = stateRoot.toUtf8();
            m_accountSession = new MegaApi("FMQml", stateRootBytes.constData());
            m_accountSession->addListener(this);
        }
        api = m_accountSession;
    }
    configureUploadConnections(api);
    return api;
}

void MegaClient::configureUploadConnections(MegaApi *api)
{
    if (!api) {
        return;
    }

    const int connections = megaUploadConnectionLimit();
    bool shouldConfigure = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_uploadConnectionsConfigured != connections) {
            m_uploadConnectionsConfigured = connections;
            shouldConfigure = true;
        }
    }

    if (!shouldConfigure) {
        return;
    }

    api->setMaxConnections(MegaTransfer::TYPE_UPLOAD, connections, nullptr);
    if (megaClientTimingEnabled()) {
        qDebug() << "[MegaTiming] upload max connections set" << connections;
    }
}

int MegaClient::loginToAccount(const QString &email, const QString &password)
{
    const QString trimmedEmail = email.trimmed();
    if (trimmedEmail.isEmpty() || password.isEmpty()) {
        return -1;
    }

    MegaApi *api = accountApiSession();
    if (!api) {
        return -1;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_accountEmail = trimmedEmail;
        m_accountAuthenticated = false;
        m_accountNodesLoaded = false;
        m_accountNodesDirty = false;
        m_accountFetchInProgress = false;
        m_ignoreAccountNodeUpdatesUntilMs = 0;
        m_accountSessionToken.clear();
    }

    api->login(trimmedEmail.toUtf8().constData(), password.toUtf8().constData());
    return 0;
}

int MegaClient::resumeAccountSession(const QString &session)
{
    const QString trimmedSession = session.trimmed();
    if (trimmedSession.isEmpty()) {
        return -1;
    }

    MegaApi *api = accountApiSession();
    if (!api) {
        return -1;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_accountSessionToken = trimmedSession;
        m_accountAuthenticated = false;
        m_accountNodesLoaded = false;
        m_accountNodesDirty = false;
        m_accountFetchInProgress = false;
        m_ignoreAccountNodeUpdatesUntilMs = 0;
        m_accountEmail = MegaAuth::savedEmail();
    }

    api->fastLogin(trimmedSession.toUtf8().constData());
    return 0;
}

bool MegaClient::logoutAccount(QString *errorString)
{
    MegaApi *api = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        api = m_accountSession;
        m_accountAuthenticated = false;
        m_accountNodesLoaded = false;
        m_accountNodesDirty = false;
        m_accountFetchInProgress = false;
        m_ignoreAccountNodeUpdatesUntilMs = 0;
        m_accountEmail.clear();
        m_accountSessionToken.clear();
        m_accountStorageUsed = -1;
        m_accountStorageMax = -1;
    }

    clearAccountCache();
    if (api) {
        api->logout();
    }
    emit accountAuthorizationChanged(false, {}, {});
    if (errorString) {
        errorString->clear();
    }
    return true;
}

bool MegaClient::isAccountAuthenticated() const
{
    QMutexLocker locker(&m_mutex);
    return m_accountAuthenticated;
}

QString MegaClient::accountEmail() const
{
    QMutexLocker locker(&m_mutex);
    return m_accountEmail;
}

qint64 MegaClient::accountStorageUsedBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_accountStorageUsed;
}

qint64 MegaClient::accountStorageMaxBytes() const
{
    QMutexLocker locker(&m_mutex);
    return m_accountStorageMax;
}

void MegaClient::requestAccountDetails()
{
    MegaApi *api = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_accountAuthenticated) {
            return;
        }
        api = m_accountSession;
    }
    if (api) {
        api->getAccountDetails(this);
    }
}

QString MegaClient::accountSessionToken() const
{
    QMutexLocker locker(&m_mutex);
    return m_accountSessionToken;
}

bool MegaClient::hasFreshAccountNodes() const
{
    QMutexLocker locker(&m_mutex);
    return m_accountAuthenticated
        && m_accountNodesLoaded
        && !m_accountNodesDirty
        && !m_accountFetchInProgress;
}

int MegaClient::loadAccountRoot()
{
    MegaApi *api = nullptr;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_accountAuthenticated) {
            return -1;
        }
        if (m_accountNodesLoaded
            && !m_accountNodesDirty
            && MegaCache::getChildren(MegaPath::Root).has_value()) {
            if (megaClientTimingEnabled()) {
                qDebug() << "[MegaTiming] account load cache-hit";
            }
            QMetaObject::invokeMethod(this, [this]() { emit accountNodesLoaded(true, {}); }, Qt::QueuedConnection);
            return 0;
        }
        if (m_accountFetchInProgress) {
            if (megaClientTimingEnabled()) {
                qDebug() << "[MegaTiming] account load already in-flight";
            }
            return 0;
        }
        m_accountFetchInProgress = true;
        if (megaClientTimingEnabled()) {
            m_accountFetchStartMs = QDateTime::currentMSecsSinceEpoch();
        }
        api = m_accountSession;
    }

    if (!api) {
        QMutexLocker locker(&m_mutex);
        m_accountFetchInProgress = false;
        return -1;
    }
    if (megaClientTimingEnabled()) {
        qDebug() << "[MegaTiming] account fetchNodes start";
    }
    api->fetchNodes();
    api->getAccountDetails(this);
    return 0;
}

MegaApi *MegaClient::sessionForLink(const QString &linkId)
{
    QMutexLocker locker(&m_mutex);
    MegaApi *api = m_sessions.value(linkId);
    if (!api) {
        // Initialize MEGA API with a generic app key, one per linkId.  Pass an
        // explicit cache/state directory so the SDK does not create
        // megaclient_state_cache* files in the process working directory.
        const QString stateRoot = megaRuntimeStateRoot(QStringLiteral("link"), linkId);
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
    const QString sdkLocalPath = sdkLocalTransferPath(localPath);
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

    MegaApi *api = nullptr;
    const QString linkId = MegaPath::linkIdForPath(path);
    if (linkId.isEmpty()) {
        api = accountApiSession();
        if (!isAccountAuthenticated()) {
            qWarning() << "[MegaClient] Account download requested while signed out:" << path;
            finishFailed(QStringLiteral("MEGA account is not signed in"));
            return requestId;
        }
    } else {
        api = sessionForLink(linkId);
    }
    if (!api) {
        qWarning() << "[MegaClient] Session not found for download path:" << path;
        finishFailed(QStringLiteral("No MEGA session for path"));
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
        m_pendingDownloadsByLocalPath.insert(sdkLocalPath, DownloadRequest{requestId, path});
        m_cancelledDownloads.remove(requestId);
        if (megaClientTransferTraceEnabled()) {
            qWarning() << "[MegaClient] Download pending inserted"
                       << "this:" << this
                       << "api:" << api
                       << "request:" << requestId
                       << "handle:" << handle
                       << "key:" << megaDiagnosticText(sdkLocalPath)
                       << "pendingDownloads:" << m_pendingDownloadsByLocalPath.size();
        }
    }


    // Do not pass this as an extra per-transfer listener: this object is already
    // registered as the session listener, and double registration produces
    // duplicate callbacks for the same SDK transfer.
    api->startDownload(node, sdkLocalPath.toUtf8().constData(), nullptr, nullptr, false, nullptr,
                       MegaTransfer::COLLISION_CHECK_ASSUMEDIFFERENT,
                       MegaTransfer::COLLISION_RESOLUTION_OVERWRITE, false, nullptr);

    delete node; // SDK returned a copy of node, we must delete it
    return requestId;
}

qint64 MegaClient::startUpload(const QString &sourceFilePath, const QString &destinationPath)
{
    const QString sdkSourceFilePath = sdkLocalTransferPath(sourceFilePath);
    qint64 requestId = 0;
    {
        QMutexLocker locker(&m_mutex);
        requestId = ++m_nextMutationRequestId;
    }

    const QString parentPath = MegaPath::parentPath(destinationPath);
    const QString name = MegaPath::fallbackFileNameForPath(destinationPath);
    const QString parentHandleStr = MegaCache::getMegaHandle(parentPath).value_or(QString{});
    bool ok = false;
    const uint64_t parentHandle = parentHandleStr.toULongLong(&ok);
    MegaApi *api = accountApiSession();
    if (!isAccountAuthenticated() || !ok || !api) {
        QMetaObject::invokeMethod(this, [this, requestId, destinationPath]() {
            emit mutationFinished(requestId, QStringLiteral("upload"), destinationPath, false,
                                  QStringLiteral("MEGA upload destination is not available"), {});
        }, Qt::QueuedConnection);
        return requestId;
    }

    MegaNode *parentNode = api->getNodeByHandle(parentHandle);
    if (!parentNode) {
        QMetaObject::invokeMethod(this, [this, requestId, destinationPath]() {
            emit mutationFinished(requestId, QStringLiteral("upload"), destinationPath, false,
                                  QStringLiteral("MEGA upload parent was not found"), {});
        }, Qt::QueuedConnection);
        return requestId;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_pendingUploadsByLocalPath.insert(sdkSourceFilePath, MutationRequest{requestId, QStringLiteral("upload"), destinationPath, destinationPath});
        if (megaClientTimingEnabled()) {
            m_uploadStartMsByRequestId.insert(requestId, QDateTime::currentMSecsSinceEpoch());
        }
    }
    if (megaClientUploadItemTimingEnabled()) {
        qDebug() << "[MegaTiming] upload start"
                 << "request:" << requestId
                 << "destination:" << megaDiagnosticText(destinationPath)
                 << "sourceBytes:" << QFileInfo(sourceFilePath).size();
    }
    api->startUpload(sdkSourceFilePath.toUtf8().constData(), parentNode, name.toUtf8().constData(),
                     0, nullptr, false, false, nullptr);
    delete parentNode;
    return requestId;
}

qint64 MegaClient::startCreateFolder(const QString &parentPath, const QString &name)
{
    qint64 requestId = 0;
    {
        QMutexLocker locker(&m_mutex);
        requestId = ++m_nextMutationRequestId;
    }
    const QString path = MegaPath::childPath(parentPath, name);
    const QString parentHandleStr = MegaCache::getMegaHandle(parentPath).value_or(QString{});
    bool ok = false;
    const uint64_t parentHandle = parentHandleStr.toULongLong(&ok);
    MegaApi *api = accountApiSession();
    if (!isAccountAuthenticated() || !ok || !api) {
        QMetaObject::invokeMethod(this, [this, requestId, path]() {
            emit mutationFinished(requestId, QStringLiteral("createFolder"), path, false,
                                  QStringLiteral("MEGA folder parent is not available"), {});
        }, Qt::QueuedConnection);
        return requestId;
    }
    MegaNode *parentNode = api->getNodeByHandle(parentHandle);
    if (!parentNode) {
        QMetaObject::invokeMethod(this, [this, requestId, path]() {
            emit mutationFinished(requestId, QStringLiteral("createFolder"), path, false,
                                  QStringLiteral("MEGA folder parent was not found"), {});
        }, Qt::QueuedConnection);
        return requestId;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_pendingRequestsByTag.insert(static_cast<int>(requestId), MutationRequest{requestId, QStringLiteral("createFolder"), path, path});
    }
    api->createFolder(name.toUtf8().constData(), parentNode, this);
    delete parentNode;
    return requestId;
}

qint64 MegaClient::startRename(const QString &path, const QString &newName)
{
    qint64 requestId = 0;
    {
        QMutexLocker locker(&m_mutex);
        requestId = ++m_nextMutationRequestId;
    }
    const QString newPath = MegaPath::childPath(MegaPath::parentPath(path), newName);
    const QString handleStr = MegaCache::getMegaHandle(path).value_or(QString{});
    bool ok = false;
    const uint64_t handle = handleStr.toULongLong(&ok);
    MegaApi *api = accountApiSession();
    MegaNode *node = (api && ok) ? api->getNodeByHandle(handle) : nullptr;
    if (!isAccountAuthenticated() || !node) {
        QMetaObject::invokeMethod(this, [this, requestId, path]() {
            emit mutationFinished(requestId, QStringLiteral("rename"), path, false,
                                  QStringLiteral("MEGA item was not found"), {});
        }, Qt::QueuedConnection);
        return requestId;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_pendingRequestsByTag.insert(static_cast<int>(requestId), MutationRequest{requestId, QStringLiteral("rename"), path, newPath});
    }
    api->renameNode(node, newName.toUtf8().constData(), this);
    delete node;
    return requestId;
}

qint64 MegaClient::startMove(const QString &sourcePath, const QString &destinationPath)
{
    qint64 requestId = 0;
    {
        QMutexLocker locker(&m_mutex);
        requestId = ++m_nextMutationRequestId;
    }
    const QString destinationParent = MegaPath::parentPath(destinationPath);
    const QString sourceHandleStr = MegaCache::getMegaHandle(sourcePath).value_or(QString{});
    const QString parentHandleStr = MegaCache::getMegaHandle(destinationParent).value_or(QString{});
    bool sourceOk = false;
    bool parentOk = false;
    MegaApi *api = accountApiSession();
    MegaNode *node = (api ? api->getNodeByHandle(sourceHandleStr.toULongLong(&sourceOk)) : nullptr);
    MegaNode *parentNode = (api ? api->getNodeByHandle(parentHandleStr.toULongLong(&parentOk)) : nullptr);
    if (!isAccountAuthenticated() || !sourceOk || !parentOk || !node || !parentNode) {
        delete node;
        delete parentNode;
        QMetaObject::invokeMethod(this, [this, requestId, sourcePath]() {
            emit mutationFinished(requestId, QStringLiteral("move"), sourcePath, false,
                                  QStringLiteral("MEGA move source or destination was not found"), {});
        }, Qt::QueuedConnection);
        return requestId;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_pendingRequestsByTag.insert(static_cast<int>(requestId), MutationRequest{requestId, QStringLiteral("move"), sourcePath, destinationPath});
    }
    api->moveNode(node, parentNode, this);
    delete node;
    delete parentNode;
    return requestId;
}

qint64 MegaClient::startRemove(const QString &path)
{
    qint64 requestId = 0;
    {
        QMutexLocker locker(&m_mutex);
        requestId = ++m_nextMutationRequestId;
    }
    const QString handleStr = MegaCache::getMegaHandle(path).value_or(QString{});
    bool ok = false;
    MegaApi *api = accountApiSession();
    MegaNode *node = (api ? api->getNodeByHandle(handleStr.toULongLong(&ok)) : nullptr);
    if (!isAccountAuthenticated() || !ok || !node) {
        delete node;
        QMetaObject::invokeMethod(this, [this, requestId, path]() {
            emit mutationFinished(requestId, QStringLiteral("remove"), path, false,
                                  QStringLiteral("MEGA item was not found"), {});
        }, Qt::QueuedConnection);
        return requestId;
    }
    {
        QMutexLocker locker(&m_mutex);
        m_pendingRequestsByTag.insert(static_cast<int>(requestId), MutationRequest{requestId, QStringLiteral("remove"), path, {}});
    }
    api->remove(node, this);
    delete node;
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
        if (m_accountSession) {
            sessions.append(m_accountSession);
        }
        qWarning() << "[MegaClient] cancelAll marked downloads"
                   << "active:" << activeRequests.size()
                   << "pending:" << pendingRequests.size()
                   << "cancelledSet:" << m_cancelledDownloads.size()
                   << "sessions:" << sessions.size();
    }

    for (MegaApi *api : sessions) {
        api->cancelTransfers(MegaTransfer::TYPE_DOWNLOAD);
        api->cancelTransfers(MegaTransfer::TYPE_UPLOAD);
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
    bool isAccountApi = false;
    {
        QMutexLocker locker(&m_mutex);
        isAccountApi = (api == m_accountSession);
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

    if (linkId.isEmpty() && !isAccountApi) {
        return;
    }

    if (isAccountApi) {
        QString operation;
        switch (request->getType()) {
        case MegaRequest::TYPE_CREATE_FOLDER:
            operation = QStringLiteral("createFolder");
            break;
        case MegaRequest::TYPE_RENAME:
            operation = QStringLiteral("rename");
            break;
        case MegaRequest::TYPE_MOVE:
            operation = QStringLiteral("move");
            break;
        case MegaRequest::TYPE_REMOVE:
            operation = QStringLiteral("remove");
            break;
        default:
            break;
        }
        if (!operation.isEmpty()) {
            MutationRequest mutation;
            bool found = false;
            {
                QMutexLocker locker(&m_mutex);
                for (auto it = m_pendingRequestsByTag.begin(); it != m_pendingRequestsByTag.end(); ++it) {
                    if (it.value().operation == operation) {
                        mutation = it.value();
                        m_pendingRequestsByTag.erase(it);
                        found = true;
                        break;
                    }
                }
                if (found && e->getErrorCode() == MegaError::API_OK) {
                    m_accountNodesLoaded = true;
                    m_ignoreAccountNodeUpdatesUntilMs = QDateTime::currentMSecsSinceEpoch() + 1500;
                }
            }
            if (found) {
                const bool success = e->getErrorCode() == MegaError::API_OK;
                if (success && mutation.operation == QStringLiteral("createFolder")) {
                    FileEntry entry;
                    entry.name = MegaPath::fallbackFileNameForPath(mutation.resultPath);
                    entry.path = mutation.resultPath;
                    entry.isDirectory = true;
                    entry.isReadOnly = false;
                    entry.iconName = QStringLiteral("folder");
                    MegaPresentation::enrichEntryPresentation(entry);
                    MegaCache::cacheEntry(mutation.resultPath, entry, QString::number(request->getNodeHandle()));
                    MegaCache::cacheChildren(mutation.resultPath, {});
                    MegaCache::appendChild(MegaPath::parentPath(mutation.resultPath), mutation.resultPath);
                }
                emit mutationFinished(mutation.id,
                                      mutation.operation,
                                      mutation.path,
                                      success,
                                      megaErrorMessage(e, QStringLiteral("MEGA account operation failed")),
                                      mutation.resultPath);
            }
            return;
        }
    }

    if (request->getType() == MegaRequest::TYPE_GET_PUBLIC_NODE) {
        bool success = (e->getErrorCode() == MegaError::API_OK);
        QString errorString = megaErrorMessage(e, QStringLiteral("Failed to load MEGA public node"));


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
        const bool success = (e->getErrorCode() == MegaError::API_OK);
        QString errorString = megaErrorMessage(e, isAccountApi
            ? QStringLiteral("Failed to sign in to MEGA")
            : QStringLiteral("Failed to open MEGA public folder"));

        if (success) {
            if (isAccountApi) {
                char *session = api->dumpSession();
                const QString sessionToken = QString::fromUtf8(session ? session : "");
                delete [] session;
                {
                    QMutexLocker locker(&m_mutex);
                    m_accountAuthenticated = true;
                    m_accountSessionToken = sessionToken;
                    m_accountNodesDirty = false;
                    m_accountFetchInProgress = true;
                    if (megaClientTimingEnabled()) {
                        m_accountFetchStartMs = QDateTime::currentMSecsSinceEpoch();
                    }
                }
                emit accountAuthorizationChanged(true, accountEmail(), accountSessionToken());
                api->getAccountDetails(this);
            }
            if (megaClientTimingEnabled()) {
                qDebug() << "[MegaTiming] account fetchNodes start";
            }
            api->fetchNodes();
        } else if (isAccountApi) {
            {
                QMutexLocker locker(&m_mutex);
                m_accountAuthenticated = false;
                m_accountNodesLoaded = false;
                m_accountNodesDirty = false;
                m_accountFetchInProgress = false;
                m_accountSessionToken.clear();
            }
            emit accountAuthorizationChanged(false, {}, {});
            emit accountNodesLoaded(false, errorString);
        } else {
            MegaCache::markLinkLoaded(linkId, false, errorString);
            emit publicLinkLoaded(linkId, false, errorString);
        }
    }
    else if (request->getType() == MegaRequest::TYPE_FETCH_NODES) {
        bool success = (e->getErrorCode() == MegaError::API_OK);
        QString errorString = megaErrorMessage(e, isAccountApi
            ? QStringLiteral("Failed to fetch MEGA account nodes")
            : QStringLiteral("Failed to fetch MEGA public folder nodes"));

        if (isAccountApi) {
            if (success) {
                MegaNode *rootNode = api->getRootNode();
                if (rootNode) {
                    clearAccountCache();
                    FileEntry rootEntry;
                    rootEntry.name = QStringLiteral("MEGA");
                    rootEntry.path = MegaPath::Root;
                    rootEntry.isDirectory = true;
                    rootEntry.isReadOnly = false;
                    rootEntry.iconName = QStringLiteral("mega");
                    MegaPresentation::enrichEntryPresentation(rootEntry);
                    MegaCache::cacheEntry(MegaPath::Root, rootEntry, {});
                    traverseAndCacheAccount(api, rootNode, QStringLiteral("mega:///Cloud Drive"));
                    MegaCache::cacheChildren(MegaPath::Root, { QStringLiteral("mega:///Cloud Drive") });
                    delete rootNode;
                    {
                        QMutexLocker locker(&m_mutex);
                        m_accountNodesLoaded = true;
                        m_accountNodesDirty = false;
                        m_accountFetchInProgress = false;
                        m_ignoreAccountNodeUpdatesUntilMs = QDateTime::currentMSecsSinceEpoch() + 1500;
                    }
                    api->getAccountDetails(this);
                } else {
                    success = false;
                    errorString = QStringLiteral("Failed to retrieve MEGA account root node");
                }
            }
            {
                QMutexLocker locker(&m_mutex);
                const qint64 fetchElapsedMs = m_accountFetchStartMs > 0
                    ? QDateTime::currentMSecsSinceEpoch() - m_accountFetchStartMs
                    : -1;
                m_accountFetchInProgress = false;
                m_accountFetchStartMs = 0;
                if (!success) {
                    m_accountNodesLoaded = false;
                }
                m_ignoreAccountNodeUpdatesUntilMs = QDateTime::currentMSecsSinceEpoch() + 1500;
                if (megaClientTimingEnabled()) {
                    qDebug() << "[MegaTiming] account fetchNodes finish"
                             << "success:" << success
                             << "elapsedMs:" << fetchElapsedMs;
                }
            }
            emit accountNodesLoaded(success, errorString);
            return;
        }

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
    else if (request->getType() == MegaRequest::TYPE_ACCOUNT_DETAILS) {
        if (e->getErrorCode() == MegaError::API_OK) {
            MegaAccountDetails *details = request->getMegaAccountDetails();
            if (details) {
                {
                    QMutexLocker locker(&m_mutex);
                    m_accountStorageUsed = details->getStorageUsed();
                    m_accountStorageMax = details->getStorageMax();
                }
                emit accountAuthorizationChanged(isAccountAuthenticated(), accountEmail(), accountSessionToken());
            }
        }
    }
}

void MegaClient::onNodesUpdate(MegaApi *api, MegaNodeList *nodes)
{
    Q_UNUSED(nodes)

    bool shouldNotify = false;
    {
        QMutexLocker locker(&m_mutex);
        if (api != m_accountSession
            || !m_accountAuthenticated
            || !m_accountNodesLoaded
            || m_accountFetchInProgress
            || m_accountNodesDirty) {
            return;
        }
        if (m_ignoreAccountNodeUpdatesUntilMs > QDateTime::currentMSecsSinceEpoch()) {
            return;
        }

        const bool ownMutationActive = !m_activeMutations.isEmpty()
            || !m_pendingUploadsByLocalPath.isEmpty()
            || !m_pendingRequestsByTag.isEmpty();
        if (ownMutationActive) {
            return;
        }

        m_accountNodesDirty = true;
        shouldNotify = true;
    }

    if (shouldNotify) {
        emit accountNodesChanged(QStringLiteral("remoteChange"));
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
    const QString localPath = sdkTransferCallbackPath(transfer);
    DownloadRequest request;
    MutationRequest uploadRequest;
    QStringList pendingDownloadKeys;
    {
        QMutexLocker locker(&m_mutex);
        m_failedTransferTags.remove(transfer->getTag());
        request = m_pendingDownloadsByLocalPath.take(localPath);
        const QString partSuffix = QStringLiteral(".part");
        if (request.id == 0 && localPath.endsWith(partSuffix, Qt::CaseInsensitive)) {
            request = m_pendingDownloadsByLocalPath.take(localPath.left(localPath.size() - partSuffix.size()));
        }
        if (request.id != 0) {
            m_activeDownloads.insert(transfer->getTag(), request);
        } else {
            pendingDownloadKeys = m_pendingDownloadsByLocalPath.keys();
            uploadRequest = m_pendingUploadsByLocalPath.take(localPath);
            if (uploadRequest.id != 0) {
                m_activeMutations.insert(transfer->getTag(), uploadRequest);
            }
        }
    }

    if (request.id != 0) {
        if (megaClientTransferTraceEnabled()) {
            qWarning() << "[MegaClient] Transfer start matched download"
                       << "this:" << this
                       << "api:" << api
                       << "tag:" << transfer->getTag()
                       << "request:" << request.id
                       << "handle:" << transfer->getNodeHandle()
                       << "localPath:" << megaDiagnosticText(QString::fromUtf8(transfer->getPath()));
        }
    }

    if (request.id == 0 && uploadRequest.id == 0) {
        qWarning() << "[MegaClient] Transfer start without pending request"
                   << "this:" << this
                   << "api:" << api
                   << "tag:" << transfer->getTag()
                   << "handle:" << transfer->getNodeHandle()
                   << "localPath:" << megaDiagnosticText(QString::fromUtf8(transfer->getPath()))
                   << "normalizedLocalPath:" << megaDiagnosticText(localPath)
                   << "pendingDownloads:" << pendingDownloadKeys.size()
                   << "pendingKeys:" << megaDiagnosticText(pendingDownloadKeys.join(QStringLiteral(" | ")));
    }

}

void MegaClient::onTransferFinish(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    Q_UNUSED(api)
    DownloadRequest request;
    MutationRequest uploadRequest;
    const QString localPath = sdkTransferCallbackPath(transfer);
    bool wasCancelled = false;
    qint64 uploadElapsedMs = -1;
    {
        QMutexLocker locker(&m_mutex);
        request = m_activeDownloads.take(transfer->getTag());
        if (request.id == 0) {
            request = m_pendingDownloadsByLocalPath.take(localPath);
            const QString partSuffix = QStringLiteral(".part");
            if (request.id == 0 && localPath.endsWith(partSuffix, Qt::CaseInsensitive)) {
                request = m_pendingDownloadsByLocalPath.take(localPath.left(localPath.size() - partSuffix.size()));
            }
        }
        if (request.id == 0) {
            uploadRequest = m_activeMutations.take(transfer->getTag());
            if (uploadRequest.id == 0) {
                uploadRequest = m_pendingUploadsByLocalPath.take(localPath);
            }
            if (uploadRequest.id != 0 && error->getErrorCode() == MegaError::API_OK) {
                m_accountNodesLoaded = true;
                m_ignoreAccountNodeUpdatesUntilMs = QDateTime::currentMSecsSinceEpoch() + 1500;
            }
            if (uploadRequest.id != 0) {
                const qint64 startedAt = m_uploadStartMsByRequestId.take(uploadRequest.id);
                if (startedAt > 0) {
                    uploadElapsedMs = QDateTime::currentMSecsSinceEpoch() - startedAt;
                }
            }
        }
        wasCancelled = request.id != 0 && m_cancelledDownloads.remove(request.id);
        if (request.id == 0 && uploadRequest.id == 0 && m_failedTransferTags.contains(transfer->getTag())) {
            return;
        }
    }

    if (uploadRequest.id != 0) {
        const bool success = error->getErrorCode() == MegaError::API_OK;
        if (success) {
            FileEntry entry;
            entry.name = MegaPath::fallbackFileNameForPath(uploadRequest.path);
            entry.path = uploadRequest.path;
            entry.isDirectory = false;
            entry.isReadOnly = false;
            entry.size = transfer->getTotalBytes();
            const int suffixIndex = entry.name.lastIndexOf(QLatin1Char('.'));
            entry.suffix = suffixIndex >= 0 ? entry.name.mid(suffixIndex + 1).toLower() : QString{};
            entry.modified = QDateTime::currentDateTimeUtc();
            entry.path = uploadRequest.path;
            MegaPresentation::enrichEntryPresentation(entry);
            MegaCache::cacheEntry(uploadRequest.path, entry, QString::number(transfer->getNodeHandle()));
            MegaCache::appendChild(MegaPath::parentPath(uploadRequest.path), uploadRequest.path);
        }
        if (megaClientUploadItemTimingEnabled()) {
            qDebug() << "[MegaTiming] upload finish"
                     << "request:" << uploadRequest.id
                     << "destination:" << megaDiagnosticText(uploadRequest.path)
                     << "success:" << success
                     << "elapsedMs:" << uploadElapsedMs
                     << "bytes:" << transfer->getTotalBytes();
        }
        emit mutationFinished(uploadRequest.id,
                              uploadRequest.operation,
                              uploadRequest.path,
                              success,
                              megaErrorMessage(error, QStringLiteral("MEGA upload failed")),
                              uploadRequest.resultPath);
        return;
    }

    if (request.id == 0 || request.path.isEmpty()) {
        qWarning() << "[MegaClient] Transfer finish without tracked request"
                   << "tag:" << transfer->getTag()
                   << "handle:" << transfer->getNodeHandle()
                   << "localPath:" << megaDiagnosticText(QString::fromUtf8(transfer->getPath()))
                   << "error:" << megaDiagnosticText(QString::fromUtf8(error->getErrorString()));
        return;
    }

    bool success = (error->getErrorCode() == MegaError::API_OK) && !wasCancelled;
    QString errorString = megaErrorMessage(error, QStringLiteral("MEGA download failed"));

    if (megaClientTransferTraceEnabled()) {
        qWarning() << "[MegaClient] Transfer finish matched download"
                   << "tag:" << transfer->getTag()
                   << "request:" << request.id
                   << "handle:" << transfer->getNodeHandle()
                   << "success:" << success
                   << "error:" << megaDiagnosticText(errorString)
                   << "localPath:" << megaDiagnosticText(QString::fromUtf8(transfer->getPath()))
                   << "bytes:" << transfer->getTotalBytes();
    }

    emit downloadFinished(request.id, request.path, success, errorString);
}

void MegaClient::onTransferUpdate(MegaApi *api, MegaTransfer *transfer)
{
    Q_UNUSED(api)
    DownloadRequest request;
    MutationRequest uploadRequest;
    {
        QMutexLocker locker(&m_mutex);
        request = m_activeDownloads.value(transfer->getTag());
        if (request.id == 0) {
            uploadRequest = m_activeMutations.value(transfer->getTag());
        }
        if (request.id == 0 && uploadRequest.id == 0 && m_failedTransferTags.contains(transfer->getTag())) {
            return;
        }
    }

    if (uploadRequest.id != 0) {
        emit uploadProgress(uploadRequest.id,
                            uploadRequest.path,
                            transfer->getTransferredBytes(),
                            transfer->getTotalBytes());
        return;
    }

    if (request.id == 0 || request.path.isEmpty()) {
        qWarning() << "[MegaClient] Transfer update without tracked request"
                   << "tag:" << transfer->getTag()
                   << "handle:" << transfer->getNodeHandle()
                   << "localPath:" << megaDiagnosticText(QString::fromUtf8(transfer->getPath()));
        return;
    }

    qint64 processed = transfer->getTransferredBytes();
    qint64 total = transfer->getTotalBytes();

    if (megaClientTransferTraceEnabled()) {
        qWarning() << "[MegaClient] Transfer update matched download"
                   << "tag:" << transfer->getTag()
                   << "request:" << request.id
                   << "handle:" << transfer->getNodeHandle()
                   << "bytes:" << processed
                   << "total:" << total
                   << "localPath:" << megaDiagnosticText(QString::fromUtf8(transfer->getPath()));
    }

    emit downloadProgress(request.id, request.path, processed, total);
}

void MegaClient::onTransferTemporaryError(MegaApi *api, MegaTransfer *transfer, MegaError *error)
{
    Q_UNUSED(api)
    DownloadRequest request;
    MutationRequest uploadRequest;
    const int tag = transfer->getTag();
    const bool failTransfer = megaTemporaryErrorShouldFailTransfer(error);
    {
        QMutexLocker locker(&m_mutex);
        request = m_activeDownloads.value(tag);
        if (request.id == 0) {
            uploadRequest = m_activeMutations.value(tag);
        }
        if (failTransfer) {
            if (request.id != 0) {
                m_activeDownloads.remove(tag);
                m_failedTransferTags.insert(tag);
            } else if (uploadRequest.id != 0) {
                m_activeMutations.remove(tag);
                m_uploadStartMsByRequestId.remove(uploadRequest.id);
                m_failedTransferTags.insert(tag);
            }
        }
    }

    if (request.id == 0 && uploadRequest.id == 0 && !megaTransferTemporaryErrorLogEnabled()) {
        return;
    }

    qWarning() << "[MegaClient] Transfer temporary error"
               << "tag:" << transfer->getTag()
               << "request:" << (request.id != 0 ? request.id : uploadRequest.id)
               << "handle:" << transfer->getNodeHandle()
               << "localPath:" << megaDiagnosticText(QString::fromUtf8(transfer->getPath()))
               << "error:" << megaDiagnosticText(megaErrorMessage(error, QStringLiteral("Temporary MEGA transfer error")));

    if (!failTransfer) {
        return;
    }

    const QString errorString = megaErrorMessage(error, QStringLiteral("MEGA transfer failed"));
    if (request.id != 0) {
        emit downloadFinished(request.id, request.path, false, errorString);
    } else if (uploadRequest.id != 0) {
        emit mutationFinished(uploadRequest.id,
                              uploadRequest.operation,
                              uploadRequest.path,
                              false,
                              errorString,
                              uploadRequest.resultPath);
    }
}


void MegaClient::clearAccountCache()
{
    MegaCache::removeSubtree(MegaPath::Root);
}

void MegaClient::traverseAndCacheAccount(MegaApi *api, MegaNode *node, const QString &virtualPath)
{
    if (!node) {
        return;
    }

    FileEntry entry;
    entry.name = virtualPath == QStringLiteral("mega:///Cloud Drive")
        ? QStringLiteral("Cloud Drive")
        : QString::fromUtf8(node->getName());
    if (entry.name.isEmpty()) {
        entry.name = QStringLiteral("unnamed");
    }
    entry.isDirectory = (node->getType() != MegaNode::TYPE_FILE);
    entry.size = entry.isDirectory ? 0 : node->getSize();
    const int suffixIndex = entry.name.lastIndexOf(QLatin1Char('.'));
    entry.suffix = (!entry.isDirectory && suffixIndex >= 0) ? entry.name.mid(suffixIndex + 1).toLower() : QString{};
    entry.isReadOnly = false;
    entry.modified = QDateTime::fromSecsSinceEpoch(node->getModificationTime());
    entry.iconName = entry.isDirectory
        ? (virtualPath == QStringLiteral("mega:///Cloud Drive") ? QStringLiteral("mega-clouddrive") : QStringLiteral("folder"))
        : QString{};
    entry.path = virtualPath;
    MegaPresentation::enrichEntryPresentation(entry);

    MegaCache::cacheEntry(virtualPath, entry, QString::number(node->getHandle()));
    if (!entry.isDirectory) {
        return;
    }

    MegaNodeList *childrenList = api->getChildren(node);
    if (!childrenList) {
        MegaCache::cacheChildren(virtualPath, {});
        return;
    }

    QStringList childPaths;
    childPaths.reserve(childrenList->size());
    for (int i = 0; i < childrenList->size(); ++i) {
        MegaNode *child = childrenList->get(i);
        if (!child) {
            continue;
        }
        QString childName = QString::fromUtf8(child->getName());
        if (childName.isEmpty()) {
            childName = QStringLiteral("unnamed");
        }
        const QString childPath = virtualPath + QLatin1Char('/') + childName;
        childPaths.append(childPath);
        traverseAndCacheAccount(api, child, childPath);
    }
    MegaCache::cacheChildren(virtualPath, childPaths);
    delete childrenList;
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
    MegaPresentation::enrichEntryPresentation(entry);

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
