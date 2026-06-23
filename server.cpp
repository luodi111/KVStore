#include "server.h"
#include "protocol.h"

#include <iostream>
#include <thread>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <sstream>
#include <chrono>
#include <cstring>
#include <ctime>

// Windows SDK defines ERROR/INFO as macros, undefine them
#ifdef ERROR
#undef ERROR
#endif
#ifdef INFO
#undef INFO
#endif
#ifdef WARN
#undef WARN
#endif

// ============================================================
// Thread Pool - avoids frequent thread creation/destruction
// ============================================================
class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop_(false) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(queue_mutex_);
                        condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            stop_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) worker.join();
        }
    }

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            if (stop_) return;
            tasks_.push(std::move(task));
        }
        condition_.notify_one();
    }

    size_t pending() const {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        return tasks_.size();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex queue_mutex_;
    std::condition_variable condition_;
    bool stop_;
};

// ============================================================
// Logging utility with timestamps
// ============================================================
class Logger {
public:
    enum Level { L_INFO, L_WARN, L_ERROR };

    static void write(Level level, const std::string& msg) {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::tm tm_buf;
        memset(&tm_buf, 0, sizeof(tm_buf));
#ifdef _WIN32
        localtime_s(&tm_buf, &time);
#else
        localtime_r(&time, &tm_buf);
#endif
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);

        const char* level_str = "INFO";
        if (level == L_WARN) level_str = "WARN";
        else if (level == L_ERROR) level_str = "ERROR";

        std::ostream& out = (level == L_ERROR) ? std::cerr : std::cout;
        out << "[" << buf << "] [" << level_str << "] " << msg << std::endl;
    }
};

// ============================================================
// TcpServer implementation
// ============================================================
TcpServer::TcpServer(KVStore& store, uint16_t port, int thread_pool_size)
    : store_(store), port_(port), thread_pool_size_(thread_pool_size) {}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::init_socket() {
#ifdef _WIN32
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        Logger::write(Logger::L_ERROR, "WSAStartup失败");
        return false;
    }
#endif

    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ == INVALID_SOCK) {
        Logger::write(Logger::L_ERROR, "socket()失败");
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);

    if (bind(server_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERR) {
        Logger::write(Logger::L_ERROR, "bind()失败，端口 " + std::to_string(port_));
        closesock(server_fd_);
        return false;
    }

    if (listen(server_fd_, SOMAXCONN) == SOCKET_ERR) {
        Logger::write(Logger::L_ERROR, "listen()失败");
        closesock(server_fd_);
        return false;
    }

    return true;
}

bool TcpServer::start() {
    if (!init_socket()) return false;
    running_ = true;
    Logger::write(Logger::L_INFO, "服务器已启动，端口 " + std::to_string(port_) +
                  "，线程池大小 " + std::to_string(thread_pool_size_));
    return true;
}

void TcpServer::stop() {
    running_ = false;
    if (server_fd_ != INVALID_SOCK) {
        closesock(server_fd_);
        server_fd_ = INVALID_SOCK;
    }
#ifdef _WIN32
    WSACleanup();
#endif
    Logger::write(Logger::L_INFO, "服务器已停止");
}

void TcpServer::accept_loop() {
    ThreadPool pool(thread_pool_size_);

    while (running_) {
        sockaddr_in client_addr{};
        socklen_t addr_len = sizeof(client_addr);
        socket_t client_fd = accept(server_fd_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (client_fd == INVALID_SOCK) {
            if (!running_) break;
            Logger::write(Logger::L_WARN, "accept()失败，重试中...");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        Logger::write(Logger::L_INFO, "新连接来自 " + std::string(client_ip) +
                      ":" + std::to_string(ntohs(client_addr.sin_port)));

        pool.enqueue([this, client_fd] {
            handle_client(client_fd);
        });
    }
}

void TcpServer::handle_client(socket_t client_fd) {
    char buffer[4096];
    std::string leftover;

    while (running_) {
        int bytes = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;

        buffer[bytes] = '\0';
        leftover += buffer;

        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string raw_cmd = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);

            std::string cmd = ProtocolParser::parse_line(raw_cmd);
            if (cmd.empty()) continue;

            if (cmd == "exit" || cmd == "quit") {
                std::string bye = ProtocolParser::serialize_ok("bye!");
                send(client_fd, bye.c_str(), static_cast<int>(bye.size()), 0);
                closesock(client_fd);
                return;
            }

            std::string result = store_.execute(cmd);
            std::string resp = ProtocolParser::to_resp(result);
            send(client_fd, resp.c_str(), static_cast<int>(resp.size()), 0);
        }
    }
    closesock(client_fd);
}

void TcpServer::set_nonblocking(socket_t fd) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
#else
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

void TcpServer::run() {
    accept_loop();
}