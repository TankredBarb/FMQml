#pragma once

#include <memory>

#include "PlacesProviderPlugin.h"

class FileProvider;

std::unique_ptr<FileProvider> createPortableDeviceProvider();
QList<ProviderPlaceItem> portableDevicePlaces();
