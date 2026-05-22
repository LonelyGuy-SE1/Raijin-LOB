#include "../../include/core/limit_order_book.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace raijin
{
    LimitOrderBook::LimitOrderBook(const BookConfig &config, RingBuffer<ExecutionReceipt> *receipt_queue)
        : config_(checked_config(config)),
          bid_pool_(config_.order_pool_capacity),
          ask_pool_(config_.order_pool_capacity),
          bid_levels_(config_.price_level_count),
          ask_levels_(config_.price_level_count),
          bid_orders_(static_cast<std::size_t>(config_.price_level_count) * config_.level_queue_capacity),
          ask_orders_(static_cast<std::size_t>(config_.price_level_count) * config_.level_queue_capacity),
          bid_words_(word_count(config_.price_level_count)),
          ask_words_(word_count(config_.price_level_count)),
          locators_(static_cast<std::size_t>(config_.max_order_id) + 1),
          best_bid_(invalid_tick),
          best_ask_(invalid_tick),
          receipt_queue_(receipt_queue)
    {
        for (std::uint32_t tick = 0; tick < config_.price_level_count; ++tick)
        {
            const std::size_t offset = static_cast<std::size_t>(tick) * config_.level_queue_capacity;
            bid_levels_[tick].bind(bid_orders_.data() + offset, config_.level_queue_capacity);
            ask_levels_[tick].bind(ask_orders_.data() + offset, config_.level_queue_capacity);
        }
    }

    bool LimitOrderBook::add_order(std::uint64_t order_id, std::uint32_t price_tick, std::uint32_t volume, bool is_buy)
    {
        if (volume == 0 || price_tick >= config_.price_level_count || !valid_order_id(order_id) || locators_[order_id].active != 0)
        {
            return false;
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
            return true;
        }

        return rest_order(incoming, is_buy);
    }

    bool LimitOrderBook::cancel_order(std::uint64_t order_id) noexcept
    {
        if (!valid_order_id(order_id))
        {
            return false;
        }

        Locator &locator = locators_[order_id];

        if (locator.active == 0)
        {
            return false;
        }

        OrderPool &pool = locator.side != 0 ? bid_pool_ : ask_pool_;

        if (pool.generation(locator.index) != locator.generation)
        {
            locator.active = 0;
            return false;
        }

        Order &order = pool.get_order(locator.index);
        PriceLevel &level = locator.side != 0 ? bid_levels_[order.price_tick] : ask_levels_[order.price_tick];

        level.remove_volume(order.volume);
        order.volume = 0;
        pool.deallocate(locator.index);
        locator.active = 0;

        if (level.total_volume() == 0)
        {
            level.clear();

            if (locator.side != 0)
            {
                erase_best_bid(order.price_tick);
            }
            else
            {
                erase_best_ask(order.price_tick);
            }
        }

        return true;
    }

    std::uint32_t LimitOrderBook::best_bid_tick() const noexcept
    {
        return best_bid_;
    }

    std::uint32_t LimitOrderBook::best_ask_tick() const noexcept
    {
        return best_ask_;
    }

    std::uint64_t LimitOrderBook::bid_volume(std::uint32_t price_tick) const noexcept
    {
        return price_tick < config_.price_level_count ? bid_levels_[price_tick].total_volume() : 0;
    }

    std::uint64_t LimitOrderBook::ask_volume(std::uint32_t price_tick) const noexcept
    {
        return price_tick < config_.price_level_count ? ask_levels_[price_tick].total_volume() : 0;
    }

    BookConfig LimitOrderBook::checked_config(const BookConfig &config)
    {
        const std::size_t max_slots = std::numeric_limits<std::size_t>::max() / sizeof(OrderRef);

        if (config.order_pool_capacity == 0 || config.price_level_count == 0 || config.level_queue_capacity == 0)
        {
            throw std::invalid_argument("invalid book dimensions");
        }

        if (!is_power_of_two(config.level_queue_capacity))
        {
            throw std::invalid_argument("level queue capacity must be power of two");
        }

        if (config.price_level_count > max_slots / config.level_queue_capacity)
        {
            throw std::invalid_argument("book queue storage overflow");
        }

        if (config.max_order_id == std::numeric_limits<std::uint64_t>::max() || config.max_order_id > std::numeric_limits<std::size_t>::max() - 1)
        {
            throw std::invalid_argument("invalid max order id");
        }

        return config;
    }

    bool LimitOrderBook::is_power_of_two(std::uint32_t value) noexcept
    {
        return value != 0 && (value & (value - 1)) == 0;
    }

    std::size_t LimitOrderBook::word_count(std::uint32_t price_level_count) noexcept
    {
        return (price_level_count + 63) >> 6;
    }

    void LimitOrderBook::set_bit(std::vector<std::uint64_t> &words, std::uint32_t tick) noexcept
    {
        words[tick >> 6] |= 1ULL << (tick & 63);
    }

    void LimitOrderBook::reset_bit(std::vector<std::uint64_t> &words, std::uint32_t tick) noexcept
    {
        words[tick >> 6] &= ~(1ULL << (tick & 63));
    }

    bool LimitOrderBook::rest_order(const Order &order, bool is_buy) noexcept
    {
        OrderPool &pool = is_buy ? bid_pool_ : ask_pool_;
        PriceLevel &level = is_buy ? bid_levels_[order.price_tick] : ask_levels_[order.price_tick];
        const PoolIndex index = pool.allocate_index();

        if (index == std::numeric_limits<PoolIndex>::max())
        {
            return false;
        }

        if (level.total_volume() == 0)
        {
            level.clear();
        }

        const OrderRef ref{index, pool.generation(index)};

        if (!level.push(ref))
        {
            level.compact([&pool](OrderRef old_ref) noexcept {
                return pool.generation(old_ref.index) == old_ref.generation && pool.get_order(old_ref.index).volume != 0;
            });

            if (!level.push(ref))
            {
                pool.deallocate(index);
                return false;
            }
        }

        pool.get_order(index) = order;
        level.add_volume(order.volume);
        locators_[order.order_id] = Locator{index, ref.generation, static_cast<std::uint8_t>(is_buy ? 1 : 0), 1};

        if (is_buy)
        {
            set_bit(bid_words_, order.price_tick);

            if (best_bid_ == invalid_tick || order.price_tick > best_bid_)
            {
                best_bid_ = order.price_tick;
            }
        }
        else
        {
            set_bit(ask_words_, order.price_tick);

            if (best_ask_ == invalid_tick || order.price_tick < best_ask_)
            {
                best_ask_ = order.price_tick;
            }
        }

        return true;
    }

    void LimitOrderBook::match_buy(Order &incoming) noexcept
    {
        while (incoming.volume != 0 && best_ask_ != invalid_tick && best_ask_ <= incoming.price_tick)
        {
            PriceLevel &level = ask_levels_[best_ask_];
            clean_front(level, ask_pool_);

            if (level.total_volume() == 0)
            {
                level.clear();
                erase_best_ask(best_ask_);
                continue;
            }

            const OrderRef ref = level.front();
            Order &resting = ask_pool_.get_order(ref.index);
            const std::uint32_t fill = std::min(incoming.volume, resting.volume);

            incoming.volume -= fill;
            resting.volume -= fill;
            level.remove_volume(fill);

            if (receipt_queue_){
                receipt_queue_->push({
                    .maker_order_id = resting.order_id,
                    .taker_order_id = incoming.order_id,
                    .price_tick = resting.price_tick,
                    .executed_volume = fill
                });
            }

            if (resting.volume == 0)
            {
                locators_[resting.order_id].active = 0;
                ask_pool_.deallocate(ref.index);
                level.pop();
            }

            if (level.total_volume() == 0)
            {
                level.clear();
                erase_best_ask(best_ask_);
            }
        }
    }

    void LimitOrderBook::match_sell(Order &incoming) noexcept
    {
        while (incoming.volume != 0 && best_bid_ != invalid_tick && incoming.price_tick <= best_bid_)
        {
            PriceLevel &level = bid_levels_[best_bid_];
            clean_front(level, bid_pool_);

            if (level.total_volume() == 0)
            {
                level.clear();
                erase_best_bid(best_bid_);
                continue;
            }

            const OrderRef ref = level.front();
            Order &resting = bid_pool_.get_order(ref.index);
            const std::uint32_t fill = std::min(incoming.volume, resting.volume);

            incoming.volume -= fill;
            resting.volume -= fill;
            level.remove_volume(fill);

            if(receipt_queue_){
                receipt_queue_->push({
                    .maker_order_id = resting.order_id,
                    .taker_order_id = incoming.order_id,
                    .price_tick = resting.price_tick,
                    .executed_volume = fill
                });
            }
            
            if (resting.volume == 0)
            {
                locators_[resting.order_id].active = 0;
                bid_pool_.deallocate(ref.index);
                level.pop();
            }

            if (level.total_volume() == 0)
            {
                level.clear();
                erase_best_bid(best_bid_);
            }
        }
    }

    void LimitOrderBook::clean_front(PriceLevel &level, OrderPool &pool) noexcept
    {
        std::uint32_t stale_pops = 0;

        while (!level.empty())
        {
            const OrderRef ref = level.front();

            if (pool.generation(ref.index) == ref.generation && pool.get_order(ref.index).volume != 0)
            {
                return;
            }

            level.pop();

            if (++stale_pops >= 8)
            {
                level.compact([&pool](OrderRef old_ref) noexcept {
                    return pool.generation(old_ref.index) == old_ref.generation &&
                           pool.get_order(old_ref.index).volume != 0;
                });
                return;
            }
        }
    }

    void LimitOrderBook::erase_best_ask(std::uint32_t tick) noexcept
    {
        reset_bit(ask_words_, tick);

        if (tick == best_ask_)
        {
            best_ask_ = next_ask(tick);
        }
    }

    void LimitOrderBook::erase_best_bid(std::uint32_t tick) noexcept
    {
        reset_bit(bid_words_, tick);

        if (tick == best_bid_)
        {
            best_bid_ = tick == 0 ? invalid_tick : next_bid(tick - 1);
        }
    }

    std::uint32_t LimitOrderBook::next_ask(std::uint32_t start) const noexcept
    {
        std::size_t word = start >> 6;

        if (word >= ask_words_.size())
        {
            return invalid_tick;
        }

        std::uint64_t bits = ask_words_[word] & (~0ULL << (start & 63));

        while (true)
        {
            if (bits != 0)
            {
                const std::uint32_t tick = static_cast<std::uint32_t>((word << 6) + __builtin_ctzll(bits));
                return tick < config_.price_level_count ? tick : invalid_tick;
            }

            ++word;

            if (word >= ask_words_.size())
            {
                return invalid_tick;
            }

            bits = ask_words_[word];
        }
    }

    std::uint32_t LimitOrderBook::next_bid(std::uint32_t start) const noexcept
    {
        std::size_t word = start >> 6;

        if (word >= bid_words_.size())
        {
            word = bid_words_.size() - 1;
        }

        std::uint64_t bits = bid_words_[word] & (~0ULL >> (63 - (start & 63)));

        while (true)
        {
            if (bits != 0)
            {
                return static_cast<std::uint32_t>((word << 6) + 63 - __builtin_clzll(bits));
            }

            if (word == 0)
            {
                return invalid_tick;
            }

            bits = bid_words_[--word];
        }
    }

    bool LimitOrderBook::valid_order_id(std::uint64_t order_id) const noexcept
    {
        return order_id < locators_.size();
    }
}
