#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <QVariantMap>
#include <QFileInfo>
#include <QRegularExpression>

class BatchRenameEngine : public QObject {
    Q_OBJECT

public:
    enum class RuleType {
        SearchReplace,
        Format, // Prefix/Suffix
        Numbering
    };

    struct RenamePreview {
        QString oldPath;
        QString oldName;
        QString newName;
        QString newPath;
        bool hasConflict = false;
        QString error;
    };

    explicit BatchRenameEngine(QObject *parent = nullptr);

    // rules: QVariantList of maps, e.g. { "type": "replace", "search": "...", "replace": "..." }
    QList<RenamePreview> generatePreview(const QStringList &paths, const QVariantList &rules);

private:
    QString applyRules(const QString &name, const QVariantList &rules, int index, int totalCount);
};
