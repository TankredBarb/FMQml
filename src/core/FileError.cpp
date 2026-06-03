#include "FileError.h"

namespace {
bool containsAny(const QString &text, const QStringList &needles)
{
    for (const QString &needle : needles) {
        if (text.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QStringList uniqueActions(const QStringList &actions)
{
    QStringList deduped;
    for (const QString &action : actions) {
        if (!action.isEmpty() && !deduped.contains(action)) {
            deduped.append(action);
        }
    }
    return deduped;
}

QString informativeMessageFor(FileError::Code code,
                              const QString &message,
                              const QString &path,
                              const QString &operation)
{
    if (code != FileError::Code::AccessDenied) {
        return message;
    }

    const QString targetPath = path.trimmed();
    const QString op = operation.trimmed().toLower();

    if (op == QStringLiteral("delete")) {
        if (!targetPath.isEmpty()) {
            return QStringLiteral("You do not have permission to delete this item from this location.");
        }
        return QStringLiteral("You do not have permission to delete this item.");
    }

    if (op == QStringLiteral("move")) {
        return QStringLiteral("You do not have permission to move items to this location.");
    }

    if (op == QStringLiteral("copy") || op == QStringLiteral("extract")) {
        return QStringLiteral("You do not have permission to write items to this location.");
    }

    return QStringLiteral("You do not have permission to complete this operation here.");
}
}

namespace FileError {

QString codeToString(Code code)
{
    switch (code) {
    case Code::None:
        return QStringLiteral("none");
    case Code::AccessDenied:
        return QStringLiteral("accessDenied");
    case Code::InUse:
        return QStringLiteral("inUse");
    case Code::AlreadyExists:
        return QStringLiteral("alreadyExists");
    case Code::InvalidName:
        return QStringLiteral("invalidName");
    case Code::DiskFull:
        return QStringLiteral("diskFull");
    case Code::PathNotFound:
        return QStringLiteral("pathNotFound");
    case Code::DriveNotReady:
        return QStringLiteral("driveNotReady");
    case Code::ReadOnly:
        return QStringLiteral("readOnly");
    case Code::NetworkUnavailable:
        return QStringLiteral("networkUnavailable");
    case Code::AuthRequired:
        return QStringLiteral("authRequired");
    case Code::QuotaExceeded:
        return QStringLiteral("quotaExceeded");
    case Code::RateLimited:
        return QStringLiteral("rateLimited");
    case Code::UnsupportedOperation:
        return QStringLiteral("unsupportedOperation");
    case Code::Unknown:
        return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString titleForCode(Code code)
{
    switch (code) {
    case Code::None:
        return {};
    case Code::AccessDenied:
        return QStringLiteral("Access denied");
    case Code::InUse:
        return QStringLiteral("File is in use");
    case Code::AlreadyExists:
        return QStringLiteral("Item already exists");
    case Code::InvalidName:
        return QStringLiteral("Invalid name");
    case Code::DiskFull:
        return QStringLiteral("Disk is full");
    case Code::PathNotFound:
        return QStringLiteral("Location not found");
    case Code::DriveNotReady:
        return QStringLiteral("Drive is not ready");
    case Code::ReadOnly:
        return QStringLiteral("Read-only location");
    case Code::NetworkUnavailable:
        return QStringLiteral("Network unavailable");
    case Code::AuthRequired:
        return QStringLiteral("Authentication required");
    case Code::QuotaExceeded:
        return QStringLiteral("Storage quota exceeded");
    case Code::RateLimited:
        return QStringLiteral("Too many requests");
    case Code::UnsupportedOperation:
        return QStringLiteral("Unsupported operation");
    case Code::Unknown:
        return QStringLiteral("Operation failed");
    }
    return QStringLiteral("Operation failed");
}

QVariantMap make(Code code,
                 const QString &path,
                 const QString &operation,
                 const QString &message,
                 const QStringList &actions)
{
    QVariantMap map;
    map.insert(QStringLiteral("code"), codeToString(code));
    map.insert(QStringLiteral("title"), titleForCode(code));
    map.insert(QStringLiteral("path"), path);
    map.insert(QStringLiteral("operation"), operation);
    map.insert(QStringLiteral("message"), message);
    map.insert(QStringLiteral("actions"), actions);
    map.insert(QStringLiteral("recoverable"), !actions.isEmpty());
    return map;
}

QVariantMap classify(const QString &message, const QString &path, const QString &operation)
{
    if (message.trimmed().isEmpty()) {
        return make(Code::None, path, operation, {}, {});
    }

    const QString text = message.toLower();
    Code code = Code::Unknown;
    QStringList actions = {QStringLiteral("copyPath")};

    if (containsAny(text, {
            QStringLiteral("password"),
            QStringLiteral("encrypted archive")
        })) {
        code = Code::AuthRequired;
        actions.prepend(QStringLiteral("retry"));
    } else if (containsAny(text, {
            QStringLiteral("not readable"),
            QStringLiteral("access denied"),
            QStringLiteral("permission denied"),
            QStringLiteral("no access"),
            QStringLiteral("unauthorized"),
            QStringLiteral("requires administrator"),
            QStringLiteral("requires elevated"),
            QStringLiteral("protected location")
        })) {
        code = Code::AccessDenied;
        actions.prepend(QStringLiteral("restartAsAdmin"));
        actions.prepend(QStringLiteral("retry"));
    } else if (containsAny(text, {
                   QStringLiteral("being used by another process"),
                   QStringLiteral("sharing violation"),
                   QStringLiteral("lock violation"),
                   QStringLiteral("in use")
               })) {
        code = Code::InUse;
        actions.prepend(QStringLiteral("retry"));
    } else if (containsAny(text, {
                   QStringLiteral("already exists"),
                   QStringLiteral("same name already exists"),
                   QStringLiteral("file exists")
               })) {
        code = Code::AlreadyExists;
    } else if (containsAny(text, {
                   QStringLiteral("name is invalid"),
                   QStringLiteral("invalid name"),
                   QStringLiteral("filename, directory name, or volume label syntax is incorrect")
               })) {
        code = Code::InvalidName;
    } else if (containsAny(text, {
                   QStringLiteral("does not exist"),
                   QStringLiteral("not found"),
                   QStringLiteral("missing"),
                   QStringLiteral("cannot find")
               })) {
        code = Code::PathNotFound;
        actions.prepend(QStringLiteral("refresh"));
    } else if (containsAny(text, {
                   QStringLiteral("drive is not ready"),
                   QStringLiteral("device is not ready"),
                   QStringLiteral("not ready")
               })) {
        code = Code::DriveNotReady;
        actions.prepend(QStringLiteral("retry"));
    } else if (containsAny(text, {
                   QStringLiteral("disk is full"),
                   QStringLiteral("drive is full"),
                   QStringLiteral("there is not enough space"),
                   QStringLiteral("not enough space"),
                   QStringLiteral("quota exceeded")
               })) {
        code = Code::DiskFull;
        actions.prepend(QStringLiteral("retry"));
    } else if (containsAny(text, {
                   QStringLiteral("read-only"),
                   QStringLiteral("read only"),
                   QStringLiteral("provider is read-only")
               })) {
        code = Code::ReadOnly;
    } else if (containsAny(text, {
                   QStringLiteral("unsupported"),
                   QStringLiteral("not supported")
               })) {
        code = Code::UnsupportedOperation;
    } else {
        actions.prepend(QStringLiteral("retry"));
    }

    return make(code, path, operation, informativeMessageFor(code, message, path, operation), uniqueActions(actions));
}

} // namespace FileError
