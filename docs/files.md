---
title: Repository
layout: default
nav_order: 9
---

# Repository

## Core

| Path | Description |
| --- | --- |
| `include/core/types.hpp` | `Order`, `OrderRef` |
| `include/core/order_pool.hpp` | Order pool interface |
| `include/core/price_level.hpp` | FIFO level |
| `include/core/limit_order_book.hpp` | `BookConfig`, `LimitOrderBook` |
| `include/core/ring_buffer.hpp` | SPSC ring, `ExecutionReceipt` |
| `include/core/config.hpp` | `load_config` |
| `src/core/order_pool.cpp` | Pool implementation |
| `src/core/limit_order_book.cpp` | Matching, cancel, radar |
| `src/core/config.cpp` | JSON loader |
| `src/main.cpp` | Config load, book construction |

## Config

| Path | Description |
| --- | --- |
| `config/settings.json` | Default `BookConfig` |

## Tests

| Path | Description |
| --- | --- |
| `tests/test_helpers.hpp` | `tiny_config()`, `small_config()` |
| `tests/test_limit_order_book.cpp` | LOB integration |
| `tests/test_lob_fifo.cpp` | FIFO, tombstones |
| `tests/test_order_pool.cpp` | Pool |
| `tests/test_ring_buffer.cpp` | Ring buffer |

## Benchmarks

| Path | Description |
| --- | --- |
| `benchmarks/lob_microbenchmarks.cpp` | Google Benchmark suite |

## CI

| Path | Description |
| --- | --- |
| `.github/workflows/ci.yaml` | Build, test, benchmark report |
| `.github/workflows/pages.yml` | Documentation deploy |

## Third party

| Path | Description |
| --- | --- |
| `third_party/json/` | nlohmann/json |
| `third_party/googletest/` | Unit tests |
| `third_party/benchmark/` | Benchmarks |

## CMake targets

| Target | Output |
| --- | --- |
| `core_objects` | Core object library |
| `raijin_engine` | `src/main.cpp` |
| `raijin_tests` | `tests/*.cpp` |
| `raijin_benchmarks` | `benchmarks/*.cpp` |
