#pragma once

#include <QObject>
#include <QRunnable>
#include <QString>
#include <atomic>
#include <memory>

class FileProvider;

class ProviderFolderSizeCalculator final : public QObject, public QRunnable {
    Q_OBJECT
public:
    explicit ProviderFolderSizeCalculator(const QString &path, int generation);
    ProviderFolderSizeCalculator(std::unique_ptr<FileProvider> provider, const QString &path, int generation);

    void run() override;
    void cancel();

signals:
    void progressUpdate(qint64 bytes, int files, int folders, bool exact, int generation);
    void resultReady(qint64 bytes,
                     int files,
                     int folders,
                     bool exact,
                     bool cancelled,
                     const QString &error,
                     int generation);

private:
    QString m_path;
    int m_generation = 0;
    std::atomic<bool> m_cancelled = false;
    std::unique_ptr<FileProvider> m_provider;
};
