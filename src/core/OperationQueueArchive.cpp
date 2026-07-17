#include "OperationQueue.h"
#include "OperationQueuePrivate.h"

#include "ArchiveFileProvider.h"
#include "ArchiveSupport.h"
#include "CleanupSubsystem.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QMetaObject>
#include <QProcess>
#include <QRegularExpression>
#include <QScopeGuard>
#include <QSet>

#include <algorithm>
#include <atomic>
#include <limits>
#include <stdexcept>

namespace {

QString sevenZipArchiveTypeForPath(const QString &archivePath)
{
    const QString lower = archivePath.toLower();
    if (lower.endsWith(QStringLiteral(".zip"))) {
        return QStringLiteral("zip");
    }
    if (lower.endsWith(QStringLiteral(".gz")) || lower.endsWith(QStringLiteral(".gzip"))) {
        return QStringLiteral("gzip");
    }
    if (lower.endsWith(QStringLiteral(".bz2")) || lower.endsWith(QStringLiteral(".bzip2"))) {
        return QStringLiteral("bzip2");
    }
    if (lower.endsWith(QStringLiteral(".xz"))) {
        return QStringLiteral("xz");
    }
    return QStringLiteral("7z");
}

bool isSingleFileCompressionType(const QString &type)
{
    return type == QLatin1String("gzip")
        || type == QLatin1String("bzip2")
        || type == QLatin1String("xz");
}

} // namespace

QStringList OperationQueue::extractArchiveContents(const QString &sourcePath, const QString &destinationPath,
                                                   qint64 totalBytes, qint64 &copiedBytes)
{
    if (m_abort) return {};

    FileProvider* srcProvider = getProviderForPath(sourcePath);
    FileProvider* destProvider = getProviderForPath(destinationPath);

    if (!ArchiveSupport::isArchivePath(sourcePath)) {
        const QString fallbackDestination = destProvider->childPath(destinationPath, srcProvider->fileName(sourcePath));
        const QString finalPath = copyPath(sourcePath, fallbackDestination, totalBytes, copiedBytes, Type::Extract);
        return finalPath.isEmpty() ? QStringList{} : QStringList{finalPath};
    }

    const QString physicalArchivePath = ArchiveSupport::physicalArchivePath(sourcePath);
    const QStringList archiveTokens = ArchiveSupport::splitArchiveTokens(sourcePath);
    if (archiveTokens.size() == 2
        && ArchiveSupport::archiveBrowsePath(sourcePath) == QLatin1String("/")
        && ArchiveSupport::isArchiveFilePath(physicalArchivePath)) {
        QString error;
        const qint64 progressBaseBytes = copiedBytes;
        const qint64 archiveBytes = (std::max<qint64>)(1, QFileInfo(physicalArchivePath).size());
        std::atomic<qint64> extractedEntries{0};
        std::atomic<qint64> lastProgressEntry{0};
        QStringList topLevelPaths;
        QSet<QString> seenTopLevelPaths;
        if (m_abort) {
            return {};
        }
        const bool extracted = ArchiveFileProvider::extractArchiveFileTo(
            physicalArchivePath,
            destinationPath,
            &error,
            [this, progressBaseBytes, archiveBytes, totalBytes](uint64_t processed) -> bool {
                if (m_abort) {
                    return false;
                }
                if (totalBytes > 0 && processed <= static_cast<uint64_t>(archiveBytes)) {
                    const qint64 progressBytes = (std::min)(
                        totalBytes,
                        progressBaseBytes + static_cast<qint64>(processed));
                    const double progress = static_cast<double>(progressBytes) / static_cast<double>(totalBytes);
                    QMetaObject::invokeMethod(this, [this, progress]() {
                        setProgress(progress);
                    }, Qt::QueuedConnection);
                    updateMetrics(progressBytes, totalBytes);
                } else if (processed > static_cast<uint64_t>(archiveBytes)) {
                    QMetaObject::invokeMethod(this, [this]() {
                        m_speedText = QStringLiteral("0 B/s");
                        m_remainingTimeText.clear();
                        emit speedChanged();
                    }, Qt::QueuedConnection);
                }
                return true;
            },
            [this, &extractedEntries, &lastProgressEntry, &topLevelPaths, &seenTopLevelPaths,
             destinationPath, destProvider](const QString &filePath) {
                const QString normalized = QDir::fromNativeSeparators(filePath);
                const QString topName = normalized.section(QLatin1Char('/'), 0, 0).trimmed();
                if (!topName.isEmpty()) {
                    const QString topPath = destProvider->childPath(destinationPath, topName);
                    if (!topPath.isEmpty() && !seenTopLevelPaths.contains(topPath)) {
                        seenTopLevelPaths.insert(topPath);
                        topLevelPaths.append(topPath);
                    }
                }
                const qint64 current = extractedEntries.fetch_add(1) + 1;
                const qint64 minStep = 200;
                const qint64 previous = lastProgressEntry.load();
                if (current - previous < minStep) {
                    return;
                }
                lastProgressEntry.store(current);
                const double progress = (std::min)(0.95, static_cast<double>(current) / static_cast<double>(current + 2000));
                QMetaObject::invokeMethod(this, [this, progress, current, fileName = QFileInfo(filePath).fileName()]() {
                    if (!fileName.isEmpty()) {
                        setCurrentLabel(OperationQueuePrivate::operationItemLabel(Type::Extract, fileName));
                    }
                    setCompletedItems(static_cast<int>((std::min<qint64>)(current, (std::numeric_limits<int>::max)())));
                    if (m_totalItems < m_completedItems) {
                        setTotalItems(m_completedItems);
                    }
                    setProgress(progress);
                }, Qt::QueuedConnection);
            });

        if (!extracted) {
            throw std::runtime_error(error.isEmpty()
                ? QStringLiteral("Cannot extract archive %1").arg(physicalArchivePath).toStdString()
                : error.toStdString());
        }

        const qint64 finalEntryCount = extractedEntries.load();
        if (finalEntryCount > 0) {
            QMetaObject::invokeMethod(this, [this, finalEntryCount]() {
                const int boundedCount = static_cast<int>((std::min<qint64>)(finalEntryCount, (std::numeric_limits<int>::max)()));
                setCompletedItems(boundedCount);
                setTotalItems(boundedCount);
            }, Qt::QueuedConnection);
        }
        copiedBytes = (std::min)(totalBytes, progressBaseBytes + archiveBytes);
        return topLevelPaths;
    }

    if (!makePath(destinationPath)) {
        throw std::runtime_error(QStringLiteral("Cannot create folder %1").arg(destinationPath).toStdString());
    }

    QString extractionRoot = sourcePath;
    const QString archiveName = ArchiveSupport::archiveFileName(sourcePath);
    if (ArchiveSupport::isArchiveExtension(QFileInfo(archiveName).suffix().toLower())
        && ArchiveSupport::archiveBrowsePath(sourcePath) != QLatin1String("/")) {
        extractionRoot = ArchiveSupport::archiveRootPathForPath(sourcePath);
    }

    const QStringList children = childPaths(extractionRoot);
    QStringList sourceItems;
    QStringList destinationItems;
    sourceItems.reserve(children.size());
    destinationItems.reserve(children.size());
    for (const QString &child : children) {
        const std::optional<FileEntry> childInfo = srcProvider->entryInfo(child);
        const QString childName = childInfo ? childInfo->name : srcProvider->fileName(child);
        if (childName.isEmpty()) {
            continue;
        }
        sourceItems.append(child);
        destinationItems.append(destProvider->childPath(destinationPath, childName));
    }

    if (sourceItems.isEmpty()) {
        copiedBytes = (std::max)(copiedBytes, totalBytes);
        return {};
    }

    QString error;
    const qint64 baseBytes = copiedBytes;
    const qint64 remainingBytes = (std::max<qint64>)(1, totalBytes - baseBytes);
    const bool extracted = ArchiveFileProvider::extractArchiveItemsTo(
        sourceItems,
        destinationItems,
        &error,
        [this, baseBytes, remainingBytes, totalBytes](uint64_t processed, uint64_t backendTotal) -> bool {
            if (m_abort) {
                return false;
            }
            const double fraction = backendTotal > 0
                ? std::clamp(static_cast<double>(processed) / static_cast<double>(backendTotal), 0.0, 1.0)
                : 0.0;
            const qint64 clampedBytes = static_cast<qint64>(fraction * static_cast<double>(remainingBytes));
            const qint64 progressBytes = baseBytes + clampedBytes;
            const double progress = static_cast<double>(progressBytes) / static_cast<double>((std::max<qint64>)(1, totalBytes));
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(progressBytes, totalBytes);
            return true;
        });
    if (!extracted) {
        if (m_abort) {
            return {};
        }
        throw std::runtime_error(error.isEmpty()
            ? QStringLiteral("Cannot extract archive contents from %1").arg(sourcePath).toStdString()
            : error.toStdString());
    }

    copiedBytes = totalBytes;
    return destinationItems;
}

void OperationQueue::compressPathsToSevenZip(const QStringList &sources, const QString &archivePath, qint64 totalBytes)
{
    if (m_abort || sources.isEmpty() || archivePath.isEmpty()) {
        return;
    }

    const QString executable = ArchiveSupport::sevenZipExecutablePath();
    if (executable.isEmpty()) {
        throw std::runtime_error("7-Zip executable was not found");
    }

    const QFileInfo archiveInfo(archivePath);
    const QString parentPath = archiveInfo.absolutePath();
    const QString archiveType = sevenZipArchiveTypeForPath(archivePath);
    if (archivePath.toLower().endsWith(QStringLiteral(".tar.gz"))
        || archivePath.toLower().endsWith(QStringLiteral(".tgz"))) {
        throw std::runtime_error("tar.gz compression is not available in a single 7-Zip pass");
    }
    if (isSingleFileCompressionType(archiveType)) {
        if (sources.size() != 1 || !QFileInfo(sources.constFirst()).isFile()) {
            throw std::runtime_error(QStringLiteral("%1 compression supports a single file only")
                                         .arg(archiveType)
                                         .toStdString());
        }
    }
    if (!QFileInfo(parentPath).isDir()) {
        throw std::runtime_error(QStringLiteral("Cannot create archive in %1").arg(parentPath).toStdString());
    }

    const QString tempArchivePath = archivePath + QStringLiteral(".part");
    QFile::remove(tempArchivePath);
    QFile::remove(archivePath);

    QString tempArchiveLeaseId;
    bool tempArchiveFinalized = false;
    CleanupSubsystem::instance().registerArtifact(
        CleanupArtifactKind::PartFile,
        tempArchivePath,
        QFileInfo(tempArchivePath).absolutePath(),
        false,
        &tempArchiveLeaseId);
    const auto tempArchiveCleanup = qScopeGuard([&]() {
        if (tempArchiveLeaseId.isEmpty()) {
            return;
        }
        if (tempArchiveFinalized) {
            CleanupSubsystem::instance().completeWithoutDelete(tempArchiveLeaseId);
        } else {
            CleanupSubsystem::instance().scheduleDeleteOnFailure(tempArchiveLeaseId);
        }
    });

    QStringList arguments = {
        QStringLiteral("a"),
        QStringLiteral("-t%1").arg(archiveType),
        QStringLiteral("-y"),
        QStringLiteral("-bso0"),
        QStringLiteral("-bsp1"),
        QStringLiteral("-bse1"),
        QDir::toNativeSeparators(tempArchivePath),
    };

    for (const QString &source : sources) {
        const QFileInfo sourceInfo(source);
        const QString sourceParent = sourceInfo.absolutePath();
        arguments.append(sourceParent.compare(parentPath, Qt::CaseInsensitive) == 0
                             ? sourceInfo.fileName()
                             : QDir::toNativeSeparators(sourceInfo.absoluteFilePath()));
    }

    QProcess process;
    process.setProgram(executable);
    process.setArguments(arguments);
    process.setWorkingDirectory(parentPath);
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();

    if (!process.waitForStarted(5000)) {
        QFile::remove(tempArchivePath);
        throw std::runtime_error(QStringLiteral("Could not start 7-Zip: %1").arg(process.errorString()).toStdString());
    }

    QByteArray outputBuffer;
    int lastPercent = -1;
    QElapsedTimer progressTimer;
    progressTimer.start();
    const qint64 boundedTotalBytes = std::max<qint64>(1, totalBytes);
    const QRegularExpression percentPattern(QStringLiteral("(\\d{1,3})%"));

    auto consumeOutput = [&]() {
        outputBuffer.append(process.readAll());
        if (outputBuffer.size() > 8192) {
            outputBuffer = outputBuffer.right(8192);
        }

        const QString text = QString::fromLocal8Bit(outputBuffer);
        QRegularExpressionMatchIterator matches = percentPattern.globalMatch(text);
        int percent = -1;
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();
            bool ok = false;
            const int value = match.captured(1).toInt(&ok);
            if (ok) {
                percent = std::clamp(value, 0, 100);
            }
        }

        if (percent >= 0 && percent != lastPercent && progressTimer.elapsed() >= 120) {
            lastPercent = percent;
            progressTimer.restart();
            const qint64 processedBytes = (boundedTotalBytes * percent) / 100;
            const double progress = static_cast<double>(processedBytes) / static_cast<double>(boundedTotalBytes);
            QMetaObject::invokeMethod(this, [this, progress]() {
                setProgress(progress);
            }, Qt::QueuedConnection);
            updateMetrics(processedBytes, boundedTotalBytes);
        }
    };

    while (!process.waitForFinished(100)) {
        consumeOutput();
        if (m_abort) {
            process.kill();
            process.waitForFinished(3000);
            QFile::remove(tempArchivePath);
            return;
        }
    }
    consumeOutput();

    if (m_abort) {
        QFile::remove(tempArchivePath);
        return;
    }

    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        const QString output = QString::fromLocal8Bit(outputBuffer).trimmed();
        QFile::remove(tempArchivePath);
        throw std::runtime_error(output.isEmpty()
            ? QStringLiteral("7-Zip compression failed").toStdString()
            : output.toStdString());
    }

    if (QFile::exists(archivePath) && !QFile::remove(archivePath)) {
        QFile::remove(tempArchivePath);
        throw std::runtime_error(QStringLiteral("Cannot replace %1").arg(archivePath).toStdString());
    }
    if (!QFile::rename(tempArchivePath, archivePath)) {
        QFile::remove(tempArchivePath);
        throw std::runtime_error(QStringLiteral("Cannot finalize %1").arg(archivePath).toStdString());
    }
    tempArchiveFinalized = true;

    QMetaObject::invokeMethod(this, [this]() {
        setProgress(1.0);
    }, Qt::QueuedConnection);
    updateMetrics(boundedTotalBytes, boundedTotalBytes);
}


qint64 OperationQueue::totalBytesForExtraction(const QStringList &sources) const
{
    qint64 total = 0;
    for (const QString &source : sources) {
        if (m_abort) {
            break;
        }

        if (ArchiveSupport::isArchivePath(source)
            && ArchiveSupport::archiveBrowsePath(source) == QLatin1String("/")
            && ArchiveSupport::splitArchiveTokens(source).size() == 2) {
            const QString physicalPath = ArchiveSupport::physicalArchivePath(source);
            if (ArchiveSupport::isArchiveFilePath(physicalPath)) {
                total += (std::max<qint64>)(1, QFileInfo(physicalPath).size());
                continue;
            }
        }

        if (ArchiveSupport::isArchivePath(source)
            && ArchiveSupport::archiveBrowsePath(source) != QLatin1String("/")
            && ArchiveSupport::isArchiveExtension(
                QFileInfo(ArchiveSupport::archiveFileName(source)).suffix().toLower())) {
            total += totalBytesForPath(ArchiveSupport::archiveRootPathForPath(source));
            continue;
        }

        total += totalBytesForPath(source);
    }
    return total;
}
