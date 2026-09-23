#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "Buffer.h"
#include "Socket.h"
#include "Channel.h"
#include <memory>
#include <string>
#include <functional>

class EventLoop;

// Represents a single TCP connection. Managed via shared_ptr.
// Uses enable_shared_from_this so callbacks can safely extend the connection's lifetime.
class TcpConnection : noncopyable,
                      public std::enable_shared_from_this<TcpConnection> {
public:
    using TcpConnectionPtr = std::shared_ptr<TcpConnection>;
    using ConnectionCallback = std::function<void(const TcpConnectionPtr&)>;
    using MessageCallback = std::function<void(const TcpConnectionPtr&, Buffer*, Timestamp)>;
    using WriteCompleteCallback = std::function<void(const TcpConnectionPtr&)>;
    using CloseCallback = std::function<void(const TcpConnectionPtr&)>;

    enum StateE { kConnecting, kConnected, kDisconnecting, kDisconnected };

    // Construct with an already-accepted socket fd and peer address.
    TcpConnection(EventLoop* loop, const std::string& name,
                  int sockfd, const InetAddress& localAddr,
                  const InetAddress& peerAddr);
    ~TcpConnection();

    // --- Accessors ---
    EventLoop* getLoop() const { return loop_; }
    const std::string& name() const { return name_; }
    const InetAddress& localAddress() const { return localAddr_; }
    const InetAddress& peerAddress() const { return peerAddr_; }
    bool connected() const { return state_ == kConnected; }

    // --- Callback setters ---
    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = std::move(cb); }

    // Called by TcpServer when the connection is established.
    void connectEstablished();

    // Called by TcpServer to start the destruction process.
    void connectDestroyed();

    // Send data. Thread-safe: if called from another thread, the actual send
    // is queued to the connection's EventLoop.
    void send(const std::string& message);
    void send(const void* data, size_t len);

    // Initiate an orderly shutdown (finishes writing pending data first).
    void shutdown();

    // Force close immediately.
    void forceClose();

private:
    void handleRead(Timestamp receiveTime);
    void handleWrite();
    void handleClose();
    void handleError();
    void sendInLoop(const std::string& message);
    void sendInLoop(const void* data, size_t len);
    void shutdownInLoop();
    void forceCloseInLoop();

    EventLoop* loop_;
    const std::string name_;
    StateE state_;

    Socket socket_;
    std::unique_ptr<Channel> channel_;
    const InetAddress localAddr_;
    const InetAddress peerAddr_;

    Buffer inputBuffer_;
    Buffer outputBuffer_;

    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    CloseCallback closeCallback_;
};
