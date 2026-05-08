#ifndef NETPULSE_ROOM_MANAGER_H
#define NETPULSE_ROOM_MANAGER_H
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "chat_room.h"

class RoomManager {
private:
    std::unordered_map<std::string, std::unique_ptr<ChatRoom>> rooms_;
    mutable std::mutex mutex_;
public:
    RoomManager() = default;

    ChatRoom& getOrCreate(std::string& name);
    ChatRoom* find(const std::string& name);
    std::vector<std::string> listRooms();
    void removeEmpty();

    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;
    RoomManager(RoomManager&&) = delete;
    RoomManager& operator=(RoomManager&&) = delete;
};

#endif  // NETPULSE_ROOM_MANAGER_H
