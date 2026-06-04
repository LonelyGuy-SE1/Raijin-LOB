---
title: Benchmarks
layout: default
nav_order: 11
---

# Benchmarks

Source: `benchmarks/lob_microbenchmarks.cpp`, `benchmarks/lob_latency_histograms.cpp`
Framework: Google Benchmark

CI job `run-benchmarks` executes the full suite and writes results to the Actions step summary.

## Executables

| Binary | Source | Purpose |
| --- | --- | --- |
| `raijin_benchmarks` | `lob_microbenchmarks.cpp` | Microbenchmarks + random-ID + replay |
| `raijin_latency_histograms` | `lob_latency_histograms.cpp` | rdtsc-based cycle percentile histograms |

## Configurations

### `kBenchConfig` (default)

| Field | Value |
| --- | --- |
| `order_pool_capacity` | 100000 |
| `price_level_count` | 8192 |
| `level_queue_capacity` | 512 |
| `max_order_id` | 500000 |

Reference tick: `kTick = 5000`.

### `kTombstoneBenchConfig`

Used by `BM_MatchThroughTombstones` only.

| Field | Value |
| --- | --- |
| `order_pool_capacity` | 1024 |
| `price_level_count` | 256 |
| `level_queue_capacity` | 512 |
| `max_order_id` | 5000 |

Reference tick: `kTombstoneTick = 100`.

## Timing

| Bench class | Method |
| --- | --- |
| `BM_BestBidAsk` | Google Benchmark CPU timer |
| All others | `UseManualTime()` with `std::chrono::steady_clock` |

Manual timing wraps only the labeled operation. Setup and teardown run outside the timed window via loop structure or `PauseTiming`.

## State management

| Benchmark | Reset policy |
| --- | --- |
| `BM_Compare_Arka_*` | Persistent book; ids 1–2 reused |
| `BM_Compare_NanoMatch_MixedAdd` | Book reset every 4096 ids and at `max_order_id` overflow |
| `BM_MultiLevelSweep` | Persistent book; five asks prefilled per iteration (paused) |
| `BM_MatchThroughTombstones` | Persistent book; tombstone prefill per iteration (paused) |

Order id reuse on hot paths avoids locator cold misses. Monotonic ids without reset bias toward locator allocation cost. Benchmarks use `AddResult.accepted` to avoid counting rejected adds.

## Catalog

| Name | Timed operation | Setup |
| --- | --- | --- |
| `BM_BestBidAsk` | `best_bid_tick`, `best_ask_tick` | One bid, one ask |
| `BM_Compare_Arka_AddNoMatch` | `add_order` (rest) | Cancel after timer |
| `BM_Compare_Arka_CancelOnly` | `cancel_order` | Re-add after timer |
| `BM_Compare_Arka_MatchOneLevel` | `add_order` (match) | Replenish ask after timer |
| `BM_Compare_Arka_MatchWithReceipts` | `add_order` (match) | Ring 65536; replenish ask |
| `BM_Compare_NanoMatch_MixedAdd` | `add_order` | Rotating side, tick, volume |
| `BM_MultiLevelSweep` | `add_order` (5-level match) | Five ask levels |
| `BM_MatchThroughTombstones` | `add_order` (match) | 0/8/64/256 front tombstones |
| `BM_RandomAdd` | `add_order` (random IDs) | Pre-generated uniform random IDs |
| `BM_RandomCancel` | `cancel_order` (random IDs) | 50K prefilled; random cancel + re-add |
| `BM_RandomMatch` | `add_order` (random IDs, match) | 50K prefilled; random taker against resting |
| `BM_ReplaySynthetic` | 1M message replay | 70% add, 25% cancel, 5% match |

### Tombstone prefill

Per iteration (paused):

1. Add orders `1000 .. 1000+N-1` at `kTombstoneTick`.
2. Add maker id 2 at same tick.
3. Cancel orders `1000 .. 1000+N-1` (level retains maker volume).
4. Time taker id 3 buy against maker.

Requires live maker volume after cancel so `level.clear()` does not remove tombstones. `MinTime(0.5)` on this benchmark.

## Measurement constraints

| Constraint | Reason |
| --- | --- |
| `price_tick < price_level_count` | Otherwise validation reject |
| Tombstone prefill with live maker | Solo cancel clears level queue |
| Id reuse on single-op benches | Isolates operation cost from locator growth |
| `PauseTiming` outside timed region | Prevents setup inflation |

## Column selection

For `manual_time` benchmarks, use the `Time` column (manual). The `CPU` column includes loop and harness overhead.

## Random-ID benchmarks

`BM_RandomAdd`, `BM_RandomCancel`, `BM_RandomMatch` expose cache locality effects that sequential-ID benchmarks hide.

All IDs and ticks are pre-generated into vectors before the timed loop. No RNG in the hot path.

| Distribution | IDs | Ticks |
| --- | --- | --- |
| Uniform random | `uniform_int_distribution(1, max_order_id)` | `uniform_int_distribution(0, price_level_count-1)` |

### Why this matters

The locator table (`max_order_id + 1` entries) often exceeds L2 cache. Sequential IDs (1, 2, 3...) keep the working set hot. Random IDs force L2/L3 misses, revealing the true cost of locator lookups in production-like workloads.

## Replay benchmark

`BM_ReplaySynthetic` runs 1M messages with realistic operation mix:

| Operation | Fraction | Behavior |
| --- | --- | --- |
| Add | 70% | Rest order at random tick |
| Cancel | 25% | Cancel most recently added |
| Match | 5% | Cross-book taker order |

Each iteration creates a fresh book and runs the full 1M message sequence. Reports wall-clock time for the entire sequence.

## Latency histograms

`raijin_latency_histograms` measures per-operation cycle distributions using `rdtsc`/`rdtscp` with `lfence` serialization.

Reported percentiles (all in CPU cycles):

| Counter | Meaning |
| --- | --- |
| `p50` | Median latency |
| `p90` | 90th percentile |
| `p99` | 99th percentile (tail) |
| `p999` | 99.9th percentile (severe tail) |
| `min` | Best-case |
| `max` | Worst-case |

### Benchmarks instrumented

| Benchmark | What it measures |
| --- | --- |
| `BM_Hist_AddNoMatch` | Add-only with random IDs |
| `BM_Hist_Cancel` | Cancel with random IDs |
| `BM_Hist_MatchOneLevel` | Single-level fill (sequential) |
| `BM_Hist_MatchWithReceipts` | Fill + ring push (sequential) |
| `BM_Hist_TombstoneMatch` | Match through 256 tombstones |

## Compiler optimizations

### LTO (Link-Time Optimization)

Enabled by default in Release builds via `CheckIPOSupported`. Cross-TU inlining, dead code elimination, constant propagation across translation units.

### PGO (Profile-Guided Optimization)

Build with PGO:

```bash
# Step 1: Instrument
cmake .. -DCMAKE_BUILD_TYPE=Release -DRAIJIN_PGO_MODE=generate
make -j$(nproc)
./raijin_benchmarks --benchmark_min_time=0.3s   # collect profile
./raijin_latency_histograms --benchmark_min_time=0.1s

# Step 2: Rebuild with profile
cmake .. -DCMAKE_BUILD_TYPE=Release -DRAIJIN_PGO_MODE=use
make -j$(nproc)
```

Profile data (`.gcda` files) must exist for `core_objects` before the `use` step. Run representative benchmarks in the `generate` step to collect profile data.
