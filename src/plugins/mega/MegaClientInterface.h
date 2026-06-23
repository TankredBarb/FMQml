#pragma once

#include <QObject>
#include <QString>

class MegaClientInterface : public QObject
{
    Q_OBJECT

public:
    explicit MegaClientInterface(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~MegaClientInterface() override = default;

    virtual int getPublicNode(const QString &linkId) = 0;
    virtual int loginToAccount(const QString &email, const QString &password) = 0;
    virtual int resumeAccountSession(const QString &session) = 0;
    virtual bool logoutAccount(QString *errorString = nullptr) = 0;
    virtual bool isAccountAuthenticated() const = 0;
    virtual QString accountEmail() const = 0;
    virtual QString accountSessionToken() const = 0;
    virtual int loadAccountRoot() = 0;
    virtual qint64 startDownload(const QString &path, const QString &localPath) = 0;
    virtual qint64 startUpload(const QString &sourceFilePath, const QString &destinationPath) = 0;
    virtual qint64 startCreateFolder(const QString &parentPath, const QString &name) = 0;
    virtual qint64 startRename(const QString &path, const QString &newName) = 0;
    virtual qint64 startMove(const QString &sourcePath, const QString &destinationPath) = 0;
    virtual qint64 startRemove(const QString &path) = 0;
    virtual void cancelAll() = 0;

signals:
    void publicLinkLoaded(const QString &linkId, bool success, const QString &errorString);
    void accountAuthorizationChanged(bool signedIn, const QString &accountEmail, const QString &session);
    void accountNodesLoaded(bool success, const QString &errorString);
    void downloadProgress(qint64 requestId, const QString &path, qint64 processedBytes, qint64 totalBytes);
    void downloadFinished(qint64 requestId, const QString &path, bool success, const QString &errorString);
    void uploadProgress(qint64 requestId, const QString &path, qint64 processedBytes, qint64 totalBytes);
    void mutationFinished(qint64 requestId, const QString &operation, const QString &path, bool success, const QString &errorString, const QString &resultPath);
};

MegaClientInterface &defaultMegaClient();
