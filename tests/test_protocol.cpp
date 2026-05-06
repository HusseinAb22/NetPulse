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

TEST(ProtocolParseTest, MsgToleratesExtraSpaceBeforeBody) {
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