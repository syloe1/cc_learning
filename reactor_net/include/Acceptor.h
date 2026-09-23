#pragma once

#include "noncopyable.h"
#include "Socket.h"
#include "Channel.h"
#include "InetAddress.h"
#include <functional>

class EventLoop;

// Accepts new TCP connections on a listening socket.
// Runs exclusively in the main (base) Reactor thread.
class Acceptor : noncopyable {
public:
    using NewConnectionCallback = std::function<void(int sockfd, const InetAddress& peerAddr)>;

    Acceptor(EventLoop* loop, const InetAddress& listenAddr);
    ~Acceptor();

    void setNewConnectionCallback(NewConnectionCallback cb) {
        newConnectionCallback_ = std::move(cb);
    }

    // Start listening for connections.
    void listen();

    bool listening() const { return listening_; }

private:
    void handleRead();

    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_;
    NewConnectionCallback newConnectionCallback_;
    bool listening_;
    int idleFd_;  // Reserve an fd to handle fd exhaustion gracefully
};
