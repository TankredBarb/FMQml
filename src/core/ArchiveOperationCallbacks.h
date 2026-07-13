#pragma once

#include <QtGlobal>

#include <cstdint>
#include <functional>

struct ArchiveOperationCallbacks
{
    std::function<bool()> abortRequested;
    std::function<void(qint64)> progressBytes;

    bool isAbortRequested() const;
    void reportProgress(uint64_t processedBytes) const;

    static const ArchiveOperationCallbacks &current();
    static void setCurrent(ArchiveOperationCallbacks callbacks);
    static void clearCurrent();
};
