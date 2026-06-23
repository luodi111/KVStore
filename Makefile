CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
LDFLAGS = -lpthread

# Linux: 娓呴櫎涓嬮潰娉ㄩ噴
# Windows (MSYS2/MinGW): 鍙栨秷涓嬮潰娉ㄩ噴锛屾坊鍔?-lws2_32
# LDFLAGS += -lws2_32

TARGET = kvstore

SRCS = main.cpp kvstore.cpp lru_cache.cpp config.cpp server.cpp
OBJS = $(SRCS:.cpp=.o)

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	./$(TARGET)
