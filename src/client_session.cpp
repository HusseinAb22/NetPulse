#include "../include/client_session.h"

#include <sys/socket.h>

#include <cstring>
#include <string>
#include <optional>
#include "../include/protocol.h"

// Linux raises SIGPIPE on write to a half-closed socket; MSG_NOSIGNAL
// suppresses it. macOS lacks the flag (it uses SO_NOSIGPIPE + the global
// SIGPIPE ignore in main), so fall back to 0 there.
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

ClientSession::ClientSession(SocketWrapper client_sock,
                             ThreadSafeQueue<Message> &server_inbox)
    : client_sock_(std::move(client_sock)),
      server_inbox_(server_inbox),
      writer_thread_([this](std::stop_token st) { writeLoop(st); }) {
    std::cout << "new Client Session created!" << std::endl;
}

void ClientSession::deliver(const Message &msg) {
    this->outbox_.push(msg);
}

void ClientSession::writeLoop(std::stop_token stop_token) {
    while (true) {
        // Wakes on a delivered message or on the jthread's stop request.
        const std::optional<Message> msg = outbox_.pop(stop_token);
        if (!msg) {
            break;  // stop requested and outbox drained -> shut the writer down
        }

        const std::string wire = protocol::serialize(*msg);
        const std::size_t total = wire.size();
        std::size_t sent = 0;
        bool failed = false;

        // send() may transmit fewer bytes than requested; loop until done.
        while (sent < total) {
            const ssize_t n = send(client_sock_.getFd(), wire.data() + sent,
                                   total - sent, MSG_NOSIGNAL);
            if (n <= 0) {
                failed = true;
                break;
            }
            sent += static_cast<std::size_t>(n);
        }

        if (failed) {
            std::cerr << "ClientSession: send error on fd "
                      << client_sock_.getFd() << std::endl;
            alive_ = false;
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
    // Tell the dispatcher this fd is gone so it runs the cleanup path
    // (release nick, leave rooms, drop from the connection table). The
    // writer thread is stopped separately by the destructor's poison pill.
    server_inbox_.push(
        {.type = MessageType::QUIT, .sender_fd = client_sock_.getFd()});
}

const SocketWrapper *ClientSession::getClientSock() const {
    return &this->client_sock_;
}

bool ClientSession::isAlive() const {
    return this->alive_.load();
}

int ClientSession::getFd() const { return client_sock_.getFd(); }

ClientSession::~ClientSession() noexcept {
    alive_ = false;
}