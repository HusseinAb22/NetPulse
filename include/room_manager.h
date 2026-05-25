#ifndef NETPULSE_ROOM_MANAGER_H
#define NETPULSE_ROOM_MANAGER_H
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "chat_room.h"
#include <string>
class RoomManager {
private:
    std::unordered_map<std::string, std::unique_ptr<ChatRoom>> rooms_;
    mutable std::mutex mutex_;
public:
    RoomManager() = default;

    ChatRoom& getOrCreate(const std::string& name);
    ChatRoom* find(const std::string& name) const;
    std::vector<std::string> listRooms() const;
    void leaveAll(int client_fd);
    void removeEmpty();

    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;
    RoomManager(RoomManager&&) = delete;
    RoomManager& operator=(RoomManager&&) = delete;
};

#endif  // NETPULSE_ROOM_MANAGER_H
