#pragma once

#include <QString>

#include "TelegramTypes.h"

namespace TelegramProviderInternal {

bool isTelegramSchemePath(const QString &path);
ParsedTelegramPath parseTelegramPath(const QString &path);
QString normalizedTelegramPath(const QString &path);
QString parentTelegramPath(const QString &path);
QString fileNameForTelegramPath(const QString &path);
QString childTelegramPath(const QString &parentPath, const QString &name);
QString telegramPathFromUserInput(const QString &value);

} // namespace TelegramProviderInternal
