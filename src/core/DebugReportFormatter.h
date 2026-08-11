#pragma once

#include <QString>
#include <QVariantMap>

namespace DebugReportFormatter {
QString format(const QVariantMap &snapshot, const QString &generatedAt, bool includePaths);
}
