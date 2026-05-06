#ifndef NETPULSE_PROTOCOL_H
#define NETPULSE_PROTOCOL_H
#include <optional>
#include <string>
#include <string_view>

#include "message.h"

namespace protocol {
std::optional<Message> parse(std::string_view line);
std::string serialize(const Message &msg);
}  // namespace protocol

#endif  // NETPULSE_PROTOCOL_H
