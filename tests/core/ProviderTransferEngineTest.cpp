#include "OperationQueuePrivate.h"

#include <QCoreApplication>

#include <iostream>

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

    OperationQueue::Request request;
    request.type = OperationQueue::Type::Copy;
    request.sources = {QStringLiteral("source")};
    request.destination = QStringLiteral("destination");

    bool aborted = false;
    OperationQueuePrivate::ExecutionContext context{
        request,
        [&]() { return aborted; },
        [](double) {},
        [](int) {},
        [](const QString &) {},
        [](const QString &) {},
        [](const QString &, const QString &) {
            return OperationQueue::ConflictResolution::Skip;
        }
    };

    QStringList calls;
    const auto transfer = [&](const QString &name, bool handled) {
        return [&, name, handled](const QStringList &, const QString &, qint64, qint64 &) {
            calls.append(name);
            return handled;
        };
    };
    const auto incremental = [&](const QStringList &, int index, const QString &, qint64, qint64 &) {
        calls.append(QStringLiteral("incremental:%1").arg(index));
        return 3;
    };

    OperationQueuePrivate::ProviderTransferEngine engine{
        context,
        transfer(QStringLiteral("upload"), false),
        transfer(QStringLiteral("download"), true),
        transfer(QStringLiteral("staged"), true),
        incremental
    };

    qint64 copiedBytes = 0;
    auto result = engine.copyWholeRequest(100, copiedBytes);
    ok &= result == OperationQueuePrivate::ProviderTransferEngine::BatchResult::Succeeded
        || fail("handled batch did not succeed");
    ok &= calls == QStringList({QStringLiteral("upload"), QStringLiteral("download")})
        || fail("batch route order or short-circuit changed");

    calls.clear();
    aborted = true;
    result = engine.copyWholeRequest(100, copiedBytes);
    ok &= result == OperationQueuePrivate::ProviderTransferEngine::BatchResult::Aborted
        || fail("handled aborted batch did not report abort");

    calls.clear();
    aborted = false;
    request.explicitDestinations = {QStringLiteral("exact")};
    result = engine.copyWholeRequest(100, copiedBytes);
    ok &= result == OperationQueuePrivate::ProviderTransferEngine::BatchResult::NotHandled
        || fail("explicit destination entered whole-request batch");
    ok &= calls.isEmpty() || fail("explicit destination invoked a batch route");

    request.explicitDestinations.clear();
    const int batchCount = engine.copyNextUploadBatch(4, 100, copiedBytes);
    ok &= batchCount == 3 || fail("incremental batch count changed");
    ok &= calls == QStringList({QStringLiteral("incremental:4")})
        || fail("incremental batch route was not invoked");

    request.type = OperationQueue::Type::Move;
    calls.clear();
    ok &= engine.copyNextUploadBatch(0, 100, copiedBytes) == 0
        || fail("non-copy request entered incremental batch");
    ok &= calls.isEmpty() || fail("non-copy request invoked incremental route");

    return ok ? 0 : 1;
}
