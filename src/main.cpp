#include <netinet/in.h>
#include <sys/socket.h>

#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

#include "../include/socket_wrapper.h"
#include "client_session.h"
#include "dispatcher.h"
#include "message.h"
#include "thread_safe_queue.h"

int main() {
    // Writing to a half-closed socket would otherwise kill the process.
    signal(SIGPIPE, SIG_IGN);

    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);

    // The single dispatcher thread owns all room/registry/connection state.
    std::jthread dispatch_thread([&dispatcher] { dispatcher.run(); });
    dispatch_thread.detach();

    const SocketWrapper server_sock(socket(AF_INET, SOCK_STREAM, 0));
    if (!server_sock) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    constexpr int yes{1};
    setsockopt(server_sock.getFd(), SOL_SOCKET, SO_REUSEADDR, &yes,
               sizeof(yes));

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

        auto session =
            std::make_shared<ClientSession>(std::move(client_sock), inbox);

        // Register BEFORE the reader starts, so the session is findable by the
        // time any of its messages reach the dispatcher.
        dispatcher.registerSession(session);

        // The reader thread holds a strong ref, keeping the session alive while
        // recv() runs. It releases that ref when it returns on disconnect.
        std::jthread reader([session] { session->readLoop(); });
        reader.detach();
    }
    return 0;
}