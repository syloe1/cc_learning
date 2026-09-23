#pragma once

#include "noncopyable.h"
#include "InetAddress.h"
#include "TcpConnection.h"
#include <memory>
#include <vector>
#include <map>
#include <string>
#include <functional>
#include <atomic>

class EventLoop;
class Acceptor;
class EventLoopThread;

// Manages all TcpConnections and supports multi-threaded (multi-reactor) operation.
//
// Architecture:
//   - baseLoop_: The main reactor thread (accepts new connections).
//   - subLoops_: Worker reactor threads (handle I/O for accepted connections).
//   - Connections are distributed via round-robin across subLoops_.
class TcpServer : noncopyable {
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
    using ConnectionCallback = TcpConnection::ConnectionCallback;
    using MessageCallback = TcpConnection::MessageCallback;
    using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;

    TcpServer(EventLoop* loop, const InetAddress& listenAddr,
              const std::string& name = "TcpServer");
    ~TcpServer();

    // --- User-facing settings ---

    void setConnectionCallback(ConnectionCallback cb) { connectionCallback_ = std::move(cb); }
    void setMessageCallback(MessageCallback cb) { messageCallback_ = std::move(cb); }
    void setWriteCompleteCallback(WriteCompleteCallback cb) { writeCompleteCallback_ = std::move(cb); }
    void setThreadInitCallback(ThreadInitCallback cb) { threadInitCallback_ = std::move(cb); }
    void setThreadNum(int numThreads);

    // Start the server. numThreads: 0 = single-threaded, N = N worker threads.
    void start();

    // --- Accessors ---
    const std::string& name() const { return name_; }
    EventLoop* getLoop() const { return baseLoop_; }
    std::vector<EventLoop*> getAllLoops();

private:
    using TcpConnectionPtr = TcpConnection::TcpConnectionPtr;
    using ConnectionMap = std::map<std::string, TcpConnectionPtr>;

    void newConnection(int sockfd, const InetAddress& peerAddr);
    void removeConnection(const TcpConnectionPtr& conn);
    void removeConnectionInLoop(const TcpConnectionPtr& conn);

    EventLoop* const baseLoop_;
    const std::string name_;
    std::unique_ptr<Acceptor> acceptor_;
    std::atomic<bool> started_;
    int nextConnId_;

    // Thread pool
    int threadNum_;
    std::vector<std::unique_ptr<EventLoopThread>> threadPool_;
    std::vector<EventLoop*> subLoops_;

    // User callbacks
    ConnectionCallback connectionCallback_;
    MessageCallback messageCallback_;
    WriteCompleteCallback writeCompleteCallback_;
    ThreadInitCallback threadInitCallback_;

    ConnectionMap connections_;
};
