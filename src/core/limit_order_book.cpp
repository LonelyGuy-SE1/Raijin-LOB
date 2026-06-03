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

    AddResult LimitOrderBook::add_order_slow(std::uint64_t order_id, std::uint32_t price_tick, std::uint32_t volume,
                                             bool is_buy, OrderType type, TimeInForce tif)
    {
        AddResult result{};
        result.valid = true;

        if (volume == 0 || !valid_order_id(order_id) || locators_[order_id].active != 0)
        {
            result.valid = false;
            return result;
        }

        if (type == OrderType::Limit && price_tick >= config_.price_level_count)
        {
            result.valid = false;
            return result;
        }

        if (tif == TimeInForce::FOK)
        {
            std::uint32_t available = 0;
            if (is_buy)
            {
                std::uint32_t tick = best_ask_;
                while (tick != invalid_tick)
                {
                    if (type == OrderType::Limit && tick > price_tick) break;
                    available += static_cast<std::uint32_t>(ask_levels_[tick].total_volume());
                    if (available >= volume) break;
                    const std::size_t word = tick >> 6;
                    const std::uint32_t pos = tick & 63;
                    std::uint64_t bits = ask_words_[word];
                    bits &= ~((1ULL << pos) - 1);
                    bits &= ~(1ULL << pos);
                    if (bits != 0)
                    {
                        tick = static_cast<std::uint32_t>((word << 6) + __builtin_ctzll(bits));
                        continue;
                    }
                    tick = invalid_tick;
                    for (std::size_t w = word + 1; w < ask_words_.size(); ++w)
                    {
                        if (ask_words_[w] != 0)
                        {
                            tick = static_cast<std::uint32_t>((w << 6) + __builtin_ctzll(ask_words_[w]));
                            break;
                        }
                    }
                }
            }
            else
            {
                std::uint32_t tick = best_bid_;
                while (tick != invalid_tick)
                {
                    if (type == OrderType::Limit && tick < price_tick) break;
                    available += static_cast<std::uint32_t>(bid_levels_[tick].total_volume());
                    if (available >= volume) break;
                    if (tick == 0) break;
                    tick = next_bid(tick - 1);
                }
            }
            if (available < volume)
            {
                result.valid = true;
                result.accepted = false;
                result.dropped_volume = volume;
                return result;
            }
        }

        const std::uint32_t cross_tick = type == OrderType::Market
                                             ? (is_buy ? config_.price_level_count - 1 : 0)
                                             : price_tick;

        Order incoming{order_id, volume, cross_tick};

        if (is_buy)
        {
            match_buy(incoming);
        }
        else
        {
            match_sell(incoming);
        }

        result.matched_volume = volume - incoming.volume;

        if (type == OrderType::Market || tif == TimeInForce::IOC)
        {
            if (incoming.volume > 0)
            {
                result.dropped_volume += incoming.volume;
                incoming.volume = 0;
            }
            result.accepted = result.matched_volume > 0;
            return result;
        }

        if (incoming.volume == 0)
        {
            result.accepted = true;
            result.rested_volume = 0;
            return result;
        }

        bool rested = rest_order(incoming, is_buy);

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

    ModifyResult LimitOrderBook::modify_order(std::uint64_t order_id, std::uint32_t new_price_tick,
                                              std::uint32_t new_volume)
    {
        ModifyResult result{};

        if (!valid_order_id(order_id) || new_volume == 0 || new_price_tick >= config_.price_level_count)
        {
            result.valid = false;
            return result;
        }

        result.valid = true;

        Locator &locator = locators_[order_id];

        if (locator.active == 0)
        {
            return result;
        }

        OrderPool &pool = locator.side != 0 ? bid_pool_ : ask_pool_;

        if (pool.generation(locator.index) != locator.generation)
        {
            locator.active = 0;
            return result;
        }

        Order &order = pool.get_order(locator.index);
        const bool is_buy = locator.side != 0;
        const std::uint32_t old_price = order.price_tick;
        PriceLevel &level = is_buy ? bid_levels_[old_price] : ask_levels_[old_price];

        level.remove_volume(order.volume);
        order.volume = 0;
        pool.deallocate(locator.index);
        locator.active = 0;

        if (level.total_volume() == 0)
        {
            level.clear();
            if (is_buy)
            {
                erase_best_bid(old_price);
            }
            else
            {
                erase_best_ask(old_price);
            }
        }

        result.cancelled = true;

        AddResult add_result = add_order(order_id, new_price_tick, new_volume, is_buy);
        result.reested = add_result.rested_volume > 0;
        result.dropped_volume = add_result.dropped_volume;

        return result;
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

    void LimitOrderBook::set_bit(std::vector<std::uint64_t> &words, std::uint32_t tick, std::uint32_t &active_levels) noexcept
    {
        const std::size_t word = tick >> 6;
        const std::uint64_t mask = 1ULL << (tick & 63);
        if ((words[word] & mask) == 0)
        {
            words[word] |= mask;
            ++active_levels;
        }
    }

    void LimitOrderBook::reset_bit(std::vector<std::uint64_t> &words, std::uint32_t tick, std::uint32_t &active_levels) noexcept
    {
        const std::size_t word = tick >> 6;
        const std::uint64_t mask = 1ULL << (tick & 63);
        if ((words[word] & mask) != 0)
        {
            words[word] &= ~mask;
            --active_levels;
        }
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
            set_bit(bid_words_, order.price_tick, bid_active_levels_);

            if (best_bid_ == invalid_tick || order.price_tick > best_bid_)
            {
                best_bid_ = order.price_tick;
            }
        }
        else
        {
            set_bit(ask_words_, order.price_tick, ask_active_levels_);

            if (best_ask_ == invalid_tick || order.price_tick < best_ask_)
            {
                best_ask_ = order.price_tick;
            }
        }

        return true;
    }

    void LimitOrderBook::match_buy(Order &incoming) noexcept
    {
        while (incoming.volume != 0 && best_ask_ != invalid_tick)
        {
            if (incoming.price_tick < best_ask_)
            {
                break;
            }

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

            const ExecutionReceipt receipt{
                .maker_order_id = resting.order_id,
                .taker_order_id = incoming.order_id,
                .price_tick = resting.price_tick,
                .executed_volume = fill};
            push_receipt(receipt);

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
        while (incoming.volume != 0 && best_bid_ != invalid_tick)
        {
            if (incoming.price_tick > best_bid_)
            {
                break;
            }

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

            const ExecutionReceipt receipt{
                .maker_order_id = resting.order_id,
                .taker_order_id = incoming.order_id,
                .price_tick = resting.price_tick,
                .executed_volume = fill};
            push_receipt(receipt);

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

    void LimitOrderBook::push_receipt(const ExecutionReceipt &receipt) noexcept
    {
        if (receipt_queue_)
        {
            receipt_queue_->push(receipt);
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
        reset_bit(ask_words_, tick, ask_active_levels_);

        if (tick == best_ask_)
        {
            best_ask_ = (ask_active_levels_ == 0) ? invalid_tick : next_ask(tick);
        }
    }

    void LimitOrderBook::erase_best_bid(std::uint32_t tick) noexcept
    {
        reset_bit(bid_words_, tick, bid_active_levels_);

        if (tick == best_bid_)
        {
            best_bid_ = (bid_active_levels_ == 0) ? invalid_tick : (tick == 0 ? invalid_tick : next_bid(tick - 1));
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
