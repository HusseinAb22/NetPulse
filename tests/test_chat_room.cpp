#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "../include/chat_room.h"
#include "../include/client_session.h"
#include "../include/socket_wrapper.h"
#include "../include/thread_safe_queue.h"
#include <iostream>


#if defined(__SANITIZE_THREAD__) || (defined(__has_feature) && __has_feature(thread_sanitizer))
constexpr int NUM_USERS = 20;
constexpr int NUM_THREADS = 3;
constexpr int ITERATIONS = 100;
#else
constexpr int NUM_USERS = 100;
constexpr int NUM_THREADS = 10;
constexpr int ITERATIONS = 1000;
#endif

// ==========================================
// Test Infrastructure: The Mock Client
// ==========================================

static ThreadSafeQueue<Message> dummy_server_inbox;

// A specialized client that intercepts messages instead of trying to send them over TCP
class MockClientSession : public ClientSession {
public:
    std::vector<Message> received_messages;
    std::mutex mock_mutex;

    // Call the base constructor with a dummy socket and inbox
    explicit MockClientSession(int fd) : ClientSession(SocketWrapper(fd), dummy_server_inbox) {}

    // Override the delivery mechanism to just store the message locally
    void deliver(const Message& msg) override {
        std::lock_guard lock(mock_mutex);
        received_messages.push_back(msg);
    }

};

// Helper to quickly generate our mock clients
std::shared_ptr<MockClientSession> make_dummy_client(int fd) {
    return std::make_shared<MockClientSession>(fd);
}

// ==========================================
// Single-Threaded Happy Paths
// ==========================================

TEST(ChatRoomTest, FreshRoomIsEmpty) {
    ChatRoom room("#general");
    EXPECT_EQ(room.memberCount(), 0);
    EXPECT_EQ(room.name(), "#general");
}

TEST(ChatRoomTest, JoinIncreasesCount) {
    ChatRoom room("#general");
    auto client = make_dummy_client(100);

    room.join(client);
    EXPECT_EQ(room.memberCount(), 1);
}

TEST(ChatRoomTest, MultipleDifferentJoins) {
    ChatRoom room("#general");

    room.join(make_dummy_client(100));
    room.join(make_dummy_client(101));

    EXPECT_EQ(room.memberCount(), 2);
}

TEST(ChatRoomTest, DuplicateJoinIgnored) {
    ChatRoom room("#general");
    auto client = make_dummy_client(100);

    room.join(client);
    room.join(client); // Try to join again

    EXPECT_EQ(room.memberCount(), 1); // Should still be 1
}

TEST(ChatRoomTest, LeaveRemovesMember) {
    ChatRoom room("#general");
    auto client = make_dummy_client(100);

    room.join(client);
    EXPECT_EQ(room.memberCount(), 1);

    room.leave(100);
    EXPECT_EQ(room.memberCount(), 0);
}

TEST(ChatRoomTest, LeaveNonMemberIsNoOp) {
    ChatRoom room("#general");
    auto client = make_dummy_client(100);

    room.join(client);
    room.leave(999); // Invalid FD

    EXPECT_EQ(room.memberCount(), 1);
}

TEST(ChatRoomTest, BroadcastExcludesSender) {
    ChatRoom room("#general");
    auto sender = make_dummy_client(100);
    auto receiver1 = make_dummy_client(101);
    auto receiver2 = make_dummy_client(102);

    room.join(sender);
    room.join(receiver1);
    room.join(receiver2);

    Message msg;
    msg.type = MessageType::MSG;
    msg.sender_fd = 100;
    msg.body = "Hello room!";

    room.broadcast(msg);

    // Sender should NOT get their own message
    EXPECT_TRUE(sender->received_messages.empty());

    // Receivers SHOULD get the message exactly once
    EXPECT_EQ(receiver1->received_messages.size(), 1);
    EXPECT_EQ(receiver2->received_messages.size(), 1);
}

TEST(ChatRoomTest, BroadcastEmptyBodyDeliversToNoOne) {
    ChatRoom room("#general");
    auto receiver = make_dummy_client(101);
    room.join(receiver);

    Message empty_msg;
    empty_msg.type = MessageType::MSG;
    empty_msg.sender_fd = 100;
    empty_msg.body = "";

    room.broadcast(empty_msg);

    EXPECT_TRUE(receiver->received_messages.empty());
}

// ==========================================
// Concurrency Stress Test
// ==========================================

TEST(ChatRoomTest, ConcurrentJoinLeaveBroadcast) {
    ChatRoom room("#chaos");

    // Using base pointers to match the ChatRoom::join signature
    std::vector<std::shared_ptr<ClientSession>> users;
    for (int i = 0; i < NUM_USERS; ++i) {
        users.push_back(make_dummy_client(2000 + i));
    }

    std::atomic<bool> broadcasting{true};
    std::cout << NUM_USERS <<" users created \n";
    // Readers: M threads continuously broadcasting
    std::vector<std::jthread> broadcasters;
    for (int i = 0; i < NUM_THREADS; ++i) {
        broadcasters.emplace_back([&]() {
            while (broadcasting) {
                Message msg;
                msg.body = "spam";
                msg.sender_fd = 9999;
                room.broadcast(msg);
                std::this_thread::yield();
            }
        });
    }

    // Writers: N threads joining and leaving users randomly
    std::vector<std::jthread> mutators;
    for (int i = 0; i < NUM_THREADS; ++i) {
        mutators.emplace_back([&, i]() {
            for (int j = 0; j < ITERATIONS; ++j) {
                const auto &user = users.at(((i * 10) + j) % NUM_USERS);
                room.join(user);
                std::this_thread::yield(); // Induce race conditions
                room.leave(user->getFd());
            }
        });
    }

    // Wait for the writers to finish their loops
    for (auto& m : mutators) {
        m.join();
    }

    // Stop the broadcasters
    broadcasting = false;
    for (auto& b : broadcasters) {
        b.join();
    }

    // Since every thread did exactly one join followed by one leave,
    // the room MUST be completely empty, with no crashes or data races.
    EXPECT_EQ(room.memberCount(), 0);
}