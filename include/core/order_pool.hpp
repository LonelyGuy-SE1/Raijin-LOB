#pragma once

#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace raijin
{
    class OrderPool
    {
    public:
        explicit OrderPool(std::size_t capacity);

        OrderPool(const OrderPool &) = delete;
        OrderPool &operator=(const OrderPool &) = delete;

        PoolIndex allocate_index() noexcept;
        void deallocate(PoolIndex index) noexcept;

        Order &get_order(PoolIndex index) noexcept;
        const Order &get_order(PoolIndex index) const noexcept;

        std::uint32_t generation(PoolIndex index) const noexcept;
        std::size_t capacity() const noexcept;

    private:
        static std::size_t checked_capacity(std::size_t capacity);

        std::vector<Order> orders_;
        std::vector<PoolIndex> free_;
        std::vector<std::uint32_t> generations_;
        std::size_t free_count_;
    };
}
