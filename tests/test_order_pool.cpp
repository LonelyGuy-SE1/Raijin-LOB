#include <gtest/gtest.h>
#include "../include/core/order_pool.hpp"
#include <limits>

using namespace raijin;

TEST(OrderPoolTest, Initialization)
{
    OrderPool pool(100);
    EXPECT_EQ(pool.capacity(), 100);
}

TEST(OrderPoolTest, AllocationAndDeallocation)
{
    OrderPool pool(10);
    PoolIndex idx1 = pool.allocate_index();
    EXPECT_NE(idx1, std::numeric_limits<PoolIndex>::max());
    pool.deallocate(idx1);
}

TEST(OrderPoolTest, GenerationalSafety)
{
    OrderPool pool(10);
    PoolIndex idx = pool.allocate_index();
    const std::uint32_t gen_before = pool.generation(idx);
    pool.deallocate(idx);
    const std::uint32_t gen_after = pool.generation(idx);
    EXPECT_EQ(gen_after, gen_before + 1);
}

TEST(OrderPoolTest, GenerationIncrementsOnEachDeallocate)
{
    OrderPool pool(10);
    PoolIndex idx = pool.allocate_index();
    const std::uint32_t gen = pool.generation(idx);
    for (int i = 0; i < 4; ++i)
    {
        pool.deallocate(idx);
        EXPECT_EQ(pool.generation(idx), gen + static_cast<std::uint32_t>(i + 1));
        idx = pool.allocate_index();
    }
}

TEST(OrderPoolTest, PoolExhaustion)
{
    OrderPool pool(2);
    PoolIndex idx1 = pool.allocate_index();
    PoolIndex idx2 = pool.allocate_index();
    PoolIndex idx3 = pool.allocate_index();
    EXPECT_NE(idx1, std::numeric_limits<PoolIndex>::max());
    EXPECT_NE(idx2, std::numeric_limits<PoolIndex>::max());
    EXPECT_EQ(idx3, std::numeric_limits<PoolIndex>::max());
}

TEST(OrderPoolTest, DataReadWrite)
{
    OrderPool pool(10);
    PoolIndex idx = pool.allocate_index();
    Order &order = pool.get_order(idx);
    order.order_id = 42;
    order.volume = 100;
    order.price_tick = 500;
    const Order &same_order = pool.get_order(idx);
    EXPECT_EQ(same_order.order_id, 42);
    EXPECT_EQ(same_order.volume, 100);
    EXPECT_EQ(same_order.price_tick, 500);
}

TEST(OrderPoolTest, ReuseAfterDeallocation)
{
    OrderPool pool(10);
    PoolIndex idx1 = pool.allocate_index();
    pool.get_order(idx1).order_id = 99;
    pool.get_order(idx1).volume = 50;
    pool.deallocate(idx1);
    PoolIndex idx2 = pool.allocate_index();
    EXPECT_EQ(idx1, idx2);
    pool.get_order(idx2).order_id = 100;
    pool.get_order(idx2).volume = 200;
    EXPECT_EQ(pool.get_order(idx2).order_id, 100);
    EXPECT_EQ(pool.get_order(idx2).volume, 200);
}

TEST(OrderPoolTest, MultipleSlotsIndependent)
{
    OrderPool pool(4);
    PoolIndex a = pool.allocate_index();
    PoolIndex b = pool.allocate_index();
    EXPECT_NE(a, b);
    pool.get_order(a).order_id = 1;
    pool.get_order(b).order_id = 2;
    EXPECT_EQ(pool.get_order(a).order_id, 1);
    EXPECT_EQ(pool.get_order(b).order_id, 2);
}

TEST(OrderPoolTest, DeallocateFreesSlotForReuse)
{
    OrderPool pool(2);
    PoolIndex a = pool.allocate_index();
    PoolIndex b = pool.allocate_index();
    EXPECT_EQ(pool.allocate_index(), std::numeric_limits<PoolIndex>::max());
    pool.deallocate(a);
    PoolIndex c = pool.allocate_index();
    EXPECT_EQ(c, a);
    EXPECT_NE(c, b);
}
