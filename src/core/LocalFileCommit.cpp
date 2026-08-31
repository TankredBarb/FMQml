#include "LocalFileCommit.h"

#include <QFile>

#ifdef Q_OS_WIN
#include <windows.h>
#else
#include <cerrno>
#include <cstdio>
#include <cstring>
#endif

namespace LocalFileCommit {

Result replaceAtomically(const QString &incomingPath, const QString &targetPath, QString *error)
{
#ifdef Q_OS_WIN
    const std::wstring incoming = incomingPath.toStdWString();
    const std::wstring target = targetPath.toStdWString();
    if (MoveFileExW(incoming.c_str(), target.c_str(),
                    MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return Result::Committed;
    }
    const DWORD nativeError = GetLastError();
    if (error) {
        *error = QStringLiteral("Windows error %1").arg(nativeError);
    }
    return nativeError == ERROR_NOT_SAME_DEVICE ? Result::CrossDevice : Result::Failed;
#else
    const QByteArray incoming = QFile::encodeName(incomingPath);
    const QByteArray target = QFile::encodeName(targetPath);
    if (::rename(incoming.constData(), target.constData()) == 0) {
        return Result::Committed;
    }
    const int nativeError = errno;
    if (error) {
        *error = QString::fromLocal8Bit(std::strerror(nativeError));
    }
    return nativeError == EXDEV ? Result::CrossDevice : Result::Failed;
#endif
}

}
