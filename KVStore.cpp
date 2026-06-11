#include <iostream>
#include <string>
#include <unordered_map>
#include <sstream>
#include <fstream>

using namespace std;

class KVStore {
private:
    unordered_map<string, string> data;
    string filename = "kvs_data.txt";

public:
    // 加载数据（程序启动时自动调用）
    void load() {
        ifstream file(filename);
        if (!file.is_open()) return;
        string key, value;
        while (file >> key) {
            file.ignore();  // 跳过空格
            getline(file, value);
            data[key] = value;
        }
        file.close();
        cout << "Loaded " << data.size() << " records from disk." << endl;
    }

    // 保存数据（每次修改后自动调用）
    void save() {
        ofstream file(filename);
        for (const auto& pair : data) {
            file << pair.first << " " << pair.second << endl;
        }
        file.close();
    }

    string set(const string& key, const string& value) {
        data[key] = value;
        save();  // 改完就存盘
        return "OK";
    }

    string get(const string& key) {
        auto it = data.find(key);
        if (it != data.end()) return it->second;
        return "(null)";
    }

    string del(const string& key) {
        auto it = data.find(key);
        if (it != data.end()) {
            data.erase(key);
            save();  // 删完就存盘
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
    KVStore kvs;
    kvs.load();  // 启动时自动加载之前存的数据

    string line;
    cout << "=== KV Store with Persistence ===" << endl;
    cout << "Commands: set <key> <value> | get <key> | del <key> | keys | exit" << endl;

    while (true) {
        cout << "> ";
        getline(cin, line);
        if (line == "exit") break;
        cout << kvs.execute(line) << endl;
    }

    cout << "bye!" << endl;
    return 0;
}