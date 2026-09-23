#include "Channel.h"
#include "EventLoop.h"
#include <sys/epoll.h>
#include <iostream>
#include <cassert>

Channel::Channel(EventLoop* loop, int fd)
    : loop_(loop),
      fd_(fd),
      events_(0),
      revents_(0),
      index_(-1),
      eventHandling_(false),
      addedToLoop_(false),
      tied_(false) {}

Channel::~Channel() {
    assert(!eventHandling_);
    assert(!addedToLoop_);
}

void Channel::enableReading() {
    events_ |= kReadEvent;
    update();
}

void Channel::disableReading() {
    events_ &= ~kReadEvent;
    update();
}

void Channel::enableWriting() {
    events_ |= kWriteEvent;
    update();
}

void Channel::disableWriting() {
    events_ &= ~kWriteEvent;
    update();
}

void Channel::disableAll() {
    events_ = kNoneEvent;
    update();
}

void Channel::update() {
    addedToLoop_ = true;
    loop_->updateChannel(this);
}

void Channel::remove() {
    assert(isNoneEvent());
    addedToLoop_ = false;
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime) {
    std::shared_ptr<void> guard;
    if (tied_) {
        guard = tie_.lock();
        if (guard) {
            handleEventWithGuard(receiveTime);
        }
    } else {
        handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime) {
    eventHandling_ = true;

    // Handle close / hang-up events
    if ((revents_ & EPOLLHUP) && !(revents_ & EPOLLIN)) {
        if (closeCallback_) closeCallback_();
    }

    // Handle error events
    if (revents_ & EPOLLERR) {
        if (errorCallback_) errorCallback_();
    }

    // Handle read events
    if (revents_ & (EPOLLIN | EPOLLPRI | EPOLLRDHUP)) {
        if (readCallback_) readCallback_(receiveTime);
    }

    // Handle write events
    if (revents_ & EPOLLOUT) {
        if (writeCallback_) writeCallback_();
    }

    eventHandling_ = false;
}

void Channel::tie(const std::shared_ptr<void>& obj) {
    tie_ = obj;
    tied_ = true;
}
