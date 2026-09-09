#include "PreviewDownloadCancellation.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QTextStream>

class SilentReply : public QNetworkReply {
public:
    bool aborted = false;
    void abort() override { aborted = true; emit finished(); }
    void report(qint64 bytes, qint64 total) { emit downloadProgress(bytes, total); }
protected:
    qint64 readData(char *, qint64) override { return -1; }
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    // Both a silent server before its first byte and a transfer stalled midway
    // must notice cancellation without another network progress event.
    for (qint64 received : {0, 128}) {
        SilentReply reply;
        bool cancelled = false;
        qint64 observed = -1;
        PreviewDownloadCancellation poll(&reply, [&](qint64 bytes, qint64) {
            observed = bytes;
            return !cancelled;
        }, [&]() { reply.abort(); });
        if (received) reply.report(received, 1024);
        QEventLoop loop;
        QObject::connect(&reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        QTimer::singleShot(20, &loop, [&]() { cancelled = true; });
        QTimer::singleShot(2000, &loop, &QEventLoop::quit);
        QElapsedTimer timer;
        timer.start();
        loop.exec();
        if (!reply.aborted || observed != received || timer.elapsed() >= 1500) {
            QTextStream(stderr) << "FAILED: silent preview did not cancel promptly\n";
            return 1;
        }
    }
    return 0;
}
