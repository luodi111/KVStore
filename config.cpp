#include "config.h"
#include <fstream>
#include <iostream>
#include <sstream>

ServerConfig ServerConfig::load(const std::string& filename) {
    ServerConfig cfg;
    std::ifstream file(filename);
    if (!file.is_open()) return cfg;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string key, value;
        if (!(iss >> key >> value)) continue;

        if (key == "port") cfg.port = static_cast<uint16_t>(std::stoi(value));
        else if (key == "cache_capacity") cfg.cache_capacity = std::stoi(value);
        else if (key == "thread_pool_size") cfg.thread_pool_size = std::stoi(value);
        else if (key == "data_file") cfg.data_file = value;
        else if (key == "log_file") cfg.log_file = value;
        else if (key == "max_key_length") cfg.max_key_length = std::stoul(value);
        else if (key == "max_value_length") cfg.max_value_length = std::stoul(value);
        else if (key == "enable_persistence") cfg.enable_persistence = (value == "true" || value == "1");
        else if (key == "enable_logging") cfg.enable_logging = (value == "true" || value == "1");
    }
    return cfg;
}

void ServerConfig::save(const std::string& filename) const {
    std::ofstream file(filename);
    file << "port " << port << std::endl;
    file << "cache_capacity " << cache_capacity << std::endl;
    file << "thread_pool_size " << thread_pool_size << std::endl;
    file << "data_file " << data_file << std::endl;
    file << "log_file " << log_file << std::endl;
    file << "max_key_length " << max_key_length << std::endl;
    file << "max_value_length " << max_value_length << std::endl;
    file << "enable_persistence " << (enable_persistence ? "true" : "false") << std::endl;
    file << "enable_logging " << (enable_logging ? "true" : "false") << std::endl;
}

void ServerConfig::print() const {
    std::cout << "=== 服务器配置 ===" << std::endl;
    std::cout << "端口:              " << port << std::endl;
    std::cout << "缓存容量:          " << cache_capacity << std::endl;
    std::cout << "线程池大小:        " << thread_pool_size << std::endl;
    std::cout << "数据文件:          " << data_file << std::endl;
    std::cout << "日志文件:          " << log_file << std::endl;
    std::cout << "最大键长度:        " << max_key_length << std::endl;
    std::cout << "最大值长度:        " << max_value_length << std::endl;
    std::cout << "持久化:            " << (enable_persistence ? "是" : "否") << std::endl;
    std::cout << "日志:              " << (enable_logging ? "是" : "否") << std::endl;
}