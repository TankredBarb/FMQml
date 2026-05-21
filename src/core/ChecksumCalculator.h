#pragma once

#include <QObject>
#include <QString>
#include <QtQml>
#include <QCryptographicHash>
#include <QFutureWatcher>

class ChecksumCalculator : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by controllers")

    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString md5 READ md5 NOTIFY resultsChanged)
    Q_PROPERTY(QString sha1 READ sha1 NOTIFY resultsChanged)
    Q_PROPERTY(QString sha256 READ sha256 NOTIFY resultsChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorOccurred)

public:
    explicit ChecksumCalculator(QObject *parent = nullptr);
    ~ChecksumCalculator() override;

    double progress() const { return m_progress; }
    bool busy() const { return m_busy; }
    QString md5() const { return m_md5; }
    QString sha1() const { return m_sha1; }
    QString sha256() const { return m_sha256; }
    QString error() const { return m_error; }

    Q_INVOKABLE void calculate(const QString &path);
    Q_INVOKABLE void abort();

signals:
    void progressChanged(double percent);
    void busyChanged();
    void resultsChanged();
    void errorOccurred(const QString &errorMsg);
    void finished();

public slots:
    void setProgress(double value);

private:
    void setBusy(bool value);
    void setResults(const QString &md5, const QString &sha1, const QString &sha256);
    void setError(const QString &msg);

    struct HashResults {
        QString md5;
        QString sha1;
        QString sha256;
        QString error;
        bool success = false;
    };

    static HashResults runCalculation(const QString &path, std::atomic<bool> *abortFlag, 
                                    std::function<void(double)> progressCallback);

    double m_progress = 0;
    bool m_busy = false;
    QString m_md5;
    QString m_sha1;
    QString m_sha256;
    QString m_error;

    std::atomic<bool> m_abortFlag{false};
    QFutureWatcher<HashResults> m_watcher;
};
