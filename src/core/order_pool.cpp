#include "../../include/core/order_pool.hpp"

#include <limits>
#include <stdexcept>

namespace raijin
{
    std::size_t OrderPool::checked_capacity(std::size_t capacity)
    {
        if (capacity == 0 || capacity > std::numeric_limits<PoolIndex>::max())
        {
            throw std::invalid_argument("invalid order pool capacity");
        }
        return capacity;
    }

    OrderPool::OrderPool(std::size_t capacity)
        : orders_(checked_capacity(capacity)),
          free_(checked_capacity(capacity)),
          generations_(checked_capacity(capacity), 1),
          free_count_(checked_capacity(capacity))
    {
        for (std::size_t i = 0; i < capacity; ++i)
        {
            free_[i] = static_cast<PoolIndex>(capacity - 1 - i);
        }
    }

    PoolIndex OrderPool::allocate_index() noexcept
    {
        if (free_count_ == 0)
        {
            return std::numeric_limits<PoolIndex>::max();
        }

        return free_[--free_count_];
    }

    void OrderPool::deallocate(PoolIndex index) noexcept
    {
        ++generations_[index];
        free_[free_count_++] = index;
    }

    Order &OrderPool::get_order(PoolIndex index) noexcept
    {
        return orders_[index];
    }

    const Order &OrderPool::get_order(PoolIndex index) const noexcept
    {
        return orders_[index];
    }

    std::uint32_t OrderPool::generation(PoolIndex index) const noexcept
    {
        return generations_[index];
    }

    std::size_t OrderPool::capacity() const noexcept
    {
        return orders_.size();
    }
}
