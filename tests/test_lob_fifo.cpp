#include <gtest/gtest.h>
#include "test_helpers.hpp"
#include "../include/core/limit_order_book.hpp"

using namespace raijin;

TEST(LobFifoTest, SamePriceSellQueueOrder)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 10, false));
    EXPECT_TRUE(book.add_order(2, 5000, 20, false));
    EXPECT_EQ(book.ask_volume(5000), 30);

    EXPECT_TRUE(book.add_order(3, 5000, 10, true));
    EXPECT_EQ(book.ask_volume(5000), 20);

    EXPECT_TRUE(book.add_order(4, 5000, 10, true));
    EXPECT_EQ(book.ask_volume(5000), 10);

    EXPECT_TRUE(book.add_order(5, 5000, 10, true));
    EXPECT_EQ(book.ask_volume(5000), 0);
    EXPECT_EQ(book.best_ask_tick(), UINT32_MAX);
}

TEST(LobFifoTest, SamePriceBuyQueueOrder)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 15, true));
    EXPECT_TRUE(book.add_order(2, 5000, 25, true));
    EXPECT_EQ(book.bid_volume(5000), 40);

    EXPECT_TRUE(book.add_order(3, 5000, 15, false));
    EXPECT_EQ(book.bid_volume(5000), 25);

    EXPECT_TRUE(book.add_order(4, 5000, 10, false));
    EXPECT_EQ(book.bid_volume(5000), 15);
}

TEST(LobFifoTest, PartialFillLeavesHeadOrder)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 100, false));
    EXPECT_TRUE(book.add_order(2, 5000, 50, false));

    EXPECT_TRUE(book.add_order(3, 5000, 40, true));
    EXPECT_EQ(book.ask_volume(5000), 110);

    EXPECT_TRUE(book.add_order(4, 5000, 80, true));
    EXPECT_EQ(book.ask_volume(5000), 30);
}

TEST(LobFifoTest, MatchSkipsCancelledHead)
{
    LimitOrderBook book(test::small_config());

    EXPECT_TRUE(book.add_order(1, 5000, 10, false));
    EXPECT_TRUE(book.add_order(2, 5000, 20, false));
    EXPECT_TRUE(book.cancel_order(1));

    EXPECT_TRUE(book.add_order(3, 5000, 20, true));
    EXPECT_EQ(book.ask_volume(5000), 0);
    EXPECT_EQ(book.best_ask_tick(), UINT32_MAX);
}
