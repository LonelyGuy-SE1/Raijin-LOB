#include <benchmark/benchmark.h>
#include "../include/core/limit_order_book.hpp"
#include "../include/core/ring_buffer.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <random>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif

using namespace raijin;

namespace
{
    constexpr std::uint32_t kTick = 5000;

    const BookConfig kBenchConfig{
        .order_pool_capacity = 100000,
        .price_level_count = 8192,
        .level_queue_capacity = 512,
        .max_order_id = 500000};

    inline std::uint64_t rdtsc_start()
    {
        _mm_lfence();
        return __rdtsc();
    }

    inline std::uint64_t rdtscp_end()
    {
        std::uint32_t aux;
        std::uint64_t tsc = __rdtscp(&aux);
        _mm_lfence();
        return tsc;
    }

    void report_percentiles(std::vector<std::uint64_t> &samples, benchmark::State &state)
    {
        std::sort(samples.begin(), samples.end());
        const std::size_t n = samples.size();
        state.counters["p50"] = static_cast<double>(samples[n * 50 / 100]);
        state.counters["p90"] = static_cast<double>(samples[n * 90 / 100]);
        state.counters["p99"] = static_cast<double>(samples[n * 99 / 100]);
        state.counters["p999"] = static_cast<double>(samples[n * 999 / 1000]);
        state.counters["max"] = static_cast<double>(samples.back());
        state.counters["min"] = static_cast<double>(samples.front());
    }
}

static void BM_Hist_AddNoMatch(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    std::vector<std::uint64_t> ids;
    std::vector<std::uint32_t> ticks;
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::uint64_t> id_dist(1, kBenchConfig.max_order_id);
    std::uniform_int_distribution<std::uint32_t> tick_dist(0, kBenchConfig.price_level_count - 1);

    constexpr std::uint32_t kPreGen = 100000;
    ids.reserve(kPreGen);
    ticks.reserve(kPreGen);
    for (std::uint32_t i = 0; i < kPreGen; ++i)
    {
        ids.push_back(id_dist(rng));
        ticks.push_back(tick_dist(rng));
    }

    std::vector<std::uint64_t> samples;
    samples.reserve(state.iterations());

    std::uint32_t idx = 0;
    for (auto _ : state)
    {
        const std::uint64_t id = ids[idx % kPreGen];
        const std::uint32_t tick = ticks[idx % kPreGen];
        std::uint64_t t0 = rdtsc_start();
        book.add_order(id, tick, 10, (id & 1) != 0);
        std::uint64_t t1 = rdtscp_end();
        samples.push_back(t1 - t0);
        book.cancel_order(id);
        ++idx;
    }

    report_percentiles(samples, state);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Hist_AddNoMatch)->Iterations(100000);

static void BM_Hist_Cancel(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    std::vector<std::uint64_t> prefill_ids;
    std::vector<std::uint32_t> prefill_ticks;
    std::mt19937 rng(42);
    std::uniform_int_distribution<std::uint64_t> id_dist(1, kBenchConfig.max_order_id);
    std::uniform_int_distribution<std::uint32_t> tick_dist(0, kBenchConfig.price_level_count - 1);

    constexpr std::uint32_t kPreGen = 100000;
    prefill_ids.reserve(kPreGen);
    prefill_ticks.reserve(kPreGen);
    for (std::uint32_t i = 0; i < kPreGen; ++i)
    {
        prefill_ids.push_back(id_dist(rng));
        prefill_ticks.push_back(tick_dist(rng));
        book.add_order(prefill_ids.back(), prefill_ticks.back(), 10, (prefill_ids.back() & 1) != 0);
    }

    std::vector<std::uint64_t> samples;
    samples.reserve(state.iterations());

    std::uint32_t idx = 0;
    for (auto _ : state)
    {
        const std::uint64_t id = prefill_ids[idx % kPreGen];
        std::uint64_t t0 = rdtsc_start();
        book.cancel_order(id);
        std::uint64_t t1 = rdtscp_end();
        samples.push_back(t1 - t0);
        book.add_order(id, prefill_ticks[idx % kPreGen], 10, (id & 1) != 0);
        ++idx;
    }

    report_percentiles(samples, state);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Hist_Cancel)->Iterations(100000);

static void BM_Hist_MatchOneLevel(benchmark::State &state)
{
    LimitOrderBook book(kBenchConfig);
    std::vector<std::uint64_t> samples;
    samples.reserve(state.iterations());

    std::uint64_t id = 1;
    for (auto _ : state)
    {
        book.add_order(id, kTick, 100, false);
        std::uint64_t t0 = rdtsc_start();
        book.add_order(id + 1, kTick, 100, true);
        std::uint64_t t1 = rdtscp_end();
        samples.push_back(t1 - t0);
        book.add_order(id, kTick, 100, false);
        id += 2;
    }

    report_percentiles(samples, state);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Hist_MatchOneLevel)->Iterations(100000);

static void BM_Hist_MatchWithReceipts(benchmark::State &state)
{
    RingBuffer<ExecutionReceipt> rb(65536);
    LimitOrderBook book(kBenchConfig, &rb);
    std::vector<std::uint64_t> samples;
    samples.reserve(state.iterations());

    std::uint64_t id = 1;
    for (auto _ : state)
    {
        book.add_order(id, kTick, 100, false);
        std::uint64_t t0 = rdtsc_start();
        book.add_order(id + 1, kTick, 100, true);
        std::uint64_t t1 = rdtscp_end();
        samples.push_back(t1 - t0);
        book.add_order(id, kTick, 100, false);
        id += 2;
    }

    report_percentiles(samples, state);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Hist_MatchWithReceipts)->Iterations(100000);

static void BM_Hist_TombstoneMatch(benchmark::State &state)
{
    constexpr std::uint32_t kTombstones = 256;
    constexpr std::uint64_t kMakerId = 2;
    constexpr std::uint64_t kTakerId = 3;
    constexpr std::uint64_t kTombstoneBase = 1000;
    constexpr std::uint32_t kTombstoneTick = 100;

    BookConfig tombConfig{
        .order_pool_capacity = 1024,
        .price_level_count = 256,
        .level_queue_capacity = 512,
        .max_order_id = 5000};

    std::vector<std::uint64_t> samples;
    samples.reserve(state.iterations());

    LimitOrderBook book(tombConfig);
    for (auto _ : state)
    {
        for (std::uint32_t i = 0; i < kTombstones; ++i)
        {
            book.add_order(kTombstoneBase + static_cast<std::uint64_t>(i), kTombstoneTick, 10, false);
        }
        book.add_order(kMakerId, kTombstoneTick, 10, false);
        for (std::uint32_t i = 0; i < kTombstones; ++i)
        {
            book.cancel_order(kTombstoneBase + static_cast<std::uint64_t>(i));
        }

        std::uint64_t t0 = rdtsc_start();
        book.add_order(kTakerId, kTombstoneTick, 10, true);
        std::uint64_t t1 = rdtscp_end();
        samples.push_back(t1 - t0);
    }

    report_percentiles(samples, state);
    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_Hist_TombstoneMatch)->Iterations(100000);

BENCHMARK_MAIN();
