#include "ArchiveOperationCallbacks.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace {
thread_local ArchiveOperationCallbacks g_currentCallbacks;
}

bool ArchiveOperationCallbacks::isAbortRequested() const
{
    return abortRequested && abortRequested();
}

void ArchiveOperationCallbacks::reportProgress(uint64_t processedBytes) const
{
    if (!progressBytes) {
        return;
    }
    const uint64_t maximum = static_cast<uint64_t>((std::numeric_limits<qint64>::max)());
    progressBytes(static_cast<qint64>((std::min)(processedBytes, maximum)));
}

const ArchiveOperationCallbacks &ArchiveOperationCallbacks::current()
{
    return g_currentCallbacks;
}

void ArchiveOperationCallbacks::setCurrent(ArchiveOperationCallbacks callbacks)
{
    g_currentCallbacks = std::move(callbacks);
}

void ArchiveOperationCallbacks::clearCurrent()
{
    g_currentCallbacks = {};
}
