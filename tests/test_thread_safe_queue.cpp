#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>
#include "../include/thread_safe_queue.h"

// gtest macro: TEST(TestSuiteName, TestName)
TEST(ThreadSafeQueueTest, ConcurrentPushPopStressTest) {
    constexpr int NUM_PRODUCERS = 10;
    constexpr int NUM_CONSUMERS = 10;
    constexpr int ITEMS_PER_PRODUCER = 10000;
    constexpr int POISON_PILL = -1;

    ThreadSafeQueue<int> queue;
    std::atomic<int> total_consumed{0};

    auto producer_task = [&queue]() {
        for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
            queue.push(1);
        }
    };

    auto consumer_task = [&queue, &total_consumed]() {
        int local_count = 0;
        while (true) {
            int item = queue.pop();
            if (item == POISON_PILL) break;
            local_count += item;
        }
        total_consumed += local_count;
    };

    // Spin up consumers and producers
    std::vector<std::thread> consumers;
    consumers.reserve(NUM_CONSUMERS);
    for (int i = 0; i < NUM_CONSUMERS; ++i) consumers.emplace_back(consumer_task);

    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (int i = 0; i < NUM_PRODUCERS; ++i) producers.emplace_back(producer_task);

    // Wait for producers
    for (auto& p : producers) p.join();

    // Push poison pills
    for (int i = 0; i < NUM_CONSUMERS; ++i) queue.push(POISON_PILL);

    // Wait for consumers
    for (auto& c : consumers) c.join();

    // -----------------------------------------------------
    // THE ASSERTION (gtest handles the pass/fail output!)
    // -----------------------------------------------------
    constexpr int expected = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    EXPECT_EQ(total_consumed.load(), expected);
}