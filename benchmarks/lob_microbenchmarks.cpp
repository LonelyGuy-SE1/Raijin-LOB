//Deterministic data.

#include <benchmark/benchmark.h>
#include "../include/core/limit_order_book.hpp"
#include "../include/core/ring_buffer.hpp"

using namespace raijin;

static BookConfig bench_config{
    .order_pool_capacity = 2000000,
    .price_level_count = 100000,
    .level_queue_capacity = 256,
    .max_order_id = 2000000};

//Bench 1 

static void BM_AddOrder(benchmark::State &state)
{
    LimitOrderBook book(bench_config);
    std::uint64_t order_id = 1;

    for (auto _ : state)
    {
        book.add_order(order_id++, 5000, 100, true);
        }
}
BENCHMARK(BM_AddOrder);

//Bench 2
static void BM_MatchAndEmit(benchmark::State &state)
{
    RingBuffer<ExecutionReceipt> rb(65536);
    LimitOrderBook book(bench_config, &rb);
    uint64_t order_id = 1;

    for (int i = 0; i < 1000000; ++i)
    {
        book.add_order(order_id++, 5000, 10, false);
    }
    uint64_t incoming_id = 1000001;

    for (auto _ : state)
    {
        book.add_order(incoming_id++, 5000, 10, true); 

    }
}
BENCHMARK(BM_MatchAndEmit);
BENCHMARK_MAIN();
