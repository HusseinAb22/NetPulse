

#ifndef NETPULSE_NICKNAME_REGISTRY_H
#define NETPULSE_NICKNAME_REGISTRY_H
#include <string>
#include <optional>
#include <mutex>
#include <unordered_map>
class NicknameRegistry {
public:
    bool tryClaim(int fd, const std::string& nick);   // false if taken
    void release(int fd);                              // on disconnect
    std::optional<std::string> nickFor(int fd) const;
    std::optional<int> fdFor(const std::string& nick) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, int> nick_to_fd_;
    std::unordered_map<int, std::string> fd_to_nick_;
};

#endif  // NETPULSE_NICKNAME_REGISTRY_H
