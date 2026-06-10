#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <thread>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

using namespace std;

class KVStore {
private:
    unordered_map<string, string> data;

public:
    string set(const string& key, const string& value) {
        data[key] = value;
        return "OK";
    }

    string get(const string& key) {
        auto it = data.find(key);
        if (it != data.end()) return data[key];
        return "(null)";
    }

    string del(const string& key) {
        auto it = data.find(key);
        if (it != data.end()) {
            data.erase(key);
            return "OK";
        }
        return "(null)";
    }

    string keys() {
        if (data.empty()) return "(empty)";
        string result;
        for (const auto& pair : data) {
            result += pair.first + " ";
        }
        return result;
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
            return keys();
        }
        else {
            return "Unknown command";
        }
    }
};

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8888);

    bind(server_fd, (sockaddr*)&address, sizeof(address));
    listen(server_fd, 10);

    cout << "=== KV Store Server ===" << endl;
    cout << "Server running on port 8888" << endl;
    cout << "Type commands below or connect via telnet." << endl;
    cout << "Commands: set <key> <value> | get <key> | del <key> | keys | exit" << endl;

    KVStore kvs;
    string line;

    while (true) {
        cout << "> ";
        getline(cin, line);
        if (line == "exit") break;
        cout << kvs.execute(line) << endl;
    }

    closesocket(server_fd);
    WSACleanup();
    cout << "bye!" << endl;
    return 0;
}