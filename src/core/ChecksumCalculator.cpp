#include "ChecksumCalculator.h"
#include <QtConcurrent>
#include <QFile>
#include <QDebug>

ChecksumCalculator::ChecksumCalculator(QObject *parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFutureWatcher<HashResults>::finished, this, [this]() {
        HashResults res = m_watcher.result();
        setBusy(false);
        if (res.success) {
            setResults(res.md5, res.sha1, res.sha256);
            emit finished();
        } else if (!res.error.isEmpty()) {
            setError(res.error);
        }
    });
}

ChecksumCalculator::~ChecksumCalculator()
{
    abort();
}

void ChecksumCalculator::calculate(const QString &path)
{
    if (m_busy) {
        return;
    }

    setBusy(true);
    setProgress(0);
    setError("");
    setResults("", "", "");
    m_abortFlag = false;

    auto progressCallback = [this](double p) {
        QMetaObject::invokeMethod(this, "setProgress", Qt::QueuedConnection, Q_ARG(double, p));
    };

    m_watcher.setFuture(QtConcurrent::run(&ChecksumCalculator::runCalculation, path, &m_abortFlag, progressCallback));
}

void ChecksumCalculator::abort()
{
    m_abortFlag = true;
}

void ChecksumCalculator::setBusy(bool value)
{
    if (m_busy != value) {
        m_busy = value;
        emit busyChanged();
    }
}

void ChecksumCalculator::setProgress(double value)
{
    if (!qFuzzyCompare(m_progress, value)) {
        m_progress = value;
        emit progressChanged(m_progress);
    }
}

void ChecksumCalculator::setResults(const QString &md5, const QString &sha1, const QString &sha256)
{
    m_md5 = md5;
    m_sha1 = sha1;
    m_sha256 = sha256;
    emit resultsChanged();
}

void ChecksumCalculator::setError(const QString &msg)
{
    m_error = msg;
    if (!msg.isEmpty()) {
        emit errorOccurred(msg);
    }
}

ChecksumCalculator::HashResults ChecksumCalculator::runCalculation(const QString &path, 
                                                                 std::atomic<bool> *abortFlag,
                                                                 std::function<void(double)> progressCallback)
{
    HashResults results;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        results.error = QString("Could not open file: %1").arg(file.errorString());
        return results;
    }

    qint64 totalSize = file.size();
    if (totalSize == 0) {
        results.md5 = "d41d8cd98f00b204e9800998ecf8427e";
        results.sha1 = "da39a3ee5e6b4b0d3255bfef95601890afd80709";
        results.sha256 = "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
        results.success = true;
        return results;
    }

    QCryptographicHash md5(QCryptographicHash::Md5);
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    QCryptographicHash sha256(QCryptographicHash::Sha256);

    const qint64 chunkSize = 1024 * 1024; // 1MB
    qint64 processed = 0;

    while (!file.atEnd()) {
        if (abortFlag && *abortFlag) {
            results.success = false;
            return results;
        }

        QByteArray data = file.read(chunkSize);
        if (data.isEmpty()) break;

        md5.addData(data);
        sha1.addData(data);
        sha256.addData(data);

        processed += data.size();
        if (progressCallback) {
            progressCallback(static_cast<double>(processed) / totalSize);
        }
    }

    results.md5 = QString::fromLatin1(md5.result().toHex());
    results.sha1 = QString::fromLatin1(sha1.result().toHex());
    results.sha256 = QString::fromLatin1(sha256.result().toHex());
    results.success = true;

    return results;
}
