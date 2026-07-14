#pragma once

#include <functional>

#include <QByteArray>
#include <QHash>
#include <QString>

namespace GDriveRequestPolicy {

bool waitForCooldown(QLatin1StringView operation, const std::function<bool()> &shouldCancel = {});
void noteSuccess();
void noteThrottle(QLatin1StringView operation,
                  const QHash<QByteArray, QByteArray> &headers,
                  int attempt,
                  const QString &message);
bool isRetryableError(const QString &message);
bool isRateLimitError(const QString &message);
int retryDelayMs(const QHash<QByteArray, QByteArray> &headers, int fallbackAttempt);

} // namespace GDriveRequestPolicy
