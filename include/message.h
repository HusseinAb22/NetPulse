#ifndef NETPULSE_MESSAGE_H
#define NETPULSE_MESSAGE_H
#include <string>
#include <chrono>

enum class MessageType {NICK, JOIN, MSG, DM, LIST, QUIT};

struct Message {
    MessageType type;

    std::string sender;
    std::string target;

    std::string body;
    std::chrono::system_clock::time_point timestamp;

    bool operator==(const Message& other) const noexcept {
        return type == other.type && sender == other.sender && target == other.target && body == other.body;
    }
};
#endif  // NETPULSE_MESSAGE_H
