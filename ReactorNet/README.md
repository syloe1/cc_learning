# ReactorNet

A minimalist C++ network library implementing the Reactor pattern, inspired by [muduo](https://github.com/chenshuo/muduo). No third-party dependencies — pure Linux system calls.

## Features

- **Epoll Event-Driven** — High-performance I/O multiplexing via `epoll`
- **Timer Heap** — Min-heap timer management using `timerfd_create` for kernel-level precision
- **Read/Write Buffers** — Efficient `readv`-based input with automatic growth, handles TCP framing
- **Single Reactor / Multi-Reactor** — One-loop-per-thread architecture with round-robin load distribution
- **HTTP 1.0 Server** — Static file serving with URL decoding and path traversal protection
- **Zero Dependencies** — C++17, Linux system calls only

## Architecture

```
┌───────────────────────────────────────────┐
│  TcpServer (baseLoop)                     │
│  ┌─────────┐                              │
│  │Acceptor │──► newConnection()           │
│  └─────────┘     │                        │
│                  │ round-robin             │
│     ┌────────────┼────────────┐           │
│     ▼            ▼            ▼           │
│  EventLoop   EventLoop   EventLoop  ...   │
│  (worker 0)  (worker 1)  (worker 2)       │
│     │            │            │            │
│  TcpConn     TcpConn     TcpConn          │
└───────────────────────────────────────────┘
```

### Module Overview

| Module | Description |
|--------|-------------|
| `EventLoop` | Per-thread event loop with cross-thread task dispatch via `eventfd` |
| `EPollPoller` | Epoll-based I/O multiplexing (implements `Poller`) |
| `Channel` | Event dispatcher: binds fd + callbacks, does not own the fd |
| `TimerQueue` | Min-heap timer management using `timerfd` |
| `TcpConnection` | TCP connection with input/output buffers, `shared_ptr` lifecycle |
| `TcpServer` | Multi-reactor server with accept loop and connection management |
| `Acceptor` | Listening socket handler, runs in base reactor |
| `Buffer` | Non-contiguous read/write buffer with prependable space |
| `Socket` | RAII wrapper around socket fd |
| `InetAddress` | `sockaddr_in` wrapper |
| `HttpRequest` | Simple HTTP 1.0 request parser (GET only) |
| `HttpResponse` | HTTP 1.0 response builder |

## Build & Run

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
```

### Echo Server

```bash
# Single-threaded (default)
./reactor_demo

# 4 worker threads
./reactor_demo 4

# Test with netcat
echo "Hello, Reactor!" | nc localhost 8080
```

### HTTP Server

```bash
# Single-threaded HTTP
./reactor_demo http

# 4 worker threads
./reactor_demo http 4

# Test with curl
curl http://localhost:8080/
```

## API Example

```cpp
#include "TcpServer.h"
#include "EventLoop.h"
#include "InetAddress.h"

int main() {
    EventLoop loop;
    TcpServer server(&loop, InetAddress(8080));

    server.setMessageCallback([](const TcpConnectionPtr& conn,
                                  Buffer* buf, Timestamp) {
        std::string msg = buf->retrieveAllAsString();
        conn->send(msg);  // Echo back
    });

    server.setThreadNum(4);  // 4 worker threads
    server.start();
    loop.loop();
}
```

## Requirements

- Linux kernel 2.6.27+ (for `eventfd`, `timerfd`, `signalfd`, `epoll`)
- GCC 8+ or Clang 7+ (C++17 support)
- CMake 3.10+

## File Structure

```
ReactorNet/
├── CMakeLists.txt
├── README.md
├── include/           # Header files
│   ├── noncopyable.h
│   ├── InetAddress.h
│   ├── Socket.h
│   ├── Channel.h
│   ├── Poller.h
│   ├── EPollPoller.h
│   ├── EventLoop.h
│   ├── Timer.h
│   ├── TimerQueue.h
│   ├── Buffer.h
│   ├── TcpConnection.h
│   ├── Acceptor.h
│   ├── TcpServer.h
│   ├── EventLoopThread.h
│   ├── HttpRequest.h
│   └── HttpResponse.h
├── src/               # Source files
│   ├── *.cpp
│   └── main.cpp
└── www/               # Static files (HTTP server)
    └── index.html
```

## License

MIT
