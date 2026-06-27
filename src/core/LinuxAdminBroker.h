#pragma once

#include <QJsonObject>
#include <QString>

#include <functional>

class LinuxAdminBroker final
{
public:
    static constexpr int CurrentProtocolVersion = 2;

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
        DeletePath
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

    Result validateRequest(const Request &request) const;
    Result submitFake(const Request &request) const;
    Result submitHelperProcess(const Request &request, const ProgressCallback &progress) const;

    BackendMode m_backendMode = BackendMode::Unavailable;
    QString m_helperPath;
    QString m_unavailableReason;
    HelperLaunchMode m_helperLaunchMode = HelperLaunchMode::Direct;
};
