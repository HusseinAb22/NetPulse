#include <gtest/gtest.h>

#include <atomic>
#include <optional>
#include <thread>
#include <vector>

#include "../include/thread_safe_queue.h"

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
    for (int i = 0; i < NUM_CONSUMERS; ++i)
        consumers.emplace_back(consumer_task);

    std::vector<std::thread> producers;
    producers.reserve(NUM_PRODUCERS);
    for (int i = 0; i < NUM_PRODUCERS; ++i)
        producers.emplace_back(producer_task);

    // Wait for producers
    for (auto &p : producers) p.join();

    // Push poison pills
    for (int i = 0; i < NUM_CONSUMERS; ++i) queue.push(POISON_PILL);

    // Wait for consumers
    for (auto &c : consumers) c.join();

    // -----------------------------------------------------
    // THE ASSERTION (gtest handles the pass/fail output!)
    // -----------------------------------------------------
    constexpr int expected = NUM_PRODUCERS * ITEMS_PER_PRODUCER;
    EXPECT_EQ(total_consumed.load(), expected);
}

TEST(ThreadSafeQueueTest, SequentialPushPop) {
    ThreadSafeQueue<int> queue;

    // Push 3 items
    queue.push(10);
    queue.push(20);
    queue.push(30);

    // Pop them and ensure FIFO (First-In, First-Out) order is preserved
    EXPECT_EQ(queue.pop(), 10);
    EXPECT_EQ(queue.pop(), 20);
    EXPECT_EQ(queue.pop(), 30);
}

TEST(ThreadSafeQueueTest, TryPopEmptyQueue) {
    ThreadSafeQueue<std::optional<int>> queue;

    // Attempt to pop from a brand new, empty queue using the non-blocking
    // method
    const std::optional<int> result = queue.tryPop();

    EXPECT_FALSE(result.has_value());
}

TEST(ThreadSafeQueueTest, TryPopPopulatedQueue) {
    ThreadSafeQueue<std::optional<int>> queue;
    queue.push(42);

    const std::optional<int> result = queue.tryPop();

    // It should instantly grab the value
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 42);

    // The queue should now be empty again
    EXPECT_FALSE(queue.tryPop().has_value());
}