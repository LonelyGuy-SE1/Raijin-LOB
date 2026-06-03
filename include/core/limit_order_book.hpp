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

    struct AddResult
    {
        bool valid = false;
        bool accepted = false;
        std::uint32_t matched_volume = 0;
        std::uint32_t rested_volume = 0;
        std::uint32_t dropped_volume = 0;
    };

    struct ModifyResult
    {
        bool valid = false;
        bool cancelled = false;
        bool reested = false;
        std::uint32_t dropped_volume = 0;
    };

    class LimitOrderBook
    {
    public:
        explicit LimitOrderBook(const BookConfig &config, RingBuffer<ExecutionReceipt> *receipt_queue = nullptr);

        inline AddResult add_order(std::uint64_t order_id, std::uint32_t price_tick, std::uint32_t volume, bool is_buy,
                                   OrderType type = OrderType::Limit, TimeInForce tif = TimeInForce::GTC,
                                   std::uint64_t timestamp = 0)
        {
            (void)timestamp;

            if (type == OrderType::Limit && tif == TimeInForce::GTC) [[likely]]
            {
                AddResult result{};
                result.valid = true;

                if (volume == 0 || price_tick >= config_.price_level_count ||
                    !valid_order_id(order_id) || locators_[order_id].active != 0)
                {
                    result.valid = false;
                    return result;
                }

                Order incoming{order_id, volume, price_tick};

                if (is_buy)
                {
                    match_buy(incoming);
                }
                else
                {
                    match_sell(incoming);
                }

                if (incoming.volume == 0)
                {
                    result.accepted = true;
                    result.matched_volume = volume;
                    return result;
                }

                bool rested = rest_order(incoming, is_buy);
                result.matched_volume = volume - incoming.volume;

                if (rested)
                {
                    result.accepted = true;
                    result.rested_volume = incoming.volume;
                    return result;
                }

                result.accepted = result.matched_volume > 0;
                result.dropped_volume = incoming.volume;
                return result;
            }

            return add_order_slow(order_id, price_tick, volume, is_buy, type, tif);
        }

        bool cancel_order(std::uint64_t order_id) noexcept;
        ModifyResult modify_order(std::uint64_t order_id, std::uint32_t new_price_tick, std::uint32_t new_volume);

        std::uint32_t best_bid_tick() const noexcept;
        std::uint32_t best_ask_tick() const noexcept;
        bool has_best_bid() const noexcept { return best_bid_ != invalid_tick; }
        bool has_best_ask() const noexcept { return best_ask_ != invalid_tick; }
        std::uint64_t bid_volume(std::uint32_t price_tick) const noexcept;
        std::uint64_t ask_volume(std::uint32_t price_tick) const noexcept;

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

        AddResult add_order_slow(std::uint64_t order_id, std::uint32_t price_tick, std::uint32_t volume, bool is_buy,
                                 OrderType type, TimeInForce tif);
        bool rest_order(const Order &order, bool is_buy) noexcept;
        void match_buy(Order &incoming) noexcept;
        void match_sell(Order &incoming) noexcept;
        void push_receipt(const ExecutionReceipt &receipt) noexcept;
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
    };
}
