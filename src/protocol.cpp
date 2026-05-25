#include "../include/protocol.h"

namespace protocol {
namespace {
/*
 * trims white lines to clean up the string view
 */
std::string_view trim(const std::string_view s) {
    constexpr std::string_view WHITESPACE = " \n\r\t\f\v";
    const size_t first = s.find_first_not_of(WHITESPACE);
    if (std::string_view::npos == first) {
        return "";
    }
    const size_t last = s.find_last_not_of(WHITESPACE);
    return s.substr(first, (last - first + 1));
}
/*
 * Peel off the first whitespace-delimited token.
 * Returns {token, remainder}. If no whitespace, remainder is empty.
 */
std::pair<std::string_view, std::string_view> splitFirst(std::string_view s) {
    size_t pos = s.find(' ');
    if (pos != std::string_view::npos) {
        return std::make_pair(s.substr(0, pos), s.substr(pos + 1));
    }
    return std::make_pair(s, "");
}
/*
 * parses the command to its MessageType enum equivalent
 */
std::optional<MessageType> parseCommand(const std::string_view word) {
    if (word == "NICK") {
        return MessageType::NICK;
    }
    if (word == "JOIN") {
        return MessageType::JOIN;
    }
    if (word == "MSG") {
        return MessageType::MSG;
    }
    if (word == "DM") {
        return MessageType::DM;
    }
    if (word == "LIST") {
        return MessageType::LIST;
    }
    if (word == "QUIT") {
        return MessageType::QUIT;
    }
    if (word == "OK") {
        return MessageType::OK;
    }
    if (word == "ERR") {
        return MessageType::ERR;
    }
    if (word == "BROADCAST") {
        return MessageType::BROADCAST;
    }
    if (word == "PRIVMSG") {
        return MessageType::PRIVMSG;
    }
    if (word == "ROOMLIST") {
        return MessageType::ROOMLIST;
    }

    return std::nullopt;
}
}  // namespace

std::optional<Message> parse(std::string_view line) {
    line = trim(line);
    if (line.empty()) {
        return std::nullopt;
    }

    auto [cmd_word, rest] = splitFirst(line);
    auto cmd = parseCommand(cmd_word);
    if (!cmd) {
        return std::nullopt;
    }

    Message msg;
    msg.type = *cmd;
    msg.timestamp = std::chrono::system_clock::now();

    switch (msg.type) {
        case MessageType::NICK:
            // expects: NICK <nickname>
            if (rest.empty()) {
                return std::nullopt;
            }
            msg.body = std::string(rest);
            return msg;
        case MessageType::JOIN:
            // expects: JOIN <room>
            if (rest.empty()) {
                return std::nullopt;
            }
            msg.target = std::string(rest);
            return msg;
        case MessageType::MSG:
        case MessageType::DM: {
            // expects: MSG <chanel> <body...>
            auto [target, body] = splitFirst(rest);
            if (target.empty() || body.empty()) {
                return std::nullopt;
            }
            msg.target = std::string(target);
            msg.body = std::string(trim(body));
            return msg;
        }
        case MessageType::LIST:
        case MessageType::QUIT:
            if (!rest.empty()) {
                return std::nullopt;
            }
            return msg;
        case MessageType::OK:
        case MessageType::ERR:
            if (rest.empty()) {
                return std::nullopt;
            }
            msg.body = std::string(rest);
            return msg;
        case MessageType::BROADCAST:
        case MessageType::PRIVMSG: {
            auto [target, body] = splitFirst(rest);
            if (target.empty() || body.empty()) {
                return std::nullopt;
            }
            auto [sender, msgBody] = splitFirst(body);
            if (sender.empty() || msgBody.empty()) {
                return std::nullopt;
            }
            msg.target = std::string(target);
            msg.sender = std::string(sender);
            msg.body = std::string(trim(msgBody));
            return msg;
        }
        case MessageType::ROOMLIST:
            if (rest.empty()) {
                return std::nullopt;
            }
            msg.body = std::string(rest);
            return msg;
    }
    return std::nullopt;
}

std::string serialize(const Message &msg){
    switch (msg.type) {
        case MessageType::OK:
            return "OK " + msg.body + "\n";
        case MessageType::ERR:
            return "ERR " + msg.body + "\n";
        case MessageType::BROADCAST:
            return "BROADCAST " + msg.target + " " + msg.sender + " " + msg.body + "\n";
        case MessageType::PRIVMSG:
            return "PRIVMSG " + msg.target + " " + msg.sender + " " + msg.body + "\n";
        case MessageType::ROOMLIST:
            return "ROOMLIST " + msg.body + "\n";

            // C→S types — server doesn't send these added them for prototyping
        case MessageType::NICK:
            return "NICK " + msg.body + "\n";
        case MessageType::JOIN:
            return "JOIN " + msg.target + "\n";
        case MessageType::MSG:
            return "MSG "  + msg.target + " " + msg.body + "\n";
        case MessageType::DM:
            return "DM "   + msg.target + " " + msg.body + "\n";
        case MessageType::LIST:
            return "LIST\n";
        case MessageType::QUIT:
            return "QUIT\n";
    }
    return "";
}
}  // namespace protocol
