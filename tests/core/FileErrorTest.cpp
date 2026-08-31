#include "FileError.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {

int fail(const QString &message)
{
    QTextStream(stderr) << message << '\n';
    return 1;
}

bool hasClassification(const QVariantMap &error,
                       const QString &code,
                       const QStringList &actions)
{
    return error.value(QStringLiteral("code")).toString() == code
        && error.value(QStringLiteral("actions")).toStringList() == actions
        && error.value(QStringLiteral("recoverable")).toBool() == !actions.isEmpty();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const QVariantMap empty = FileError::classify({}, QStringLiteral("/tmp/item"), QStringLiteral("copy"));
    if (!hasClassification(empty, QStringLiteral("none"), {})
        || !empty.value(QStringLiteral("message")).toString().isEmpty()) {
        return fail(QStringLiteral("empty errors should remain non-recoverable"));
    }

    const QVariantMap denied = FileError::classify(
        QStringLiteral("Permission denied"), QStringLiteral("/protected/item"), QStringLiteral("delete"));
    if (!hasClassification(denied,
                           QStringLiteral("accessDenied"),
                           {QStringLiteral("retry"), QStringLiteral("restartAsAdmin"), QStringLiteral("copyPath")})
        || denied.value(QStringLiteral("title")).toString() != QStringLiteral("Access denied")
        || denied.value(QStringLiteral("message")).toString()
            != QStringLiteral("You do not have permission to delete this item from this location.")) {
        return fail(QStringLiteral("access-denied delete classification is incorrect"));
    }

    struct ClassificationCase {
        QString message;
        QString code;
        QStringList actions;
    };
    const QList<ClassificationCase> cases = {
        {QStringLiteral("File is being used by another process"), QStringLiteral("inUse"),
         {QStringLiteral("retry"), QStringLiteral("copyPath")}},
        {QStringLiteral("An item with the same name already exists"), QStringLiteral("alreadyExists"),
         {QStringLiteral("copyPath")}},
        {QStringLiteral("The filename is invalid"), QStringLiteral("invalidName"),
         {QStringLiteral("copyPath")}},
        {QStringLiteral("Cannot find the requested path"), QStringLiteral("pathNotFound"),
         {QStringLiteral("refresh"), QStringLiteral("copyPath")}},
        {QStringLiteral("There is not enough space on the drive"), QStringLiteral("diskFull"),
         {QStringLiteral("retry"), QStringLiteral("copyPath")}},
        {QStringLiteral("Provider is read-only"), QStringLiteral("readOnly"),
         {QStringLiteral("copyPath")}},
        {QStringLiteral("Operation is not supported"), QStringLiteral("unsupportedOperation"),
         {QStringLiteral("copyPath")}},
        {QStringLiteral("Encrypted archive requires a password"), QStringLiteral("authRequired"),
         {QStringLiteral("retry"), QStringLiteral("copyPath")}},
        {QStringLiteral("Unexpected provider failure"), QStringLiteral("unknown"),
         {QStringLiteral("retry"), QStringLiteral("copyPath")}}
    };

    for (const ClassificationCase &test : cases) {
        const QVariantMap error = FileError::classify(test.message, QStringLiteral("provider://item"),
                                                      QStringLiteral("open"));
        if (!hasClassification(error, test.code, test.actions)) {
            return fail(QStringLiteral("classification failed for '%1'").arg(test.message));
        }
    }

    return 0;
}
