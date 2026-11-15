#pragma once

#include <QString>
#include <QDBusUnixFileDescriptor>
#include <QList>
#include <QMetaType>

namespace BtrfsSpaceProtocol {
inline constexpr auto Service = "io.fmqml.fm.Space";
inline constexpr auto Path = "/io/fmqml/fm/Space";
inline constexpr auto Interface = "io.fmqml.fm.Space2";
inline constexpr int Version = 2;
inline constexpr int TimeoutMs = 1500;
inline constexpr int BatchSize = 16;
using Descriptors = QList<QDBusUnixFileDescriptor>;
}

Q_DECLARE_METATYPE(BtrfsSpaceProtocol::Descriptors)
