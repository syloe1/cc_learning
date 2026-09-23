#pragma once

#include "noncopyable.h"
#include "Timer.h"
#include <functional>
#include <memory>
#include <sys/epoll.h>
#include <cstdint>

class EventLoop;

// Channel is the core event dispatcher. It does NOT own the fd;
// it is given an fd by an external owner (Socket, timerfd, eventfd).
//
// Each Channel belongs to exactly one EventLoop thread and tracks:
//   events_  - which events we are interested in (EPOLLIN, EPOLLOUT, etc.)
//   revents_ - which events actually fired (set by EPollPoller::poll).
class Channel : noncopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop, int fd);
    ~Channel();

    int fd() const { return fd_; }
    int events() const { return events_; }
    int index() const { return index_; }

    // Called by EPollPoller to record the fd's position in the epoll interest list.
    void setIndex(int idx) { index_ = idx; }

    // Called by EPollPoller when poll() returns events for this fd.
    void setRevents(uint32_t revents) { revents_ = revents; }
    uint32_t revents() const { return revents_; }

    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isReading() const { return events_ & kReadEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }

    // Enable / disable specific event interests.
    void enableReading();
    void disableReading();
    void enableWriting();
    void disableWriting();
    void disableAll();

    // --- Callback setters ---
    void setReadCallback(ReadEventCallback cb) { readCallback_ = std::move(cb); }
    void setWriteCallback(EventCallback cb) { writeCallback_ = std::move(cb); }
    void setCloseCallback(EventCallback cb) { closeCallback_ = std::move(cb); }
    void setErrorCallback(EventCallback cb) { errorCallback_ = std::move(cb); }

    // Called by EventLoop when poll returns events for this fd.
    // Dispatches to the appropriate callback based on revents_.
    void handleEvent(Timestamp receiveTime);

    // Remove this channel from its EventLoop.
    void remove();

    EventLoop* ownerLoop() const { return loop_; }

    // Tie this channel to a shared_ptr owner to prevent premature destruction.
    void tie(const std::shared_ptr<void>& obj);

private:
    static const uint32_t kNoneEvent = 0;
    static const uint32_t kReadEvent = EPOLLIN | EPOLLPRI;
    static const uint32_t kWriteEvent = EPOLLOUT;

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    EventLoop* const loop_;
    const int fd_;
    uint32_t events_;    // Events we're interested in
    uint32_t revents_;   // Events that were returned by epoll_wait
    int index_;          // Used by Poller (-1 = new, 0 = not added, 1 = added)

    bool eventHandling_;
    bool addedToLoop_;

    std::weak_ptr<void> tie_;
    bool tied_;

    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};
