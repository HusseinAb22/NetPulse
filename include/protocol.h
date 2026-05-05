//
// Created by Hussein Abbas on 05/05/2026.
//

#ifndef NETPULSE_PROTOCOL_H
#define NETPULSE_PROTOCOL_H
#include <optional>
#include <string>
#include "message.h"

namespace  Protocol {
    std::optional<Message> parse(std::string_view line);
};

#endif  // NETPULSE_PROTOCOL_H
