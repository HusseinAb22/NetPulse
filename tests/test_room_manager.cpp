#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include "../include/room_manager.h"
#include "test_helpers.h"

// ==========================================
// Single-Threaded Logic
// ==========================================

TEST(RoomManagerTest, FreshManagerHasNoRooms) {
    RoomManager manager;
    EXPECT_TRUE(manager.listRooms().empty());
}

TEST(RoomManagerTest, GetOrCreateIsIdempotent) {
    RoomManager manager;
    ChatRoom& first = manager.getOrCreate("#general");
    ChatRoom& second = manager.getOrCreate("#general");

    // They should share the exact same memory address
    EXPECT_EQ(&first, &second);
    EXPECT_EQ(manager.listRooms().size(), 1);
}

TEST(RoomManagerTest, GetOrCreateDifferentNamesAreDifferent) {
    RoomManager manager;
    ChatRoom& a = manager.getOrCreate("#general");
    ChatRoom& b = manager.getOrCreate("#gaming");

    EXPECT_NE(&a, &b);
    EXPECT_EQ(manager.listRooms().size(), 2);
}

TEST(RoomManagerTest, FindReturnsNullForMissing) {
    RoomManager manager;
    EXPECT_EQ(manager.find("#nope"), nullptr);
}

TEST(RoomManagerTest, FindReturnsRoomForExisting) {
    RoomManager manager;
    ChatRoom& created = manager.getOrCreate("#general");
    ChatRoom* found = manager.find("#general");

    EXPECT_EQ(&created, found);
}

TEST(RoomManagerTest, RemoveEmptyDropsEmpty) {
    RoomManager manager;
    manager.getOrCreate("#ghost-town");

    EXPECT_EQ(manager.listRooms().size(), 1);

    manager.removeEmpty();

    EXPECT_TRUE(manager.listRooms().empty());
}

TEST(RoomManagerTest, RemoveEmptyKeepsOccupied) {
    RoomManager manager;
    ChatRoom& room = manager.getOrCreate("#active-room");

    // Add a dummy client to keep the room alive
    auto client = make_dummy_client(100);
    room.join(client);

    manager.removeEmpty();

    // Room should survive the purge
    auto active_rooms = manager.listRooms();
    ASSERT_EQ(active_rooms.size(), 1);
    EXPECT_EQ(active_rooms[0], "#active-room");
}

// ==========================================
// Concurrency Stress Test
// ==========================================

TEST(RoomManagerTest, ConcurrentGetOrCreateCreatesOneRoom) {
    RoomManager manager;
    constexpr int NUM_THREADS = 50;

    std::vector<std::jthread> threads;
    // Store the memory address each thread gets back
    std::vector<ChatRoom*> results(NUM_THREADS, nullptr);

    // Blast the manager with 50 threads trying to create the same room simultaneously
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            results[i] = &manager.getOrCreate("#shared");
        });
    }

    // Wait for all threads to finish
    for (auto& t : threads) t.join();

    // Verify only ONE room was actually created
    auto rooms = manager.listRooms();
    ASSERT_EQ(rooms.size(), 1);
    EXPECT_EQ(rooms[0], "#shared");

    // Verify every single thread received a pointer to the EXACT same room
    ChatRoom* baseline_ptr = results[0];
    for (int i = 1; i < NUM_THREADS; ++i) {
        EXPECT_EQ(results[i], baseline_ptr);
    }
}
