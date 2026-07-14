#include "GDriveRequestPolicy.h"

#include <algorithm>

#include <QDateTime>
#include <QDebug>
#include <QMutex>
#include <QRandomGenerator>
#include <QThread>

namespace GDriveRequestPolicy {
namespace {

constexpr int DriveApiCooldownMaxMs = 180000;
QMutex s_driveApiCooldownMutex;
qint64 s_driveApiCooldownUntilMs = 0;
int s_driveApiThrottleCount = 0;

bool loggingEnabled()
{
    return qEnvironmentVariableIntValue("FMQML_GDRIVE_UPLOAD_LOG") > 0;
}

qint64 monotonicNowMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}

} // namespace

bool isRetryableError(const QString &message)
{
    const QString lower = message.toLower();
    return lower.contains(QStringLiteral("429"))
        || lower.contains(QStringLiteral("too many requests"))
        || lower.contains(QStringLiteral("ratelimit"))
        || lower.contains(QStringLiteral("rate limit"))
        || lower.contains(QStringLiteral("user rate limit"))
        || lower.contains(QStringLiteral("http 500"))
        || lower.contains(QStringLiteral("http 502"))
        || lower.contains(QStringLiteral("http 503"))
        || lower.contains(QStringLiteral("http 504"))
        || lower.contains(QStringLiteral("temporarily unavailable"))
        || lower.contains(QStringLiteral("timed out"))
        || lower.contains(QStringLiteral("unknown protocol specified"))
        || lower.contains(QStringLiteral("protocolunknownerror"))
        || lower.contains(QStringLiteral("connection closed"))
        || lower.contains(QStringLiteral("remote host closed"));
}

bool isRateLimitError(const QString &message)
{
    const QString lower = message.toLower();
    return lower.contains(QStringLiteral("429"))
        || lower.contains(QStringLiteral("too many requests"))
        || lower.contains(QStringLiteral("ratelimit"))
        || lower.contains(QStringLiteral("rate limit"))
        || lower.contains(QStringLiteral("user rate limit"));
}

int retryDelayMs(const QHash<QByteArray, QByteArray> &headers, int fallbackAttempt)
{
    bool ok = false;
    const int retryAfterSeconds = QString::fromLatin1(headers.value(QByteArrayLiteral("retry-after")).trimmed()).toInt(&ok);
    if (ok && retryAfterSeconds > 0) {
        return std::clamp(retryAfterSeconds * 1000, 1000, 120000);
    }
    const int cappedAttempt = std::clamp(fallbackAttempt, 1, 8);
    const int baseMs = 1000 << (cappedAttempt - 1);
    const int jitterMs = QRandomGenerator::global()->bounded(500);
    return std::clamp(baseMs + jitterMs, 1000, 120000);
}

bool waitForCooldown(QLatin1StringView operation, const std::function<bool()> &shouldCancel)
{
    while (true) {
        if (shouldCancel && shouldCancel()) {
            return false;
        }
        qint64 waitMs = 0;
        int throttleCount = 0;
        {
            QMutexLocker locker(&s_driveApiCooldownMutex);
            waitMs = s_driveApiCooldownUntilMs - monotonicNowMs();
            throttleCount = s_driveApiThrottleCount;
        }
        if (waitMs <= 0) {
            return true;
        }
        const int sleepMs = static_cast<int>(std::clamp<qint64>(waitMs, 250, 5000));
        if (loggingEnabled()) {
            qInfo() << "GDrive API cooldown wait"
                    << "operation" << QString(operation)
                    << "delayMs" << sleepMs
                    << "remainingMs" << waitMs
                    << "throttleCount" << throttleCount;
        }
        QThread::msleep(static_cast<unsigned long>(sleepMs));
    }
}

void noteSuccess()
{
    QMutexLocker locker(&s_driveApiCooldownMutex);
    if (s_driveApiThrottleCount > 0 && s_driveApiCooldownUntilMs <= monotonicNowMs()) {
        --s_driveApiThrottleCount;
    }
}

void noteThrottle(QLatin1StringView operation,
                  const QHash<QByteArray, QByteArray> &headers,
                  int attempt,
                  const QString &message)
{
    const bool rateLimit = isRateLimitError(message);
    const int backoffAttempt = rateLimit ? attempt + 4 : attempt + 1;
    const int delayMs = std::clamp(retryDelayMs(headers, backoffAttempt), 1000, DriveApiCooldownMaxMs);
    qint64 cooldownUntil = 0;
    int throttleCount = 0;
    {
        QMutexLocker locker(&s_driveApiCooldownMutex);
        s_driveApiThrottleCount = std::clamp(s_driveApiThrottleCount + (rateLimit ? 2 : 1), 1, 12);
        const qint64 now = monotonicNowMs();
        s_driveApiCooldownUntilMs = (std::max)(s_driveApiCooldownUntilMs, now + delayMs);
        cooldownUntil = s_driveApiCooldownUntilMs;
        throttleCount = s_driveApiThrottleCount;
    }
    if (loggingEnabled()) {
        qInfo() << "GDrive API cooldown set"
                << "operation" << QString(operation)
                << "delayMs" << delayMs
                << "untilMs" << cooldownUntil
                << "throttleCount" << throttleCount
                << "message" << message.left(180);
    }
}

} // namespace GDriveRequestPolicy
