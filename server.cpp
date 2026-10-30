#include "dns.hpp"
#include "protocol.hpp"
#include "socket_raii.hpp"
#include "student_directory.hpp"
#include <arpa/inet.h>
#include <csignal>
#include <cerrno>
#include <poll.h>
#include <iostream>
#include <array>
#include <atomic>
#include <thread>
#include <chrono>
#include <netdb.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

constexpr uint16_t PORT = 1234;
// A signal handler must avoid allocations, logging, locks, and thread operations.
volatile std::sig_atomic_t stop_requested = 0;
extern "C" void request_stop(int) { stop_requested = 1; }
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
            const auto email = directory.lookup(argument);
            response = email ? "Email: " + *email + "\n" : "No such student ID\n";
        }
        if (!send_message(fd, response)) break;
    }
}

int main(int argc, char** argv) {
    std::signal(SIGPIPE, SIG_IGN);
    struct sigaction stop_action {};
    stop_action.sa_handler = request_stop;
    sigemptyset(&stop_action.sa_mask);
    stop_action.sa_flags = 0; // Do not restart interrupted syscalls.
    if (sigaction(SIGINT, &stop_action, nullptr) < 0 ||
        sigaction(SIGTERM, &stop_action, nullptr) < 0) {
        perror("sigaction");
        return 1;
    }
    const std::string file_path = argc > 1 ? argv[1] : "students_sample.txt";
    StudentDirectory directory;
    if (!directory.load(file_path)) {
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
    // At most 4 client-handler threads, no unbounded thread growth.
    constexpr std::size_t MAX_CLIENTS = 4;
    struct WorkerSlot {
        std::thread worker;
        std::atomic<bool> finished{true};
        int client_fd{-1}; // Used by main only, for shutdown before joining.
    };
    std::array<WorkerSlot, MAX_CLIENTS> slots;
    while (!stop_requested) {
        // A bounded poll timeout makes shutdown observable even with idle clients.
        pollfd listener_poll{listener.get(), POLLIN, 0};
        int ready = poll(&listener_poll, 1, 200);
        if (stop_requested) break;
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            break;
        }
        if (ready == 0) continue;
        if (!(listener_poll.revents & POLLIN)) {
            std::cerr << "Listener is no longer readable\n";
            break;
        }
        sockaddr_in peer{};
        socklen_t len = sizeof(peer);
        Socket client(accept(listener.get(), reinterpret_cast<sockaddr*>(&peer), &len));
        if (!client.valid()) {
            if (errno == EINTR) continue;
            perror("accept");
            break;
        }
        WorkerSlot* slot = nullptr;
        for (auto& candidate : slots) {
            if (candidate.finished.load()) {
                if (candidate.worker.joinable()) candidate.worker.join();
                slot = &candidate;
                break;
            }
        }
        if (!slot) {
            // Client expects a length-prefixed message. A full server rejects
            // extra connections rather than creating unlimited threads.
            send_message(client.get(), "Server busy: maximum 4 concurrent clients\n");
            continue;
        }
        slot->client_fd = client.get();
        slot->finished.store(false);
        // Transfer unique ownership of the socket to this worker.
        slot->worker = std::thread(
            [client = std::move(client), &directory, slot]() mutable {
                handle_client(client.get(), directory);
                slot->finished.store(true);
            });
    }
    // Stop the clients' blocking reads/writes without stealing ownership of
    // the descriptors from worker threads. Workers close them through RAII.
    for (auto& slot : slots) {
        if (slot.worker.joinable() && !slot.finished.load()) {
            ::shutdown(slot.client_fd, SHUT_RDWR);
        }
    }
    for (auto& slot : slots) {
        if (slot.worker.joinable()) slot.worker.join();
    }
    std::cout << "Graceful shutdown complete" << std::endl;
    return 0;
}
