#pragma once

#include "FileProvider.h"
#include "TelegramTypes.h"

namespace TelegramProviderInternal {

FileEntry fileEntryFromTelegramEntry(const TelegramEntry &entry);
TelegramEntry rootEntry(const QString &name, const QString &path, const QString &label);

} // namespace TelegramProviderInternal
