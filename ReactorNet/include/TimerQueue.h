#pragma once

#include "noncopyable.h"
#include "Timer.h"
#include "Channel.h"
#include <vector>
#include <set>
#include <memory>

class EventLoop;

// Manages a collection of timers using a min-heap.
// Uses timerfd_create for kernel-level timer notification.
//
// All timer operations must happen in the owning EventLoop's thread.
class TimerQueue : noncopyable {
public:
    using TimerCallback = std::function<void()>;

    explicit TimerQueue(EventLoop* loop);
    ~TimerQueue();

    // Add a timer. The callback will be invoked after 'delay' seconds.
    // If interval > 0, the timer repeats every 'interval' seconds.
    // Thread-safe: must be called from the EventLoop thread.
    Timer* addTimer(TimerCallback cb, Timestamp when, double interval);

    // Convenience overload: add a timer relative to now.
    Timer* addTimer(TimerCallback cb, double delay, double interval);

    // Cancel a timer. Thread-safe.
    void cancel(Timer* timer);

private:
    using TimerList = std::vector<Timer*>;
    using ActiveTimerSet = std::set<Timer*>;  // For efficient cancellation lookup

    void handleRead(Timestamp receiveTime);

    // Move expired timers from heap to expired list, reset timerfd.
    std::vector<Timer*> getExpired(Timestamp now);

    // For repeating timers: restart or delete.
    void reset(const std::vector<Timer*>& expired, Timestamp now);

    bool insert(Timer* timer);

    EventLoop* loop_;
    const int timerfd_;
    Channel timerfdChannel_;
    TimerList timers_;          // Min-heap of Timer* ordered by expiration
    ActiveTimerSet activeTimers_;  // For cancel() lookup
    bool callingExpiredTimers_;
};
