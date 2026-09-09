#pragma once

#include <QNetworkReply>
#include <QTimer>
#include <functional>

// Poll independently of network progress so a superseded preview can abort
// while the server is silent. Construct and destroy on the reply's thread.
class PreviewDownloadCancellation {
public:
    PreviewDownloadCancellation(QNetworkReply *reply,
                                const std::function<bool(qint64, qint64)> &progress,
                                const std::function<void()> &cancel)
    {
        QObject::connect(reply, &QNetworkReply::downloadProgress, &m_timer,
                         [this](qint64 received, qint64 total) {
            m_received = received;
            m_total = total;
        });
        if (progress) {
            QObject::connect(&m_timer, &QTimer::timeout, &m_timer, [this, progress, cancel]() {
                if (!progress(m_received, m_total)) {
                    m_timer.stop();
                    cancel();
                }
            });
            m_timer.start(100);
        }
    }
    void stop() { m_timer.stop(); }

private:
    QTimer m_timer;
    qint64 m_received = 0;
    qint64 m_total = 0;
};
