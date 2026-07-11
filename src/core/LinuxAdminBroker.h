#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>

class LinuxAdminBroker final
{
public:
    static constexpr int CurrentProtocolVersion = 5;

    enum class BackendMode {
        Unavailable,
        Fake,
        HelperProcess
    };

    enum class Operation {
        CopyFile,
        MakeDirectory,
        AtomicReplace,
        CreateFile,
        RenamePath,
        DeletePath,
        ChangeMode,
        ChangeOwnership
    };

    struct Request {
        int protocolVersion = CurrentProtocolVersion;
        QString operationId;
        QString sessionNonce;
        Operation operation = Operation::CopyFile;
        QString sourcePath;
        QString destinationPath;
        bool overwrite = false;
        bool preserveMetadata = false;
        quint32 mode = 0;
        quint32 modeMask = 0;
        bool recursive = false;
        qint64 ownerId = -1;
        qint64 groupId = -1;
    };

    struct Result {
        bool success = false;
        QString errorCode;
        QString errorMessage;
        QString failedPath;
    };

    using ProgressCallback = std::function<void(qint64 processedBytes, qint64 totalBytes)>;

    LinuxAdminBroker();

    bool available() const;
    QString backendName() const;
    QString unavailableReason() const;
    BackendMode backendMode() const;
    void setBackendModeForTesting(BackendMode mode);

    Result authenticateBlocking() const;
    static void revokeSession();
    static QString activeSessionNonce();
    static void cancelActiveSessionOperation();
    Result submitBlocking(const Request &request, const ProgressCallback &progress = {}) const;
    Result submitUnprivilegedBlocking(const Request &request) const;
    static QJsonObject requestToJson(const Request &request);
    static Result requestFromJson(const QJsonObject &object, Request *request);
    static QJsonObject resultToJson(const Result &result);
    static Result resultFromJson(const QJsonObject &object);

private:
    enum class HelperLaunchMode {
        Direct,
        Pkexec
    };

    static QString operationToString(Operation operation);
    static bool operationFromString(const QString &value, Operation *operation);

    Result validateRequest(const Request &request, bool requireSession) const;
    Result submitFake(const Request &request) const;
    Result submitHelperProcess(const Request &request, const ProgressCallback &progress) const;
    Result submitDirectHelperProcess(const Request &request) const;

    BackendMode m_backendMode = BackendMode::Unavailable;
    QString m_helperPath;
    QString m_userHelperPath;
    QString m_unavailableReason;
    HelperLaunchMode m_helperLaunchMode = HelperLaunchMode::Direct;
};
