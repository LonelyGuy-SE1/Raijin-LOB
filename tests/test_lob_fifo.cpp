#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include "../include/core/limit_order_book.hpp"

using namespace raijin;

TEST(LobFifoTest, SamePriceSellQueueOrder)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 10, false).accepted);
    EXPECT_TRUE(book.add_order(2, 5000, 20, false).accepted);
    EXPECT_EQ(book.ask_volume(5000), 30);

    EXPECT_TRUE(book.add_order(3, 5000, 10, true).accepted);
    EXPECT_EQ(book.ask_volume(5000), 20);

    EXPECT_TRUE(book.add_order(4, 5000, 10, true).accepted);
    EXPECT_EQ(book.ask_volume(5000), 10);

    EXPECT_TRUE(book.add_order(5, 5000, 10, true).accepted);
    EXPECT_EQ(book.ask_volume(5000), 0);
    EXPECT_EQ(book.best_ask_tick(), UINT32_MAX);
}

TEST(LobFifoTest, SamePriceBuyQueueOrder)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 15, true).accepted);
    EXPECT_TRUE(book.add_order(2, 5000, 25, true).accepted);
    EXPECT_EQ(book.bid_volume(5000), 40);

    EXPECT_TRUE(book.add_order(3, 5000, 15, false).accepted);
    EXPECT_EQ(book.bid_volume(5000), 25);

    EXPECT_TRUE(book.add_order(4, 5000, 10, false).accepted);
    EXPECT_EQ(book.bid_volume(5000), 15);
}

TEST(LobFifoTest, PartialFillLeavesHeadOrder)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 100, false).accepted);
    EXPECT_TRUE(book.add_order(2, 5000, 50, false).accepted);

    EXPECT_TRUE(book.add_order(3, 5000, 40, true).accepted);
    EXPECT_EQ(book.ask_volume(5000), 110);

    EXPECT_TRUE(book.add_order(4, 5000, 80, true).accepted);
    EXPECT_EQ(book.ask_volume(5000), 30);
}

TEST(LobFifoTest, MatchSkipsCancelledHead)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 10, false).accepted);
    EXPECT_TRUE(book.add_order(2, 5000, 20, false).accepted);
    EXPECT_TRUE(book.cancel_order(1));

    EXPECT_TRUE(book.add_order(3, 5000, 20, true).accepted);
    EXPECT_EQ(book.ask_volume(5000), 0);
    EXPECT_EQ(book.best_ask_tick(), UINT32_MAX);
}

TEST(LobFifoTest, SoloCancelClearsLevelQueue)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 10, false).accepted);
    EXPECT_TRUE(book.cancel_order(1));
    EXPECT_TRUE(book.add_order(2, 5000, 10, false).accepted);
    EXPECT_TRUE(book.add_order(3, 5000, 10, true).accepted);
    EXPECT_EQ(book.ask_volume(5000), 0);
    EXPECT_EQ(book.best_ask_tick(), UINT32_MAX);
}

TEST(LobFifoTest, MatchSkipsManyTombstonesBeforeLiveMaker)
{
    LimitOrderBook book(test::small_config());
    constexpr std::uint32_t tick = 5000;
    constexpr std::uint64_t kTombstones = 32;

    for (std::uint64_t i = 10; i < 10 + kTombstones; ++i)
    {
        EXPECT_TRUE(book.add_order(i, tick, 10, false).accepted);
    }
    EXPECT_TRUE(book.add_order(99, tick, 25, false).accepted);
    for (std::uint64_t i = 10; i < 10 + kTombstones; ++i)
    {
        EXPECT_TRUE(book.cancel_order(i));
    }
    EXPECT_EQ(book.ask_volume(tick), 25);

    EXPECT_TRUE(book.add_order(100, tick, 25, true).accepted);
    EXPECT_EQ(book.ask_volume(tick), 0);
    EXPECT_EQ(book.best_ask_tick(), UINT32_MAX);
}
