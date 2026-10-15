#pragma once

#include "SftpPath.h"
#include <QVariantList>
#include <optional>
#include <functional>

namespace SftpProfiles {
QVariantList connections();
bool save(const SftpPath::Address &address, const QString &name, const QString &password,
          bool rememberPassword, QString *error, const std::function<bool()> &cancelled = {});
bool remove(const SftpPath::Address &address, QString *error, const std::function<bool()> &cancelled = {});
std::optional<QString> password(const SftpPath::Address &address, QString *error,
                              const std::function<bool()> &cancelled = {});
}
