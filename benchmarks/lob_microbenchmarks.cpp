#include <benchmark/benchmark.h>
#include "../include/core/limit_order_book.hpp"
#include "../include/core/ring_buffer.hpp"

using namespace raijin;

namespace
{
    constexpr std::uint32_t kTick = 5000;

    const BookConfig kPathConfig{
        .order_pool_capacity = 4096,
        .price_level_count = 8192,
        .level_queue_capacity = 256,
        .max_order_id = 100000};

    const BookConfig kMatchConfig{
        .order_pool_capacity = 1000000,
        .price_level_count = 8192,
        .level_queue_capacity = 4096,
        .max_order_id = 2000000};

    constexpr std::uint64_t kPrefillDepth = 4096;

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
    LimitOrderBook book(kPathConfig);
    for (auto _ : state)
    {
        bool ok = book.add_order(1, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        std::uint32_t best = book.best_bid_tick();
        benchmark::DoNotOptimize(best);
        book.cancel_order(1);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RestAdd_Bid);

static void BM_RestAdd_Ask(benchmark::State &state)
{
    LimitOrderBook book(kPathConfig);
    for (auto _ : state)
    {
        bool ok = book.add_order(1, kTick, 10, false);
        benchmark::DoNotOptimize(ok);
        std::uint32_t best = book.best_ask_tick();
        benchmark::DoNotOptimize(best);
        book.cancel_order(1);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_RestAdd_Ask);

static void BM_Cancel(benchmark::State &state)
{
    LimitOrderBook book(kPathConfig);
    for (auto _ : state)
    {
        book.add_order(1, kTick, 10, true);
        bool ok = book.cancel_order(1);
        benchmark::DoNotOptimize(ok);
        std::uint32_t best = book.best_bid_tick();
        benchmark::DoNotOptimize(best);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Cancel);

static void BM_BestBidAsk(benchmark::State &state)
{
    LimitOrderBook book(kPathConfig);
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

static void BM_SingleFill_NoReceipts(benchmark::State &state)
{
    LimitOrderBook book(kMatchConfig);
    std::uint64_t maker = 1;
    std::uint64_t taker = 1000000;
    book.add_order(maker++, kTick, 10, false);
    for (auto _ : state)
    {
        bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        std::uint64_t vol = book.ask_volume(kTick);
        benchmark::DoNotOptimize(vol);
        book.add_order(maker++, kTick, 10, false);
        std::uint32_t best = book.best_ask_tick();
        benchmark::DoNotOptimize(best);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SingleFill_NoReceipts);

static void BM_SingleFill_WithReceipts(benchmark::State &state)
{
    RingBuffer<ExecutionReceipt> rb(65536);
    LimitOrderBook book(kMatchConfig, &rb);
    std::uint64_t maker = 1;
    std::uint64_t taker = 1000000;
    book.add_order(maker++, kTick, 10, false);
    for (auto _ : state)
    {
        bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        std::uint64_t vol = book.ask_volume(kTick);
        benchmark::DoNotOptimize(vol);
        book.add_order(maker++, kTick, 10, false);
        std::uint32_t best = book.best_ask_tick();
        benchmark::DoNotOptimize(best);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SingleFill_WithReceipts);

static void BM_DeepFifo(benchmark::State &state)
{
    const int depth = static_cast<int>(state.range(0));
    const std::uint32_t vol = 10;
    LimitOrderBook book(kMatchConfig);
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
        bool ok = book.add_order(taker++, kTick, vol, true);
        benchmark::DoNotOptimize(ok);
        std::uint64_t vol_left = book.ask_volume(kTick);
        benchmark::DoNotOptimize(vol_left);
        book.add_order(maker++, kTick, vol, false);
        std::uint32_t best = book.best_ask_tick();
        benchmark::DoNotOptimize(best);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DeepFifo)->Arg(1)->Arg(8)->Arg(64)->Arg(256)->Arg(1024);

static void BM_MultiLevelSweep(benchmark::State &state)
{
    const std::uint32_t levels = 5;
    const std::uint32_t vol = 10;
    LimitOrderBook book(kMatchConfig);
    std::uint64_t id = 1;
    for (auto _ : state)
    {
        state.PauseTiming();
        for (std::uint32_t t = kTick; t < kTick + levels; ++t)
        {
            book.add_order(id++, t, vol, false);
        }
        state.ResumeTiming();
        bool ok = book.add_order(id++, kTick + levels - 1, vol * levels, true);
        benchmark::DoNotOptimize(ok);
        std::uint32_t best = book.best_ask_tick();
        benchmark::DoNotOptimize(best);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MultiLevelSweep);

static void BM_MatchThroughTombstones(benchmark::State &state)
{
    const int tombstones = static_cast<int>(state.range(0));
    LimitOrderBook book(kMatchConfig);
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
        bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        std::uint64_t vol = book.ask_volume(kTick);
        benchmark::DoNotOptimize(vol);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_MatchThroughTombstones)->Arg(0)->Arg(8)->Arg(64)->Arg(256);

static void BM_DeterministicMatch_NoReceipts(benchmark::State &state)
{
    LimitOrderBook book(kMatchConfig);
    if (!prefill_asks(book, kPrefillDepth, kTick, 10))
    {
        state.SkipWithError("prefill failed");
        return;
    }
    std::uint64_t taker = kPrefillDepth + 1;
    for (auto _ : state)
    {
        bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        std::uint64_t vol = book.ask_volume(kTick);
        benchmark::DoNotOptimize(vol);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DeterministicMatch_NoReceipts);

static void BM_DeterministicMatch_WithReceipts(benchmark::State &state)
{
    RingBuffer<ExecutionReceipt> rb(65536);
    LimitOrderBook book(kMatchConfig, &rb);
    if (!prefill_asks(book, kPrefillDepth, kTick, 10))
    {
        state.SkipWithError("prefill failed");
        return;
    }
    std::uint64_t taker = kPrefillDepth + 1;
    for (auto _ : state)
    {
        bool ok = book.add_order(taker++, kTick, 10, true);
        benchmark::DoNotOptimize(ok);
        std::uint64_t vol = book.ask_volume(kTick);
        benchmark::DoNotOptimize(vol);
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_DeterministicMatch_WithReceipts);

static void BM_Compare_Arka_AddNoMatch(benchmark::State &state)
{
    LimitOrderBook book(kPathConfig);
    std::uint64_t id = 1;
    for (auto _ : state)
    {
        const std::uint32_t tick = kTick + static_cast<std::uint32_t>(id % 20);
        bool ok = book.add_order(id, tick, 100, true);
        benchmark::DoNotOptimize(ok);
        std::uint32_t best = book.best_bid_tick();
        benchmark::DoNotOptimize(best);
        state.PauseTiming();
        book.cancel_order(id);
        ++id;
        state.ResumeTiming();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_Arka_AddNoMatch);

static void BM_Compare_Arka_CancelOnly(benchmark::State &state)
{
    LimitOrderBook book(kPathConfig);
    std::uint64_t id = 1;
    for (auto _ : state)
    {
        const std::uint32_t tick = kTick + static_cast<std::uint32_t>(id % 20);
        state.PauseTiming();
        book.add_order(id, tick, 100, true);
        state.ResumeTiming();
        bool ok = book.cancel_order(id);
        benchmark::DoNotOptimize(ok);
        std::uint32_t best = book.best_bid_tick();
        benchmark::DoNotOptimize(best);
        ++id;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_Arka_CancelOnly);

static void BM_Compare_Arka_MatchOneLevel(benchmark::State &state)
{
    LimitOrderBook book(kPathConfig);
    std::uint64_t id = 1;
    for (auto _ : state)
    {
        state.PauseTiming();
        book.add_order(id, kTick, 100, false);
        state.ResumeTiming();
        bool ok = book.add_order(id + 1, kTick, 100, true);
        benchmark::DoNotOptimize(ok);
        std::uint64_t vol = book.ask_volume(kTick);
        benchmark::DoNotOptimize(vol);
        state.PauseTiming();
        id += 2;
        state.ResumeTiming();
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_Arka_MatchOneLevel);

static void BM_Compare_NanoMatch_MixedAdd(benchmark::State &state)
{
    LimitOrderBook book(kMatchConfig);
    std::uint64_t id = 1;
    for (auto _ : state)
    {
        const bool is_buy = (id & 1) != 0;
        const std::uint32_t tick = kTick + static_cast<std::uint32_t>(id % 200);
        const std::uint32_t vol = static_cast<std::uint32_t>((id % 500) + 1);
        bool ok = book.add_order(id, tick, vol, is_buy);
        benchmark::DoNotOptimize(ok);
        ++id;
    }
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Compare_NanoMatch_MixedAdd);

BENCHMARK_MAIN();
