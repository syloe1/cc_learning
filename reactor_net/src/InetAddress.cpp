#include "InetAddress.h"
#include <cstring>
#include <iostream>

InetAddress::InetAddress(uint16_t port, const std::string& ip, bool loopbackOnly) {
    std::memset(&addr_, 0, sizeof(addr_));
    addr_.sin_family = AF_INET;

    if (loopbackOnly) {
        addr_.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    } else if (ip.empty() || ip == "INADDR_ANY") {
        addr_.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (::inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr) <= 0) {
            std::cerr << "[InetAddress] Invalid IP address: " << ip
                      << ", falling back to INADDR_ANY" << std::endl;
            addr_.sin_addr.s_addr = htonl(INADDR_ANY);
        }
    }

    addr_.sin_port = htons(port);
}

InetAddress::InetAddress(const sockaddr_in& addr) : addr_(addr) {}

std::string InetAddress::toIpPort() const {
    char buf[64];
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    char result[128];
    std::snprintf(result, sizeof(result), "%s:%u", buf, ntohs(addr_.sin_port));
    return result;
}

std::string InetAddress::toIp() const {
    char buf[64];
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}
