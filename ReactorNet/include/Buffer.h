#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <sys/types.h>

// Non-contiguous read/write buffer designed for TCP stream processing.
// Uses prependable + readable + writable layout:
//   [prependable (8 bytes)] [readable bytes] [writable bytes]
//
// readIndex_ points to start of readable data.
// writeIndex_ points to end of readable data (start of writable area).
class Buffer {
public:
    static const size_t kCheapPrepend = 8;   // Reserved prependable space
    static const size_t kInitialSize = 1024;  // Initial buffer capacity

    explicit Buffer(size_t initialSize = kInitialSize);

    // --- Read operations ---

    // Number of bytes available for reading.
    size_t readableBytes() const { return writeIndex_ - readIndex_; }

    // Number of bytes available for writing in the current buffer.
    size_t writableBytes() const { return buffer_.size() - writeIndex_; }

    // Prepended space before readable data.
    size_t prependableBytes() const { return readIndex_; }

    // Returns a pointer to the start of readable data.
    const char* peek() const { return begin() + readIndex_; }
    char* peek() { return begin() + readIndex_; }

    // Retrieve len bytes from the buffer (advance readIndex_).
    void retrieve(size_t len);

    // Retrieve all bytes (reset indices to initial state).
    void retrieveAll();

    // Retrieve len bytes as a string, advancing readIndex_.
    std::string retrieveAsString(size_t len);

    // Retrieve all readable bytes as a string.
    std::string retrieveAllAsString();

    // Read data from fd into buffer. Uses readv with a stack buffer
    // to minimize system calls and avoid premature buffer resizing.
    // Returns the number of bytes read, or -1 on error (errno set via savedErrno).
    ssize_t readFd(int fd, int* savedErrno);

    // Write readable data to fd. Returns bytes written, or -1 on error.
    ssize_t writeFd(int fd, int* savedErrno);

    // --- Write operations ---

    // Append data to the buffer, resizing if necessary.
    void append(const char* data, size_t len);

    void append(const std::string& str) { append(str.data(), str.size()); }

    // Ensure there is at least len bytes of writable space.
    void ensureWritableBytes(size_t len);

    // --- Utility ---

    // Search for CRLF in readable data. Returns pointer to '\r' or nullptr.
    const char* findCRLF() const;

    // Returns a const reference to the underlying vector (for debugging).
    const std::vector<char>& data() const { return buffer_; }

private:
    char* begin() { return buffer_.data(); }
    const char* begin() const { return buffer_.data(); }

    void makeSpace(size_t len);

    std::vector<char> buffer_;
    size_t readIndex_;
    size_t writeIndex_;
};
