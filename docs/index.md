---
title: Home
layout: home
nav_order: 1
description: "Limit order book core: technical reference"
permalink: /
---

# Raijin

<div align="center">
  <a href="https://github.com/LonelyGuy-SE1/Raijin-LOB/blob/main/LICENSE">
    <img src="https://img.shields.io/badge/License-GPLv3-22c55e?style=for-the-badge" />
  </a>
  <a href="https://lonelyguy-se1.github.io/Raijin-LOB/">
    <img src="https://img.shields.io/badge/Docs-Reference-4a90d9?style=for-the-badge&logo=githubpages&logoColor=white" />
  </a>
  <a href="https://github.com/LonelyGuy-SE1/Raijin-LOB/actions">
    <img src="https://img.shields.io/badge/CI-Passing-2088ff?style=for-the-badge&logo=githubactions&logoColor=white" />
  </a>
  <img src="https://img.shields.io/badge/C%2B%2B-20-00599C?style=for-the-badge&logo=cplusplus&logoColor=white" />
  <img src="https://img.shields.io/badge/Linux-GCC%20%7C%20Clang-fcc624?style=for-the-badge&logo=linux&logoColor=white" />
</div>

C++20 limit order book with config-owned dimensions, generational order pools, per-tick FIFO queues, and optional SPSC execution receipts.

## Quick start

### Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
ctest --output-on-failure
```

| Requirement | Value |
| --- | --- |
| OS | Linux |
| Compiler | GCC >= 10 or Clang >= 12 |
| Standard | C++20 |
| CMake | >= 3.14 |

Release flags: `-O3 -march=native -DNDEBUG`. All builds: `-Wall -Wextra -Werror`. LTO enabled by default in Release.

### Minimal usage

```cpp
#include "core/limit_order_book.hpp"
#include "core/ring_buffer.hpp"

int main()
{
    raijin::BookConfig config{
        .order_pool_capacity = 100000,
        .price_level_count = 8192,
        .level_queue_capacity = 512,
        .max_order_id = 500000};

    raijin::RingBuffer<raijin::ExecutionReceipt> receipts(65536);
    raijin::LimitOrderBook book(config, &receipts);

    // Add a limit buy order (rests at tick 5000)
    auto result = book.add_order(1, 5000, 100, true);
    // result.valid = true
    // result.accepted = true
    // result.rested_volume = 100

    // Add a limit sell order that crosses the bid
    auto fill = book.add_order(2, 5000, 50, false);
    // fill.matched_volume = 50 (partial fill)
    // fill.rested_volume = 50 (remainder rests on ask side)

    // Cancel an order
    book.cancel_order(1);

    // Query best prices
    uint32_t best_bid = book.best_bid_tick();
    uint32_t best_ask = book.best_ask_tick();
}
```

### Order types

```cpp
// Limit order (default): cross as much as possible, rest remainder
book.add_order(id, price_tick, volume, is_buy,
               raijin::OrderType::Limit,
               raijin::TimeInForce::GTC);

// Market order: cross entire book, no rest
book.add_order(id, price_tick, volume, is_buy,
               raijin::OrderType::Market);

// IOC: fill what you can, cancel rest
book.add_order(id, price_tick, volume, is_buy,
               raijin::OrderType::Limit,
               raijin::TimeInForce::IOC);

// FOK: reject if insufficient volume, otherwise fill all
book.add_order(id, price_tick, volume, is_buy,
               raijin::OrderType::Limit,
               raijin::TimeInForce::FOK);
```

Limit + GTC is the fast path, inlined in the header. All other combinations forward to a cold path in the .cpp.

### Modify order

Cancel-and-re-add in a single call. Preserves side from the original order's locator.

```cpp
auto mod = book.modify_order(order_id, new_price_tick, new_volume);
// mod.valid, mod.cancelled, mod.reested, mod.dropped_volume
```

### Execution receipts

When a `RingBuffer<ExecutionReceipt>*` is passed at construction, each fill writes a receipt:

```cpp
struct ExecutionReceipt {
    uint64_t maker_order_id;  // resting order
    uint64_t taker_order_id;  // aggressive order
    uint32_t price_tick;
    uint32_t executed_volume;
};
```

Consumer drains at its own pace via `pop()`. When the ring is full, receipts are silently dropped. The LOB never blocks.

### Configuration

From JSON file:

```cpp
raijin::BookConfig config = raijin::load_config("config/settings.json");
raijin::LimitOrderBook book(config);
```

```json
{
    "order_pool_capacity": 1000000,
    "price_level_count": 100000,
    "level_queue_capacity": 256,
    "max_order_id": 1000000
}
```

Or construct programmatically:

```cpp
raijin::BookConfig config{
    .order_pool_capacity = 100000,
    .price_level_count = 8192,
    .level_queue_capacity = 512,
    .max_order_id = 500000};
```

### AddResult semantics

| Field | Type | Description |
| --- | --- | --- |
| `valid` | `bool` | Input passed validation |
| `accepted` | `bool` | Any volume matched or rested |
| `matched_volume` | `uint32_t` | Total volume filled |
| `rested_volume` | `uint32_t` | Volume resting in the book |
| `dropped_volume` | `uint32_t` | Unmatched remainder that failed to rest |

Matching precedes resting. `accepted` is true when any volume matched or rested. Residual volume that cannot rest is reported as `dropped_volume`.

### Validation (returns `AddResult{valid=false}`)

- `volume == 0`
- `price_tick >= price_level_count`
- `order_id > max_order_id`
- `order_id` already active

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
| [Known issues](known-issues.md) | Issue ledger |
