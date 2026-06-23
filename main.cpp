#include "kvstore.h"
#include "config.h"
#include "server.h"

#include <iostream>
#include <thread>
#include <csignal>
#include <atomic>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

static std::atomic<bool> g_running(true);

void init_console() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void signal_handler(int signum) {
    std::cout << "\n收到信号 " << signum << "，正在关闭..." << std::endl;
    g_running = false;
}

void print_banner(const ServerConfig& cfg) {
    std::cout << R"(
  Lightweight Key-Value Store
)" << std::endl;
    cfg.print();
    std::cout << std::endl;
}

void print_help() {
    std::cout << "支持的命令:" << std::endl;
    std::cout << "  SET <key> <value>         设置键值" << std::endl;
    std::cout << "  SETEX <key> <ttl> <value> 设置带过期时间的键值(秒)" << std::endl;
    std::cout << "  GET <key>                 获取键的值" << std::endl;
    std::cout << "  DEL <key>                 删除键" << std::endl;
    std::cout << "  EXISTS <key>              检查键是否存在" << std::endl;
    std::cout << "  KEYS                      列出所有键" << std::endl;
    std::cout << "  STATS                     显示服务器统计信息" << std::endl;
    std::cout << "  PING                      测试连接" << std::endl;
    std::cout << "  exit                      退出" << std::endl;
    std::cout << std::endl;
}

void interactive_loop(KVStore& store) {
    std::string line;
    while (g_running) {
        std::cout << "kvstore> ";
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        if (line == "exit" || line == "quit") break;
        std::cout << store.execute(line) << std::endl;
    }
}

int main(int argc, char* argv[]) {
    init_console();

    // 注册信号处理，支持优雅退出
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // 加载配置文件
    ServerConfig config = ServerConfig::load("kvs.conf");
    if (argc > 1) {
        config.port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    print_banner(config);

    // 初始化存储引擎
    KVStore store(config);
    size_t loaded = store.load_from_disk();

    if (loaded > 0) {
        std::cout << "已加载 " << loaded << " 条历史记录。" << std::endl;
    }

    print_help();

    // 启动网络服务
    TcpServer server(store, config.port, config.thread_pool_size);
    if (!server.start()) {
        std::cerr << "服务器启动失败!" << std::endl;
        return 1;
    }

    std::cout << "服务器监听端口 " << config.port << std::endl;
    std::cout << "连接方式: telnet 127.0.0.1 " << config.port << std::endl;
    std::cout << "输入 'exit' 退出。" << std::endl << std::endl;

    // 启动网络线程
    std::thread server_thread([&server] { server.run(); });

    // 主线程运行交互命令行
    interactive_loop(store);

    // 清理资源
    server.stop();
    g_running = false;
    if (server_thread.joinable()) server_thread.join();

    std::cout << "再见!" << std::endl;
    return 0;
}