#ifndef NETPULSE_CHAT_ROOM_H
#define NETPULSE_CHAT_ROOM_H
#include <memory>
#include <vector>
#include <shared_mutex>
#include <string>
#include "message.h"
#include <deque>
class ClientSession;
class ChatRoom {
private:
    static constexpr std::size_t kHistoryLimit = 50;
    std::string name_;
    std::vector<std::weak_ptr<ClientSession>> members_;
    std::deque<Message> history_;
    mutable std::shared_mutex mutex_;
public:
    explicit ChatRoom(std::string name);

    void broadcast(const Message &msg) const;
    void join(const std::shared_ptr<ClientSession>& client);
    void leave(int client_fd);
    bool contains(int client_fd) const;
    std::size_t memberCount() const;
    void addToHistory(const Message& msg);
    void replayHistory(const std::shared_ptr<ClientSession>& client) const;

    const std::string& name() const;

    ChatRoom(const ChatRoom&) = delete;
    ChatRoom& operator=(const ChatRoom&) = delete;
    ChatRoom(ChatRoom&&) = delete;
    ChatRoom& operator=(ChatRoom&&) = delete;

};

#endif  // NETPULSE_CHAT_ROOM_H
