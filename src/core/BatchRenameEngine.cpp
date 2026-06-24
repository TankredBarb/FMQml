#include "BatchRenameEngine.h"
#include <QDir>
#include <QSet>

namespace {
bool sameFilesystemPath(const QString &left, const QString &right)
{
#ifdef Q_OS_WIN
    return QDir::fromNativeSeparators(left).compare(QDir::fromNativeSeparators(right), Qt::CaseInsensitive) == 0;
#else
    return QDir::fromNativeSeparators(left) == QDir::fromNativeSeparators(right);
#endif
}
}

BatchRenameEngine::BatchRenameEngine(QObject *parent)
    : QObject(parent)
{
}

QList<BatchRenameEngine::RenamePreview> BatchRenameEngine::generatePreview(const QStringList &paths, const QVariantList &rules)
{
    QList<RenamePreview> previews;
    QSet<QString> targetPaths;
    QSet<QString> targetNamesInSession;

    for (int i = 0; i < paths.size(); ++i) {
        const QString &path = paths.at(i);
        QFileInfo info(path);
        
        RenamePreview p;
        p.oldPath = path;
        p.oldName = info.fileName();
        
        QString newName = applyRules(p.oldName, rules, i);
        p.newName = newName;
        
        p.newPath = info.dir().absoluteFilePath(newName);
        
        // Conflict detection
        if (p.newName.isEmpty()) {
            p.hasConflict = true;
            p.error = "New name cannot be empty";
        } else if (p.newName == p.oldName) {
            // No change, not a conflict per se but keep track
        } else if (QFile::exists(p.newPath) && !sameFilesystemPath(path, p.newPath)) {
            p.hasConflict = true;
            p.error = "File already exists";
        } else if (targetNamesInSession.contains(p.newName.toLower())) {
            p.hasConflict = true;
            p.error = "Duplicate name in batch";
        }
        
        targetNamesInSession.insert(p.newName.toLower());
        previews.append(p);
    }

    return previews;
}

QString BatchRenameEngine::applyRules(const QString &name, const QVariantList &rules, int index)
{
    QFileInfo info(name);
    QString baseName = info.completeBaseName();
    QString suffix = info.suffix();
    if (!suffix.isEmpty()) suffix.prepend('.');

    QString result = baseName;

    for (const QVariant &vRule : rules) {
        QVariantMap rule = vRule.toMap();
        QString type = rule["type"].toString();

        if (type == "replace") {
            QString search = rule["search"].toString();
            QString replace = rule["replace"].toString();
            bool caseSensitive = rule["caseSensitive"].toBool();
            
            if (!search.isEmpty()) {
                result.replace(search, replace, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
            }
        } else if (type == "format") {
            QString prefix = rule["prefix"].toString();
            QString suffixStr = rule["suffix"].toString();
            result = prefix + result + suffixStr;
        } else if (type == "numbering") {
            int start = rule["start"].toInt();
            int padding = rule["padding"].toInt();
            QString numStr = QString::number(start + index).rightJustified(padding, '0');
            
            QString pos = rule["position"].toString(); // "prefix" or "suffix"
            if (pos == "prefix") result = numStr + result;
            else result = result + numStr;
        } else if (type == "template") {
            QString text = rule["text"].toString();
            int start = rule["start"].toInt();
            int padding = rule["padding"].toInt();
            QString numStr = QString::number(start + index).rightJustified(padding, '0');
            result = text + numStr;
        }
    }

    return result + suffix;
}
