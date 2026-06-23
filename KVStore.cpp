#include "kvstore.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>

KVStore::KVStore(const ServerConfig& cfg)
    : config_(cfg), lru_(cfg.cache_capacity) {}

KVStore::~KVStore() {
    if (config_.enable_persistence) {
        save_to_disk();
    }
}

size_t KVStore::load_from_disk() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::ifstream file(config_.data_file);
    if (!file.is_open()) return 0;

    size_t count = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string key, value;
        if (!(iss >> key)) continue;
        std::getline(iss, value);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);
        if (key.size() > config_.max_key_length) continue;
        if (value.size() > config_.max_value_length) continue;

        data_[key] = {value, std::chrono::steady_clock::time_point{}, false};
        lru_.put(key, value);
        count++;
    }
    std::cout << "已从 " << config_.data_file << " 加载 " << count << " 条记录" << std::endl;
    return count;
}

void KVStore::save_to_disk() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::ofstream file(config_.data_file, std::ios::trunc);
    for (const auto& pair : data_) {
        file << pair.first << " " << pair.second.value << std::endl;
    }
    file.close();
    if (on_save_) on_save_();
}

std::string KVStore::set(const std::string& key, const std::string& value) {
    if (key.empty() || key.size() > config_.max_key_length) return "ERR 键太长";
    if (value.size() > config_.max_value_length) return "ERR 值太长";

    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        data_[key] = {value, std::chrono::steady_clock::time_point{}, false};
    }
    lru_.put(key, value);
    if (config_.enable_persistence) save_to_disk();
    cmd_count_++;
    return "OK";
}

std::string KVStore::setex(const std::string& key, const std::string& value, int ttl_seconds) {
    if (key.empty() || key.size() > config_.max_key_length) return "ERR 键太长";
    if (value.size() > config_.max_value_length) return "ERR 值太长";
    if (ttl_seconds <= 0) return "ERR TTL无效";

    auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds);
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        data_[key] = {value, expiry, true};
    }
    lru_.put(key, value);
    if (config_.enable_persistence) save_to_disk();
    cmd_count_++;
    return "OK";
}

std::string KVStore::get(const std::string& key) {
    remove_expired();

    std::string cached = lru_.get(key);
    if (!cached.empty()) {
        cmd_count_++;
        return cached;
    }

    std::lock_guard<std::mutex> lock(data_mutex_);
    auto it = data_.find(key);
    if (it == data_.end()) {
        cmd_count_++;
        return "(nil)";
    }
    if (it->second.has_ttl && std::chrono::steady_clock::now() > it->second.expiry) {
        data_.erase(it);
        lru_.remove(key);
        cmd_count_++;
        return "(nil)";
    }
    lru_.put(key, it->second.value);
    cmd_count_++;
    return it->second.value;
}

std::string KVStore::del(const std::string& key) {
    {
        std::lock_guard<std::mutex> lock(data_mutex_);
        if (data_.erase(key) == 0) {
            cmd_count_++;
            return "(nil)";
        }
    }
    lru_.remove(key);
    if (config_.enable_persistence) save_to_disk();
    cmd_count_++;
    return "OK";
}

std::vector<std::string> KVStore::keys() const {
    std::lock_guard<std::mutex> lock(data_mutex_);
    std::vector<std::string> result;
    result.reserve(data_.size());
    for (const auto& pair : data_) {
        result.push_back(pair.first);
    }
    std::sort(result.begin(), result.end());
    return result;
}

bool KVStore::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(data_mutex_);
    return data_.find(key) != data_.end();
}

std::string KVStore::stats() const {
    int total = lru_.hit_count() + lru_.miss_count();
    double rate = total > 0 ? (double)lru_.hit_count() / total * 100.0 : 0.0;

    std::lock_guard<std::mutex> lock(data_mutex_);
    std::ostringstream ss;
    ss << "--- KVStore 统计 ---" << std::endl;
    ss << "键总数:        " << data_.size() << std::endl;
    ss << "命令数:        " << cmd_count_ << std::endl;
    ss << "缓存命中:      " << lru_.hit_count() << std::endl;
    ss << "缓存未命中:    " << lru_.miss_count() << std::endl;
    ss << "命中率:        " << static_cast<int>(rate) << "%" << std::endl;
    ss << "缓存大小:      " << lru_.size() << std::endl;
    ss << "缓存容量:      " << config_.cache_capacity << std::endl;
    ss << "线程池:        " << config_.thread_pool_size << std::endl;
    ss << "持久化:        " << (config_.enable_persistence ? "开启" : "关闭") << std::endl;
    ss << "数据文件:      " << config_.data_file << std::endl;
    return ss.str();
}

void KVStore::set_on_save(std::function<void()> callback) {
    on_save_ = std::move(callback);
}

void KVStore::remove_expired() {
    std::lock_guard<std::mutex> lock(data_mutex_);
    auto now = std::chrono::steady_clock::now();
    for (auto it = data_.begin(); it != data_.end(); ) {
        if (it->second.has_ttl && now > it->second.expiry) {
            lru_.remove(it->first);
            it = data_.erase(it);
        } else {
            ++it;
        }
    }
}

std::string KVStore::execute(const std::string& command) {
    std::istringstream iss(command);
    std::string cmd;
    iss >> cmd;

    if (cmd == "SET" || cmd == "set") {
        std::string key, value;
        iss >> key;
        std::getline(iss, value);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);
        if (key.empty() || value.empty()) return "ERR 用法: SET <key> <value>";
        return set(key, value);
    }
    else if (cmd == "SETEX" || cmd == "setex") {
        std::string key, value;
        int ttl;
        iss >> key >> ttl;
        std::getline(iss, value);
        if (!value.empty() && value[0] == ' ') value.erase(0, 1);
        if (key.empty() || value.empty()) return "ERR 用法: SETEX <key> <ttl_sec> <value>";
        return setex(key, value, ttl);
    }
    else if (cmd == "GET" || cmd == "get") {
        std::string key;
        iss >> key;
        if (key.empty()) return "ERR 用法: GET <key>";
        return get(key);
    }
    else if (cmd == "DEL" || cmd == "del") {
        std::string key;
        iss >> key;
        if (key.empty()) return "ERR 用法: DEL <key>";
        return del(key);
    }
    else if (cmd == "KEYS" || cmd == "keys") {
        auto k = keys();
        if (k.empty()) return "(空)";
        std::string result;
        for (const auto& s : k) result += s + " ";
        result.pop_back();
        return result;
    }
    else if (cmd == "EXISTS" || cmd == "exists") {
        std::string key;
        iss >> key;
        if (key.empty()) return "ERR 用法: EXISTS <key>";
        return exists(key) ? "1" : "0";
    }
    else if (cmd == "STATS" || cmd == "stats") {
        return stats();
    }
    else if (cmd == "PING" || cmd == "ping") {
        return "PONG";
    }
    else {
        return "ERR 未知命令: " + cmd;
    }
}