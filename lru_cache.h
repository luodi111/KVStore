#pragma once

#include <string>
#include <list>
#include <unordered_map>
#include <mutex>

class LRUCache {
public:
    explicit LRUCache(int capacity);

    void put(const std::string& key, const std::string& value);
    std::string get(const std::string& key);
    void remove(const std::string& key);
    bool contains(const std::string& key);
    size_t size() const;

    int hit_count() const;
    int miss_count() const;
    void reset_stats();

private:
    int capacity_;
    mutable std::mutex mutex_;
    std::list<std::pair<std::string, std::string>> items_;
    using ListIter = std::list<std::pair<std::string, std::string>>::iterator;
    std::unordered_map<std::string, ListIter> index_;
    int hits_ = 0;
    int misses_ = 0;
};
