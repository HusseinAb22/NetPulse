#include "nickname_registry.h"

bool NicknameRegistry::tryClaim(int fd, const std::string& nick) {
    std::lock_guard lock(mutex_);

    auto checkNick = nick_to_fd_.find(nick);
    auto checkFd = fd_to_nick_.find(fd);

    if (checkNick != nick_to_fd_.end() && checkNick->second != fd) {
        return false;
    }

    if (checkNick != nick_to_fd_.end() && checkNick->second == fd) {
        return true;
    }

    if (checkFd != fd_to_nick_.end()) {
        nick_to_fd_.erase(checkFd->second);
        checkFd->second = nick;
    } else {
        fd_to_nick_.emplace(fd, nick);
    }

    nick_to_fd_.emplace(nick, fd);
    return true;
}

void NicknameRegistry::release(int fd) {
    std::lock_guard lock(mutex_);

    auto iter = fd_to_nick_.find(fd);
    if (iter == fd_to_nick_.end()) {
        return;
    }

    nick_to_fd_.erase(iter->second);
    fd_to_nick_.erase(iter);
}

std::optional<std::string> NicknameRegistry::nickFor(int fd) const {
    std::lock_guard lock(mutex_);

    auto iter = fd_to_nick_.find(fd);
    if (iter != fd_to_nick_.end()) {
        return iter->second;
    }
    return std::nullopt;
}

std::optional<int> NicknameRegistry::fdFor(const std::string& nick) const {
    std::lock_guard lock(mutex_);

    auto iter = nick_to_fd_.find(nick);
    if (iter != nick_to_fd_.end()) {
        return iter->second;
    }
    return std::nullopt;
}