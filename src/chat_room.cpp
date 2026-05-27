#include "../include/chat_room.h"
#include "../include/client_session.h"

#include <iostream>
#include <mutex>

ChatRoom::ChatRoom(std::string name):name_(std::move(name)){}

std::size_t ChatRoom::memberCount() const {
    std::shared_lock lock(mutex_);
    return members_.size();
}
const std::string& ChatRoom::name() const {
    return name_;
}

void ChatRoom::join(const std::shared_ptr<ClientSession>& client) {
    if (!client) {
        std::cerr<<"[ChatRoom::join()] Client is nullptr\n";
        return;
    }

    std::unique_lock lock(mutex_);

    for (const auto &cl : members_) {
        if (auto member = cl.lock(); member == client) {
            return;
        }

    }
    members_.push_back(client);
}

void ChatRoom::leave(const int client_fd) {
    std::unique_lock lock(mutex_);
    std::erase_if(members_, [client_fd](const std::weak_ptr<ClientSession>& weak) {
            const auto member = weak.lock();
        return !member || member->getFd() == client_fd;
    });
}

bool ChatRoom::contains(const int client_fd) const {
    std::shared_lock lock(mutex_);
    for (const auto &cl : members_) {
        if (const auto member = cl.lock()) {
            if (member->getFd() == client_fd) {
                return true;
            }
        }
    }
    return false;
}

void ChatRoom::broadcast(const Message &msg) const {
    if (msg.body.empty()) {
        return;
    }
    std::shared_lock lock(mutex_);
    for (const auto &cl : members_) {
        if (const auto member = cl.lock()) {
            if (member->getFd() != msg.sender_fd) {
                member->deliver(msg);
            }
        }
    }
}

void ChatRoom::addToHistory(const Message &msg) {
    std::unique_lock lock(mutex_);
    history_.push_back(msg);
    if (history_.size() > kHistoryLimit) {
        history_.pop_front();  // bounded: drop the oldest
    }
}

void ChatRoom::replayHistory(
    const std::shared_ptr<ClientSession> &client) const {
    if (!client) {
        return;
    }
    std::shared_lock lock(mutex_);
    for (const auto &msg : history_) {
        client->deliver(msg);
    }
}