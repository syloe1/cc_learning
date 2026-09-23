#pragma once

// Base class that disables copy construction and copy assignment.
// Inherit from this class to make derived classes non-copyable.
class noncopyable {
public:
    noncopyable() = default;
    ~noncopyable() = default;

    noncopyable(const noncopyable&) = delete;
    noncopyable& operator=(const noncopyable&) = delete;

    // Allow move construction and move assignment
    noncopyable(noncopyable&&) = default;
    noncopyable& operator=(noncopyable&&) = default;
};
