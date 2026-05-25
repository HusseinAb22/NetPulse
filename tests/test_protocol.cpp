#include <gtest/gtest.h>

#include "../include/message.h"
#include "../include/protocol.h"

// ==========================================
// 1. Happy Path Tests
// ==========================================

TEST(ParseProtocolTest, NickPutsNicknameInBody) {
    const auto msg = protocol::parse("NICK alice");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::NICK);
    EXPECT_EQ(msg->body, "alice");
}

TEST(ParseProtocolTest, JoinPutsRoomInTarget) {
    const auto msg = protocol::parse("JOIN #general");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::JOIN);
    EXPECT_EQ(msg->target, "#general");
}

TEST(ParseProtocolTest, MsgCapturesTargetAndMultipleWordBody) {
    const auto msg = protocol::parse("MSG #general Hello world from me!");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::MSG);
    EXPECT_EQ(msg->target, "#general");
    // Ensure the body captured all the subsequent words!
    EXPECT_EQ(msg->body, "Hello world from me!");
}

TEST(ParseProtocolTest, DmCapturesTargetAndMultipleWordBody) {
    const auto msg = protocol::parse("DM bob hi there");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::DM);
    EXPECT_EQ(msg->target, "bob");
    EXPECT_EQ(msg->body, "hi there");
}

TEST(ParseProtocolTest, ListParsesCorrectly) {
    const auto msg = protocol::parse("LIST");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::LIST);
}

TEST(ParseProtocolTest, QuitParsesCorrectly) {
    const auto msg = protocol::parse("QUIT");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::QUIT);
}

// ==========================================
// 2. Whitespace Handling Tests
// ==========================================

TEST(ParseProtocolTest, TrimsLeadingAndTrailingWhitespace) {
    const auto msg = protocol::parse("   NICK alice  \t");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::NICK);
    EXPECT_EQ(msg->body, "alice");
}

TEST(ParseProtocolTest, TrimsCarriageReturnNewline) {
    const auto msg = protocol::parse("JOIN #general\r\n");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::JOIN);
    EXPECT_EQ(msg->target, "#general");
}

TEST(ParseProtocolTest, FailsOnOnlyWhitespace) {
    const auto msg = protocol::parse("   \r\n\t  ");
    EXPECT_FALSE(msg.has_value());
}

TEST(ParseProtocolTest, MsgToleratesExtraSpaceBeforeBody) {
    const auto msg = protocol::parse("MSG #general    hi");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->body, "hi");
}

// ==========================================
// 3. Missing Arguments Tests
// ==========================================

TEST(ParseProtocolTest, NickFailsWithoutNickname) {
    EXPECT_FALSE(protocol::parse("NICK").has_value());
}

TEST(ParseProtocolTest, JoinFailsWithoutRoom) {
    EXPECT_FALSE(protocol::parse("JOIN").has_value());
}

TEST(ParseProtocolTest, MsgFailsWithoutTargetAndBody) {
    EXPECT_FALSE(protocol::parse("MSG").has_value());
}

TEST(ParseProtocolTest, MsgFailsWithoutBody) {
    EXPECT_FALSE(protocol::parse("MSG #general").has_value());
}

TEST(ParseProtocolTest, DmFailsWithoutTargetAndBody) {
    EXPECT_FALSE(protocol::parse("DM").has_value());
}

TEST(ParseProtocolTest, DmFailsWithoutBody) {
    EXPECT_FALSE(protocol::parse("DM bob").has_value());
}

// ==========================================
// 4. Trailing Junk on No-Arg Commands
// ==========================================

TEST(ParseProtocolTest, ListFailsWithTrailingJunk) {
    EXPECT_FALSE(protocol::parse("LIST extra_stuff").has_value());
}

TEST(ParseProtocolTest, QuitFailsWithTrailingJunk) {
    EXPECT_FALSE(protocol::parse("QUIT now").has_value());
}

// ==========================================
// 5. Unknown Command & Empty Input
// ==========================================

TEST(ParseProtocolTest, FailsOnUnknownCommand) {
    EXPECT_FALSE(protocol::parse("UNKNOWN foo").has_value());
}

TEST(ParseProtocolTest, FailsOnEmptyInput) {
    EXPECT_FALSE(protocol::parse("").has_value());
}

// ==========================================
// 6. Edge Cases & Protocol Enforcement
// ==========================================

TEST(ParseProtocolTest, FailsOnLowercaseCommand) {
    // Commands must be strictly uppercase
    EXPECT_FALSE(protocol::parse("nick alice").has_value());
    EXPECT_FALSE(protocol::parse("Msg #general hi").has_value());
}

TEST(ParseProtocolTest, MsgTrimsWhitespaceFromBody) {
    // Leniency: Users accidentally typing multiple spaces should still yield a
    // clean body
    const auto msg = protocol::parse("MSG #general     hi there    ");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::MSG);
    EXPECT_EQ(msg->target, "#general");
    EXPECT_EQ(msg->body, "hi there");
}

// ==========================================
// 7. Server-to-Client Parse Tests
// ==========================================

TEST(ParseProtocolTest, OkParsesBody) {
    const auto msg = protocol::parse("OK welcome");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::OK);
    EXPECT_EQ(msg->body, "welcome");
}

TEST(ParseProtocolTest, OkFailsWithoutBody) {
    EXPECT_FALSE(protocol::parse("OK").has_value());
}

TEST(ParseProtocolTest, ErrParsesBody) {
    const auto msg = protocol::parse("ERR nickname taken");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::ERR);
    EXPECT_EQ(msg->body, "nickname taken");
}

TEST(ParseProtocolTest, BroadcastParsesAllFields) {
    const auto msg = protocol::parse("BROADCAST #general alice hi there");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::BROADCAST);
    EXPECT_EQ(msg->target, "#general");
    EXPECT_EQ(msg->sender, "alice");
    EXPECT_EQ(msg->body, "hi there");
}

TEST(ParseProtocolTest, BroadcastFailsWithoutSenderAndBody) {
    EXPECT_FALSE(protocol::parse("BROADCAST #general").has_value());
}

TEST(ParseProtocolTest, BroadcastFailsWithoutBody) {
    EXPECT_FALSE(protocol::parse("BROADCAST #general alice").has_value());
}

TEST(ParseProtocolTest, RoomlistParsesBody) {
    const auto msg = protocol::parse("ROOMLIST #a #b #c");
    ASSERT_TRUE(msg.has_value());
    EXPECT_EQ(msg->type, MessageType::ROOMLIST);
    EXPECT_EQ(msg->body, "#a #b #c");
}

// ==========================================
// 8. Round-Trip Serialization Tests
// ==========================================

TEST(SerializeProtocolTest, RoundTrip_OK) {
    Message original;
    original.type = MessageType::OK;
    original.body = "welcome";

    auto parsed = protocol::parse(protocol::serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(SerializeProtocolTest, RoundTrip_ERR) {
    Message original;
    original.type = MessageType::ERR;
    original.body = "nickname taken";

    auto parsed = protocol::parse(protocol::serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(SerializeProtocolTest, RoundTrip_BROADCAST) {
    Message original;
    original.type = MessageType::BROADCAST;
    original.target = "#general";
    original.sender = "alice";
    original.body = "hello everyone!";

    auto parsed = protocol::parse(protocol::serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(SerializeProtocolTest, RoundTrip_PRIVMSG) {
    Message original;
    original.type = MessageType::PRIVMSG;
    original.target = "bob";
    original.sender = "alice";
    original.body = "secret message";

    auto parsed = protocol::parse(protocol::serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(SerializeProtocolTest, RoundTrip_ROOMLIST) {
    Message original;
    original.type = MessageType::ROOMLIST;
    original.body = "#general #gaming #random";

    auto parsed = protocol::parse(protocol::serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(SerializeProtocolTest, RoundTrip_MSG) {
    Message original;
    original.type = MessageType::MSG;
    original.target = "#general";
    original.body = "hi there";

    auto parsed = protocol::parse(protocol::serialize(original));
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed, original);
}

TEST(SerializeProtocolTest, RoundTrip_NICK) {
    Message original;
    original.type = MessageType::NICK;
    original.body = "bob";
    EXPECT_EQ(*protocol::parse(protocol::serialize(original)), original);
}

TEST(SerializeProtocolTest, RoundTrip_JOIN) {
    Message original;
    original.type = MessageType::JOIN;
    original.target = "#gaming";
    EXPECT_EQ(*protocol::parse(protocol::serialize(original)), original);
}

TEST(SerializeProtocolTest, RoundTrip_DM) {
    Message original;
    original.type = MessageType::DM;
    original.target = "alice";
    original.body = "psst";
    EXPECT_EQ(*protocol::parse(protocol::serialize(original)), original);
}

TEST(SerializeProtocolTest, RoundTrip_LIST) {
    Message original;
    original.type = MessageType::LIST;
    EXPECT_EQ(*protocol::parse(protocol::serialize(original)), original);
}

TEST(SerializeProtocolTest, RoundTrip_QUIT) {
    Message original;
    original.type = MessageType::QUIT;
    EXPECT_EQ(*protocol::parse(protocol::serialize(original)), original);
}

// ==========================================
// 9. Exact Wire Format Tests
// ==========================================

TEST(SerializeProtocolTest, WireFormat_BROADCAST) {
    Message m;
    m.type = MessageType::BROADCAST;
    m.target = "#general";
    m.sender = "alice";
    m.body = "hi there";
    EXPECT_EQ(protocol::serialize(m), "BROADCAST #general alice hi there\n");
}

TEST(SerializeProtocolTest, WireFormat_OK) {
    Message m;
    m.type = MessageType::OK;
    m.body = "Nickname updated";
    EXPECT_EQ(protocol::serialize(m), "OK Nickname updated\n");
}

TEST(SerializeProtocolTest, WireFormat_ERR) {
    Message m;
    m.type = MessageType::ERR;
    m.body = "Room not found";
    EXPECT_EQ(protocol::serialize(m), "ERR Room not found\n");
}

TEST(SerializeProtocolTest, WireFormat_NICK) {
    Message m;
    m.type = MessageType::NICK;
    m.body = "charlie";
    EXPECT_EQ(protocol::serialize(m), "NICK charlie\n");
}

TEST(SerializeProtocolTest, WireFormat_MSG) {
    Message m;
    m.type = MessageType::MSG;
    m.target = "#general";
    m.body = "hello everyone";
    EXPECT_EQ(protocol::serialize(m), "MSG #general hello everyone\n");
}

TEST(SerializeProtocolTest, WireFormat_NoArgCommands) {
    Message list_msg;
    Message quit_msg;
    list_msg.type = MessageType::LIST;
    quit_msg.type = MessageType::QUIT;

    EXPECT_EQ(protocol::serialize(list_msg), "LIST\n");
    EXPECT_EQ(protocol::serialize(quit_msg), "QUIT\n");
}