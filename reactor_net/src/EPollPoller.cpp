#include "EPollPoller.h"
#include "Channel.h"
#include "EventLoop.h"
#include <unistd.h>
#include <cstring>
#include <iostream>

EPollPoller::EPollPoller(EventLoop* loop)
    : Poller(loop),
      epollfd_(::epoll_create1(EPOLL_CLOEXEC)),
      events_(kInitEventListSize) {
    if (epollfd_ < 0) {
        std::cerr << "[EPollPoller] epoll_create1 failed: " << strerror(errno) << std::endl;
        std::abort();
    }
}

EPollPoller::~EPollPoller() {
    if (epollfd_ >= 0) {
        ::close(epollfd_);
    }
}

Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels) {
    int numEvents = ::epoll_wait(epollfd_, events_.data(),
                                  static_cast<int>(events_.size()), timeoutMs);
    int savedErrno = errno;
    Timestamp now(Timestamp::now());

    if (numEvents > 0) {
        fillActiveChannels(numEvents, activeChannels);
        // Expand the event buffer if it's full
        if (static_cast<size_t>(numEvents) == events_.size()) {
            events_.resize(events_.size() * 2);
        }
    } else if (numEvents < 0) {
        if (savedErrno != EINTR) {
            std::cerr << "[EPollPoller] epoll_wait error: " << strerror(savedErrno) << std::endl;
        }
    }
    return now;
}

void EPollPoller::updateChannel(Channel* channel) {
    const int index = channel->index();
    int fd = channel->fd();

    if (index == -1 || index == 0) {
        // New or previously removed channel
        if (index == -1) {
            // Brand new channel
            channels_[fd] = channel;
        }
        channel->setIndex(1);
        update(EPOLL_CTL_ADD, channel);
    } else {
        // Existing channel
        if (channel->isNoneEvent()) {
            update(EPOLL_CTL_DEL, channel);
            channel->setIndex(0);
        } else {
            update(EPOLL_CTL_MOD, channel);
        }
    }
}

void EPollPoller::removeChannel(Channel* channel) {
    int fd = channel->fd();
    channels_.erase(fd);

    if (channel->index() == 1) {
        update(EPOLL_CTL_DEL, channel);
    }
    channel->setIndex(-1);
}

void EPollPoller::update(int operation, Channel* channel) {
    epoll_event event;
    std::memset(&event, 0, sizeof(event));
    event.events = channel->events();
    event.data.ptr = channel;

    if (::epoll_ctl(epollfd_, operation, channel->fd(), &event) < 0) {
        if (operation != EPOLL_CTL_DEL) {
            std::cerr << "[EPollPoller] epoll_ctl op=" << operation
                      << " fd=" << channel->fd() << " failed: "
                      << strerror(errno) << std::endl;
        }
    }
}

bool EPollPoller::hasChannel(Channel* channel) const {
    auto it = channels_.find(channel->fd());
    return it != channels_.end() && it->second == channel;
}

// Static factory: creates the platform's default Poller.
Poller* Poller::newDefaultPoller(EventLoop* loop) {
    return new EPollPoller(loop);
}

void EPollPoller::fillActiveChannels(int numEvents, ChannelList* activeChannels) const {
    for (int i = 0; i < numEvents; ++i) {
        Channel* channel = static_cast<Channel*>(events_[i].data.ptr);
        channel->setRevents(events_[i].events);
        activeChannels->push_back(channel);
    }
}
