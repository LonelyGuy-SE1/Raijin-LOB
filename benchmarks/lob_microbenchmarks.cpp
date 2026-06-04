#include <benchmark/benchmark.h>
#include "../include/core/limit_order_book.hpp"
#include "../include/core/ring_buffer.hpp"
#include <chrono>
#include <memory>
#include <random>
#include <vector>

using namespace raijin;

namespace
{
    constexpr std::uint32_t kTick = 5000;

    const BookConfig kBenchConfig{
        .order_pool_capacity = 100000,
        .price_level_count = 8192,
        .level_queue_capacity = 512,
        .max_order_id = 500000};

    const BookConfig kTombstoneBenchConfig{
        .order_pool_capacity = 1024,
        .price_level_count = 256,
        .level_queue_capacity = 512,
        .max_order_id = 5000};

    constexpr std::uint32_t kTombstoneTick = 100;

    double elapsed_seconds(const std::chrono::steady_clock::time_point &t0,
                           const std::chrono::steady_clock::time_point &t1)
    {
        return std::chrono::duration<double>(t1 - t0).count();
    }
}

static void BM_BestBidAsk(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    book.add_order(1, kTick, 10, true);
    book.add_order(2, kTick + 10, 10, false);
    for (auto _ : state)
    {
        std::uint32_t bid = book.best_bid_tick();
        std::uint32_t ask = book.best_ask_tick();
        benchmark::DoNotOptimize(bid);
        benchmark::DoNotOptimize(ask);
    }
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_BestBidAsk);

static void BM_Compare_Arka_AddNoMatch(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    std::uint32_t rot = 0;
    for (auto _ : state)
    {
        const std::uint32_t tick = kTick + (rot % 20);
        const auto t0 = std::chrono::steady_clock::now();
        bool ok = book.add_order(1, tick, 100, true).accepted;
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(ok);
        state.SetIterationTime(elapsed_seconds(t0, t1));
        book.cancel_order(1);
        ++rot;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_Arka_AddNoMatch)->UseManualTime();

static void BM_Compare_Arka_CancelOnly(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    std::uint32_t rot = 0;
    book.add_order(1, kTick, 100, true);
    for (auto _ : state)
    {
        const auto t0 = std::chrono::steady_clock::now();
        book.cancel_order(1);
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(true);
        state.SetIterationTime(elapsed_seconds(t0, t1));
        book.add_order(1, kTick + (rot % 20), 100, true);
        ++rot;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_Arka_CancelOnly)->UseManualTime();

static void BM_Compare_Arka_MatchOneLevel(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    book.add_order(1, kTick, 100, false);
    for (auto _ : state)
    {
        const auto t0 = std::chrono::steady_clock::now();
        bool ok = book.add_order(2, kTick, 100, true).accepted;
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(ok);
        state.SetIterationTime(elapsed_seconds(t0, t1));
        book.add_order(1, kTick, 100, false);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_Arka_MatchOneLevel)->UseManualTime();

static void BM_Compare_Arka_MatchWithReceipts(benchmark::State &state)
{
    RingBuffer<ExecutionReceipt> rb(65536);
    LimitOrderBook book(kBenchConfig, &rb);
    book.add_order(1, kTick, 100, false);
    for (auto _ : state)
    {
        const auto t0 = std::chrono::steady_clock::now();
        bool ok = book.add_order(2, kTick, 100, true).accepted;
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(ok);
        state.SetIterationTime(elapsed_seconds(t0, t1));
        book.add_order(1, kTick, 100, false);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_Arka_MatchWithReceipts)->UseManualTime();

static void BM_Compare_NanoMatch_MixedAdd(benchmark::State &state)
{
    std::unique_ptr<LimitOrderBook> book = std::make_unique<LimitOrderBook>(kBenchConfig);
    std::uint64_t id = 1;
    for (auto _ : state)
    {
        if (id > kBenchConfig.max_order_id || id % 4096 == 0)
        {
            state.PauseTiming();
            book = std::make_unique<LimitOrderBook>(kBenchConfig);
            if (id > kBenchConfig.max_order_id)
            {
                id = 1;
            }
            state.ResumeTiming();
        }
        const bool is_buy = (id & 1) != 0;
        const std::uint32_t tick = kTick + static_cast<std::uint32_t>(id % 200);
        const std::uint32_t vol = static_cast<std::uint32_t>((id % 500) + 1);
        const auto t0 = std::chrono::steady_clock::now();
        bool ok = book->add_order(id, tick, vol, is_buy).accepted;
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(ok);
        state.SetIterationTime(elapsed_seconds(t0, t1));
        ++id;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_NanoMatch_MixedAdd)->UseManualTime();

static void BM_MultiLevelSweep(benchmark::State &state)
{
    constexpr std::uint32_t kLevels = 5;
    constexpr std::uint32_t kVol = 10;
    LimitOrderBook book(kBenchConfig);
    for (auto _ : state)
    {
        state.PauseTiming();
        book.add_order(1, kTick, kVol, false);
        book.add_order(2, kTick + 1, kVol, false);
        book.add_order(3, kTick + 2, kVol, false);
        book.add_order(4, kTick + 3, kVol, false);
        book.add_order(5, kTick + 4, kVol, false);
        state.ResumeTiming();
        const auto t0 = std::chrono::steady_clock::now();
        bool ok = book.add_order(6, kTick + kLevels - 1, kVol * kLevels, true).accepted;
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(ok);
        state.SetIterationTime(elapsed_seconds(t0, t1));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MultiLevelSweep)->UseManualTime();

static void BM_MatchThroughTombstones(benchmark::State &state)
{
    const int tombstones = static_cast<int>(state.range(0));
    constexpr std::uint64_t kMakerId = 2;
    constexpr std::uint64_t kTakerId = 3;
    constexpr std::uint64_t kTombstoneBase = 1000;

    LimitOrderBook book(kTombstoneBenchConfig);
    for (auto _ : state)
    {
        state.PauseTiming();
        for (int i = 0; i < tombstones; ++i)
        {
            book.add_order(kTombstoneBase + static_cast<std::uint64_t>(i), kTombstoneTick, 10, false);
        }
        book.add_order(kMakerId, kTombstoneTick, 10, false);
        for (int i = 0; i < tombstones; ++i)
        {
            book.cancel_order(kTombstoneBase + static_cast<std::uint64_t>(i));
        }
        state.ResumeTiming();
        const auto t0 = std::chrono::steady_clock::now();
        bool ok = book.add_order(kTakerId, kTombstoneTick, 10, true).accepted;
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(ok);
        state.SetIterationTime(elapsed_seconds(t0, t1));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchThroughTombstones)
    ->Arg(0)
    ->Arg(8)
    ->Arg(64)
    ->Arg(256)
    ->UseManualTime()
    ->MinTime(0.5);

static void BM_RandomAdd(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    constexpr std::uint32_t kCount = 100000;
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::uint64_t> id_dist(1, kBenchConfig.max_order_id);
    std::uniform_int_distribution<std::uint32_t> tick_dist(0, kBenchConfig.price_level_count - 1);

    std::vector<std::uint64_t> ids;
    std::vector<std::uint32_t> ticks;
    ids.reserve(kCount);
    ticks.reserve(kCount);
    for (std::uint32_t i = 0; i < kCount; ++i)
    {
        ids.push_back(id_dist(rng));
        ticks.push_back(tick_dist(rng));
    }

    std::uint32_t idx = 0;
    for (auto _ : state)
    {
        const std::uint64_t id = ids[idx % kCount];
        const std::uint32_t tick = ticks[idx % kCount];
        const auto t0 = std::chrono::steady_clock::now();
        bool ok = book.add_order(id, tick, 10, (id & 1) != 0).accepted;
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(ok);
        state.SetIterationTime(elapsed_seconds(t0, t1));
        book.cancel_order(id);
        ++idx;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RandomAdd)->UseManualTime();

static void BM_RandomCancel(benchmark::State &state)
{
    constexpr std::uint32_t kPreFill = 50000;
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::uint64_t> id_dist(1, kBenchConfig.max_order_id);
    std::uniform_int_distribution<std::uint32_t> tick_dist(0, kBenchConfig.price_level_count - 1);

    std::vector<std::uint64_t> ids;
    std::vector<std::uint32_t> ticks;
    ids.reserve(kPreFill);
    ticks.reserve(kPreFill);
    for (std::uint32_t i = 0; i < kPreFill; ++i)
    {
        ids.push_back(id_dist(rng));
        ticks.push_back(tick_dist(rng));
    }

    LimitOrderBook book(kBenchConfig);
    for (std::uint32_t i = 0; i < kPreFill; ++i)
    {
        book.add_order(ids[i], ticks[i], 10, (ids[i] & 1) != 0);
    }

    std::mt19937 cancel_rng(123);
    std::vector<std::uint64_t> cancel_ids;
    cancel_ids.reserve(kPreFill);
    for (std::uint32_t i = 0; i < kPreFill; ++i)
    {
        cancel_ids.push_back(ids[cancel_rng() % kPreFill]);
    }

    std::uint32_t idx = 0;
    for (auto _ : state)
    {
        const std::uint64_t id = cancel_ids[idx % kPreFill];
        const auto t0 = std::chrono::steady_clock::now();
        book.cancel_order(id);
        const auto t1 = std::chrono::steady_clock::now();
        state.SetIterationTime(elapsed_seconds(t0, t1));
        book.add_order(id, ticks[idx % kPreFill], 10, (id & 1) != 0);
        ++idx;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RandomCancel)->UseManualTime();

static void BM_RandomMatch(benchmark::State &state)
{
    constexpr std::uint32_t kPreFill = 50000;
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::uint64_t> id_dist(1, kBenchConfig.max_order_id);
    std::uniform_int_distribution<std::uint32_t> tick_dist(0, kBenchConfig.price_level_count - 1);

    std::vector<std::uint64_t> rest_ids;
    std::vector<std::uint32_t> rest_ticks;
    rest_ids.reserve(kPreFill);
    rest_ticks.reserve(kPreFill);
    for (std::uint32_t i = 0; i < kPreFill; ++i)
    {
        rest_ids.push_back(id_dist(rng));
        rest_ticks.push_back(tick_dist(rng));
    }

    LimitOrderBook book(kBenchConfig);
    for (std::uint32_t i = 0; i < kPreFill; ++i)
    {
        book.add_order(rest_ids[i], rest_ticks[i], 100, (rest_ids[i] & 1) != 0);
    }

    std::mt19937 taker_rng(123);
    std::vector<std::uint64_t> taker_ids;
    std::vector<std::uint32_t> taker_ticks;
    std::vector<bool> taker_sides;
    taker_ids.reserve(kPreFill);
    taker_ticks.reserve(kPreFill);
    taker_sides.reserve(kPreFill);
    for (std::uint32_t i = 0; i < kPreFill; ++i)
    {
        taker_ids.push_back(id_dist(taker_rng));
        taker_ticks.push_back(rest_ticks[taker_rng() % kPreFill]);
        taker_sides.push_back((taker_ids.back() & 1) == 0);
    }

    std::uint32_t idx = 0;
    for (auto _ : state)
    {
        const std::uint64_t id = taker_ids[idx % kPreFill];
        const std::uint32_t tick = taker_ticks[idx % kPreFill];
        const bool side = taker_sides[idx % kPreFill];
        const auto t0 = std::chrono::steady_clock::now();
        bool ok = book.add_order(id, tick, 10, side).accepted;
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(ok);
        state.SetIterationTime(elapsed_seconds(t0, t1));
        ++idx;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RandomMatch)->UseManualTime();

static void BM_ReplaySynthetic(benchmark::State &state)
{
    constexpr std::uint32_t kMessages = 1000000;
    constexpr double kAddFrac = 0.70;
    constexpr double kCancelFrac = 0.25;

    std::mt19937 rng(42);
    std::uniform_int_distribution<std::uint64_t> id_dist(1, kBenchConfig.max_order_id);
    std::uniform_int_distribution<std::uint32_t> tick_dist(0, kBenchConfig.price_level_count - 1);
    std::uniform_real_distribution<double> op_dist(0.0, 1.0);

    std::vector<std::uint64_t> ids;
    std::vector<std::uint32_t> ticks;
    std::vector<bool> sides;
    std::vector<char> ops;
    ids.reserve(kMessages);
    ticks.reserve(kMessages);
    sides.reserve(kMessages);
    ops.reserve(kMessages);

    std::vector<std::uint64_t> active_ids;
    for (std::uint32_t i = 0; i < kMessages; ++i)
    {
        const double op = op_dist(rng);
        const std::uint64_t id = id_dist(rng);
        const std::uint32_t tick = tick_dist(rng);
        const bool side = (id & 1) != 0;
        ids.push_back(id);
        ticks.push_back(tick);
        sides.push_back(side);
        if (op < kAddFrac)
        {
            ops.push_back('A');
            active_ids.push_back(id);
        }
        else if (op < kAddFrac + kCancelFrac)
        {
            ops.push_back('C');
        }
        else
        {
            ops.push_back('M');
        }
    }

    for (auto _ : state)
    {
        LimitOrderBook book(kBenchConfig);
        std::vector<std::uint64_t> live;
        live.reserve(kMessages / 2);

        const auto t0 = std::chrono::steady_clock::now();
        for (std::uint32_t i = 0; i < kMessages; ++i)
        {
            switch (ops[i])
            {
            case 'A':
            {
                auto result = book.add_order(ids[i], ticks[i], 10, sides[i]);
                if (result.accepted)
                {
                    live.push_back(ids[i]);
                }
                break;
            }
            case 'C':
                if (!live.empty())
                {
                    book.cancel_order(live.back());
                    live.pop_back();
                }
                break;
            case 'M':
            {
                const bool taker_side = !sides[i];
                book.add_order(ids[i] + kMessages, ticks[i], 10, taker_side);
                break;
            }
            }
        }
        const auto t1 = std::chrono::steady_clock::now();
        benchmark::DoNotOptimize(book);
        state.SetIterationTime(elapsed_seconds(t0, t1));
    }
    state.SetItemsProcessed(state.iterations() * kMessages);
}
BENCHMARK(BM_ReplaySynthetic)->UseManualTime()->MinTime(0.5);

BENCHMARK_MAIN();
