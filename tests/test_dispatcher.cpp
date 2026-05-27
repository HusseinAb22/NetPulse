#include "../include/dispatcher.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "../include/message.h"
#include "../include/thread_safe_queue.h"
#include "test_helpers.h"

namespace {

std::vector<Message> received(MockClientSession &m) {
    std::lock_guard<std::mutex> lock(m.mock_mutex);
    return m.received_messages;
}

void clearReceived(MockClientSession &m) {
    std::lock_guard<std::mutex> lock(m.mock_mutex);
    m.received_messages.clear();
}

bool hasType(const std::vector<Message> &msgs, MessageType type) {
    return std::any_of(msgs.begin(), msgs.end(),
                       [type](const Message &m) { return m.type == type; });
}

std::vector<Message> broadcastsOf(MockClientSession &m) {
    std::vector<Message> out;
    for (const auto &msg : received(m)) {
        if (msg.type == MessageType::BROADCAST) {
            out.push_back(msg);
        }
    }
    return out;
}

Message from(int fd, MessageType type, std::string target, std::string body) {
    Message m;
    m.type = type;
    m.sender_fd = fd;
    m.target = std::move(target);
    m.body = std::move(body);
    return m;
}

}  // namespace

TEST(DispatcherTest, NickClaimIsAckedWithOk) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    dispatcher.registerSession(alice);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));

    auto msgs = received(*alice);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::OK);
}

TEST(DispatcherTest, DuplicateNickIsRejected) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    auto impostor = make_dummy_client(102);
    dispatcher.registerSession(alice);
    dispatcher.registerSession(impostor);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    dispatcher.handle(from(102, MessageType::NICK, "", "alice"));

    auto msgs = received(*impostor);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::ERR);
}

TEST(DispatcherTest, CommandBeforeNickIsRejected) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    dispatcher.registerSession(alice);

    dispatcher.handle(from(101, MessageType::JOIN, "#general", ""));

    auto msgs = received(*alice);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::ERR);
}

TEST(DispatcherTest, JoinIsAckedAndAnnouncedToExistingMembers) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    auto bob = make_dummy_client(102);
    dispatcher.registerSession(alice);
    dispatcher.registerSession(bob);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    dispatcher.handle(from(102, MessageType::NICK, "", "bob"));
    dispatcher.handle(from(101, MessageType::JOIN, "#general", ""));
    clearReceived(*alice);

    dispatcher.handle(from(102, MessageType::JOIN, "#general", ""));

    EXPECT_TRUE(hasType(received(*bob), MessageType::OK));
    EXPECT_TRUE(hasType(received(*alice), MessageType::BROADCAST));
}

TEST(DispatcherTest, MsgBroadcastsToOthersButNotSender) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    auto bob = make_dummy_client(102);
    dispatcher.registerSession(alice);
    dispatcher.registerSession(bob);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    dispatcher.handle(from(102, MessageType::NICK, "", "bob"));
    dispatcher.handle(from(101, MessageType::JOIN, "#general", ""));
    dispatcher.handle(from(102, MessageType::JOIN, "#general", ""));
    clearReceived(*alice);
    clearReceived(*bob);

    dispatcher.handle(from(101, MessageType::MSG, "#general", "hello"));

    auto bobMsgs = received(*bob);
    ASSERT_EQ(bobMsgs.size(), 1u);
    EXPECT_EQ(bobMsgs[0].type, MessageType::BROADCAST);
    EXPECT_EQ(bobMsgs[0].target, "#general");
    EXPECT_EQ(bobMsgs[0].sender, "alice");
    EXPECT_EQ(bobMsgs[0].body, "hello");

    EXPECT_TRUE(received(*alice).empty());
}

TEST(DispatcherTest, MsgToNonexistentRoomIsRejected) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    dispatcher.registerSession(alice);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    clearReceived(*alice);

    dispatcher.handle(from(101, MessageType::MSG, "#ghost", "anyone?"));

    auto msgs = received(*alice);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::ERR);
}

TEST(DispatcherTest, MsgToRoomYouHaveNotJoinedIsRejected) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    auto bob = make_dummy_client(102);
    dispatcher.registerSession(alice);
    dispatcher.registerSession(bob);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    dispatcher.handle(from(102, MessageType::NICK, "", "bob"));
    dispatcher.handle(from(101, MessageType::JOIN, "#general", ""));
    clearReceived(*bob);

    dispatcher.handle(from(102, MessageType::MSG, "#general", "sneaky"));

    auto msgs = received(*bob);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::ERR);
}

TEST(DispatcherTest, DmIsDeliveredToRecipient) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    auto bob = make_dummy_client(102);
    dispatcher.registerSession(alice);
    dispatcher.registerSession(bob);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    dispatcher.handle(from(102, MessageType::NICK, "", "bob"));
    clearReceived(*bob);

    dispatcher.handle(from(101, MessageType::DM, "bob", "hey there"));

    auto msgs = received(*bob);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::PRIVMSG);
    EXPECT_EQ(msgs[0].sender, "alice");
    EXPECT_EQ(msgs[0].body, "hey there");
}

TEST(DispatcherTest, DmToUnknownUserIsRejected) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    dispatcher.registerSession(alice);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    clearReceived(*alice);

    dispatcher.handle(from(101, MessageType::DM, "ghost", "anyone?"));

    auto msgs = received(*alice);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::ERR);
}

TEST(DispatcherTest, ListReturnsRoomNames) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    dispatcher.registerSession(alice);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    dispatcher.handle(from(101, MessageType::JOIN, "#alpha", ""));
    dispatcher.handle(from(101, MessageType::JOIN, "#beta", ""));
    clearReceived(*alice);

    dispatcher.handle(from(101, MessageType::LIST, "", ""));

    auto msgs = received(*alice);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::ROOMLIST);
    EXPECT_NE(msgs[0].body.find("#alpha"), std::string::npos);
    EXPECT_NE(msgs[0].body.find("#beta"), std::string::npos);
}

TEST(DispatcherTest, ServerOnlyTypeFromClientIsRejected) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    dispatcher.registerSession(alice);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    clearReceived(*alice);

    dispatcher.handle(from(101, MessageType::BROADCAST, "#general", "forged"));

    auto msgs = received(*alice);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::ERR);
}

TEST(DispatcherTest, QuitReleasesTheNickname) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    dispatcher.registerSession(alice);

    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    dispatcher.handle(from(101, MessageType::QUIT, "", ""));

    auto newcomer = make_dummy_client(102);
    dispatcher.registerSession(newcomer);
    dispatcher.handle(from(102, MessageType::NICK, "", "alice"));

    auto msgs = received(*newcomer);
    ASSERT_EQ(msgs.size(), 1u);
    EXPECT_EQ(msgs[0].type, MessageType::OK);
}

TEST(DispatcherTest, LateJoinerReceivesHistory) {
    ThreadSafeQueue<Message> inbox;
    Dispatcher dispatcher(inbox);
    auto alice = make_dummy_client(101);
    dispatcher.registerSession(alice);
    dispatcher.handle(from(101, MessageType::NICK, "", "alice"));
    dispatcher.handle(from(101, MessageType::JOIN, "#general", ""));
    dispatcher.handle(from(101, MessageType::MSG, "#general", "first"));
    dispatcher.handle(from(101, MessageType::MSG, "#general", "second"));

    auto bob = make_dummy_client(102);
    dispatcher.registerSession(bob);
    dispatcher.handle(from(102, MessageType::NICK, "", "bob"));
    dispatcher.handle(from(102, MessageType::JOIN, "#general", ""));

    auto bcasts = broadcastsOf(*bob);
    ASSERT_EQ(bcasts.size(), 2u);
    EXPECT_EQ(bcasts[0].body, "first");
    EXPECT_EQ(bcasts[0].sender, "alice");
    EXPECT_EQ(bcasts[0].target, "#general");
    EXPECT_EQ(bcasts[1].body, "second");
}
