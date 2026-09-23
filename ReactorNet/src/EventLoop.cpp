#include "EventLoop.h"
#include "Channel.h"
#include "Poller.h"
#include "EPollPoller.h"
#include "TimerQueue.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <cassert>
#include <csignal>

// Thread-local pointer to the EventLoop for the current thread.
// nullptr if no EventLoop has been created on this thread.
__thread EventLoop* t_loopInThisThread = nullptr;

const int kPollTimeMs = 10000;  // Default poll timeout: 10 seconds

// --- Wakeup mechanism ---

int createEventfd() {
    int evtfd = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (evtfd < 0) {
        std::cerr << "[EventLoop] eventfd creation failed: " << strerror(errno) << std::endl;
        std::abort();
    }
    return evtfd;
}

// Ignore SIGPIPE to prevent crashes when writing to closed connections.
class IgnoreSigPipe {
public:
    IgnoreSigPipe() {
        ::signal(SIGPIPE, SIG_IGN);
    }
};

static IgnoreSigPipe initObj;

EventLoop::EventLoop()
    : looping_(false),
      quit_(false),
      eventHandling_(false),
      callingPendingFunctors_(false),
      iteration_(0),
      threadId_(std::this_thread::get_id()),
      poller_(new EPollPoller(this)),
      timerQueue_(new TimerQueue(this)),
      wakeupFd_(createEventfd()),
      wakeupChannel_(new Channel(this, wakeupFd_)) {
    if (t_loopInThisThread) {
        std::cerr << "[EventLoop] Another EventLoop " << t_loopInThisThread
                  << " exists in this thread " << threadId_ << std::endl;
        std::abort();
    }
    t_loopInThisThread = this;

    wakeupChannel_->setReadCallback(
        std::bind(&EventLoop::handleWakeup, this));
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop() {
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);

    delete poller_;
    poller_ = nullptr;

    t_loopInThisThread = nullptr;
}

void EventLoop::loop() {
    assert(!looping_);
    assertInLoopThread();
    looping_ = true;
    quit_ = false;

    while (!quit_) {
        activeChannels_.clear();
        pollReturnTime_ = poller_->poll(kPollTimeMs, &activeChannels_);
        ++iteration_;

        eventHandling_ = true;
        for (Channel* channel : activeChannels_) {
            channel->handleEvent(pollReturnTime_);
        }
        eventHandling_ = false;

        doPendingFunctors();
    }

    looping_ = false;
}

void EventLoop::quit() {
    quit_ = true;
    // Wake up the loop if it's blocked in poll
    if (!isInLoopThread()) {
        wakeup();
    }
}

void EventLoop::runInLoop(Functor cb) {
    if (isInLoopThread()) {
        cb();
    } else {
        queueInLoop(std::move(cb));
    }
}

void EventLoop::queueInLoop(Functor cb) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        pendingFunctors_.push_back(std::move(cb));
    }
    // Wake up the loop if:
    // - Called from another thread, OR
    // - Called from loop thread while doPendingFunctors is running
    if (!isInLoopThread() || callingPendingFunctors_) {
        wakeup();
    }
}

void EventLoop::updateChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel* channel) {
    assert(channel->ownerLoop() == this);
    assertInLoopThread();
    return poller_->hasChannel(channel);
}

void EventLoop::assertInLoopThread() {
    if (!isInLoopThread()) {
        abortNotInLoopThread();
    }
}

bool EventLoop::isInLoopThread() const {
    return threadId_ == std::this_thread::get_id();
}

EventLoop* EventLoop::getEventLoopOfCurrentThread() {
    return t_loopInThisThread;
}

void EventLoop::abortNotInLoopThread() {
    std::cerr << "[EventLoop] abortNotInLoopThread - EventLoop " << this
              << " was created in threadId_ = " << threadId_
              << ", current thread id = " << std::this_thread::get_id()
              << std::endl;
    std::abort();
}

void EventLoop::wakeup() {
    uint64_t one = 1;
    ssize_t n = ::write(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        std::cerr << "[EventLoop] wakeup write error: wrote " << n
                  << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::handleWakeup() {
    uint64_t one;
    ssize_t n = ::read(wakeupFd_, &one, sizeof(one));
    if (n != sizeof(one)) {
        std::cerr << "[EventLoop] handleWakeup read error: read " << n
                  << " bytes instead of 8" << std::endl;
    }
}

void EventLoop::doPendingFunctors() {
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }
    for (const Functor& functor : functors) {
        functor();
    }
    callingPendingFunctors_ = false;
}
