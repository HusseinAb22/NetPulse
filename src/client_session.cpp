#include "../include/client_session.h"

#include <sys/socket.h>

#include <cstring>

ClientSession::ClientSession(SocketWrapper client_sock,
                             ThreadSafeQueue<Message> &server_inbox)
    : client_sock_(std::move(client_sock)),
      server_inbox_(server_inbox),
      writer_thread_([this]() { writeLoop(); }) {
    std::cout << "new Client Session created!" << std::endl;
}

void ClientSession::deliver(const Message &msg) {
    this->outbox_.push(msg);
}

void ClientSession::writeLoop() {
    while (true) {
        auto msg = this->outbox_.pop();

        if (msg.type == MessageType::QUIT) {
            break;
        }

        ssize_t bytes_sent =
            send(client_sock_.getFd(), msg.body.c_str(), msg.body.length(), 0);
        if (bytes_sent < 0) {
            std::cerr
                << "ClientSession: Client send error or client disconnected "
                << std::endl;
            break;
        }
    }
}

void ClientSession::readLoop() {
    char buffer[1024];
    while (true) {
        const ssize_t bytes =
            recv(client_sock_.getFd(), buffer, sizeof(buffer), 0);
        if (bytes == 0) {
            std::cout << "Client: " << client_sock_.getFd() << " disconnected!"
                      << std::endl;
            break;
        }
        if (bytes < 0) {
            std::cerr << "Error reading from socket" << std::endl;
            break;
        }
        framer_.feed({buffer, static_cast<size_t>(bytes)},
                     [this](std::string_view line) {
                         auto parsed = protocol::parse(line);
                         if (!parsed) {
                             std::cerr
                                 << "ClientSession: malformed line ignored: \""
                                 << line << "\"\n";
                             return;
                         }
                         parsed->sender_fd = client_sock_.getFd();
                         server_inbox_.push(*parsed);
                     });

        if (framer_.size() > kMaxBufferBytes) {
            std::cerr << "ClientSession: buffer cap exceeded; dropping client "
                      << client_sock_.getFd() << "\n";
            break;
        }
    }
    alive_ = false;
    outbox_.push({.type = MessageType::QUIT, .sender_fd = -1});
}

const SocketWrapper *ClientSession::getClientSock() const {
    return &this->client_sock_;
}

bool ClientSession::isAlive() const {
    return this->alive_.load();
}

int ClientSession::getFd() const { return client_sock_.getFd(); }
