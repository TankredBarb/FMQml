#pragma once

#include <QString>

namespace LocalFileCommit {

enum class Result {
    Committed,
    CrossDevice,
    Failed
};

Result replaceAtomically(const QString &incomingPath,
                         const QString &targetPath,
                         QString *error = nullptr);

}
