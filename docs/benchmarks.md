---
title: Benchmarks
layout: default
nav_order: 11
---

# Benchmarks

Source: `benchmarks/lob_microbenchmarks.cpp`  
Framework: Google Benchmark

CI job `run-benchmarks` executes the full suite and writes results to the Actions step summary.

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

Order id reuse on hot paths avoids locator cold misses. Monotonic ids without reset bias toward locator allocation cost.

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
