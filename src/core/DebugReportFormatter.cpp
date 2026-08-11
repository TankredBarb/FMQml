#include "DebugReportFormatter.h"

#include "DebugReportSanitizer.h"

#include <QVariantList>

QString DebugReportFormatter::format(const QVariantMap &snapshot,
                                     const QString &generatedAt,
                                     bool includePaths)
{
    QStringList lines{QStringLiteral("FM Debug Information"),
                      QStringLiteral("Generated: %1").arg(generatedAt), QString()};
    const QVariantList sections = snapshot.value(QStringLiteral("sections")).toList();
    for (const QVariant &sectionValue : sections) {
        const QVariantMap sectionMap = sectionValue.toMap();
        lines.append(sectionMap.value(QStringLiteral("title")).toString());
        const QVariantList rows = sectionMap.value(QStringLiteral("rows")).toList();
        for (const QVariant &rowValue : rows) {
            const QVariantMap rowMap = rowValue.toMap();
            if (rowMap.value(QStringLiteral("path")).toBool() && !includePaths) {
                continue;
            }
            lines.append(QStringLiteral("%1: %2")
                .arg(rowMap.value(QStringLiteral("label")).toString(),
                     DebugReportSanitizer::sanitize(rowMap.value(QStringLiteral("value")).toString())));
        }
        lines.append(QString());
    }
    return lines.join(QLatin1Char('\n')).trimmed();
}
