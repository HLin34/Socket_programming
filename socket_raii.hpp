#pragma once

#include <unistd.h>
#include <utility>

// Unique owner of one POSIX socket file descriptor.
class Socket {
public:
    explicit Socket(int fd = -1) noexcept : fd_(fd) {}
    ~Socket() { reset(); }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    Socket(Socket&& other) noexcept : fd_(std::exchange(other.fd_, -1)) {}

    Socket& operator=(Socket&& other) noexcept {
        if (this != &other) reset(std::exchange(other.fd_, -1));
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }

    void reset(int replacement = -1) noexcept {
        if (fd_ >= 0) ::close(fd_);
        fd_ = replacement;
    }

private:
    int fd_ = -1;
};
