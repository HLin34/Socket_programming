#include "dns.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>

std::string lookup_dns(const std::string& hostname) {
    if (hostname.empty() || hostname.size() > 253 ||
        hostname.find_first_of(" \r\n\t") != std::string::npos) {
        return "Invalid hostname";
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    const int rc = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    if (rc != 0) {
        return std::string("DNS lookup failed: ") + gai_strerror(rc);
    }

    char addr[INET_ADDRSTRLEN]{};
    const bool ok = result != nullptr &&
        inet_ntop(AF_INET,
                  &reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr,
                  addr, sizeof(addr)) != nullptr;
    freeaddrinfo(result);
    return ok ? std::string("IP address: ") + addr : "DNS address conversion failed";
}
