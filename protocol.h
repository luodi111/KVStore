#include "kvstore.h"
#include <sstream>

// RESP (Redis Serialization Protocol) parser
// Parses client input using a simple text-based protocol
class ProtocolParser {
public:
    // Parse a raw line into a command string that KVStore can execute
    static std::string parse_line(const std::string& line) {
        std::string cmd = line;
        // Trim leading whitespace
        size_t start = 0;
        while (start < cmd.size() && (cmd[start] == ' ' || cmd[start] == '\t'))
            start++;
        if (start > 0) cmd.erase(0, start);
        // Trim trailing whitespace and CR
        while (!cmd.empty() && (cmd.back() == '\r' || cmd.back() == '\n' || cmd.back() == ' '))
            cmd.pop_back();
        return cmd;
    }

    // Serialize response for network transmission
    // Simple string: "+OK\r\n" or "-ERR message\r\n"
    // Bulk string: "$length\r\n<data>\r\n"
    // Nil: "$-1\r\n"
    static std::string serialize_ok(const std::string& msg) {
        return "+" + msg + "\r\n";
    }

    static std::string serialize_err(const std::string& msg) {
        return "-" + msg + "\r\n";
    }

    static std::string serialize_bulk(const std::string& data) {
        return "$" + std::to_string(data.size()) + "\r\n" + data + "\r\n";
    }

    static std::string serialize_nil() {
        return "$-1\r\n";
    }

    static std::string serialize_integer(int64_t n) {
        return ":" + std::to_string(n) + "\r\n";
    }

    static std::string serialize_array(const std::vector<std::string>& items) {
        std::ostringstream ss;
        ss << "*" << items.size() << "\r\n";
        for (const auto& item : items) {
            ss << serialize_bulk(item);
        }
        return ss.str();
    }

    // Convert KVStore result to RESP format  
    static std::string to_resp(const std::string& result) {
        if (result == "(nil)") return serialize_nil();
        if (result.rfind("ERR", 0) == 0) return serialize_err(result);
        if (result == "OK" || result == "PONG") return serialize_ok(result);
        if (result == "1") return serialize_integer(1);
        if (result == "0") return serialize_integer(0);
        if (result.find(' ') != std::string::npos) {
            // Space-separated list (like KEYS output)
            std::vector<std::string> items;
            std::istringstream iss(result);
            std::string item;
            while (iss >> item) items.push_back(item);
            return serialize_array(items);
        }
        return serialize_bulk(result);
    }
};
