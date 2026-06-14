#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <list>
#include <thread>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

class LRUCache {
private:
    int capacity;
    list<pair<string, string>> items;
    unordered_map<string, list<pair<string, string>>::iterator> cache;

public:
    LRUCache(int cap = 10) : capacity(cap) {}

    void put(const string& key, const string& value) {
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
        auto it = cache.find(key);
        if (it == cache.end()) return "(null)";
        items.splice(items.begin(), items, it->second);
        return it->second->second;
    }

    void remove(const string& key) {
        auto it = cache.find(key);
        if (it != cache.end()) {
            items.erase(it->second);
            cache.erase(key);
        }
    }
};

class KVStore {
private:
    unordered_map<string, string> data;
    LRUCache lru;
    string filename = "kvs_data.txt";

public:
    void load() {
        ifstream file(filename);
        if (!file.is_open()) return;
        string key, value;
        while (file >> key) {
            file.ignore();
            getline(file, value);
            data[key] = value;
        }
        file.close();
        cout << "Loaded " << data.size() << " records." << endl;
    }

    void save() {
        ofstream file(filename);
        for (const auto& pair : data) {
            file << pair.first << " " << pair.second << endl;
        }
        file.close();
    }

    string set(const string& key, const string& value) {
        data[key] = value;
        lru.put(key, value);
        save();
        return "OK";
    }

    string get(const string& key) {
        string val = lru.get(key);
        if (val != "(null)") return val;
        auto it = data.find(key);
        if (it != data.end()) {
            lru.put(key, it->second);
            return it->second;
        }
        return "(null)";
    }

    string del(const string& key) {
        auto it = data.find(key);
        if (it != data.end()) {
            data.erase(key);
            lru.remove(key);
            save();
            return "OK";
        }
        return "(null)";
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
            string result;
            for (const auto& pair : data)
                result += pair.first + " ";
            return result.empty() ? "(empty)" : result;
        }
        else {
            return "Unknown command";
        }
    }
};

// 处理单个客户端连接
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
            if (cmd == "exit") {
                closesocket(client_fd);
                return;
            }

            string result = kvs->execute(cmd);
            result += "\n";
            send(client_fd, result.c_str(), result.length(), 0);
        }
    }

    closesocket(client_fd);
}

void start_server(KVStore* kvs) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8888);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    cout << "Network server started on port 8888" << endl;

    while (true) {
        sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &addr_len);
        thread(handle_client, client_fd, kvs).detach();
    }

    closesocket(server_fd);
    WSACleanup();
}

int main() {
    KVStore kvs;
    kvs.load();

    // 启动网络服务器（后台线程）
    thread server_thread(start_server, &kvs);
    server_thread.detach();

    cout << "=== KV Store Server ===" << endl;
    cout << "Network: telnet 127.0.0.1 8888" << endl;
    cout << "Type commands below or connect via network." << endl;
    cout << "Commands: set <key> <value> | get <key> | del <key> | keys | exit" << endl;

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