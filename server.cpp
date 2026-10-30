#include "protocol.hpp"
#include "socket_raii.hpp"
#include <arpa/inet.h>
#include <csignal>
#include <fstream>
#include <iostream>
#include <netdb.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unordered_map>
#include <unistd.h>

constexpr uint16_t PORT = 1234;
using StudentDirectory = std::unordered_map<std::string, std::string>;

bool load_students(const std::string& path, StudentDirectory& directory) {
    std::ifstream file(path);
    if (!file) return false;
    std::string id, email;
    while (file >> id >> email) directory[id] = email;
    return file.eof();
}

std::string lookup_dns(const std::string& hostname) {
    if (hostname.empty() || hostname.size() > 253 || hostname.find_first_of(" \r\n\t") != std::string::npos)
        return "Invalid hostname";
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* result = nullptr;
    int rc = getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    if (rc != 0) return std::string("DNS lookup failed: ") + gai_strerror(rc);
    char addr[INET_ADDRSTRLEN]{};
    bool ok = inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in*>(result->ai_addr)->sin_addr, addr, sizeof(addr)) != nullptr;
    freeaddrinfo(result);
    return ok ? std::string("IP address: ") + addr : "DNS address conversion failed";
}

void handle_client(int fd, const StudentDirectory& directory) {
    while (send_message(fd, "What's your requirement? 1.DNS 2.QUERY 3.QUIT : ")) {
        std::string option;
        if (!receive_message(fd, option)) break;
        if (option == "3") break;
        if (option != "1" && option != "2") {
            if (!send_message(fd, "Invalid requirement! (choose 1, 2 or 3)\n")) break;
            continue;
        }
        if (!send_message(fd, option == "1" ? "Input hostname: " : "Input student ID: ")) break;
        std::string argument;
        if (!receive_message(fd, argument)) break;
        std::string response;
        if (option == "1") response = lookup_dns(argument) + "\n";
        else {
            auto it = directory.find(argument);
            response = it == directory.end() ? "No such student ID\n" : "Email: " + it->second + "\n";
        }
        if (!send_message(fd, response)) break;
    }
}

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    const std::string file_path = argc > 1 ? argv[1] : "students_sample.txt";
    StudentDirectory directory;
    if (!load_students(file_path, directory)) {
        std::cerr << "Could not load valid student data file: " << file_path << '\n';
        return 1;
    }
    Socket listener(socket(AF_INET, SOCK_STREAM, 0));
    if (!listener.valid()) { perror("socket"); return 1; }
    int reuse = 1;
    if (setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt"); return 1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    // Bind only to loopback to keep demo service local.
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(listener.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 || listen(listener.get(), 10) < 0) {
        perror("bind/listen"); return 1;
    }
    std::cout << "Listening on 127.0.0.1:" << PORT << std::endl;
    while (true) {
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        Socket client(accept(listener.get(), reinterpret_cast<sockaddr*>(&peer), &len));
        if (!client.valid()) { if (errno == EINTR) continue; perror("accept"); break; }
        handle_client(client.get(), directory);
    }
    return 0;
}
