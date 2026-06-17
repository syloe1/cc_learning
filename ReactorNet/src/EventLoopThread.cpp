#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread()
    : loop_(nullptr),
      exiting_(false) {}

EventLoopThread::~EventLoopThread() {
    exiting_ = true;
    if (loop_ != nullptr) {
        loop_->quit();
    }
    if (thread_ && thread_->joinable()) {
        thread_->join();
    }
}

EventLoop* EventLoopThread::startLoop() {
    thread_ = std::make_unique<std::thread>(&EventLoopThread::threadFunc, this);

    // Block until the thread has created the EventLoop
    std::unique_lock<std::mutex> lock(mutex_);
    cond_.wait(lock, [this]() { return loop_ != nullptr; });

    return loop_;
}

void EventLoopThread::threadFunc() {
    EventLoop loop;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        loop_ = &loop;
        cond_.notify_one();
    }

    loop.loop();

    // When loop exits
    std::lock_guard<std::mutex> lock(mutex_);
    loop_ = nullptr;
}
