#pragma once

#include "noncopyable.h"
#include "Timer.h"
#include <functional>
#include <memory>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>

class Channel;
class Poller;
class TimerQueue;

// Core event loop. Each thread can have at most one EventLoop instance.
// Drives I/O multiplexing (via Poller), timer execution (via TimerQueue),
// and cross-thread task dispatch.
class EventLoop : noncopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    // Enter the event loop. Blocks until quit() is called.
    void loop();

    // Signal the loop to stop after the current iteration.
    void quit();

    // --- Thread-safe task submission ---

    // Run cb immediately if in loop thread; otherwise queue it.
    void runInLoop(Functor cb);

    // Queue cb for execution in the loop thread. Safe to call from any thread.
    void queueInLoop(Functor cb);

    // --- Poller delegation ---

    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    // --- Thread affinity checks ---

    void assertInLoopThread();
    bool isInLoopThread() const;

    // Wake up the event loop (called from other threads).
    void wakeup();

    // --- Accessors ---

    Poller* poller() const { return poller_; }

    static EventLoop* getEventLoopOfCurrentThread();

private:
    void handleWakeup();
    void doPendingFunctors();
    void abortNotInLoopThread();

    using ChannelList = std::vector<Channel*>;

    std::atomic<bool> looping_;
    std::atomic<bool> quit_;
    bool eventHandling_;
    bool callingPendingFunctors_;
    int64_t iteration_;
    const std::thread::id threadId_;

    Poller* poller_;
    std::unique_ptr<TimerQueue> timerQueue_;
    Timestamp pollReturnTime_;
    ChannelList activeChannels_;

    // Wakeup mechanism
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    // Pending functors queue (cross-thread task dispatch)
    std::vector<Functor> pendingFunctors_;
    std::mutex mutex_;
};
