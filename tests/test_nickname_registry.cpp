#include "nickname_registry.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

// 1. Core Lookup Behavior
TEST(NicknameRegistryTest, UnknownLookupsReturnNullopt) {
    NicknameRegistry registry;

    EXPECT_EQ(registry.nickFor(42), std::nullopt);
    EXPECT_EQ(registry.fdFor("alice"), std::nullopt);
}

TEST(NicknameRegistryTest, ClaimFreeNickMapsBothDirections) {
    NicknameRegistry registry;

    EXPECT_TRUE(registry.tryClaim(42, "alice"));
    EXPECT_EQ(registry.nickFor(42), "alice");
    EXPECT_EQ(registry.fdFor("alice"), 42);
}

// 2. Collision & Idempotency Handling
TEST(NicknameRegistryTest, ClaimingTakenNickFailsAndPreservesOwner) {
    NicknameRegistry registry;
    ASSERT_TRUE(registry.tryClaim(5, "alice"));

    // Target name is currently owned by someone else
    EXPECT_FALSE(registry.tryClaim(7, "alice"));

    // State must remain completely unchanged
    EXPECT_EQ(registry.nickFor(5), "alice");
    EXPECT_EQ(registry.fdFor("alice"), 5);
    EXPECT_EQ(registry.nickFor(7), std::nullopt);
}

TEST(NicknameRegistryTest, ReclaimingOwnNickReturnsTrueIdempotently) {
    NicknameRegistry registry;
    ASSERT_TRUE(registry.tryClaim(5, "alice"));

    // Attempting to re-claim the identical pair
    EXPECT_TRUE(registry.tryClaim(5, "alice"));
    EXPECT_EQ(registry.nickFor(5), "alice");
    EXPECT_EQ(registry.fdFor("alice"), 5);
}

// 3. Mutation Paths (Nick Changing)
TEST(NicknameRegistryTest, ChangingNickFreesOldAndRegistersNew) {
    NicknameRegistry registry;
    ASSERT_TRUE(registry.tryClaim(1, "alice"));

    // FD 1 switches active handles to "bob"
    EXPECT_TRUE(registry.tryClaim(1, "bob"));

    // Invariants check: Old name scrubbed, new link established
    EXPECT_EQ(registry.fdFor("alice"), std::nullopt);
    EXPECT_EQ(registry.nickFor(1), "bob");
    EXPECT_EQ(registry.fdFor("bob"), 1);
}

TEST(NicknameRegistryTest, ChangingToTakenNickFailsAndPreservesOld) {
    NicknameRegistry registry;
    ASSERT_TRUE(registry.tryClaim(1, "alice"));
    ASSERT_TRUE(registry.tryClaim(2, "bob"));

    // FD 1 tries switching to "bob", which is already registered
    EXPECT_FALSE(registry.tryClaim(1, "bob"));

    // Ensure both individual configurations were left intact
    EXPECT_EQ(registry.nickFor(1), "alice");
    EXPECT_EQ(registry.fdFor("alice"), 1);
    EXPECT_EQ(registry.nickFor(2), "bob");
    EXPECT_EQ(registry.fdFor("bob"), 2);
}

// 4. Disconnection Lifecycles
TEST(NicknameRegistryTest, ReleaseClearsBothDirections) {
    NicknameRegistry registry;
    ASSERT_TRUE(registry.tryClaim(5, "alice"));

    registry.release(5);

    // State completely unmapped
    EXPECT_EQ(registry.nickFor(5), std::nullopt);
    EXPECT_EQ(registry.fdFor("alice"), std::nullopt);

    // Verification: The name is back in the public pool for other FDs
    EXPECT_TRUE(registry.tryClaim(9, "alice"));
    EXPECT_EQ(registry.nickFor(9), "alice");
}

// 5. Thread Safety Verification
TEST(NicknameRegistryTest, ConcurrentClaimsAllowOnlyOneWinner) {
    NicknameRegistry registry;
    const int kNumThreads = 50;
    const std::string target_nick = "high_lander";

    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};

    // Pack 50 threads tightly onto the starting block
    for (int i = 1; i <= kNumThreads; ++i) {
        threads.emplace_back([&registry, &success_count, i, target_nick]() {
            if (registry.tryClaim(i, target_nick)) {
                success_count++;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Mutex invariant check: Exactly one thread should ever pull off a true return
    EXPECT_EQ(success_count.load(), 1);

    // Ensure the winning mapping is internally consistent
    auto winner_fd = registry.fdFor(target_nick);
    ASSERT_TRUE(winner_fd.has_value());
    EXPECT_EQ(registry.nickFor(*winner_fd), target_nick);
}
