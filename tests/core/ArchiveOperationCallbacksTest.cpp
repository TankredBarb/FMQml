#include "ArchiveOperationCallbacks.h"

#include <QCoreApplication>

#include <atomic>
#include <iostream>
#include <limits>
#include <thread>

namespace {
bool fail(const char *message)
{
    std::cerr << message << '\n';
    return false;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    bool ok = true;

    ok &= !ArchiveOperationCallbacks::current().isAbortRequested()
        || fail("default callbacks report abort");

    bool aborted = false;
    qint64 reportedBytes = -1;
    ArchiveOperationCallbacks::setCurrent({
        [&]() { return aborted; },
        [&](qint64 bytes) { reportedBytes = bytes; }
    });

    ArchiveOperationCallbacks::current().reportProgress(42);
    ok &= reportedBytes == 42 || fail("progress callback did not receive bytes");
    aborted = true;
    ok &= ArchiveOperationCallbacks::current().isAbortRequested()
        || fail("abort callback was not observed");

    ArchiveOperationCallbacks::current().reportProgress(
        (std::numeric_limits<uint64_t>::max)());
    ok &= reportedBytes == (std::numeric_limits<qint64>::max)()
        || fail("progress callback did not clamp uint64 bytes");

    std::atomic_bool otherThreadSawCallbacks = false;
    std::thread otherThread([&]() {
        otherThreadSawCallbacks = ArchiveOperationCallbacks::current().isAbortRequested();
        ArchiveOperationCallbacks::current().reportProgress(7);
    });
    otherThread.join();
    ok &= !otherThreadSawCallbacks.load() || fail("callbacks leaked into another thread");
    ok &= reportedBytes == (std::numeric_limits<qint64>::max)()
        || fail("other thread invoked this thread's progress callback");

    ArchiveOperationCallbacks::clearCurrent();
    ok &= !ArchiveOperationCallbacks::current().isAbortRequested()
        || fail("clearCurrent did not clear abort callback");

    return ok ? 0 : 1;
}
