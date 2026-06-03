#include <benchmark/benchmark.h>
#include "../include/core/limit_order_book.hpp"
#include "../include/core/ring_buffer.hpp"
#include <chrono>
#include <memory>

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

BENCHMARK_MAIN();
