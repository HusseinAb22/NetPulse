#include "../include/dispatcher.h"
#include <sys/socket.h>
#include <iostream>
#include <utility>




namespace {
constexpr std::size_t kMaxNickLen = 32;

bool validNick(const std::string &nick) {
    if (nick.empty() || nick.size() > kMaxNickLen) {
        return false;
    }
    // A nick is a single whitespace-free token (the wire protocol is
    // space-delimited, so a space would corrupt framing on the way back out).
    return nick.find_first_of(" \t\r\n") == std::string::npos;
}
}  // namespace

Dispatcher::Dispatcher(ThreadSafeQueue<Message> &inbox) : inbox_(inbox) {}

void Dispatcher::registerSession(std::shared_ptr<ClientSession> session) {
    const int fd = session->getFd();
    std::lock_guard lock(connections_mutex_);
    connections_[fd] = std::move(session);
}

void Dispatcher::shutdownConnections() {
    std::lock_guard lock(connections_mutex_);
    for (const auto &[fd, session] : connections_) {
        ::shutdown(fd, SHUT_RDWR);  // unblocks the reader's recv() on this fd
    }
}

void Dispatcher::run() {
    while (true) {
        const Message msg = inbox_.pop();
        handle(msg);
    }
}

void Dispatcher::handle(const Message &msg) {
    switch (msg.type) {
        case MessageType::NICK:
            handleNick(msg);
            break;
        case MessageType::JOIN:
            if (requireNick(msg.sender_fd)) {
                handleJoin(msg);
            }
            break;
        case MessageType::MSG:
            if (requireNick(msg.sender_fd)) {
                handleMsg(msg);
            }
            break;
        case MessageType::DM:
            if (requireNick(msg.sender_fd)) {
                handleDm(msg);
            }
            break;
        case MessageType::LIST:
            if (requireNick(msg.sender_fd)) {
                handleList(msg.sender_fd);
            }
            break;
        case MessageType::QUIT:
            handleQuit(msg.sender_fd);
            break;

        // Server->client types must never arrive from a client. Enumerated
        // explicitly (not via default:) so -Wswitch-enum forces us to revisit
        // this switch if a new MessageType is ever added.
        case MessageType::OK:
        case MessageType::ERR:
        case MessageType::BROADCAST:
        case MessageType::PRIVMSG:
        case MessageType::ROOMLIST:
            sendError(msg.sender_fd, "protocol violation");
            break;
    }
}

void Dispatcher::handleNick(const Message &msg) {
    const std::string &nick = msg.body;
    if (!validNick(nick)) {
        sendError(msg.sender_fd, "invalid nickname");
        return;
    }
    if (registry_.tryClaim(msg.sender_fd, nick)) {
        sendOk(msg.sender_fd, "nick " + nick);
    } else {
        sendError(msg.sender_fd, "nickname taken");
    }
}

void Dispatcher::handleJoin(const Message &msg) {
    const std::shared_ptr<ClientSession> session = lookup(msg.sender_fd);
    if (!session) {
        return;
    }

    ChatRoom &room = rooms_.getOrCreate(msg.target);
    room.join(session);
    sendOk(msg.sender_fd, "joined " + msg.target);

    // Announce the arrival to the rest of the room (the joiner is skipped via
    // sender_fd; they already got the OK above).
    const auto nick = registry_.nickFor(msg.sender_fd);
    Message note;
    note.type = MessageType::BROADCAST;
    note.target = msg.target;
    note.sender = "server";
    note.body = (nick ? *nick : std::string("someone")) + " joined";
    note.sender_fd = msg.sender_fd;
    room.broadcast(note);
}

void Dispatcher::handleMsg(const Message &msg) {
    ChatRoom *room = rooms_.find(msg.target);
    if (room == nullptr) {
        sendError(msg.sender_fd, "no such room " + msg.target);
        return;
    }
    if (!room->contains(msg.sender_fd)) {
        sendError(msg.sender_fd, "not in room " + msg.target);
        return;
    }

    const auto nick = registry_.nickFor(msg.sender_fd);
    Message out;
    out.type = MessageType::BROADCAST;
    out.target = msg.target;
    out.sender = nick ? *nick : std::string("?");
    out.body = msg.body;
    out.sender_fd = msg.sender_fd;  // room.broadcast skips the sender
    room->broadcast(out);
}

void Dispatcher::handleDm(const Message &msg) {
    const auto target_fd = registry_.fdFor(msg.target);
    if (!target_fd) {
        sendError(msg.sender_fd, "user not found " + msg.target);
        return;
    }
    const std::shared_ptr<ClientSession> recipient = lookup(*target_fd);
    if (!recipient) {
        sendError(msg.sender_fd, "user not found " + msg.target);
        return;
    }

    const auto nick = registry_.nickFor(msg.sender_fd);
    Message out;
    out.type = MessageType::PRIVMSG;
    out.target = msg.target;  // recipient nick
    out.sender = nick ? *nick : std::string("?");
    out.body = msg.body;
    recipient->deliver(out);
}

void Dispatcher::handleList(int fd) {
    const std::vector<std::string> rooms = rooms_.listRooms();
    std::string body;
    for (std::size_t i = 0; i < rooms.size(); ++i) {
        if (i != 0) {
            body += ' ';
        }
        body += rooms[i];
    }
    Message out;
    out.type = MessageType::ROOMLIST;
    out.body = body;
    if (const std::shared_ptr<ClientSession> session = lookup(fd)) {
        session->deliver(out);
    }
}

void Dispatcher::handleQuit(int fd) {
    // Every step is idempotent so a duplicate QUIT (e.g. a typed QUIT followed
    // by the reader's disconnect signal) is harmless.
    registry_.release(fd);
    rooms_.leaveAll(fd);

    // Drop from the connection table. Move the shared_ptr OUT under the lock
    // and let it die after the lock is released: destroying a ClientSession
    // joins its writer thread, and we must never block on a join while holding
    // connections_mutex_.
    std::shared_ptr<ClientSession> doomed;
    {
        std::lock_guard lock(connections_mutex_);
        const auto it = connections_.find(fd);
        if (it != connections_.end()) {
            doomed = std::move(it->second);
            connections_.erase(it);
        }
    }
    // doomed's destructor (if this was the last ref) runs here, lock-free.
}

bool Dispatcher::requireNick(int fd) {
    if (registry_.nickFor(fd)) {
        return true;
    }
    sendError(fd, "set a nickname first (NICK <name>)");
    return false;
}

void Dispatcher::sendOk(int fd, const std::string &context) {
    if (const std::shared_ptr<ClientSession> session = lookup(fd)) {
        Message m;
        m.type = MessageType::OK;
        m.body = context;
        session->deliver(m);
    }
}

void Dispatcher::sendError(int fd, const std::string &reason) {
    if (const std::shared_ptr<ClientSession> session = lookup(fd)) {
        Message m;
        m.type = MessageType::ERR;
        m.body = reason;
        session->deliver(m);
    }
}

std::shared_ptr<ClientSession> Dispatcher::lookup(int fd) {
    std::lock_guard lock(connections_mutex_);
    const auto it = connections_.find(fd);
    return it == connections_.end() ? nullptr : it->second;
}