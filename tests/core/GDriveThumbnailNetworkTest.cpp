#include "GDriveThumbnailLoader.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QFutureWatcher>
#include <QNetworkProxy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTextStream>
#include <QTimer>
#include <QtConcurrent>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
        QTextStream(stderr) << "FAILED: loopback server: " << server.errorString() << '\n';
        return 1;
    }
    int connections = 0;
    int requests = 0;
    QObject::connect(&server, &QTcpServer::newConnection, &app, [&]() {
        while (auto *socket = server.nextPendingConnection()) {
            ++connections;
            auto buffer = std::make_shared<QByteArray>();
            QObject::connect(socket, &QTcpSocket::readyRead, socket, [&, socket, buffer]() {
                buffer->append(socket->readAll());
                if (!buffer->contains("\r\n\r\n")) return;
                const bool oversized = buffer->startsWith("GET /oversized ");
                buffer->clear();
                ++requests;
                if (oversized) {
                    socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2097153\r\nConnection: keep-alive\r\n\r\n");
                } else {
                    socket->write("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: keep-alive\r\n\r\nok");
                }
            });
            QObject::connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
        }
    });
    const QUrl url(QStringLiteral("http://127.0.0.1:%1/thumbnail").arg(server.serverPort()));
    QFutureWatcher<bool> watcher;
    QEventLoop loop;
    QObject::connect(&watcher, &QFutureWatcher<bool>::finished, &loop, &QEventLoop::quit);
    watcher.setFuture(QtConcurrent::run([url]() {
        for (int i = 0; i < 8; ++i) {
            const auto result = GDriveThumbnailLoader::downloadBytes(url, QStringLiteral("test-token"));
            if (result.body != "ok" || result.httpStatus != 200 || result.timedOut) return false;
        }
        QUrl oversizedUrl = url;
        oversizedUrl.setPath(QStringLiteral("/oversized"));
        const auto oversized = GDriveThumbnailLoader::downloadBytes(
            oversizedUrl, QStringLiteral("test-token"));
        if (!oversized.oversize || !oversized.body.isEmpty()) return false;
        return true;
    }));
    loop.exec();
    const bool ok = watcher.result() && requests == 9 && connections == 4;
    QTextStream(stdout) << requests << " requests, " << connections << " TCP connections\n";
    return ok ? 0 : 1;
}
