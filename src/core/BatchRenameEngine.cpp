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
        
        QString ruleError;
        QString newName = applyRules(p.oldName, rules, i, &ruleError);
        p.newName = newName;
        
        p.newPath = info.dir().absoluteFilePath(newName);
        
        // Conflict detection
        if (!ruleError.isEmpty()) {
            p.hasConflict = true;
            p.error = ruleError;
        } else if (p.newName.isEmpty()) {
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

QString BatchRenameEngine::applyRules(const QString &name, const QVariantList &rules, int index, QString *error)
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
            bool regex = rule["regex"].toBool();
            
            if (!search.isEmpty()) {
                if (regex) {
                    QRegularExpression expression(search,
                                                  caseSensitive
                                                      ? QRegularExpression::NoPatternOption
                                                      : QRegularExpression::CaseInsensitiveOption);
                    if (!expression.isValid()) {
                        if (error) {
                            *error = QStringLiteral("Invalid regular expression: %1").arg(expression.errorString());
                        }
                        return QString();
                    }
                    result.replace(expression, normalizeRegexReplacement(replace));
                } else {
                    result.replace(search, replace, caseSensitive ? Qt::CaseSensitive : Qt::CaseInsensitive);
                }
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
        } else if (type == "transform") {
            result = applyTransform(result, rule["mode"].toString());
        }
    }

    return result + suffix;
}

QString BatchRenameEngine::applyTransform(const QString &name, const QString &mode) const
{
    if (mode == QStringLiteral("lowercase")) {
        return name.toLower();
    }
    if (mode == QStringLiteral("uppercase")) {
        return name.toUpper();
    }
    if (mode == QStringLiteral("titlecase")) {
        QString result = name.toLower();
        bool capitalizeNext = true;
        for (qsizetype i = 0; i < result.size(); ++i) {
            const QChar ch = result.at(i);
            if (ch.isLetterOrNumber()) {
                if (capitalizeNext) {
                    result[i] = ch.toUpper();
                    capitalizeNext = false;
                }
            } else {
                capitalizeNext = true;
            }
        }
        return result;
    }
    if (mode == QStringLiteral("trim")) {
        return name.trimmed();
    }
    if (mode == QStringLiteral("collapse-spaces")) {
        QString result = name.trimmed();
        result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        return result;
    }
    if (mode == QStringLiteral("spaces-underscore")) {
        QString result = name.trimmed();
        result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("_"));
        return result;
    }
    if (mode == QStringLiteral("spaces-dash")) {
        QString result = name.trimmed();
        result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral("-"));
        return result;
    }
    if (mode == QStringLiteral("remove-special")) {
        QString result = name;
        result.remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N} ._-]+")));
        result.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
        return result.trimmed();
    }
    return name;
}

QString BatchRenameEngine::normalizeRegexReplacement(const QString &replacement) const
{
    QString result;
    result.reserve(replacement.size());
    for (qsizetype i = 0; i < replacement.size(); ++i) {
        const QChar ch = replacement.at(i);
        if (ch != QLatin1Char('$') || i + 1 >= replacement.size()) {
            result.append(ch);
            continue;
        }

        const QChar next = replacement.at(i + 1);
        if (next == QLatin1Char('$')) {
            result.append(QLatin1Char('$'));
            ++i;
            continue;
        }
        if (!next.isDigit()) {
            result.append(ch);
            continue;
        }

        result.append(QLatin1Char('\\'));
        ++i;
        while (i < replacement.size() && replacement.at(i).isDigit()) {
            result.append(replacement.at(i));
            ++i;
        }
        --i;
    }
    return result;
}
