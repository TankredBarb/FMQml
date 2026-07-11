#pragma once

#include <QString>

class LinuxAdminPolicy final
{
public:
    enum class Operation {
        CopyFile,
        MakeDirectory,
        AtomicReplace,
        CreateFile,
        RenamePath,
        DeletePath,
        ChangeMode,
        ChangeOwnership
    };

    struct Decision {
        bool allowed = false;
        QString errorCode;
        QString errorMessage;
        QString failedPath;
    };

    static Decision validate(Operation operation, const QString &sourcePath, const QString &destinationPath);
};
