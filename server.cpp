#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include "thread_safe_lru_cache.h"
#include "thread_pool.h"
#include "protocol.h"

static const int DEFAULT_PORT = 6380;      // avoid clashing with real redis (6379)
static const size_t CACHE_CAPACITY = 1000; // number of keys the cache holds
static const size_t POOL_SIZE = 8;         // worker threads handling clients

// Reads and handles commands from a single connected client until it
// disconnects. Runs on a thread-pool worker thread.
void handleClient(int clientFd, Protocol& protocol) {
    char buffer[4096];
    std::string pending; // holds a partial line across recv() calls

    while (true) {
        ssize_t n = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break; // client disconnected or error

        buffer[n] = '\0';
        pending += buffer;

        // Process every complete line (\n-terminated) we've received so far.
        size_t pos;
        while ((pos = pending.find('\n')) != std::string::npos) {
            std::string line = pending.substr(0, pos);
            pending.erase(0, pos + 1);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;

            std::string response = protocol.handleCommand(line) + "\n";
            send(clientFd, response.c_str(), response.size(), 0);
        }
    }
    close(clientFd);
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    if (argc > 1) port = std::atoi(argv[1]);

    ThreadSafeLRUCache cache(CACHE_CAPACITY);
    Protocol protocol(cache);
    ThreadPool pool(POOL_SIZE);

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        perror("socket");
        return 1;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(serverFd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return 1;
    }

    if (listen(serverFd, /*backlog=*/128) < 0) {
        perror("listen");
        return 1;
    }

    std::cout << "LRU cache server listening on port " << port
              << " (capacity=" << CACHE_CAPACITY
              << ", worker threads=" << POOL_SIZE << ")\n";

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd, (sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) {
            perror("accept");
            continue;
        }

        // Disable Nagle's algorithm so small command/response packets
        // (typical for a cache protocol) aren't delayed -- lower latency
        // per request, which matters for a "high performance" cache.
        int one = 1;
        setsockopt(clientFd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));

        pool.enqueue([clientFd, &protocol] {
            handleClient(clientFd, protocol);
        });
    }

    close(serverFd);
    return 0;
}
