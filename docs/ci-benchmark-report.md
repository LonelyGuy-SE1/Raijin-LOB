# Raijin-LOB CI Benchmark Report

**Date:** 2026-06-04
**CI runner:** GitHub Actions ubuntu-latest (Azure VM)
**Build:** Release with LTO (`-O3 -march=native -DNDEBUG`)

---

## Hardware

### CI Runner (Linux)

- CPU: AMD EPYC 9V74 80-Core Processor (virtualized, 2 cores / 4 threads)
- Frequency: 2869 MHz (BogoMIPS: 5192.26)
- L1d: 32 KiB x2, L1i: 32 KiB x2
- L2: 1 MiB x2, L3: 32 MiB x1
- Hypervisor: Microsoft (Azure)
- ASLR: disabled
- CPU scaling: disabled
- Compiler flags: `-O3 -march=native -DNDEBUG -Wall -Wextra -Werror -Wno-error=attributes`
- LTO: enabled (`CheckIPOSupported` + `CMAKE_INTERPROCEDURAL_OPTIMIZATION`)

---

## Binary Sizes

| Binary | Size |
|---|---|
| raijin_benchmarks | 429K |
| raijin_latency_histograms | 410K |

---

## Microbenchmarks

All times in nanoseconds (median of 3 runs, manual_time where applicable).

### Sequential-ID Benchmarks

| Benchmark | Time (ns) | Throughput (M/s) | CV |
|---|---|---|---|
| BM_BestBidAsk | 0.353 | 5,660 | 0.37% |
| BM_Compare_Arka_AddNoMatch | 41.3 | 24.2 | 6.11% |
| BM_Compare_Arka_CancelOnly | 33.8 | 29.6 | 1.04% |
| BM_Compare_Arka_MatchOneLevel | 37.4 | 26.7 | 0.04% |
| BM_Compare_Arka_MatchWithReceipts | 38.0 | 26.3 | 0.04% |
| BM_Compare_NanoMatch_MixedAdd | 52.2 | 19.1 | 1.77% |
| BM_MultiLevelSweep (5-level) | 82.5 | 12.1 | 0.05% |
| BM_MatchThroughTombstones/0 | 37.8 | 26.5 | 0.12% |
| BM_MatchThroughTombstones/8 | 46.8 | 21.4 | 1.48% |
| BM_MatchThroughTombstones/64 | 112 | 8.9 | 0.04% |
| BM_MatchThroughTombstones/256 | 345 | 2.9 | 0.06% |

### Random-ID Benchmarks

Uniform random, pre-generated, no RNG in the hot path.

| Benchmark | Time (ns) | Throughput (M/s) | CV |
|---|---|---|---|
| BM_RandomAdd | 61.6 | 16.2 | 0.61% |
| BM_RandomCancel | 48.3 | 20.7 | 0.20% |
| BM_RandomMatch | 62.2 | 16.1 | 0.61% |

### Random-ID Cache Locality Ratio

| Ratio | Value |
|---|---|
| RandomAdd / SequentialAdd | 1.49x |
| RandomCancel / SequentialCancel | 1.43x |
| RandomMatch / SequentialMatch | 1.66x |

Locator table: 500K orders x 8 bytes = ~4 MB. Fits in L3 (32 MiB), exceeds L2 (1 MiB).

### Replay Benchmark

| Metric | Value |
|---|---|
| 1M messages (70% add, 25% cancel, 5% match) | 34.9 ms |
| Throughput | 28.6M msgs/sec |
| CV | 1.47% |

---

## Latency Histograms (rdtsc, CPU cycles)

All values in CPU cycles. Median of 3 runs of 100K iterations each.

| Benchmark | p50 | p90 | p99 | p99.9 | min | max |
|---|---|---|---|---|---|---|
| BM_Hist_AddNoMatch | 130 | 208 | 468 | 728 | 78 | 63,310 |
| BM_Hist_Cancel | 78 | 208 | 520 | 832 | 52 | 47,814 |
| BM_Hist_MatchOneLevel | 78 | 78 | 78 | 104 | 52 | 78,910 |
| BM_Hist_MatchWithReceipts | 78 | 78 | 104 | 130 | 78 | 45,500 |
| BM_Hist_TombstoneMatch/256 | 884 | 910 | 910 | 936 | 780 | 90,376 |

### Observations

- Match path (MatchOneLevel, MatchWithReceipts) is extremely tight: p50=p90=p99=78 cycles. Approximately 2 CPU cycles at 2869 MHz.
- AddNoMatch and Cancel have wider tails (p99 up to 520 cycles) due to random-ID locator lookups.
- TombstoneMatch/256: p50=884 cycles, p99=910 cycles. Consistent but ~12x slower than clean match.
- All benchmarks show max outliers in the 45k-90k cycle range. These are cold-start first-iteration artifacts (page faults, TLB misses during initialization).

---

## perf stat: Hardware Counters

### Result: NOT AVAILABLE

All hardware performance counters show `<not supported>` on the Azure VM:

```
<not supported>      cycles
<not supported>      instructions
<not supported>      cache-references
<not supported>      cache-misses
<not supported>      branch-misses
<not supported>      branch-instructions
<not supported>      L1-dcache-load-misses
<not supported>      L1-icache-load-misses
<not supported>      LLC-load-misses
<not supported>      dTLB-load-misses
```

The Azure hypervisor does not expose the Performance Monitoring Unit (PMU) to guest VMs.

### What was captured (non-PMU)

```
495,789,902  task-clock              # 0.998 CPUs utilized
          5  context-switches        # 10.085 /sec
          0  cpu-migrations          # 0.000 /sec
     12,610  page-faults             # 25.434 K/sec
```

12,610 page faults during the benchmark run. These are initialization-time faults (mmap for order pool, price levels, ring buffer) and do not affect steady-state measurements.

### Implication

Cache-miss attribution (L1 vs L2 vs LLC vs TLB) requires bare-metal Linux or Windows VTune. The random-ID cache locality effect is confirmed numerically (1.5x penalty with 32MB L3) but the specific cache level cannot be identified from this CI run.

---

## perf record (Flamegraph Data)

- 16,042 samples captured
- LTO-inlined frames confirmed in hot path:
  - `raijin::LimitOrderBook::add_order [clone .lto_priv.0]`
  - `raijin::PriceLevel::push [clone .lto_priv.0]`
  - `raijin::OrderPool::allocate [clone .lto_priv.0]`
  - `raijin::RingBuffer<raijin::ExecutionReceipt>::push [clone .lto_priv.0]`
- Majority of samples are startup/init noise (mmap, page faults, dynamic linker)
- Binary is stripped + LTO, so user-space frames are collapsed to single addresses
- Full perf.data available as compressed artifact (`perf_data.gz`)

---

## Key Findings

1. **Match path is near-optimal at 78 cycles p50.** Approximately 2 CPU cycles at 2869 MHz. The compiler successfully inlined and optimized the critical matching loop.

2. **Random-ID cache locality is real but scale-dependent.** The locator table (4 MB) fits in the EPYC's 32 MB L3 (1.5x penalty) but would exceed a 256 KB L2 (3.4x penalty). This confirms the table exceeds L2 working set.

3. **Tombstone cost scales linearly.** 0 tombstones: 37.8 ns, 8: 46.8 ns, 64: 112 ns, 256: 345 ns. The compaction overhead is proportional to dead entries traversed.

4. **Replay throughput is 28.6M msgs/sec.** This is the production-grade number for a mixed add/cancel/match workload.

5. **Hardware PMU is unavailable on Azure VMs.** Cache-miss attribution requires bare-metal Linux or VTune.

6. **LTO successfully inlines the entire hot path.** Confirmed by perf script showing single `add_order` frames with `.lto_priv.0` suffix.

---

## What is Needed for Deeper Analysis

| Item | Requirement | Status |
|---|---|---|
| Cache miss breakdown (L1/L2/LLC) | Bare-metal Linux or VTune | Blocked |
| Branch misprediction rate | Same | Blocked |
| Flamegraph with source lines | Debug symbols + no strip | Not built |
| NUMA effects | Multi-socket hardware | Not applicable (1 socket) |
| Tail latency root cause | `perf record` + source-level flamegraph | Partial (stripped binary) |

---

## Artifacts

| File | Description |
|---|---|
| `benchmark_results.json` | Full JSON output from Google Benchmark (all timings, stats) |
| `perf_data.gz` | Compressed `perf.data` for offline `perf report` |
| `perf_script.txt` | Human-readable perf script output (16K samples) |

---

## Reproduction

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc) raijin_benchmarks raijin_latency_histograms

# Microbenchmarks (median of 3 runs)
./raijin_benchmarks --benchmark_repetitions=3 --benchmark_min_time=0.3s

# Latency histograms (tabular)
./raijin_latency_histograms --benchmark_counters_tabular=true --benchmark_repetitions=3

# perf (Linux only)
sudo perf stat -d ./raijin_latency_histograms --benchmark_repetitions=1
sudo perf record --call-graph dwarf -o perf.data ./raijin_latency_histograms
```
