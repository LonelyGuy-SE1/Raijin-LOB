#pragma once

#include <atomic>
#include <cstddef>
#include <vector>
#include <stdexcept>

namespace raijin
{
    struct ExecutionReceipt
    {
        std::uint64_t maker_order_id;
        std::uint64_t taker_order_id;
        std::uint32_t price_tick;
        std::uint32_t executed_volume;
    };
    template <typename T>
    class RingBuffer
    {
    private:
        std::vector<T> buffer_;
        std::size_t mask_;

        // alignas(64) to prevent false sharing between producer and consumer cores.
        alignas(64) std::atomic<std::size_t> write_pos_{0};
        alignas(64) std::atomic<std::size_t> read_pos_{0};

    public:
        explicit RingBuffer(std::size_t capacity)
        {
            if (capacity == 0 || (capacity & (capacity - 1)) != 0)
            {
                throw std::invalid_argument("RingBuffer capacity must be a power of 2.");
            }
            buffer_.resize(capacity);
            mask_ = capacity - 1;
        }
        // prevent copying

        RingBuffer(const RingBuffer &) = delete;
        RingBuffer &operator=(const RingBuffer &) = delete;

        bool push(const T &item) noexcept
        {
            const std::size_t write_idx = write_pos_.load(std::memory_order_relaxed);
            const std::size_t read_idx = read_pos_.load(std::memory_order_acquire);

            // if buffer is full (write caught upto read form behind)
            if (write_idx - read_idx == buffer_.size())
            {
                return false;
            }
            buffer_[write_idx & mask_] = item;

            write_pos_.store(write_idx + 1, std::memory_order_release);
            return true;
        }

        bool pop(T &item) noexcept
        {
            const std::size_t read_idx = read_pos_.load(std::memory_order_relaxed);
            const std::size_t write_idx = write_pos_.load(std::memory_order_acquire);

            if (read_idx == write_idx)
            {
                return false;
            }
            item = buffer_[read_idx & mask_];
            read_pos_.store(read_idx + 1, std::memory_order_release);
            return true;
        }
    };

}
