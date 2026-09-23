#pragma once

#include "noncopyable.h"
#include "InetAddress.h"

// RAII wrapper around a socket file descriptor.
// Owns the fd and closes it on destruction.
class Socket : noncopyable {
public:
    explicit Socket(int sockfd) : sockfd_(sockfd) {}

    // Create a new non-blocking TCP socket.
    Socket();

    ~Socket();

    // Move constructor / assignment
    Socket(Socket&& other) noexcept : sockfd_(other.sockfd_) {
        other.sockfd_ = -1;
    }

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) {
            close();
            sockfd_ = other.sockfd_;
            other.sockfd_ = -1;
        }
        return *this;
    }

    int fd() const { return sockfd_; }

    // Bind to the given address.
    void bind(const InetAddress& addr);

    // Start listening with the given backlog.
    void listen(int backlog = SOMAXCONN);

    // Accept a new connection, filling in peerAddr. Returns the new fd.
    // Returns -1 on error.
    int accept(InetAddress* peerAddr);

    // Set SO_REUSEADDR (typically called before bind).
    void setReuseAddr(bool on);

    // Set SO_REUSEPORT (for multi-reactor accept).
    void setReusePort(bool on);

    // Set TCP_NODELAY (disable Nagle's algorithm).
    void setTcpNoDelay(bool on);

    // Set SO_KEEPALIVE.
    void setKeepAlive(bool on);

    // Set the socket to non-blocking mode.
    void setNonBlocking();

    // Shutdown the write side of the connection.
    void shutdownWrite();

    // Close the socket.
    void close();

private:
    int sockfd_;
};
