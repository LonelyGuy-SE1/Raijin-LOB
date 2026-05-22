#include <gtest/gtest.h>
#include "../include/core/ring_buffer.hpp"
#include <atomic>
#include <thread>

using namespace raijin;

TEST(RingBufferTest, BasicPushPop)
{
    RingBuffer<ExecutionReceipt> rb(4);
    ExecutionReceipt item{};
    EXPECT_FALSE(rb.pop(item));
    EXPECT_TRUE(rb.push({100, 200, 5000, 50}));
    EXPECT_TRUE(rb.pop(item));
    EXPECT_EQ(item.maker_order_id, 100);
    EXPECT_EQ(item.taker_order_id, 200);
    EXPECT_EQ(item.price_tick, 5000);
    EXPECT_EQ(item.executed_volume, 50);
    EXPECT_FALSE(rb.pop(item));
}

TEST(RingBufferTest, BufferFull)
{
    RingBuffer<ExecutionReceipt> rb(2);
    EXPECT_TRUE(rb.push({1, 2, 100, 10}));
    EXPECT_TRUE(rb.push({3, 4, 100, 10}));
    EXPECT_FALSE(rb.push({5, 6, 100, 10}));
}

TEST(RingBufferTest, InvalidCapacityThrows)
{
    EXPECT_THROW(RingBuffer<ExecutionReceipt>(3), std::invalid_argument);
    EXPECT_THROW(RingBuffer<ExecutionReceipt>(0), std::invalid_argument);
}

TEST(RingBufferTest, FillDrainOrder)
{
    RingBuffer<ExecutionReceipt> rb(8);
    for (std::uint64_t i = 0; i < 8; ++i)
    {
        EXPECT_TRUE(rb.push({i, i + 1, 100, static_cast<std::uint32_t>(i)}));
    }
    EXPECT_FALSE(rb.push({99, 99, 100, 1}));
    ExecutionReceipt item{};
    for (std::uint64_t i = 0; i < 8; ++i)
    {
        ASSERT_TRUE(rb.pop(item));
        EXPECT_EQ(item.maker_order_id, i);
        EXPECT_EQ(item.executed_volume, static_cast<std::uint32_t>(i));
    }
    EXPECT_FALSE(rb.pop(item));
}

TEST(RingBufferTest, SpscProducerConsumer)
{
    RingBuffer<std::uint64_t> rb(1024);
    constexpr int total = 100000;
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::thread consumer([&] {
        std::uint64_t value = 0;
        while (consumed.load(std::memory_order_relaxed) < total)
        {
            if (rb.pop(value))
            {
                consumed.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });
    for (int i = 0; i < total; ++i)
    {
        while (!rb.push(static_cast<std::uint64_t>(i)))
        {
        }
        produced.fetch_add(1, std::memory_order_relaxed);
    }
    consumer.join();
    EXPECT_EQ(produced.load(), total);
    EXPECT_EQ(consumed.load(), total);
}
