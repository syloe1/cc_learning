#pragma once

#include "noncopyable.h"
#include "Timer.h"
#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

// Abstract base class for I/O multiplexing.
// Concrete implementation: EPollPoller.
class Poller : noncopyable {
public:
    using ChannelList = std::vector<Channel*>;
    using ChannelMap = std::unordered_map<int, Channel*>;

    explicit Poller(EventLoop* loop) : ownerLoop_(loop) {}
    virtual ~Poller() = default;

    // Must be called before poll(). Updates or adds the given channel.
    virtual void updateChannel(Channel* channel) = 0;

    // Remove a channel from the interest list.
    virtual void removeChannel(Channel* channel) = 0;

    // Wait for I/O events. Fills activeChannels with ready channels.
    // Returns the timestamp of the poll return.
    virtual Timestamp poll(int timeoutMs, ChannelList* activeChannels) = 0;

    // Check if a channel is registered. Default implementation returns false.
    virtual bool hasChannel(Channel* /*channel*/) const { return false; }

    // Factory method: creates the default poller for this platform.
    static Poller* newDefaultPoller(EventLoop* loop);

protected:
    EventLoop* const ownerLoop_;
};
