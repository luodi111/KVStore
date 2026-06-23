#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>
#include <functional>
#include "lru_cache.h"
#include "config.h"

struct KeyEntry {
    std::string value;
    std::chrono::steady_clock::time_point expiry;
    bool has_ttl = false;
};

class KVStore {
public:
    explicit KVStore(const ServerConfig& cfg);
    ~KVStore();

    size_t load_from_disk();
    void save_to_disk();

    std::string set(const std::string& key, const std::string& value);
    std::string setex(const std::string& key, const std::string& value, int ttl_seconds);
    std::string get(const std::string& key);
    std::string del(const std::string& key);
    std::vector<std::string> keys() const;
    bool exists(const std::string& key);
    std::string stats() const;
    std::string execute(const std::string& command);

    void set_on_save(std::function<void()> callback);

private:
    void remove_expired();

    ServerConfig config_;
    std::unordered_map<std::string, KeyEntry> data_;
    LRUCache lru_;
    mutable std::mutex data_mutex_;
    uint64_t cmd_count_ = 0;
    std::function<void()> on_save_;
};
