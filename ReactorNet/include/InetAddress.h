#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

// Encapsulates sockaddr_in, providing a clean C++ interface
// for specifying network addresses (IP + port).
class InetAddress {
public:
    // Construct with port and IP. If ip is empty or "INADDR_ANY", binds to all interfaces.
    // If loopbackOnly is true, binds to 127.0.0.1.
    explicit InetAddress(uint16_t port = 0, const std::string& ip = "", bool loopbackOnly = false);

    // Construct directly from an existing sockaddr_in structure.
    explicit InetAddress(const sockaddr_in& addr);

    // Returns the raw sockaddr pointer (for system calls like bind, accept).
    const sockaddr* getSockAddr() const { return reinterpret_cast<const sockaddr*>(&addr_); }

    sockaddr_in* getSockAddrIn() { return &addr_; }
    const sockaddr_in* getSockAddrIn() const { return &addr_; }

    socklen_t getSockLen() const { return static_cast<socklen_t>(sizeof(addr_)); }

    // Returns "IP:PORT" string representation.
    std::string toIpPort() const;

    // Returns the IP address as a string.
    std::string toIp() const;

    uint16_t toPort() const { return ntohs(addr_.sin_port); }

    // Set from an existing sockaddr_in (e.g., result of accept).
    void setSockAddr(const sockaddr_in& addr) { addr_ = addr; }

private:
    sockaddr_in addr_;
};
