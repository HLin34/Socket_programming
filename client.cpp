#include "protocol.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include "socket_raii.hpp"

int main() {
    Socket fd(socket(AF_INET, SOCK_STREAM, 0));
    if (!fd.valid()){
        perror("socket");
        return 1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(1234);
    if (inet_pton(AF_INET, "127.0.0.1", &address.sin_addr) != 1 ||
        connect(fd.get(), reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
        perror("connect");
        return 1;
    }
    while (true) {
        std::string menu, option;
        if (!receive_message(fd.get(), menu)) { std::cerr << "Server disconnected\n"; break; }
        std::cout << menu << std::flush;
        if (!std::getline(std::cin, option) || !send_message(fd.get(), option)) break;
        if (option == "3") break;
        std::string reply;
        if (!receive_message(fd.get(), reply)) { std::cerr << "Server disconnected\n"; break; }
        std::cout << reply << std::flush;
        if (option == "1" || option == "2") {
            std::string argument;
            if (!std::getline(std::cin, argument) || !send_message(fd.get(), argument)) break;
            if (!receive_message(fd.get(), reply)) { std::cerr << "Server disconnected\n"; break; }
            std::cout << reply << std::flush;
        }
    }
    return 0;
}
