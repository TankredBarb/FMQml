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
        ChangeOwnership,
        ListDirectory,
        ReadFile
    };

    struct Decision {
        bool allowed = false;
        QString errorCode;
        QString errorMessage;
        QString failedPath;
    };

    // Performs only validation that does not require filesystem access.  This
    // is suitable for the unprivileged side of requests whose target may be
    // hidden behind a directory the current user cannot traverse.  The helper
    // must still call validate() before executing the request.
    static Decision validateSourcePathShape(const QString &sourcePath);
    static Decision validateDestinationPathShape(const QString &destinationPath);
    static Decision validate(Operation operation, const QString &sourcePath, const QString &destinationPath);
};
