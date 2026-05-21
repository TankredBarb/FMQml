#pragma once

#include <QString>
#include <QDateTime>
#include <map>
#include <memory>
#include <vector>
#include <mutex>
#include <unordered_map>

namespace bit7z {
    class BitArchiveReader;
    class Bit7zLibrary;
}

struct ArchiveNode {
    QString name;
    QString path;
    bool isDirectory = false;
    qint64 size = 0;
    QDateTime modified;
    QDateTime created;
    std::map<QString, std::unique_ptr<ArchiveNode>> children;
    ArchiveNode* parent = nullptr;
    uint32_t itemIndex = static_cast<uint32_t>(-1);
};

struct CachedArchive {
    QString diskPath;
    QDateTime lastModified;
    std::unique_ptr<ArchiveNode> root;
    std::shared_ptr<bit7z::BitArchiveReader> reader;
    QDateTime lastAccess;
};

class ArchiveCache {
public:
    static ArchiveCache& instance();

    // Returns a shared pointer to the reader to keep it alive while in use.
    // The root node is returned as a raw pointer (owned by the cache).
    std::shared_ptr<bit7z::BitArchiveReader> getArchive(const QString& diskPath, ArchiveNode** outRoot);

private:
    ArchiveCache() = default;
    ~ArchiveCache() = default;

    std::mutex m_mutex;
    std::unordered_map<std::wstring, std::unique_ptr<CachedArchive>> m_cache;
    static constexpr int MaxCacheSize = 5;

    void cleanup();
};
