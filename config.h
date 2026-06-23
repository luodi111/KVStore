#pragma once

#include <string>
#include <cstdint>

struct ServerConfig {
    uint16_t port = 8888;
    int cache_capacity = 10;
    int thread_pool_size = 4;
    std::string data_file = "kvs_data.txt";
    std::string log_file = "kvs.log";
    size_t max_key_length = 256;
    size_t max_value_length = 4096;
    bool enable_persistence = true;
    bool enable_logging = false;

    static ServerConfig load(const std::string& filename = "kvs.conf");
    void save(const std::string& filename = "kvs.conf") const;
    void print() const;
};
