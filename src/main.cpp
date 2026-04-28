#include <iostream>
#include "../include/socket_wrapper.h"
#include <sys/socket.h>
#include <netinet/in.h>

int main() {
    const SocketWrapper server_sock(socket(AF_INET, SOCK_STREAM, 0));
    if (!server_sock) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }
    struct sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bind_result = bind(server_sock.getFd(), reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr));
    if (bind_result < 0) {
        std::cerr << "Error binding socket" << std::endl;
        return 1;
    }

    listen(server_sock.getFd(), 10);
    std::cout << "server listening on port " << ntohs(server_addr.sin_port) << std::endl;

    const SocketWrapper client_sock(accept(server_sock.getFd(),nullptr,nullptr));
    if (!client_sock) {
        std::cerr << "Error creating socket" << std::endl;
    }

    char buffer[1024];
    while (true) {
        const ssize_t bytes = recv(client_sock.getFd(), buffer, sizeof(buffer), 0);
        if (bytes <= 0) {
            std::cerr << "Error reading from socket" << std::endl;
            break;
        }
        send(client_sock.getFd(), buffer, bytes, 0);

    }
    return 0;
}