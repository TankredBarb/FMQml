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

namespace {
constexpr qint64 CopyBufferSize = 1024 * 1024;
constexpr qint64 ProgressUpdateIntervalMs = 100;
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

OperationQueue::ConflictResolution OperationQueue::waitForResolution(const QString &source, const QString &destination)
{
    if (m_applyToAll && m_lastResolution != ConflictResolution::Pending) {
        return m_lastResolution;
    }

    QMutexLocker locker(&m_mutex);
    m_resolution = ConflictResolution::Pending;
    emit conflictDetected(source, destination);
    while (m_resolution == ConflictResolution::Pending) {
        m_condition.wait(&m_mutex);
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
    setBusy(true);
    setProgress(0.0);
    setError({});
    m_applyToAll = false;
    m_lastResolution = ConflictResolution::Pending;
    QString label;
    switch (request.type) {
    case Type::Copy: label = QStringLiteral("Copying..."); break;
    case Type::Move: label = QStringLiteral("Moving..."); break;
    case Type::Delete: label = QStringLiteral("Deleting..."); break;
    }
    setCurrentLabel(label);

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

void OperationQueue::execute(const Request &request)
{
    qint64 copiedBytes = 0;
    const bool isCountingItems = (request.type == Type::Delete);
    const qint64 totalBytes = isCountingItems
        ? static_cast<qint64>(request.sources.size())
        : std::max<qint64>(1, totalBytesFor(request.sources));

    for (const QString &source : request.sources) {
        if (m_abort) return;
        const QFileInfo sourceInfo(source);
        const QString destinationPath = request.destination.isEmpty() ? QString() : QDir(request.destination).filePath(sourceInfo.fileName());

        try {
            if (request.type == Type::Copy) {
                copyPath(source, destinationPath, totalBytes, copiedBytes);
            } else if (request.type == Type::Move) {
                movePath(source, destinationPath, totalBytes, copiedBytes);
            } else if (request.type == Type::Delete) {
                QMetaObject::invokeMethod(this, [this, name = sourceInfo.fileName()]() {
                    setCurrentLabel(QStringLiteral("Deleting %1").arg(name));
                }, Qt::QueuedConnection);

                if (sourceInfo.isDir()) {
                    if (!QDir(source).removeRecursively()) {
                        throw std::runtime_error(QStringLiteral("Cannot delete folder %1").arg(source).toStdString());
                    }
                } else {
                    if (!QFile::remove(source)) {
                        throw std::runtime_error(QStringLiteral("Cannot delete file %1").arg(source).toStdString());
                    }
                }

                copiedBytes += 1;
                const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
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
    if (info.isFile()) {
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
    QString targetPath = destinationPath;
    if (QFileInfo::exists(targetPath)) {
        const ConflictResolution res = waitForResolution(sourcePath, targetPath);
        if (res == ConflictResolution::Skip) {
            copiedBytes += totalBytesForPath(sourcePath);
            return;
        } else if (res == ConflictResolution::KeepBoth) {
            targetPath = uniqueDestinationPath(targetPath);
        } else if (res == ConflictResolution::Replace) {
            // Will overwrite below
        }
    }

    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.isDir()) {
        QDir().mkpath(targetPath);
        const QDir sourceDir(sourcePath);
        const QFileInfoList children = sourceDir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        for (const QFileInfo &child : children) {
            const QString childDestination = QDir(targetPath).filePath(child.fileName());
            copyPath(child.absoluteFilePath(), childDestination, totalBytes, copiedBytes);
        }
        return;
    }

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

    QByteArray buffer;
    buffer.resize(CopyBufferSize);
    QElapsedTimer timer;
    timer.start();

    while (!source.atEnd()) {
        const qint64 read = source.read(buffer.data(), buffer.size());
        if (read < 0) {
            throw std::runtime_error(QStringLiteral("Read failed: %1").arg(sourcePath).toStdString());
        }
        if (destination.write(buffer.constData(), read) != read) {
            throw std::runtime_error(QStringLiteral("Write failed: %1").arg(targetPath).toStdString());
        }
        copiedBytes += read;

        if (!timer.isValid() || timer.elapsed() >= ProgressUpdateIntervalMs) {
            if (m_abort) return;
            const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
            const QString label = QFileInfo(sourcePath).fileName();
            QMetaObject::invokeMethod(this, [this, progress, label]() {
                setProgress(progress);
                setCurrentLabel(label);
            }, Qt::QueuedConnection);
            timer.restart();
        }
    }

    destination.close();
    source.close();

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
    if (QFile::rename(sourcePath, destinationPath)) {
        copiedBytes += std::max<qint64>(1, totalBytesForPath(destinationPath));
        const double progress = static_cast<double>(copiedBytes) / static_cast<double>(totalBytes);
        QMetaObject::invokeMethod(this, [this, progress]() {
            setProgress(progress);
        }, Qt::QueuedConnection);
        return;
    }

    copyPath(sourcePath, destinationPath, totalBytes, copiedBytes);
    const QFileInfo sourceInfo(sourcePath);
    if (sourceInfo.isDir()) {
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
