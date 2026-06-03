#pragma once

#include <cstdint>

namespace raijin
{
    using PoolIndex = std::uint32_t;

    enum class OrderType : std::uint8_t
    {
        Limit = 0,
        Market = 1
    };

    enum class TimeInForce : std::uint8_t
    {
        GTC = 0,
        IOC = 1,
        FOK = 2
    };

    struct alignas(16) Order
    {
        std::uint64_t order_id;
        std::uint32_t volume;
        std::uint32_t price_tick;
    };

    struct OrderRef
    {
        PoolIndex index;
        std::uint32_t generation;
    };

    static_assert(sizeof(Order) == 16);
    static_assert(alignof(Order) == 16);
}
