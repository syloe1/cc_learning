#pragma once

#include <chrono>
#include <functional>

// Represents a timestamp as microseconds since epoch.
// Used for timer expiration comparison.
class Timestamp {
public:
    using Clock = std::chrono::system_clock;
    using Microseconds = std::chrono::microseconds;

    Timestamp() : microSecondsSinceEpoch_(0) {}

    explicit Timestamp(int64_t microSecondsSinceEpoch)
        : microSecondsSinceEpoch_(microSecondsSinceEpoch) {}

    static Timestamp now() {
        auto now = Clock::now().time_since_epoch();
        return Timestamp(std::chrono::duration_cast<Microseconds>(now).count());
    }

    int64_t microSecondsSinceEpoch() const { return microSecondsSinceEpoch_; }

    bool valid() const { return microSecondsSinceEpoch_ > 0; }

    Timestamp& operator+=(double seconds) {
        int64_t delta = static_cast<int64_t>(seconds * 1000000);
        microSecondsSinceEpoch_ += delta;
        return *this;
    }

    bool operator<(const Timestamp& rhs) const {
        return microSecondsSinceEpoch_ < rhs.microSecondsSinceEpoch_;
    }

    bool operator<=(const Timestamp& rhs) const {
        return microSecondsSinceEpoch_ <= rhs.microSecondsSinceEpoch_;
    }

    bool operator>(const Timestamp& rhs) const {
        return microSecondsSinceEpoch_ > rhs.microSecondsSinceEpoch_;
    }

    // Returns the difference in seconds.
    double operator-(const Timestamp& rhs) const {
        int64_t diff = microSecondsSinceEpoch_ - rhs.microSecondsSinceEpoch_;
        return static_cast<double>(diff) / 1000000.0;
    }

private:
    int64_t microSecondsSinceEpoch_;
};

// A timer that fires a callback at a given expiration time.
// Supports one-shot (interval == 0) and repeating timers.
class Timer {
public:
    using TimerCallback = std::function<void()>;

    Timer(TimerCallback cb, Timestamp expiration, double interval = 0.0)
        : callback_(std::move(cb)),
          expiration_(expiration),
          interval_(interval),
          repeat_(interval > 0.0),
          sequence_(++s_numCreated_) {}

    void run() const { if (callback_) callback_(); }

    Timestamp expiration() const { return expiration_; }
    bool repeat() const { return repeat_; }
    int64_t sequence() const { return sequence_; }

    // Restart a repeating timer: advance expiration by interval_.
    void restart(Timestamp now);

    // For ordering in the timer heap (min-heap by expiration).
    // Note: inverted for std::greater / heap ordering.
    bool operator<(const Timer& rhs) const {
        return expiration_ > rhs.expiration_;  // Inverted for min-heap convenience
    }

    // For comparing Timer* in heap operations.
    struct TimerPtrComparator {
        bool operator()(const Timer* a, const Timer* b) const {
            return a->expiration() > b->expiration();  // Min-heap: earlier expires first
        }
    };

private:
    const TimerCallback callback_;
    Timestamp expiration_;
    const double interval_;
    const bool repeat_;
    const int64_t sequence_;

    static int64_t s_numCreated_;
};
