#include "ChecksumCalculator.h"
#include <QtConcurrent>
#include <QFile>
#include <QDebug>
#include <QElapsedTimer>

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

        if (!m_pendingPath.isEmpty()) {
            QString nextPath = m_pendingPath;
            QString nextAlgo = m_pendingAlgorithm;
            m_pendingPath.clear();
            m_pendingAlgorithm.clear();
            calculate(nextPath, nextAlgo);
        }
    });
}

ChecksumCalculator::~ChecksumCalculator()
{
    abort();
}

void ChecksumCalculator::calculate(const QString &path, const QString &algorithm)
{
    if (m_busy) {
        m_abortFlag = true;
        m_pendingPath = path;
        m_pendingAlgorithm = algorithm;
        return;
    }

    if (m_currentPath != path) {
        m_currentPath = path;
        clear();
    }

    setBusy(true);
    setProgress(0);
    setError("");
    m_abortFlag = false;

    auto progressCallback = [this](double p) {
        QMetaObject::invokeMethod(this, "setProgress", Qt::QueuedConnection, Q_ARG(double, p));
    };

    m_watcher.setFuture(QtConcurrent::run(&ChecksumCalculator::runCalculation, path, algorithm, &m_abortFlag, progressCallback));
}

void ChecksumCalculator::abort()
{
    m_abortFlag = true;
    m_pendingPath.clear();
    m_pendingAlgorithm.clear();
}

void ChecksumCalculator::clear()
{
    m_md5 = QString();
    m_sha1 = QString();
    m_sha256 = QString();
    m_error = QString();
    m_progress = 0;
    emit resultsChanged();
    emit progressChanged(0);
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
    bool changed = false;
    if (!md5.isEmpty() && m_md5 != md5) {
        m_md5 = md5;
        changed = true;
    }
    if (!sha1.isEmpty() && m_sha1 != sha1) {
        m_sha1 = sha1;
        changed = true;
    }
    if (!sha256.isEmpty() && m_sha256 != sha256) {
        m_sha256 = sha256;
        changed = true;
    }
    if (changed) {
        emit resultsChanged();
    }
}

void ChecksumCalculator::setError(const QString &msg)
{
    m_error = msg;
    if (!msg.isEmpty()) {
        emit errorOccurred(msg);
    }
}

ChecksumCalculator::HashResults ChecksumCalculator::runCalculation(const QString &path, 
                                                                 const QString &algorithm,
                                                                 std::atomic<bool> *abortFlag,
                                                                 std::function<void(double)> progressCallback)
{
    HashResults results;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        results.error = QString("Could not open file: %1").arg(file.errorString());
        return results;
    }

    QString algoLower = algorithm.toLower();
    bool calcMd5 = (algoLower == QLatin1String("all") || algoLower == QLatin1String("md5"));
    bool calcSha1 = (algoLower == QLatin1String("all") || algoLower == QLatin1String("sha1"));
    bool calcSha256 = (algoLower == QLatin1String("all") || algoLower == QLatin1String("sha256"));

    qint64 totalSize = file.size();
    if (totalSize == 0) {
        if (calcMd5) results.md5 = QStringLiteral("d41d8cd98f00b204e9800998ecf8427e");
        if (calcSha1) results.sha1 = QStringLiteral("da39a3ee5e6b4b0d3255bfef95601890afd80709");
        if (calcSha256) results.sha256 = QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
        if (progressCallback) {
            progressCallback(1.0);
        }
        results.success = true;
        return results;
    }

    QCryptographicHash md5(QCryptographicHash::Md5);
    QCryptographicHash sha1(QCryptographicHash::Sha1);
    QCryptographicHash sha256(QCryptographicHash::Sha256);

    const qint64 chunkSize = 1024 * 1024; // 1MB
    qint64 processed = 0;
    double lastProgress = 0.0;
    QElapsedTimer progressTimer;
    progressTimer.start();

    while (!file.atEnd()) {
        if (abortFlag && *abortFlag) {
            results.success = false;
            return results;
        }

        QByteArray data = file.read(chunkSize);
        if (data.isEmpty()) break;

        if (calcMd5) md5.addData(data);
        if (calcSha1) sha1.addData(data);
        if (calcSha256) sha256.addData(data);

        processed += data.size();
        if (progressCallback) {
            const double progress = static_cast<double>(processed) / totalSize;
            if (progress >= 1.0 || progress - lastProgress >= 0.01 || progressTimer.elapsed() >= 50) {
                progressCallback(progress);
                lastProgress = progress;
                progressTimer.restart();
            }
        }
    }

    if (progressCallback && lastProgress < 1.0) {
        progressCallback(1.0);
    }

    if (calcMd5) results.md5 = QString::fromLatin1(md5.result().toHex());
    if (calcSha1) results.sha1 = QString::fromLatin1(sha1.result().toHex());
    if (calcSha256) results.sha256 = QString::fromLatin1(sha256.result().toHex());
    results.success = true;

    return results;
}
