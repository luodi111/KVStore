#pragma once

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using socket_t = SOCKET;
    constexpr socket_t INVALID_SOCK = INVALID_SOCKET;
    constexpr int SOCKET_ERR = SOCKET_ERROR;
    #define closesock closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <cerrno>
    #include <cstring>
    using socket_t = int;
    constexpr socket_t INVALID_SOCK = -1;
    constexpr int SOCKET_ERR = -1;
    #define closesock close
#endif

#include "kvstore.h"
#include <string>

class TcpServer {
public:
    TcpServer(KVStore& store, uint16_t port, int thread_pool_size = 4);
    ~TcpServer();

    bool start();
    void run();
    void stop();

private:
    bool init_socket();
    void accept_loop();
    void handle_client(socket_t client_fd);
    static void set_nonblocking(socket_t fd);

    KVStore& store_;
    uint16_t port_;
    int thread_pool_size_;
    socket_t server_fd_ = INVALID_SOCK;
    bool running_ = false;
};
