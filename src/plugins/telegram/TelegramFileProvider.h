#pragma once

#include <memory>

class FileProvider;

namespace TelegramProviderInternal {

std::unique_ptr<FileProvider> createTelegramFileProvider();

} // namespace TelegramProviderInternal
