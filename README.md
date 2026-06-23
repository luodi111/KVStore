# KVStore - Lightweight Key-Value Store

A high-performance, network-accessible key-value store written in C++17.

## Features

- **LRU Cache**: Least Recently Used eviction with configurable capacity
- **Persistence**: Automatic disk persistence with configurable data file
- **Networking**: TCP server with multi-threaded connection handling (thread pool)
- **Cross-Platform**: Compiles on Linux, macOS, and Windows (MSYS2/MinGW)
- **RESP Protocol**: Redis-compatible serialization protocol
- **TTL Support**: Key expiry with SETEX command
- **Thread Safety**: Mutex-protected data access for concurrent clients
- **Configuration**: File-based configuration (kvs.conf)
- **Logging**: Timestamped logging with severity levels
- **Signal Handling**: Graceful shutdown on SIGINT/SIGTERM

## Build

### Linux / macOS
```bash
make
```

### Windows (MSYS2/MinGW)
```bash
# Edit Makefile: uncomment LDFLAGS += -lws2_32
make
```

### Manual Compilation
```bash
# Linux
g++ -std=c++17 -O2 -Wall main.cpp kvstore.cpp lru_cache.cpp config.cpp server.cpp -o kvstore -lpthread

# Windows
g++ -std=c++17 -O2 -Wall main.cpp kvstore.cpp lru_cache.cpp config.cpp server.cpp -o kvstore.exe -lws2_32 -lpthread
```

## Usage

```bash
./kvstore [port]
```

### Commands
| Command | Description |
|---------|-------------|
| SET key value | Store a key-value pair |
| SETEX key ttl value | Store with TTL in seconds |
| GET key | Retrieve value by key |
| DEL key | Delete a key |
| EXISTS key | Check if key exists |
| KEYS | List all keys |
| STATS | Show server statistics |
| PING | Test connection |
| exit | Quit |

### Network Access
```bash
telnet 127.0.0.1 8888
```

## Configuration (kvs.conf)
```
port 8888
cache_capacity 100
thread_pool_size 4
data_file kvs_data.txt
enable_persistence true
```

## Architecture

```
main.cpp         - Entry point, CLI, signal handling
kvstore.h/cpp    - Core key-value store engine
lru_cache.h/cpp  - Thread-safe LRU cache
config.h/cpp     - Configuration loading/saving
server.h/cpp     - Cross-platform TCP server + thread pool
protocol.h       - RESP protocol parser/serializer
```
