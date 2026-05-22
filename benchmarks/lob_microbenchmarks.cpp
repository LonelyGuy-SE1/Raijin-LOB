#include <benchmark/benchmark.h>
#include "../include/core/limit_order_book.hpp"
#include "../include/core/ring_buffer.hpp"

using namespace raijin;

namespace
{
    constexpr std::uint32_t kTick = 5000;

    const BookConfig kBenchConfig{
        .order_pool_capacity = 4000000,
        .price_level_count = 8192,
        .level_queue_capacity = 4096,
        .max_order_id = 8000000};

    bool prefill_asks(LimitOrderBook &book, std::uint64_t count, std::uint32_t tick, std::uint32_t vol)
    {
        for (std::uint64_t i = 1; i <= count; ++i)
        {
            if (!book.add_order(i, tick, vol, false))
            {
                return false;
            }
        }
        return book.ask_volume(tick) == static_cast<std::uint64_t>(count) * vol;
    }
}

static void BM_RestAdd_Bid(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.PauseTiming();
        LimitOrderBook book(kBenchConfig);
        state.ResumeTiming();
        const bool ok = book.add_order(1, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.best_bid_tick());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RestAdd_Bid);

static void BM_RestAdd_Ask(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.PauseTiming();
        LimitOrderBook book(kBenchConfig);
        state.ResumeTiming();
        const bool ok = book.add_order(1, kTick, 10, false);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.best_ask_tick());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RestAdd_Ask);

static void BM_Cancel(benchmark::State &state)
{
    for (auto _ : state)
    {
        state.PauseTiming();
        LimitOrderBook book(kBenchConfig);
        book.add_order(1, kTick, 10, true);
        state.ResumeTiming();
        const bool ok = book.cancel_order(1);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.best_bid_tick());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Cancel);

static void BM_BestBidAsk(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    book.add_order(1, kTick, 10, true);
    book.add_order(2, kTick + 10, 10, false);
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(book.best_bid_tick());
        benchmark::DoNotOptimize(book.best_ask_tick());
    }
    state.SetItemsProcessed(state.iterations() * 2);
}
BENCHMARK(BM_BestBidAsk);

static void BM_SingleFill_NoReceipts(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    std::uint64_t maker = 1;
    std::uint64_t taker = 1000000;
    book.add_order(maker++, kTick, 10, false);
    for (auto _ : state)
    {
        const bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.ask_volume(kTick));
        book.add_order(maker++, kTick, 10, false);
        benchmark::DoNotOptimize(book.best_ask_tick());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SingleFill_NoReceipts);

static void BM_SingleFill_WithReceipts(benchmark::State &state)
{
    RingBuffer<ExecutionReceipt> rb(1048576);
    LimitOrderBook book(kBenchConfig, &rb);
    std::uint64_t maker = 1;
    std::uint64_t taker = 1000000;
    book.add_order(maker++, kTick, 10, false);
    for (auto _ : state)
    {
        const bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.ask_volume(kTick));
        book.add_order(maker++, kTick, 10, false);
        benchmark::DoNotOptimize(book.best_ask_tick());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SingleFill_WithReceipts);

static void BM_DeepFifo(benchmark::State &state)
{
    const int depth = static_cast<int>(state.range(0));
    const std::uint32_t vol = 10;
    LimitOrderBook book(kBenchConfig);
    for (int i = 1; i <= depth; ++i)
    {
        if (!book.add_order(static_cast<std::uint64_t>(i), kTick, vol, false))
        {
            state.SkipWithError("prefill failed");
            return;
        }
    }
    if (book.ask_volume(kTick) != static_cast<std::uint64_t>(depth) * vol)
    {
        state.SkipWithError("prefill volume mismatch");
        return;
    }
    std::uint64_t taker = static_cast<std::uint64_t>(depth) + 1;
    std::uint64_t maker = static_cast<std::uint64_t>(depth) + 1000000;
    for (auto _ : state)
    {
        const bool ok = book.add_order(taker++, kTick, vol, true);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.ask_volume(kTick));
        book.add_order(maker++, kTick, vol, false);
        benchmark::DoNotOptimize(book.best_ask_tick());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DeepFifo)->Arg(1)->Arg(8)->Arg(64)->Arg(256)->Arg(1024);

static void BM_MultiLevelSweep(benchmark::State &state)
{
    const std::uint32_t levels = 5;
    const std::uint32_t vol = 10;
    LimitOrderBook book(kBenchConfig);
    std::uint64_t id = 1;
    for (auto _ : state)
    {
        state.PauseTiming();
        for (std::uint32_t t = kTick; t < kTick + levels; ++t)
        {
            book.add_order(id++, t, vol, false);
        }
        state.ResumeTiming();
        const bool ok = book.add_order(id++, kTick + levels - 1, vol * levels, true);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.best_ask_tick());
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MultiLevelSweep);

static void BM_MatchThroughTombstones(benchmark::State &state)
{
    const int tombstones = static_cast<int>(state.range(0));
    LimitOrderBook book(kBenchConfig);
    std::uint64_t id = 1;
    std::uint64_t taker = 1000000;
    for (auto _ : state)
    {
        state.PauseTiming();
        for (int i = 0; i < tombstones; ++i)
        {
            book.add_order(id, kTick, 10, false);
            book.cancel_order(id++);
        }
        book.add_order(id++, kTick, 10, false);
        state.ResumeTiming();
        const bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.ask_volume(kTick));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchThroughTombstones)->Arg(0)->Arg(8)->Arg(64)->Arg(256);

static void BM_DeterministicMatch_NoReceipts(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    if (!prefill_asks(book, 1000000, kTick, 10))
    {
        state.SkipWithError("prefill failed");
        return;
    }
    std::uint64_t taker = 1000001;
    for (auto _ : state)
    {
        const bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.ask_volume(kTick));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DeterministicMatch_NoReceipts);

static void BM_DeterministicMatch_WithReceipts(benchmark::State &state)
{
    RingBuffer<ExecutionReceipt> rb(1048576);
    LimitOrderBook book(kBenchConfig, &rb);
    if (!prefill_asks(book, 1000000, kTick, 10))
    {
        state.SkipWithError("prefill failed");
        return;
    }
    std::uint64_t taker = 1000001;
    for (auto _ : state)
    {
        const bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        benchmark::DoNotOptimize(book.ask_volume(kTick));
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DeterministicMatch_WithReceipts);

BENCHMARK_MAIN();
