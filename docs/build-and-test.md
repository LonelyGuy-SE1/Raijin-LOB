---
title: Build and test
layout: default
nav_order: 10
---

# Build and test

## Toolchain

| Component | Version |
| --- | --- |
| OS | Linux |
| Compiler | GCC ≥ 10 or Clang ≥ 12 |
| C++ | C++20 |
| CMake | ≥ 3.14 |

| Flag set | Flags |
| --- | --- |
| All | `-Wall -Wextra -Werror` |
| Release | `-O3 -march=native -DNDEBUG` |

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

## Targets

| Binary | Command |
| --- | --- |
| Engine | `./raijin_engine` |
| Tests | `./raijin_tests` or `ctest --output-on-failure` |
| Benchmarks | `./raijin_benchmarks` |

`raijin_engine` loads `config/settings.json` by relative path; run from repository root.

## CI

Workflow: `.github/workflows/ci.yaml`

Triggers on changes to `src/`, `include/`, `tests/`, `benchmarks/`, `CMakeLists.txt`, `.github/workflows/ci.yaml`.

| Job | Steps |
| --- | --- |
| `build-and-test` | Checkout, Release build, `ctest --output-on-failure` |
| `run-benchmarks` | Build `raijin_benchmarks`, append output to Actions summary |

Submodules: `git submodule update --init --recursive`

## Test matrix

55 cases across four files.

### `test_limit_order_book.cpp`

| Area | Cases |
| --- | --- |
| Config | Zero dimensions, non-power-of-two queue, JSON load |
| Add / rest | Single buy, single sell |
| Match | Full fill, partial both directions, multi-level sweep |
| Cancel | Active, unknown, double, post-fill |
| Validation | Zero volume, invalid tick, invalid id, duplicate id |
| ID reuse | After cancel, after fill |
| Best price | Gap refresh, bid and ask |
| Capacity | Pool exhaustion, queue full, match-then-rest failure |
| Receipts | Full fill, partial, multi-level, ring full, cancel |

Fixture default: `price_level_count=100000`, `level_queue_capacity=256`, `max_order_id=1000`.

### `test_lob_fifo.cpp`

| Case | Subject |
| --- | --- |
| `SamePriceSellQueueOrder` | FIFO sell |
| `SamePriceBuyQueueOrder` | FIFO buy |
| `PartialFillLeavesHeadOrder` | Partial fill head retention |
| `MatchSkipsCancelledHead` | Tombstone skip |
| `SoloCancelClearsLevelQueue` | Level clear on last cancel |
| `MatchSkipsManyTombstonesBeforeLiveMaker` | Front tombstone chain |

### `test_order_pool.cpp`

Allocation, deallocation, generation increment, exhaustion, read/write, reuse, slot independence.

### `test_ring_buffer.cpp`

Push/pop, full buffer, invalid capacity, ordering, SPSC thread.

## Test configs

```cpp
test::tiny_config()   // pool 8, ticks 256, queue 4, max_id 64
test::small_config()  // pool 256, ticks 8192, queue 64, max_id 4096
```

Ticks in `tiny_config` tests must satisfy `tick < 256`.
