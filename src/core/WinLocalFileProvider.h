#pragma once

// LocalFileProvider.h pulls in Qt (which defines Q_OS_WIN) so this include
// MUST come before the #ifdef Q_OS_WIN guard below.
#include "LocalFileProvider.h"

#ifdef Q_OS_WIN

// WinLocalFileProvider overrides the scan() method of LocalFileProvider to use
// the Windows-native FindFirstFileExW API with FIND_FIRST_EX_LARGE_FETCH and
// FindExInfoBasic flags. This avoids a separate GetFileAttributesEx syscall per
// file that Qt's QDirIterator triggers internally, giving a significant speedup
// on directories with hundreds of entries.
//
// All other FileProvider methods (rename, create, remove, etc.) are inherited
// unchanged from LocalFileProvider.
class WinLocalFileProvider final : public LocalFileProvider {

public:
    explicit WinLocalFileProvider(QObject *parent = nullptr);
    ~WinLocalFileProvider() override = default;

    void scan(const QString &path) override;
};

#endif // Q_OS_WIN
