---
title: Home
layout: home
nav_order: 1
description: "Limit order book core — technical reference"
permalink: /
---

# Raijin-LOB

C++20 limit order book with config-owned dimensions, generational order pools, per-tick FIFO queues, and optional SPSC execution receipts.

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ctest --output-on-failure
```

| Requirement | Value |
| --- | --- |
| OS | Linux |
| Compiler | GCC ≥ 10 or Clang ≥ 12 |
| Standard | C++20 |
| CMake | ≥ 3.14 |

Release flags: `-O3 -march=native -DNDEBUG`. All builds: `-Wall -Wextra -Werror`.

## Reference

| Page | Subject |
| --- | --- |
| [Architecture](architecture.md) | Structure, memory layout, invariants |
| [API](api.md) | `LimitOrderBook` interface and semantics |
| [Configuration](config.md) | `BookConfig`, JSON loader |
| [Components](components.md) | Price levels, order pool, ring buffer |
| [Matching](theory.md) | Price-time priority, tick model |
| [Repository](files.md) | Source layout |
| [Build and test](build-and-test.md) | Targets, CI, test matrix |
| [Benchmarks](benchmarks.md) | Microbenchmark definitions |
