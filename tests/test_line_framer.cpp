#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "../include/line_framer.h"

// 1. Single complete line in one feed.
TEST(LineFramerTest, SingleCompleteLine) {
    LineFramer framer;
    std::vector<std::string> lines;
    framer.feed("hello\n",
                [&](std::string_view line) { lines.emplace_back(line); });

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "hello");
    EXPECT_EQ(framer.size(), 0);
}

// 2. Multiple complete lines in one feed.
TEST(LineFramerTest, MultipleCompleteLines) {
    LineFramer framer;
    std::vector<std::string> lines;
    framer.feed("line1\nline2\n",
                [&](std::string_view line) { lines.emplace_back(line); });

    ASSERT_EQ(lines.size(), 2);
    EXPECT_EQ(lines[0], "line1");
    EXPECT_EQ(lines[1], "line2");
    EXPECT_EQ(framer.size(), 0);
}

// 3. A line split across two feeds.
TEST(LineFramerTest, SplitAcrossTwoFeeds) {
    LineFramer framer;
    std::vector<std::string> lines;

    framer.feed("hel",
                [&](std::string_view line) { lines.emplace_back(line); });
    EXPECT_EQ(lines.size(), 0);  // Not complete yet

    framer.feed("lo\n",
                [&](std::string_view line) { lines.emplace_back(line); });
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "hello");
}

// 4. Complete line plus a partial leftover.
TEST(LineFramerTest, CompleteLinePlusPartialLeftover) {
    LineFramer framer;
    std::vector<std::string> lines;
    framer.feed("done\npart",
                [&](std::string_view line) { lines.emplace_back(line); });

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "done");
    EXPECT_EQ(framer.size(), 4);  // "part" should remain in the buffer
}

// 5. Empty line (feed("\n")) emits one empty string.
TEST(LineFramerTest, EmptyLineEmitsEmptyString) {
    LineFramer framer;
    std::vector<std::string> lines;
    framer.feed("\n", [&](std::string_view line) { lines.emplace_back(line); });

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "");
}

// 6. Three partial feeds that together form one line.
TEST(LineFramerTest, ThreePartialFeedsFormOneLine) {
    LineFramer framer;
    std::vector<std::string> lines;

    framer.feed("a", [&](std::string_view line) { lines.emplace_back(line); });
    framer.feed("b", [&](std::string_view line) { lines.emplace_back(line); });
    EXPECT_EQ(lines.size(), 0);

    framer.feed("c\n",
                [&](std::string_view line) { lines.emplace_back(line); });
    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "abc");
}

// ==========================================
// Extra Tests (Protocol Robustness)
// ==========================================

// 7. Strip carriage returns (Windows/Telnet clients send \r\n instead of \n)
TEST(LineFramerTest, StripsCarriageReturn) {
    LineFramer framer;
    std::vector<std::string> lines;
    framer.feed("windows\r\n",
                [&](std::string_view line) { lines.emplace_back(line); });

    ASSERT_EQ(lines.size(), 1);
    EXPECT_EQ(lines[0], "windows");
}

// 8. Multiple consecutive empty lines (spam/flood protection check)
TEST(LineFramerTest, MultipleEmptyLines) {
    LineFramer framer;
    std::vector<std::string> lines;
    framer.feed("\n\n\n",
                [&](std::string_view line) { lines.emplace_back(line); });

    ASSERT_EQ(lines.size(), 3);
    EXPECT_EQ(lines[0], "");
    EXPECT_EQ(lines[1], "");
    EXPECT_EQ(lines[2], "");
}
