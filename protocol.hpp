#pragma once
#include <arpa/inet.h>
#include <cerrno>
#include <cstdint>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

// Length-prefixed TCP messages: uint32 big-endian size + payload.
constexpr uint32_t MAX_MESSAGE_SIZE = 4096;

inline bool send_all(int fd, const void* data, size_t length) {
    const auto* ptr = static_cast<const char*>(data);
    while (length > 0) {
        ssize_t n = send(fd, ptr, length, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        ptr += n;
        length -= static_cast<size_t>(n);
    }
    return true;
}
inline bool receive_all(int fd, void* data, size_t length) {
    auto* ptr = static_cast<char*>(data);
    while (length > 0) {
        ssize_t n = recv(fd, ptr, length, 0);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return false;
        ptr += n;
        length -= static_cast<size_t>(n);
    }
    return true;
}
inline bool send_message(int fd, const std::string& message) {
    if (message.size() > MAX_MESSAGE_SIZE) return false;
    uint32_t size = htonl(static_cast<uint32_t>(message.size()));
    return send_all(fd, &size, sizeof(size)) && send_all(fd, message.data(), message.size());
}
inline bool receive_message(int fd, std::string& message) {
    uint32_t wire_size = 0;
    if (!receive_all(fd, &wire_size, sizeof(wire_size))) return false;
    uint32_t size = ntohl(wire_size);
    if (size > MAX_MESSAGE_SIZE) return false;
    message.resize(size);
    return size == 0 || receive_all(fd, message.data(), size);
}
