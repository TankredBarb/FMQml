#include "ProviderFolderSizeCalculator.h"

#include "FileProviderFactory.h"

#include <QElapsedTimer>
#include <QStack>
#include <QStringList>

#include <memory>
#include <optional>

ProviderFolderSizeCalculator::ProviderFolderSizeCalculator(const QString &path, int generation)
    : m_path(path)
    , m_generation(generation)
{
    setAutoDelete(false);
}

ProviderFolderSizeCalculator::ProviderFolderSizeCalculator(std::unique_ptr<FileProvider> provider,
                                                           const QString &path,
                                                           int generation)
    : m_path(path)
    , m_generation(generation)
    , m_provider(std::move(provider))
{
    setAutoDelete(false);
}

void ProviderFolderSizeCalculator::cancel()
{
    m_cancelled = true;
}

void ProviderFolderSizeCalculator::run()
{
    std::unique_ptr<FileProvider> ownedProvider;
    if (!m_provider) {
        ownedProvider = FileProviderFactory::createProvider(m_path);
    }
    FileProvider *provider = m_provider ? m_provider.get() : ownedProvider.get();
    if (!provider || provider->scheme() == QLatin1String("file")) {
        emit resultReady(0, 0, 0, false, false, QStringLiteral("Provider unavailable."), m_generation);
        return;
    }

    const QString rootPath = provider->normalizedPath(m_path);
    const std::optional<FileEntry> rootEntry = provider->entryInfo(rootPath);
    if (!rootEntry) {
        const QString error = provider->lastErrorString().isEmpty()
            ? QStringLiteral("Root metadata is unavailable.")
            : provider->lastErrorString();
        emit resultReady(0, 0, 0, false, false, error, m_generation);
        return;
    }

    if (!rootEntry->isDirectory) {
        emit resultReady(rootEntry->size, 1, 0, true, false, {}, m_generation);
        return;
    }

    qint64 totalBytes = 0;
    int fileCount = 0;
    int folderCount = 0;
    bool exact = true;
    QString lastError;

    QElapsedTimer progressTimer;
    progressTimer.start();

    QStack<QString> stack;
    stack.push(rootPath);

    while (!stack.isEmpty()) {
        if (m_cancelled) {
            emit resultReady(totalBytes, fileCount, folderCount, false, true, QStringLiteral("Cancelled"), m_generation);
            return;
        }

        const QString currentPath = stack.pop();
        provider->clearLastError();
        const QStringList children = provider->childPaths(currentPath, true);
        const QString childListError = provider->lastErrorString();
        if (!childListError.isEmpty()) {
            exact = false;
            lastError = childListError;
        }

        for (const QString &childPath : children) {
            if (m_cancelled) {
                emit resultReady(totalBytes, fileCount, folderCount, false, true, QStringLiteral("Cancelled"), m_generation);
                return;
            }

            const std::optional<FileEntry> childEntry = provider->entryInfo(childPath);
            if (!childEntry) {
                exact = false;
                lastError = provider->lastErrorString().isEmpty()
                    ? QStringLiteral("Some item metadata is unavailable.")
                    : provider->lastErrorString();
                continue;
            }

            if (childEntry->isDirectory) {
                ++folderCount;
                stack.push(childPath);
            } else {
                ++fileCount;
                totalBytes += childEntry->size;
            }

            if (progressTimer.elapsed() >= 200) {
                emit progressUpdate(totalBytes, fileCount, folderCount, exact, m_generation);
                progressTimer.restart();
            }
        }
    }

    emit resultReady(totalBytes, fileCount, folderCount, exact, false, lastError, m_generation);
}
