#ifndef THREAD_SAFE_LRU_CACHE_H
#define THREAD_SAFE_LRU_CACHE_H

#include <mutex>
#include "lru_cache.h"

// Thread-safe wrapper around LRUCache.
//
// Why a single mutex around the whole cache instead of fine-grained
// locking per node? Because moveToFront/evictLRU mutate the shared
// linked-list pointers (head_/tail_) on every single get() -- even
// reads mutate state in an LRU cache. That makes per-node locking far
// more complex (you'd need to lock multiple adjacent nodes to safely
// unlink one) for a benefit that only shows up under very high
// contention. A single coarse-grained mutex is the correct, defensible
// tradeoff for a cache server at this scale -- and it's what most
// production LRU implementations (e.g. Go's groupcache) do too.
class ThreadSafeLRUCache {
public:
    explicit ThreadSafeLRUCache(size_t capacity) : cache_(capacity) {}

    bool get(const std::string& key, std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.get(key, value);
    }

    void put(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        cache_.put(key, value);
    }

    bool remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.remove(key);
    }

    size_t size() {
        std::lock_guard<std::mutex> lock(mutex_);
        return cache_.size();
    }

private:
    LRUCache cache_;
    std::mutex mutex_;
};

#endif // THREAD_SAFE_LRU_CACHE_H
