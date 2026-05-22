#include <benchmark/benchmark.h>
#include "../include/core/limit_order_book.hpp"
#include "../include/core/ring_buffer.hpp"

using namespace raijin;

// Large capacity to ensure we are measuring raw matching speed, not pool/queue limits.
static BookConfig bench_config{
    .order_pool_capacity = 2000000,
    .price_level_count = 100,
    .level_queue_capacity = 1048576, 
    .max_order_id = 4000000
};

static void BM_DeterministicMatch_NoReceipts(benchmark::State& state) {
    LimitOrderBook book(bench_config);
    // Pre-fill with 1,000,000 resting orders
    for (uint64_t i = 1; i <= 1000000; ++i) {
        book.add_order(i, 5000, 10, false);
    }

    uint64_t taker_id = 1000001;
    for (auto _ : state) {
        // Taker crosses the spread, triggers a fill
        book.add_order(taker_id++, 5000, 10, true);
    }
    
    state.SetItemsProcessed(state.iterations());
    state.counters["Total_Orders"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kDefaults);
}
BENCHMARK(BM_DeterministicMatch_NoReceipts);

static void BM_DeterministicMatch_WithReceipts(benchmark::State& state) {
    RingBuffer<ExecutionReceipt> rb(1048576);
    LimitOrderBook book(bench_config, &rb);
    for (uint64_t i = 1; i <= 1000000; ++i) {
        book.add_order(i, 5000, 10, false);
    }

    uint64_t taker_id = 1000001;
    for (auto _ : state) {
        book.add_order(taker_id++, 5000, 10, true);
    }

    state.SetItemsProcessed(state.iterations());
    state.counters["Total_Orders"] = benchmark::Counter(static_cast<double>(state.iterations()), benchmark::Counter::kDefaults);
}
BENCHMARK(BM_DeterministicMatch_WithReceipts);

BENCHMARK_MAIN();
