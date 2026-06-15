#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <list>
#include <thread>
#include <mutex>
#include <chrono>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;
using namespace chrono;

// ========== 配置 ==========
struct Config {
    int port = 8888;
    int cache_capacity = 10;
    string data_file = "kvs_data.txt";
};

Config load_config(const string& filename = "config.txt") {
    Config cfg;
    ifstream file(filename);
    if (!file.is_open()) return cfg;
    string key, value;
    while (file >> key >> value) {
        if (key == "port") cfg.port = stoi(value);
        else if (key == "cache_capacity") cfg.cache_capacity = stoi(value);
        else if (key == "data_file") cfg.data_file = value;
    }
    file.close();
    return cfg;
}

// ========== LRU 缓存 ==========
class LRUCache {
private:
    int capacity;
    list<pair<string, string>> items;
    unordered_map<string, list<pair<string, string>>::iterator> cache;
    mutex lru_mutex;
    int hits = 0, misses = 0;

public:
    LRUCache(int cap) : capacity(cap) {}

    void put(const string& key, const string& value) {
        lock_guard<mutex> lock(lru_mutex);
        auto it = cache.find(key);
        if (it != cache.end()) {
            items.erase(it->second);
        }
        else if (items.size() >= capacity) {
            cache.erase(items.back().first);
            items.pop_back();
        }
        items.push_front({ key, value });
        cache[key] = items.begin();
    }

    string get(const string& key) {
        lock_guard<mutex> lock(lru_mutex);
        auto it = cache.find(key);
        if (it == cache.end()) {
            misses++;
            return "(null)";
        }
        hits++;
        items.splice(items.begin(), items, it->second);
        return it->second->second;
    }

    void remove(const string& key) {
        lock_guard<mutex> lock(lru_mutex);
        auto it = cache.find(key);
        if (it != cache.end()) {
            items.erase(it->second);
            cache.erase(key);
        }
    }

    int get_hits() { lock_guard<mutex> lock(lru_mutex); return hits; }
    int get_misses() { lock_guard<mutex> lock(lru_mutex); return misses; }
};

// ========== KV 存储 ==========
class KVStore {
private:
    unordered_map<string, string> data;
    LRUCache lru;
    Config config;
    mutex data_mutex;
    int cmd_count = 0;

public:
    KVStore(const Config& cfg) : lru(cfg.cache_capacity), config(cfg) {}

    void load() {
        lock_guard<mutex> lock(data_mutex);
        ifstream file(config.data_file);
        if (!file.is_open()) return;
        string key, value;
        while (file >> key) {
            file.ignore();
            getline(file, value);
            data[key] = value;
        }
        file.close();
        cout << "Loaded " << data.size() << " records from " << config.data_file << endl;
    }

    void save() {
        lock_guard<mutex> lock(data_mutex);
        ofstream file(config.data_file);
        for (const auto& pair : data)
            file << pair.first << " " << pair.second << endl;
        file.close();
    }

    string set(const string& key, const string& value) {
        {
            lock_guard<mutex> lock(data_mutex);
            data[key] = value;
        }
        lru.put(key, value);
        save();
        cmd_count++;
        return "OK";
    }

    string get(const string& key) {
        string val = lru.get(key);
        if (val != "(null)") { cmd_count++; return val; }
        {
            lock_guard<mutex> lock(data_mutex);
            auto it = data.find(key);
            if (it != data.end()) {
                lru.put(key, it->second);
                cmd_count++;
                return it->second;
            }
        }
        cmd_count++;
        return "(null)";
    }

    string del(const string& key) {
        {
            lock_guard<mutex> lock(data_mutex);
            auto it = data.find(key);
            if (it != data.end()) {
                data.erase(key);
                lru.remove(key);
                save();
                cmd_count++;
                return "OK";
            }
        }
        cmd_count++;
        return "(null)";
    }

    string stats() {
        int total = lru.get_hits() + lru.get_misses();
        double rate = total > 0 ? (double)lru.get_hits() / total * 100 : 0;
        lock_guard<mutex> lock(data_mutex);
        stringstream ss;
        ss << "--- Server Stats ---" << endl;
        ss << "Total keys:  " << data.size() << endl;
        ss << "Commands:    " << cmd_count << endl;
        ss << "Cache hits:  " << lru.get_hits() << endl;
        ss << "Cache miss:  " << lru.get_misses() << endl;
        ss << "Hit rate:    " << (int)rate << "%" << endl;
        ss << "Data file:   " << config.data_file << endl;
        ss << "Port:        " << config.port << endl;
        ss << "Cache cap:   " << config.cache_capacity << endl;
        return ss.str();
    }

    string execute(const string& cmd_line) {
        stringstream ss(cmd_line);
        string cmd;
        ss >> cmd;
        if (cmd == "set") {
            string key, value;
            ss >> key;
            getline(ss, value);
            if (!value.empty() && value[0] == ' ') value.erase(0, 1);
            if (key.empty() || value.empty()) return "Usage: set <key> <value>";
            return set(key, value);
        }
        else if (cmd == "get") {
            string key;
            ss >> key;
            if (key.empty()) return "Usage: get <key>";
            return get(key);
        }
        else if (cmd == "del") {
            string key;
            ss >> key;
            if (key.empty()) return "Usage: del <key>";
            return del(key);
        }
        else if (cmd == "keys") {
            lock_guard<mutex> lock(data_mutex);
            string result;
            for (const auto& pair : data)
                result += pair.first + " ";
            return result.empty() ? "(empty)" : result;
        }
        else if (cmd == "stats") {
            return stats();
        }
        else {
            return "Unknown command";
        }
    }
};

// ========== 客户端处理 ==========
void handle_client(int client_fd, KVStore* kvs) {
    char buffer[4096];
    string leftover;

    while (true) {
        int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;

        buffer[bytes] = '\0';
        leftover += buffer;

        size_t pos;
        while ((pos = leftover.find('\n')) != string::npos) {
            string cmd = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);

            while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == '\n'))
                cmd.pop_back();
            while (!cmd.empty() && cmd[0] == ' ') cmd.erase(0, 1);

            if (cmd.empty()) continue;
            if (cmd == "exit") { closesocket(client_fd); return; }

            string result = kvs->execute(cmd);
            result += "\n";
            send(client_fd, result.c_str(), result.length(), 0);
        }
    }
    closesocket(client_fd);
}

void start_server(KVStore* kvs, int port) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    cout << "Network server started on port " << port << " (multi-threaded)" << endl;

    while (true) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &addr_len);
        thread(handle_client, client_fd, kvs).detach();
    }

    closesocket(server_fd);
    WSACleanup();
}

// ========== 主函数 ==========
int main() {
    Config config = load_config();

    KVStore kvs(config);
    kvs.load();

    thread server_thread(start_server, &kvs, config.port);
    server_thread.detach();

    cout << "=== KV Store Server ===" << endl;
    cout << "Commands: set | get | del | keys | stats | exit" << endl;

    while (true) {
        cout << "> ";
        string line;
        getline(cin, line);
        if (line == "exit") break;
        cout << kvs.execute(line) << endl;
    }

    cout << "bye!" << endl;
    return 0;
}