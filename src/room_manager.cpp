#include "room_manager.h"

ChatRoom& RoomManager::getOrCreate(const std::string& name) {
    std::lock_guard lock(mutex_);
    auto& slot = rooms_[name];
    if (!slot) {slot = std::make_unique<ChatRoom>(name);}
    return *slot;
}

ChatRoom* RoomManager::find(const std::string& name) const {
    std::lock_guard lock(mutex_);
    const auto iter = rooms_.find(name);
    if (iter == rooms_.end()) {
        return nullptr;
    }
    return iter->second.get();
}
std::vector<std::string> RoomManager::listRooms() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> rooms;
    rooms.reserve(rooms_.size());
    for (const auto& [roomname, room] : rooms_) {
        rooms.push_back(roomname);
    }
    return rooms;
}

void RoomManager::leaveAll(const int client_fd) {
    std::lock_guard lock(mutex_);
    for (auto& [name, room] : rooms_) {
        room->leave(client_fd);
    }
}

void RoomManager::removeEmpty() {
    std::lock_guard lock(mutex_);
    std::erase_if(rooms_, [](const auto& keyval) {
        return keyval.second->memberCount() == 0;
    });
}