#include "OperationQueue.h"

#include <QtConcurrent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QElapsedTimer>
#include <QMutexLocker>

#include <algorithm>
#include <stdexcept>

// Windows-specific implementation
#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0602 // Windows 8+
#endif
#include <windows.h>
#endif

namespace {

constexpr qint64 SmallFileLimit = 10 * 1024 * 1024; // 10MB
constexpr qint64 LargeFileLimit = 50 * 1024 * 1024; // 50MB for CopyFile2
constexpr qint64 LargeFileBufferSizeFallback = 1024 * 1024; // 512KB for default
constexpr qint64 MetricsUpdateIntervalMs = 500;
constexpr qint64 UIUpdateIntervalMs = 500; //

#ifdef _WIN32
struct CopyFile2Context {
    OperationQueue *queue;
    qint64 totalBytes;
    qint64 *globalCopiedBytes;
    qint64 lastFileBytesTransferred = 0;
};

COPYFILE2_MESSAGE_ACTION CALLBACK CopyFile2ProgressRoutine(
    const COPYFILE2_MESSAGE *pMessage,
    PVOID pvCallbackContext)
{
    //TODO: need to optimize this piece of good later.
    CopyFile2Context *ctx = static_cast<CopyFile2Context *>(pvCallbackContext);
    
    if (ctx->queue->isAborted()) { 
        return COPYFILE2_PROGRESS_CANCEL;
    }

    if (pMessage->Type == COPYFILE2_CALLBACK_CHUNK_FINISHED) {

        const qint64 currentFileTransferred = static_cast<qint64>(pMessage->Info.ChunkFinished.uliTotalBytesTransferred.QuadPart);
        const qint64 chunkDelta = currentFileTransferred - ctx->lastFileBytesTransferred;
        
        *ctx->globalCopiedBytes += chunkDelta;
        ctx->lastFileBytesTransferred = currentFileTransferred;
        
        ctx->queue->updateMetrics(*ctx->globalCopiedBytes, ctx->totalBytes);
        
        const double progress = static_cast<double>(*ctx->globalCopiedBytes) / static_cast<double>(ctx->totalBytes);
        QMetaObject::invokeMethod(ctx->queue, [q = ctx->queue, progress]() {
            q->setProgress(progress);
        }, Qt::QueuedConnection);
    } else if (pMessage->Type == COPYFILE2_CALLBACK_POLL_CONTINUE) {
        if (ctx->queue->busy() == false) return COPYFILE2_PROGRESS_CANCEL;
    }

    return COPYFILE2_PROGRESS_CONTINUE;
}
#endif

bool isRealDirectory(const QFileInfo &info)
{
    return info.isDir() && !info.isSymLink();
}

bool removePathIfExists(const QString &path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return true;
    }

    if (isRealDirectory(info)) {
        return QDir(path).removeRecursively();
    }

    return QFile::remove(path);
}

QString formatSize(qint64 bytes) {
    if (bytes < 1024) return QString::number(bytes) + " B";
    if (bytes < 1024 * 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    if (bytes < 1024 * 1024 * 1024) return QString::number(bytes / (1024.0 * 1024.0), 'f', 1) + " MB";
    return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 1) + " GB";
}

QString formatTime(qint64 seconds) {
    if (seconds < 60) return QString::number(seconds) + "s";
    if (seconds < 3600) return QString("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    return QString("%1h %2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
}
}

OperationQueue::OperationQueue(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<Request>::finished, this, &OperationQueue::finishCurrent);
}

OperationQueue::~OperationQueue()
{
    m_abort = true;
    QMutexLocker locker(&m_mutex);
    m_condition.wakeAll();
    locker.unlock();

    if (m_watcher.isRunning()) {
        m_watcher.waitForFinished();
    }
}

//TODO: move!
OperationQueue::DriveStorageType OperationQueue::getDriveTypeByPath(const QString &filePath)
{
#if defined(Q_OS_WIN)
    QString root = QFileInfo(filePath).absoluteDir().rootPath();
    if (root.isEmpty()) {
        return DriveStorageType::Unknown;
    }

    std::wstring stdRoot = root.toStdWString();
    LPCWSTR driveRoot = stdRoot.c_str();

    UINT winDriveType = GetDriveTypeW(driveRoot);
    if (winDriveType == DRIVE_REMOVABLE) {
        return DriveStorageType::USB_Flash;
    }
    if (winDriveType != DRIVE_FIXED) {
        return DriveStorageType::Unknown;
    }

    QString volumePath = QString(R"(\\.\)") + root.left(2);
    HANDLE hDevice = CreateFileW(
        volumePath.toStdWString().c_str(),
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hDevice == INVALID_HANDLE_VALUE) {
        return DriveStorageType::Unknown;
    }

    DriveStorageType detectedType = DriveStorageType::HDD;

    STORAGE_PROPERTY_QUERY query;
    query.PropertyId = StorageDeviceSeekPenaltyProperty;
    query.QueryType = PropertyStandardQuery;

    DEVICE_SEEK_PENALTY_DESCRIPTOR seekPenaltyDesc = {0};
    DWORD bytesReturned = 0;

    BOOL result = DeviceIoControl(
        hDevice,
        IOCTL_STORAGE_QUERY_PROPERTY,
        &query, sizeof(query),
        &seekPenaltyDesc, sizeof(seekPenaltyDesc),
        &bytesReturned, NULL
    );

    if (result && !seekPenaltyDesc.IncursSeekPenalty) {
        detectedType = DriveStorageType::SATA_SSD;

        query.PropertyId = StorageAdapterProperty;
        query.QueryType = PropertyStandardQuery;

        STORAGE_ADAPTER_DESCRIPTOR adapterDesc = {0};
        result = DeviceIoControl(
            hDevice,
            IOCTL_STORAGE_QUERY_PROPERTY,
            &query, sizeof(query),
            &adapterDesc, sizeof(adapterDesc),
            &bytesReturned, NULL
        );

        if (result) {
            if (adapterDesc.BusType == BusTypeNvme) {
                detectedType = DriveStorageType::NVME_SSD;
            } else if (adapterDesc.BusType == BusTypeUsb) {
                detectedType = DriveStorageType::USB_Flash;
            }
        }
    }

    CloseHandle(hDevice);
    return detectedType;
#else
    Q_UNUSED(filePath);
    return DriveStorageType::Unknown;
#endif
}

qint64 getBufferSizeByStorageType(OperationQueue::DriveStorageType type)
{
    switch (type) {
        case OperationQueue::DriveStorageType::HDD:
        case OperationQueue::DriveStorageType::USB_Flash:
            return 512 * 1024; // 512 КБ

        case OperationQueue::DriveStorageType::SATA_SSD:
            return 4 * 1024 * 1024; // 4 МБ

        case OperationQueue::DriveStorageType::NVME_SSD:
            return 8 * 1024 * 1024; // 8 МБ

        case OperationQueue::DriveStorageType::Unknown:
        default:
            return 1 * 1024 * 1024; // fallback
    }
}

bool OperationQueue::busy() const
{
    return m_busy;
}

double OperationQueue::progress() const
{
    return m_progress;
}

QString OperationQueue::currentLabel() const
{
    return m_currentLabel;
}

QString OperationQueue::error() const
{
    return m_error;
}

QString OperationQueue::statusMessage() const
{
    return m_statusMessage;
}

QString OperationQueue::speedText() const
{
    return m_speedText;
}

QString OperationQueue::remainingTimeText() const
{
    return m_remainingTimeText;
}

void OperationQueue::copyTo(const QStringList &sources, const QString &destination)
{
    if (sources.isEmpty() || destination.isEmpty()) {
        return;
    }
    enqueue({Type::Copy, sources, destination});
}

void OperationQueue::moveTo(const QStringList &sources, const QString &destination)
{
    if (sources.isEmpty() || destination.isEmpty()) {
        return;
    }
    enqueue({Type::Move, sources, destination});
}

void OperationQueue::deletePaths(const QStringList &paths)
{
    if (paths.isEmpty()) {
        return;
    }
    enqueue({Type::Delete, paths, {}});
}

void OperationQueue::resolveConflict(ConflictResolution resolution, bool applyToAll)
{
    QMutexLocker locker(&m_mutex);
    m_resolution = resolution;
    m_applyToAll = applyToAll;
    m_lastResolution = resolution;
    m_condition.wakeAll();
}

void OperationQueue::cancel()
{
    m_abort = true;
    QMutexLocker locker(&m_mutex);
    m_condition.wakeAll();
}

OperationQueue::ConflictResolution OperationQueue::waitForResolution(const QString &source, const QString &destination)
{
    if (m_abort) {
        return ConflictResolution::Cancel;
    }

    if (m_applyToAll && m_lastResolution != ConflictResolution::Pending) {
        return m_lastResolution;
    }

    QFileInfo sourceInfo(source);
    QFileInfo destInfo(destination);

    QMutexLocker locker(&m_mutex);
    m_resolution = ConflictResolution::Pending;
    emit conflictDetected(source, destination, 
                          sourceInfo.size(), sourceInfo.lastModified(),
                          destInfo.size(), destInfo.lastModified());
    while (m_resolution == ConflictResolution::Pending && !m_abort) {
        m_condition.wait(&m_mutex);
    }

    if (m_abort) {
        return ConflictResolution::Cancel;
    }

    return m_resolution;
}

void OperationQueue::enqueue(Request request)
{
    m_pending.append(std::move(request));
    if (!m_busy) {
        runNext();
    }
}

void OperationQueue::runNext()
{
    if (m_pending.isEmpty()) {
        return;
    }

    const Request request = m_pending.takeFirst();
    m_abort = false;
    setBusy(true);
    setProgress(0.0);
    setCompletedItems(0);
    setTotalItems(0);
    setError({});
    setStatusMessage({});
    m_speedText = QString();
    m_remainingTimeText = QString();
    m_lastBytes = 0;
    m_lastTime = 0;
    m_currentSpeed = 0.0;
    m_applyToAll = false;
    m_lastResolution = ConflictResolution::Pending;
    
    QString label;
    switch (request.type) {
    case Type::Copy: label = QStringLiteral("Starting..."); break;
    case Type::Move: label = QStringLiteral("Moving..."); break;
    case Type::Delete: label = QStringLiteral("Deleting..."); break;
    }
    setCurrentLabel(label);

    m_operationTimer.start();
    m_watcher.setFuture(QtConcurrent::run([this, request]() {
        execute(request);
        return request;
    }));
}

void OperationQueue::finishCurrent()
{
    const Request request = m_watcher.future().result();
    if (m_error.isEmpty()) {
        setProgress(1.0);
        setCurrentLabel(QStringLiteral("Done"));
    }
    setBusy(false);
    m_speedText = QString();
    m_remainingTimeText = QString();
    emit speedChanged();
    emit operationFinished(request.type, request.sources, request.destination);
    runNext();
}

void OperationQueue::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged();
}

void OperationQueue::setProgress(double progress)
{
    const double bounded = std::clamp(progress, 0.0, 1.0);
    if (qFuzzyCompare(m_progress, bounded)) {
        return;
    }
    m_progress = bounded;
    emit progressChanged();
}

void OperationQueue::setCurrentLabel(const QString &label)
{
    if (m_currentLabel == label) {
        return;
    }
    m_currentLabel = label;
    emit currentLabelChanged();
}

void OperationQueue::setError(const QString &error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

void OperationQueue::setStatusMessage(const QString &msg)
{
    m_statusMessage = msg;
    emit statusMessageChanged();
}

int OperationQueue::completedItems() const
{
    return m_completedItems;
}

int OperationQueue::totalItems() const
{
    return m_totalItems;
}

void OperationQueue::setCompletedItems(int completed)
{
    if (m_completedItems == completed) return;
    m_completedItems = completed;
    emit progressChanged();
}

void OperationQueue::setTotalItems(int total)
{
    if (m_totalItems == total) return;
    m_totalItems = total;
    emit progressChanged();
}

void OperationQueue::updateMetrics(qint64 currentBytes, qint64 totalBytes)
{
    const qint64 currentTime = m_operationTimer.elapsed();
    if (currentTime - m_lastTime < MetricsUpdateIntervalMs) return;

    const qint64 bytesSinceLast = currentBytes - m_lastBytes;
    const qint64 timeSinceLast = currentTime - m_lastTime;
    
    if (timeSinceLast > 0) {
        const double instantSpeed = (static_cast<double>(bytesSinceLast) / timeSinceLast) * 1000.0;
        
        if (m_currentSpeed <= 0) {
            m_currentSpeed = instantSpeed;
        } else {
            const double alpha = 0.25;
            m_currentSpeed = (alpha * instantSpeed) + (1.0 - alpha) * m_currentSpeed;
        }

        const QString speedTxt = formatSize(static_cast<qint64>(m_currentSpeed)) + "/s";
        
        const qint64 remainingBytes = totalBytes - currentBytes;
        QString remainingTxt;
        if (m_currentSpeed > 1024 && remainingBytes > 0) { 
            const qint64 remainingSec = static_cast<qint64>(remainingBytes / m_currentSpeed);
            remainingTxt = formatTime(remainingSec) + " remaining";
        }

        QMetaObject::invokeMethod(this, [this, speedTxt, remainingTxt]() {
            m_speedText = speedTxt;
            m_remainingTimeText = remainingTxt;
            emit speedChanged();
        }, Qt::QueuedConnection);
    }

    m_lastBytes = currentBytes;
    m_lastTime = currentTime;
}

void OperationQueue::execute(const Request &request)
{
    qint64 currentProgressBytes = 0;
    const int totalFileCount = request.sources.size();
    const bool isCountingItems = (request.type == Type::Delete);
    const qint64 totalBytes = isCountingItems
        ? static_cast<qint64>(totalFileCount)
        : std::max<qint64>(1, totalBytesFor(request.sources));

    QMetaObject::invokeMethod(this, [this, totalFileCount]() {
        setTotalItems(totalFileCount);
        setCompletedItems(0);
    }, Qt::QueuedConnection);

    for (int i = 0; i < totalFileCount; ++i) {
        if (m_abort) return;
        const QString &source = request.sources.at(i);
        const QFileInfo sourceInfo(source);
        const QString destinationPath = request.destination.isEmpty() ? QString() : QDir(request.destination).filePath(sourceInfo.fileName());

        try {
            if (request.type == Type::Copy) {
                copyPath(source, destinationPath, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Move) {
                movePath(source, destinationPath, totalBytes, currentProgressBytes);
            } else if (request.type == Type::Delete) {
                QMetaObject::invokeMethod(this, [this, name = sourceInfo.fileName(), i]() {
                    setCurrentLabel(name);
                    setCompletedItems(i);
                }, Qt::QueuedConnection);

                if (isRealDirectory(sourceInfo)) {
                    if (!QDir(source).removeRecursively()) {
                        throw std::runtime_error(QStringLiteral("Cannot delete folder %1").arg(source).toStdString());
                    }
                } else {
                    if (!QFile::remove(source)) {
                        throw std::runtime_error(QStringLiteral("Cannot delete file %1").arg(source).toStdString());
                    }
                }

                currentProgressBytes += 1;
                const double progress = static_cast<double>(i + 1) / static_cast<double>(totalFileCount);
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
            }
        } catch (const std::exception &exception) {
            const QString message = QString::fromUtf8(exception.what());
            QMetaObject::invokeMethod(this, [this, message]() {
                setError(message);
                setCurrentLabel(QStringLiteral("Operation failed"));
            }, Qt::QueuedConnection);
            return;
        }
    }
}

qint64 OperationQueue::totalBytesFor(const QStringList &sources) const
{
    qint64 total = 0;
    for (const QString &source : sources) {
        total += totalBytesForPath(source);
    }
    return total;
}

qint64 OperationQueue::totalBytesForPath(const QString &path) const
{
    const QFileInfo info(path);
    if (!info.exists()) {
        return 0;
    }
    if (info.isFile() || info.isSymLink()) {
        return info.size();
    }
    if (!info.isDir()) {
        return info.size();
    }

    qint64 total = 0;
    const QDir dir(path);
    const QFileInfoList children = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo &child : children) {
        total += totalBytesForPath(child.absoluteFilePath());
    }
    return total;
}

void OperationQueue::copyPath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes)
{
    if (m_abort) return;

    const QFileInfo sourceInfo(sourcePath);
    const QString fileName = sourceInfo.fileName();
    
    QMetaObject::invokeMethod(this, [this, fileName]() {
        setCurrentLabel(fileName);
    }, Qt::QueuedConnection);

    if (QFileInfo(sourcePath) == QFileInfo(destinationPath)) {
        copiedBytes += totalBytesForPath(sourcePath);
        QMetaObject::invokeMethod(this, [this]() {
            setStatusMessage("Some files skipped (source is same as destination)");
        }, Qt::QueuedConnection);
        return;
    }

    QString targetPath = destinationPath;
    if (QFileInfo::exists(targetPath)) {
        const ConflictResolution res = waitForResolution(sourcePath, targetPath);
        if (res == ConflictResolution::Skip) {
            copiedBytes += totalBytesForPath(sourcePath);
            return;
        } else if (res == ConflictResolution::KeepBoth) {
            targetPath = uniqueDestinationPath(targetPath);
        } else if (res == ConflictResolution::Replace) {
            if (!removePathIfExists(targetPath)) {
                throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(targetPath).toStdString());
            }
        } else if (res == ConflictResolution::Cancel) {
            m_abort = true;
            return;
        }
    }

    if (m_abort) return;

    if (isRealDirectory(sourceInfo)) {
        QDir().mkpath(targetPath);
        const QDir sourceDir(sourcePath);
        const QFileInfoList children = sourceDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &child : children) {
            if (m_abort) return;
            const QString childDestination = QDir(targetPath).filePath(child.fileName());
            copyPath(child.absoluteFilePath(), childDestination, totalBytes, copiedBytes);
        }
        return;
    }

    const qint64 fileSize = sourceInfo.size();

#ifdef _WIN32
    if (m_useNativeCopy && fileSize > LargeFileLimit) {
        CopyFile2Context ctx{this, totalBytes, &copiedBytes, 0};
        COPYFILE2_EXTENDED_PARAMETERS params = { sizeof(COPYFILE2_EXTENDED_PARAMETERS) };
        params.dwCopyFlags = 0; // Use default buffering - much faster!
        params.pProgressRoutine = CopyFile2ProgressRoutine;
        params.pvCallbackContext = &ctx;

        const std::wstring src = QDir::toNativeSeparators(sourcePath).toStdWString();
        const std::wstring dst = QDir::toNativeSeparators(targetPath).toStdWString();

        HRESULT hr = CopyFile2(src.c_str(), dst.c_str(), &params);
        
        if (SUCCEEDED(hr)) {
            return;
        } else if (hr == HRESULT_FROM_WIN32(ERROR_REQUEST_ABORTED)) {
            m_abort = true;
            return;
        }
        // If it failed for other reasons, it will fall through to manual copy
    }
#endif

    QFile source(sourcePath);
    if (!source.open(QIODevice::ReadOnly)) {
        throw std::runtime_error(QStringLiteral("Cannot read %1").arg(sourcePath).toStdString());
    }

    QDir().mkpath(QFileInfo(targetPath).absolutePath());
    const QString tempPath = targetPath + QStringLiteral(".part");
    QFile destination(tempPath);
    if (!destination.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw std::runtime_error(QStringLiteral("Cannot write %1").arg(targetPath).toStdString());
    }

    QElapsedTimer uiTimer;
    uiTimer.start();
    
    if (fileSize <= SmallFileLimit) {
        const QByteArray data = source.readAll();
        if (destination.write(data) != data.size()) {
            throw std::runtime_error(QStringLiteral("Write failed: %1").arg(targetPath).toStdString());
        }
        copiedBytes += data.size();
        
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        updateMetrics(copiedBytes, totalBytes);
    } else {
        QByteArray buffer;
        qint64 bufferSize = getBufferSizeByStorageType(getDriveTypeByPath(destinationPath));

        buffer.resize(bufferSize);

        while (!source.atEnd()) {
            if (m_abort) {
                destination.close();
                QFile::remove(tempPath);
                return;
            }

            const qint64 read = source.read(buffer.data(), buffer.size());
            if (read < 0) {
                throw std::runtime_error(QStringLiteral("Read failed: %1").arg(sourcePath).toStdString());
            }
            if (destination.write(buffer.constData(), read) != read) {
                throw std::runtime_error(QStringLiteral("Write failed: %1").arg(targetPath).toStdString());
            }
            copiedBytes += read;

            if (uiTimer.elapsed() > UIUpdateIntervalMs) {
                const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
                QMetaObject::invokeMethod(this, [this, progress]() {
                    setProgress(progress);
                }, Qt::QueuedConnection);
                updateMetrics(copiedBytes, totalBytes);
                uiTimer.restart();
            }
        }
    }

    destination.close();
    source.close();

    if (m_abort) {
        QFile::remove(tempPath);
        return;
    }

    if (QFile::exists(targetPath)) {
        QFile::remove(targetPath);
    }
    if (!QFile::rename(tempPath, targetPath)) {
        QFile::remove(tempPath);
        throw std::runtime_error(QStringLiteral("Cannot finalize %1").arg(targetPath).toStdString());
    }
}

void OperationQueue::movePath(const QString &sourcePath, const QString &destinationPath, qint64 totalBytes, qint64 &copiedBytes)
{
    if (m_abort) return;

    if (QFileInfo(sourcePath) == QFileInfo(destinationPath)) {
        copiedBytes += std::max<qint64>(1, totalBytesForPath(destinationPath));
        QMetaObject::invokeMethod(this, [this]() {
            setStatusMessage("Some files skipped (source is same as destination)");
        }, Qt::QueuedConnection);
        return;
    }

    if (QFileInfo::exists(destinationPath)) {
        const ConflictResolution res = waitForResolution(sourcePath, destinationPath);
        if (res == ConflictResolution::Skip) {
            copiedBytes += std::max<qint64>(1, totalBytesForPath(sourcePath));
            return;
        } else if (res == ConflictResolution::KeepBoth) {
            const QString uniquePath = uniqueDestinationPath(destinationPath);
            return movePath(sourcePath, uniquePath, totalBytes, copiedBytes);
        } else if (res == ConflictResolution::Replace) {
            if (!removePathIfExists(destinationPath)) {
                throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(destinationPath).toStdString());
            }
        } else if (res == ConflictResolution::Cancel) {
            m_abort = true;
            return;
        }
    }

    if (m_abort) return;

    if (QFile::rename(sourcePath, destinationPath)) {
        copiedBytes += std::max<qint64>(1, totalBytesForPath(destinationPath));
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        updateMetrics(copiedBytes, totalBytes);
        return;
    }

    copyPath(sourcePath, destinationPath, totalBytes, copiedBytes);

    if (m_abort) return;

    const QFileInfo sourceInfo(sourcePath);
    if (isRealDirectory(sourceInfo)) {
        QDir dir(sourcePath);
        if (!dir.removeRecursively()) {
            throw std::runtime_error(QStringLiteral("Cannot remove %1").arg(sourcePath).toStdString());
        }
    } else if (!QFile::remove(sourcePath)) {
        throw std::runtime_error(QStringLiteral("Cannot remove %1").arg(sourcePath).toStdString());
    }
}

QString OperationQueue::uniqueDestinationPath(const QString &path) const
{
    if (!QFileInfo::exists(path)) {
        return path;
    }

    const QFileInfo info(path);
    const QString dir = info.absolutePath();
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix();

    for (int i = 1; i < 10000; ++i) {
        const QString name = suffix.isEmpty()
            ? QStringLiteral("%1 copy %2").arg(base).arg(i)
            : QStringLiteral("%1 copy %2.%3").arg(base).arg(i).arg(suffix);
        const QString candidate = QDir(dir).filePath(name);
        if (!QFileInfo::exists(candidate)) {
            return candidate;
        }
    }

    return path;
}
