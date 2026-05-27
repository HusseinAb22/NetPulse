#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "../include/chat_room.h"
#include "../include/client_session.h"
#include "../include/socket_wrapper.h"
#include "../include/thread_safe_queue.h"
#include <iostream>
#include "test_helpers.h"
#include <chrono>

#if defined(__SANITIZE_THREAD__)
#  define NETPULSE_UNDER_TSAN 1
#elif defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define NETPULSE_UNDER_TSAN 1
#  endif
#endif

#ifndef NETPULSE_UNDER_TSAN
#  define NETPULSE_UNDER_TSAN 0
#endif

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
#if NETPULSE_UNDER_TSAN
    constexpr int NUM_USERS = 20;
    constexpr int NUM_THREADS = 3;
    constexpr int ITERATIONS = 100;
#else
    constexpr int NUM_USERS = 100;
    constexpr int NUM_THREADS = 10;
    constexpr int ITERATIONS = 1000;
#endif
    ChatRoom room("#chaos");

    // Using base pointers to match the ChatRoom::join signature
    std::vector<std::shared_ptr<ClientSession>> users;
    users.reserve(NUM_USERS);
    for (int i = 0; i < NUM_USERS; ++i) {
        users.push_back(make_dummy_client(2000 + i));
    }

    std::atomic<bool> broadcasting{true};
    std::cout << NUM_USERS <<" users created \n";
    // Readers: M threads continuously broadcasting
    std::vector<std::jthread> broadcasters;
    broadcasters.reserve(NUM_THREADS);
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
    mutators.reserve(NUM_THREADS);
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

//

TEST(ChatRoomTest, HistoryIsBoundedAndFifo) {
    ChatRoom room("#x");
    for (int i = 0; i < 55; ++i) {
        Message m;
        m.type = MessageType::BROADCAST;
        m.target = "#x";
        m.sender = "u";
        m.body = "m" + std::to_string(i);
        room.addToHistory(m);
    }
    auto sink = make_dummy_client(303);
    room.replayHistory(sink);
    ASSERT_EQ(sink->received_messages.size(), 50u);
    EXPECT_EQ(sink->received_messages.front().body, "m5");   // m0..m4 evicted
    EXPECT_EQ(sink->received_messages.back().body, "m54");
}

TEST(ChatRoomTest, HistoryPreservesTimestamp) {
    ChatRoom room("#x");
    Message m;
    m.type = MessageType::BROADCAST;
    m.target = "#x";
    m.sender = "u";
    m.body = "hi";
    const auto stamp = std::chrono::system_clock::now() - std::chrono::hours(3);
    m.timestamp = stamp;
    room.addToHistory(m);

    auto sink = make_dummy_client(304);
    room.replayHistory(sink);
    ASSERT_EQ(sink->received_messages.size(), 1u);
    EXPECT_TRUE(sink->received_messages[0].timestamp == stamp);
}

TEST(ChatRoomTest, EmptyHistoryReplayIsNoop) {
    ChatRoom room("#x");
    auto sink = make_dummy_client(305);
    room.replayHistory(sink);
    EXPECT_TRUE(sink->received_messages.empty());
}