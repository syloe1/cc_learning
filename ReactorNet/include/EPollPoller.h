#pragma once

#include "Poller.h"
#include <sys/epoll.h>
#include <vector>

// Epoll-based I/O multiplexing implementation.
class EPollPoller : public Poller {
public:
    explicit EPollPoller(EventLoop* loop);
    ~EPollPoller() override;

    void updateChannel(Channel* channel) override;
    void removeChannel(Channel* channel) override;
    bool hasChannel(Channel* channel) const override;
    Timestamp poll(int timeoutMs, ChannelList* activeChannels) override;

private:
    static const int kInitEventListSize = 16;

    void update(int operation, Channel* channel);
    void fillActiveChannels(int numEvents, ChannelList* activeChannels) const;

    using EventList = std::vector<epoll_event>;

    int epollfd_;
    ChannelMap channels_;   // fd -> Channel*
    EventList events_;      // epoll_wait result buffer
};
