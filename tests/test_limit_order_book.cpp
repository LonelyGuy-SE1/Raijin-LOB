#include <gtest/gtest.h>
#include "../include/core/config.hpp"
#include "../include/core/limit_order_book.hpp"
#include "../include/core/ring_buffer.hpp"
#include "test_helpers.hpp"
#include <fstream>

using namespace raijin;

class LimitOrderBookTest : public ::testing::Test
{
protected:
    BookConfig config{
        .order_pool_capacity = 1000,
        .price_level_count = 100000,
        .level_queue_capacity = 256,
        .max_order_id = 1000};

    std::unique_ptr<LimitOrderBook> book;

    void SetUp() override
    {
        book = std::make_unique<LimitOrderBook>(config);
    }
};

TEST(LimitOrderBookConfigTest, RejectsZeroPoolCapacity)
{
    EXPECT_THROW(
        LimitOrderBook(BookConfig{0, 100, 256, 1000}),
        std::invalid_argument);
}

TEST(LimitOrderBookConfigTest, RejectsNonPowerOfTwoQueue)
{
    EXPECT_THROW(
        LimitOrderBook(BookConfig{100, 100, 3, 1000}),
        std::invalid_argument);
}

TEST(LimitOrderBookConfigTest, RejectsZeroPriceLevelCount)
{
    EXPECT_THROW(
        LimitOrderBook(BookConfig{100, 0, 256, 1000}),
        std::invalid_argument);
}

TEST(LimitOrderBookConfigTest, LoadConfigFromJson)
{
    const char *path = "raijin_test_config.json";
    {
        std::ofstream file(path);
        file << R"({"order_pool_capacity":64,"price_level_count":1024,"level_queue_capacity":16,"max_order_id":1000})";
    }
    const BookConfig loaded = load_config(path);
    EXPECT_EQ(loaded.order_pool_capacity, 64);
    EXPECT_EQ(loaded.price_level_count, 1024);
    EXPECT_EQ(loaded.level_queue_capacity, 16);
    EXPECT_EQ(loaded.max_order_id, 1000);
    std::remove(path);
}

TEST_F(LimitOrderBookTest, AddSingleBuyOrder)
{
    EXPECT_TRUE(book->add_order(1, 5000, 100, true));
    EXPECT_EQ(book->best_bid_tick(), 5000u);
    EXPECT_EQ(book->bid_volume(5000), 100u);
    EXPECT_EQ(book->best_ask_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, AddSingleSellOrder)
{
    EXPECT_TRUE(book->add_order(2, 5010, 50, false));
    EXPECT_EQ(book->best_ask_tick(), 5010u);
    EXPECT_EQ(book->ask_volume(5010), 50u);
    EXPECT_EQ(book->best_bid_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, MatchFullFill)
{
    EXPECT_TRUE(book->add_order(1, 5000, 100, false));
    EXPECT_TRUE(book->add_order(2, 5000, 100, true));
    EXPECT_EQ(book->ask_volume(5000), 0u);
    EXPECT_EQ(book->bid_volume(5000), 0u);
    EXPECT_EQ(book->best_ask_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, MatchPartialFillIncomingSmaller)
{
    EXPECT_TRUE(book->add_order(1, 5000, 100, false));
    EXPECT_TRUE(book->add_order(2, 5000, 40, true));
    EXPECT_EQ(book->ask_volume(5000), 60u);
    EXPECT_EQ(book->bid_volume(5000), 0u);
    EXPECT_EQ(book->best_ask_tick(), 5000u);
}

TEST_F(LimitOrderBookTest, MatchPartialFillIncomingLarger)
{
    EXPECT_TRUE(book->add_order(1, 5000, 100, false));
    EXPECT_TRUE(book->add_order(2, 5000, 150, true));
    EXPECT_EQ(book->ask_volume(5000), 0u);
    EXPECT_EQ(book->bid_volume(5000), 50u);
    EXPECT_EQ(book->best_ask_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, CancelOrder)
{
    EXPECT_TRUE(book->add_order(1, 5000, 100, true));
    EXPECT_TRUE(book->cancel_order(1));
    EXPECT_EQ(book->bid_volume(5000), 0u);
    EXPECT_EQ(book->best_bid_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, ToxicFlowCompaction)
{
    for (std::uint64_t i = 1; i <= 256; ++i)
    {
        EXPECT_TRUE(book->add_order(i, 5000, 10, true));
    }
    for (std::uint64_t i = 1; i <= 100; ++i)
    {
        EXPECT_TRUE(book->cancel_order(i));
    }
    EXPECT_TRUE(book->add_order(257, 5000, 10, true));
    EXPECT_EQ(book->bid_volume(5000), 1570u);
}

TEST_F(LimitOrderBookTest, RejectZeroVolume)
{
    EXPECT_FALSE(book->add_order(1, 5000, 0, true));
    EXPECT_EQ(book->best_bid_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, RejectInvalidPriceTick)
{
    EXPECT_FALSE(book->add_order(1, config.price_level_count, 10, true));
    EXPECT_FALSE(book->add_order(2, config.price_level_count + 1000, 10, false));
}

TEST_F(LimitOrderBookTest, RejectInvalidOrderId)
{
    EXPECT_FALSE(book->add_order(config.max_order_id + 1, 5000, 10, true));
}

TEST_F(LimitOrderBookTest, RejectDuplicateActiveOrderId)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, true));
    EXPECT_FALSE(book->add_order(1, 5001, 10, true));
    EXPECT_EQ(book->bid_volume(5000), 10u);
    EXPECT_EQ(book->bid_volume(5001), 0u);
}

TEST_F(LimitOrderBookTest, OrderIdReuseAfterCancel)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, true));
    EXPECT_TRUE(book->cancel_order(1));
    EXPECT_TRUE(book->add_order(1, 5010, 20, false));
    EXPECT_EQ(book->ask_volume(5010), 20u);
    EXPECT_EQ(book->bid_volume(5000), 0u);
}

TEST_F(LimitOrderBookTest, OrderIdReuseAfterFullFill)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, false));
    EXPECT_TRUE(book->add_order(2, 5000, 10, true));
    EXPECT_TRUE(book->add_order(1, 5000, 15, false));
    EXPECT_EQ(book->ask_volume(5000), 15u);
}

TEST_F(LimitOrderBookTest, FifoSamePriceLevel)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, false));
    EXPECT_TRUE(book->add_order(2, 5000, 20, false));
    EXPECT_TRUE(book->add_order(3, 5000, 10, true));
    EXPECT_EQ(book->ask_volume(5000), 20u);
    EXPECT_TRUE(book->add_order(4, 5000, 10, true));
    EXPECT_EQ(book->ask_volume(5000), 10u);
}

TEST_F(LimitOrderBookTest, MatchBuyRestingSellTaker)
{
    EXPECT_TRUE(book->add_order(1, 5000, 100, true));
    EXPECT_TRUE(book->add_order(2, 5000, 40, false));
    EXPECT_EQ(book->bid_volume(5000), 60u);
    EXPECT_EQ(book->ask_volume(5000), 0u);
}

TEST_F(LimitOrderBookTest, NoCrossWhenPricesDoNotOverlap)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, true));
    EXPECT_TRUE(book->add_order(2, 5010, 10, false));
    EXPECT_EQ(book->best_bid_tick(), 5000u);
    EXPECT_EQ(book->best_ask_tick(), 5010u);
    EXPECT_EQ(book->bid_volume(5000), 10u);
    EXPECT_EQ(book->ask_volume(5010), 10u);
}

TEST_F(LimitOrderBookTest, AggressiveBuyCrossesRestingAsk)
{
    EXPECT_TRUE(book->add_order(1, 5010, 10, false));
    EXPECT_TRUE(book->add_order(2, 5020, 10, true));
    EXPECT_EQ(book->ask_volume(5010), 0u);
    EXPECT_EQ(book->bid_volume(5020), 0u);
}

TEST_F(LimitOrderBookTest, MultiLevelSweep)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, false));
    EXPECT_TRUE(book->add_order(2, 5010, 10, false));
    EXPECT_TRUE(book->add_order(3, 5020, 10, false));
    EXPECT_TRUE(book->add_order(4, 5020, 30, true));
    EXPECT_EQ(book->ask_volume(5000), 0u);
    EXPECT_EQ(book->ask_volume(5010), 0u);
    EXPECT_EQ(book->ask_volume(5020), 0u);
    EXPECT_EQ(book->best_ask_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, BestBidRefreshOverGaps)
{
    EXPECT_TRUE(book->add_order(1, 100, 10, true));
    EXPECT_TRUE(book->add_order(2, 200, 10, true));
    EXPECT_TRUE(book->add_order(3, 300, 10, true));
    EXPECT_EQ(book->best_bid_tick(), 300u);
    EXPECT_TRUE(book->cancel_order(3));
    EXPECT_EQ(book->best_bid_tick(), 200u);
    EXPECT_TRUE(book->cancel_order(2));
    EXPECT_EQ(book->best_bid_tick(), 100u);
    EXPECT_TRUE(book->cancel_order(1));
    EXPECT_EQ(book->best_bid_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, BestAskRefreshOverGaps)
{
    EXPECT_TRUE(book->add_order(1, 300, 10, false));
    EXPECT_TRUE(book->add_order(2, 200, 10, false));
    EXPECT_TRUE(book->add_order(3, 100, 10, false));
    EXPECT_EQ(book->best_ask_tick(), 100u);
    EXPECT_TRUE(book->cancel_order(3));
    EXPECT_EQ(book->best_ask_tick(), 200u);
    EXPECT_TRUE(book->cancel_order(2));
    EXPECT_EQ(book->best_ask_tick(), 300u);
    EXPECT_TRUE(book->cancel_order(1));
    EXPECT_EQ(book->best_ask_tick(), UINT32_MAX);
}

TEST_F(LimitOrderBookTest, CancelUnknownOrder)
{
    EXPECT_FALSE(book->cancel_order(1));
}

TEST_F(LimitOrderBookTest, DoubleCancelFails)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, true));
    EXPECT_TRUE(book->cancel_order(1));
    EXPECT_FALSE(book->cancel_order(1));
}

TEST_F(LimitOrderBookTest, CancelAfterFullFillFails)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, false));
    EXPECT_TRUE(book->add_order(2, 5000, 10, true));
    EXPECT_FALSE(book->cancel_order(1));
}

TEST_F(LimitOrderBookTest, StaleTombstoneSkippedOnMatch)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, false));
    EXPECT_TRUE(book->cancel_order(1));
    EXPECT_TRUE(book->add_order(2, 5000, 10, false));
    EXPECT_TRUE(book->add_order(3, 5000, 10, true));
    EXPECT_EQ(book->ask_volume(5000), 0u);
}

TEST_F(LimitOrderBookTest, VolumeQueryOutOfRangeReturnsZero)
{
    EXPECT_TRUE(book->add_order(1, 5000, 10, true));
    EXPECT_EQ(book->bid_volume(config.price_level_count), 0u);
    EXPECT_EQ(book->ask_volume(config.price_level_count + 1), 0u);
}

TEST(LimitOrderBookCapacityTest, PoolExhaustionOnRest)
{
    const BookConfig cfg = test::tiny_config();
    LimitOrderBook book(cfg);
    EXPECT_TRUE(book.add_order(1, 100, 1, true));
    EXPECT_TRUE(book.add_order(2, 101, 1, true));
    EXPECT_TRUE(book.add_order(3, 102, 1, true));
    EXPECT_TRUE(book.add_order(4, 103, 1, true));
    EXPECT_TRUE(book.add_order(5, 104, 1, true));
    EXPECT_TRUE(book.add_order(6, 105, 1, true));
    EXPECT_TRUE(book.add_order(7, 106, 1, true));
    EXPECT_TRUE(book.add_order(8, 107, 1, true));
    EXPECT_FALSE(book.add_order(9, 108, 1, true));
}

TEST(LimitOrderBookCapacityTest, LevelQueueFullOnRest)
{
    const BookConfig cfg = test::tiny_config();
    LimitOrderBook book(cfg);
    EXPECT_TRUE(book.add_order(1, 100, 1, true));
    EXPECT_TRUE(book.add_order(2, 100, 1, true));
    EXPECT_TRUE(book.add_order(3, 100, 1, true));
    EXPECT_TRUE(book.add_order(4, 100, 1, true));
    EXPECT_FALSE(book.add_order(5, 100, 1, true));
    EXPECT_EQ(book.bid_volume(100), 4u);
}

TEST(LimitOrderBookCapacityTest, PartialMatchRestFailsWhenPoolFull)
{
    BookConfig cfg{
        .order_pool_capacity = 1,
        .price_level_count = 8192,
        .level_queue_capacity = 16,
        .max_order_id = 16};
    LimitOrderBook book(cfg);
    EXPECT_TRUE(book.add_order(1, 5010, 50, true));
    EXPECT_TRUE(book.add_order(2, 5020, 100, false));
    EXPECT_FALSE(book.add_order(3, 5020, 200, true));
    EXPECT_EQ(book.ask_volume(5020), 0u);
    EXPECT_EQ(book.bid_volume(5010), 50u);
    EXPECT_EQ(book.bid_volume(5020), 0u);
}

TEST(LimitOrderBookReceiptTest, FullFillReceipt)
{
    RingBuffer<ExecutionReceipt> rb(16);
    LimitOrderBook book(test::small_config(), &rb);
    EXPECT_TRUE(book.add_order(1, 5000, 100, false));
    EXPECT_TRUE(book.add_order(2, 5000, 100, true));
    ExecutionReceipt receipt{};
    EXPECT_TRUE(rb.pop(receipt));
    EXPECT_EQ(receipt.maker_order_id, 1u);
    EXPECT_EQ(receipt.taker_order_id, 2u);
    EXPECT_EQ(receipt.price_tick, 5000u);
    EXPECT_EQ(receipt.executed_volume, 100u);
    EXPECT_FALSE(rb.pop(receipt));
}

TEST(LimitOrderBookReceiptTest, PartialFillReceiptVolume)
{
    RingBuffer<ExecutionReceipt> rb(16);
    LimitOrderBook book(test::small_config(), &rb);
    EXPECT_TRUE(book.add_order(1, 5000, 100, false));
    EXPECT_TRUE(book.add_order(2, 5000, 40, true));
    ExecutionReceipt receipt{};
    EXPECT_TRUE(rb.pop(receipt));
    EXPECT_EQ(receipt.executed_volume, 40u);
    EXPECT_EQ(book.ask_volume(5000), 60u);
}

TEST(LimitOrderBookReceiptTest, MultiLevelMultipleReceipts)
{
    RingBuffer<ExecutionReceipt> rb(16);
    LimitOrderBook book(test::small_config(), &rb);
    EXPECT_TRUE(book.add_order(1, 5000, 10, false));
    EXPECT_TRUE(book.add_order(2, 5010, 10, false));
    EXPECT_TRUE(book.add_order(3, 5020, 10, false));
    EXPECT_TRUE(book.add_order(4, 5020, 30, true));
    ExecutionReceipt receipt{};
    EXPECT_TRUE(rb.pop(receipt));
    EXPECT_EQ(receipt.price_tick, 5000u);
    EXPECT_EQ(receipt.executed_volume, 10u);
    EXPECT_TRUE(rb.pop(receipt));
    EXPECT_EQ(receipt.price_tick, 5010u);
    EXPECT_EQ(receipt.executed_volume, 10u);
    EXPECT_TRUE(rb.pop(receipt));
    EXPECT_EQ(receipt.price_tick, 5020u);
    EXPECT_EQ(receipt.executed_volume, 10u);
    EXPECT_FALSE(rb.pop(receipt));
}

TEST(LimitOrderBookReceiptTest, FullRingDropsReceiptSilently)
{
    RingBuffer<ExecutionReceipt> rb(1);
    EXPECT_TRUE(rb.push({99, 98, 5000, 1}));
    LimitOrderBook book(test::small_config(), &rb);
    EXPECT_TRUE(book.add_order(1, 5000, 10, false));
    EXPECT_TRUE(book.add_order(2, 5000, 10, true));
    ExecutionReceipt receipt{};
    EXPECT_TRUE(rb.pop(receipt));
    EXPECT_EQ(receipt.maker_order_id, 99u);
    EXPECT_FALSE(rb.pop(receipt));
}

TEST(LimitOrderBookReceiptTest, CancelDoesNotEmitReceipt)
{
    RingBuffer<ExecutionReceipt> rb(16);
    LimitOrderBook book(test::small_config(), &rb);
    EXPECT_TRUE(book.add_order(1, 5000, 10, true));
    EXPECT_TRUE(book.cancel_order(1));
    ExecutionReceipt receipt{};
    EXPECT_FALSE(rb.pop(receipt));
}
