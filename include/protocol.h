#ifndef NETPULSE_PROTOCOL_H
#define NETPULSE_PROTOCOL_H
#include <optional>
#include <string>
#include "message.h"
#include <string_view>

namespace  protocol {
    std::optional<Message> parse(std::string_view line);
    std::string serialize(const Message& msg);
}

#endif  // NETPULSE_PROTOCOL_H
