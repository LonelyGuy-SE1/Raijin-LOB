#pragma once

#include "order_pool.hpp"
#include "price_level.hpp"
#include "types.hpp"
#include "ring_buffer.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace raijin
{
    struct BookConfig
    {
        std::size_t order_pool_capacity;
        std::uint32_t price_level_count;
        std::uint32_t level_queue_capacity;
        std::uint64_t max_order_id;
    };

    class LimitOrderBook
    {
    public:
        explicit LimitOrderBook(const BookConfig &config, RingBuffer<ExecutionReceipt> *receipt_queue = nullptr);

        bool add_order(std::uint64_t order_id, std::uint32_t price_tick, std::uint32_t volume, bool is_buy);
        bool cancel_order(std::uint64_t order_id) noexcept;

        std::uint32_t best_bid_tick() const noexcept;
        std::uint32_t best_ask_tick() const noexcept;
        bool has_best_bid() const noexcept { return best_bid_ != invalid_tick; }
        bool has_best_ask() const noexcept { return best_ask_ != invalid_tick; }
        std::uint64_t bid_volume(std::uint32_t price_tick) const noexcept;
        std::uint64_t ask_volume(std::uint32_t price_tick) const noexcept;
        std::uint64_t receipt_overflows() const noexcept { return receipt_overflows_; }

    private:
        struct Locator
        {
            PoolIndex index = 0;
            std::uint32_t generation = 0;
            std::uint8_t side = 0;
            std::uint8_t active = 0;
        };

        static constexpr std::uint32_t invalid_tick = UINT32_MAX;

        static BookConfig checked_config(const BookConfig &config);
        static bool is_power_of_two(std::uint32_t value) noexcept;
        static std::size_t word_count(std::uint32_t price_level_count) noexcept;
        static void set_bit(std::vector<std::uint64_t> &words, std::uint32_t tick, std::uint32_t &active_levels) noexcept;
        static void reset_bit(std::vector<std::uint64_t> &words, std::uint32_t tick, std::uint32_t &active_levels) noexcept;

        bool rest_order(const Order &order, bool is_buy) noexcept;
        void match_buy(Order &incoming) noexcept;
        void match_sell(Order &incoming) noexcept;
        void clean_front(PriceLevel &level, OrderPool &pool) noexcept;
        void erase_best_ask(std::uint32_t tick) noexcept;
        void erase_best_bid(std::uint32_t tick) noexcept;
        std::uint32_t next_ask(std::uint32_t start) const noexcept;
        std::uint32_t next_bid(std::uint32_t start) const noexcept;
        bool valid_order_id(std::uint64_t order_id) const noexcept;

        BookConfig config_;
        OrderPool bid_pool_;
        OrderPool ask_pool_;
        std::vector<PriceLevel> bid_levels_;
        std::vector<PriceLevel> ask_levels_;
        std::vector<OrderRef> bid_orders_;
        std::vector<OrderRef> ask_orders_;
        std::vector<std::uint64_t> bid_words_;
        std::vector<std::uint64_t> ask_words_;
        std::vector<Locator> locators_;
        std::uint32_t best_bid_;
        std::uint32_t best_ask_;
        std::uint32_t bid_active_levels_ = 0;
        std::uint32_t ask_active_levels_ = 0;
        RingBuffer<ExecutionReceipt> *receipt_queue_;
        std::uint64_t receipt_overflows_ = 0;
    };
}
