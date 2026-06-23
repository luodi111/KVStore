#include "lru_cache.h"

LRUCache::LRUCache(int capacity) : capacity_(capacity) {}

void LRUCache::put(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_.find(key);
    if (it != index_.end()) {
        items_.erase(it->second);
    } else if (items_.size() >= static_cast<size_t>(capacity_)) {
        index_.erase(items_.back().first);
        items_.pop_back();
    }
    items_.push_front({key, value});
    index_[key] = items_.begin();
}

std::string LRUCache::get(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_.find(key);
    if (it == index_.end()) {
        misses_++;
        return "";
    }
    hits_++;
    items_.splice(items_.begin(), items_, it->second);
    return it->second->second;
}

void LRUCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = index_.find(key);
    if (it != index_.end()) {
        items_.erase(it->second);
        index_.erase(key);
    }
}

bool LRUCache::contains(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    return index_.find(key) != index_.end();
}

size_t LRUCache::size() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return items_.size();
}

int LRUCache::hit_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hits_;
}

int LRUCache::miss_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return misses_;
}

void LRUCache::reset_stats() {
    std::lock_guard<std::mutex> lock(mutex_);
    hits_ = 0;
    misses_ = 0;
}
