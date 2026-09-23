#pragma once

#include "noncopyable.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>

class EventLoop;

// Encapsulates a thread that runs an EventLoop.
// The EventLoop is created inside the thread and returned via startLoop().
class EventLoopThread : noncopyable {
public:
    EventLoopThread();
    ~EventLoopThread();

    // Start the thread and return its EventLoop pointer.
    // Blocks until the thread has created and started the loop.
    EventLoop* startLoop();

private:
    void threadFunc();

    EventLoop* loop_;
    bool exiting_;
    std::unique_ptr<std::thread> thread_;
    std::mutex mutex_;
    std::condition_variable cond_;
};
