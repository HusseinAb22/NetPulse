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
};
#endif  // NETPULSE_MESSAGE_H
