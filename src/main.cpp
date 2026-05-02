#include <netinet/in.h>
#include <sys/socket.h>

#include <iostream>
#include <memory>
#include <thread>
#include <vector>

#include "../include/socket_wrapper.h"
#include "client_session.h"
#include "message.h"
#include "thread_safe_queue.h"
#include <csignal>

ThreadSafeQueue<Message> global_inbox;
std::vector<std::shared_ptr<ClientSession>> active_clients;
std::mutex clients_mutex;

void router_loop() {
    while (true) {
        auto msg = global_inbox.pop();
        std::lock_guard lock(clients_mutex);
        for (const auto &client : active_clients) {
            if (msg.sender_fd == client->getClientSock()->getFd()) {
            } else {
                client->deliver(msg);
            }
        }

        std::erase_if(active_clients, [](const std::shared_ptr<ClientSession>& client) {
            return !client->isAlive();
        });
    }
}

int main() {
    signal(SIGPIPE, SIG_IGN);

    std::jthread client_delivery_thread(router_loop);
    client_delivery_thread.detach();

    const SocketWrapper server_sock(socket(AF_INET, SOCK_STREAM, 0));
    if (!server_sock) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    constexpr int yes{1};
    setsockopt(server_sock.getFd(), SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in server_addr = {};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9000);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int bind_result = bind(server_sock.getFd(),
                           reinterpret_cast<struct sockaddr *>(&server_addr),
                           sizeof(server_addr));
    if (bind_result < 0) {
        std::cerr << "Error binding socket" << std::endl;
        return 1;
    }

    listen(server_sock.getFd(), 10);
    std::cout << "server listening on port " << ntohs(server_addr.sin_port)
              << std::endl;

    while (true) {
        SocketWrapper client_sock(
            accept(server_sock.getFd(), nullptr, nullptr));
        if (!client_sock) {
            std::cerr << "Error accepting client" << std::endl;
            continue;
        }

        auto session = std::make_shared<ClientSession>(std::move(client_sock),
                                                       global_inbox);

        {
            std::lock_guard lock(clients_mutex);
            active_clients.push_back(session);
        }
        std::jthread client_thread([session] {
            session->readLoop();
        });
        client_thread.detach();
    }
    return 0;
}