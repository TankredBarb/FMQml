#include "ArchiveCache.h"
#include <bit7z/bit7z.hpp>
#include <QFileInfo>
#include <QDebug>
#include <algorithm>

namespace {
    QString fromTString(const bit7z::tstring& str) {
#if defined(_WIN32) && defined(BIT7Z_USE_NATIVE_STRING)
        return QString::fromStdWString(str);
#else
        return QString::fromStdString(str);
#endif
    }

    bit7z::tstring toTString(const QString& str) {
#if defined(_WIN32) && defined(BIT7Z_USE_NATIVE_STRING)
        return str.toStdWString();
#else
        return str.toStdString();
#endif
    }
    
    static std::unique_ptr<bit7z::Bit7zLibrary> s_lib;
    static std::mutex s_libMutex;
    
    bit7z::Bit7zLibrary& getLib() {
        std::lock_guard<std::mutex> lock(s_libMutex);
        if (!s_lib) {
            try {
#ifdef Q_OS_WIN
                s_lib = std::make_unique<bit7z::Bit7zLibrary>(BIT7Z_STRING("7zip.dll"));
#else
                s_lib = std::make_unique<bit7z::Bit7zLibrary>(BIT7Z_STRING("7z.so"));
#endif
            } catch (const bit7z::BitException& ex) {
                qWarning() << "Failed to initialize bit7z library in cache:" << ex.what();
                throw;
            }
        }
        return *s_lib;
    }
}

ArchiveCache& ArchiveCache::instance() {
    static ArchiveCache inst;
    return inst;
}

std::shared_ptr<bit7z::BitArchiveReader> ArchiveCache::getArchive(const QString& diskPath, ArchiveNode** outRoot) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    QFileInfo fi(diskPath);
    if (!fi.exists()) return nullptr;
    
    QDateTime mtime = fi.lastModified();
    std::wstring wPath = diskPath.toStdWString();
    
    auto it = m_cache.find(wPath);
    if (it != m_cache.end()) {
        if (it->second->lastModified == mtime) {
            it->second->lastAccess = QDateTime::currentDateTime();
            if (outRoot) *outRoot = it->second->root.get();
            return it->second->reader;
        }
        // File changed, remove old entry
        m_cache.erase(it);
    }
    
    // Load new archive
    try {
        auto reader = std::make_shared<bit7z::BitArchiveReader>(getLib(), toTString(diskPath), bit7z::BitFormat::Auto);
        
        auto root = std::make_unique<ArchiveNode>();
        root->name = QStringLiteral("/");
        root->path = QStringLiteral("/");
        root->isDirectory = true;
        
        const auto items = reader->items();
        for (const auto& item : items) {
            QString itemPathStr = fromTString(item.path());
            itemPathStr = itemPathStr.replace(QLatin1Char('\\'), QLatin1Char('/'));
            if (!itemPathStr.startsWith(QLatin1Char('/'))) {
                itemPathStr.prepend(QLatin1Char('/'));
            }

            QStringList parts = itemPathStr.split(QLatin1Char('/'), Qt::SkipEmptyParts);
            ArchiveNode* current = root.get();
            QString buildPath = QStringLiteral("");

            for (int i = 0; i < parts.size(); ++i) {
                const QString &part = parts.at(i);
                buildPath += QLatin1Char('/') + part;

                auto itChild = current->children.find(part);
                if (itChild == current->children.end()) {
                    auto node = std::make_unique<ArchiveNode>();
                    node->name = part;
                    node->path = buildPath;
                    node->parent = current;
                    
                    if (i == parts.size() - 1) {
                        node->isDirectory = item.isDir();
                        node->size = item.size();
                        node->itemIndex = item.index();
                    } else {
                        node->isDirectory = true;
                    }
                    ArchiveNode* next = node.get();
                    current->children[part] = std::move(node);
                    current = next;
                } else {
                    current = itChild->second.get();
                }
            }
        }
        
        auto cached = std::make_unique<CachedArchive>();
        cached->diskPath = diskPath;
        cached->lastModified = mtime;
        cached->root = std::move(root);
        cached->reader = reader;
        cached->lastAccess = QDateTime::currentDateTime();
        
        if (outRoot) *outRoot = cached->root.get();
        auto sharedReader = cached->reader;
        
        m_cache[wPath] = std::move(cached);
        cleanup();
        
        return sharedReader;
        
    } catch (const bit7z::BitException& ex) {
        qWarning() << "ArchiveCache: Failed to open archive:" << diskPath << "Error:" << ex.what();
        return nullptr;
    }
}

void ArchiveCache::cleanup() {
    if (m_cache.size() <= MaxCacheSize) return;
    
    std::vector<std::wstring> keys;
    for (const auto& pair : m_cache) {
        keys.push_back(pair.first);
    }
    
    std::sort(keys.begin(), keys.end(), [this](const std::wstring& a, const std::wstring& b) {
        return m_cache[a]->lastAccess < m_cache[b]->lastAccess;
    });
    
    while (m_cache.size() > MaxCacheSize) {
        m_cache.erase(keys.front());
        keys.erase(keys.begin());
    }
}
