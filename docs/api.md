---
title: API
layout: default
nav_order: 3
---

# API

Headers: `include/core/limit_order_book.hpp`, `include/core/types.hpp`, `include/core/ring_buffer.hpp`, `include/core/config.hpp`.

## Construction

```cpp
raijin::BookConfig config{
    .order_pool_capacity = 100000,
    .price_level_count = 8192,
    .level_queue_capacity = 512,
    .max_order_id = 500000};

raijin::RingBuffer<raijin::ExecutionReceipt> receipts(65536);
raijin::LimitOrderBook book(config, &receipts);
```

| Parameter | Default | Description |
| --- | --- | --- |
| `config` | required | Validated at construction |
| `receipt_queue` | `nullptr` | SPSC ring for fill events; omit to disable |

Invalid `BookConfig` throws `std::invalid_argument`.

## Types

### `Order`

| Field | Type | Description |
| --- | --- | --- |
| `order_id` | `uint64_t` | Caller-assigned identifier |
| `volume` | `uint32_t` | Remaining quantity |
| `price_tick` | `uint32_t` | Zero-based tick index |

16 bytes, 16-byte aligned. Side is determined by bid or ask pool membership.

### `ExecutionReceipt`

| Field | Type |
| --- | --- |
| `maker_order_id` | `uint64_t` |
| `taker_order_id` | `uint64_t` |
| `price_tick` | `uint32_t` |
| `executed_volume` | `uint32_t` |

One receipt per fill. Maker = resting order; taker = aggressive order.

### `BookConfig`

| Field | Type | Description |
| --- | --- | --- |
| `order_pool_capacity` | `size_t` | Max resting orders per side |
| `price_level_count` | `uint32_t` | Tick count `0 .. N-1` |
| `level_queue_capacity` | `uint32_t` | FIFO depth per tick; power of two |
| `max_order_id` | `uint64_t` | Max valid `order_id`; locator size = `max_order_id + 1` |

## `add_order`

```cpp
bool add_order(uint64_t order_id, uint32_t price_tick, uint32_t volume, bool is_buy);
```

### Preconditions (returns `false`, no state change)

| Condition |
| --- |
| `volume == 0` |
| `price_tick >= price_level_count` |
| `order_id > max_order_id` |
| `order_id` already active |

### Matching phase

Executed before resting.

| Side | Cross condition |
| --- | --- |
| Buy | `best_ask != UINT32_MAX` and `best_ask <= price_tick` |
| Sell | `best_bid != UINT32_MAX` and `price_tick <= best_bid` |

Walks opposite side best-to-worst. Within each tick, FIFO head after `clean_front`. Partial fills reduce incoming volume and continue.

### Resting phase

Executed when incoming `volume > 0` after matching.

Returns `false` when pool is exhausted or level queue is full after compaction.

### Return value semantics

| Outcome | Return |
| --- | --- |
| Fully matched, nothing to rest | `true` |
| Rest succeeds | `true` |
| Match commits fills, rest fails | `false` (fills retained) |
| Validation failure | `false` |

Matching precedes rest. A `false` return after partial or full match does not roll back fills.

## `cancel_order`

```cpp
bool cancel_order(uint64_t order_id) noexcept;
```

Returns `true` when an active order is cancelled.

Returns `false` when:

| Condition |
| --- |
| `order_id` out of range |
| Locator inactive |
| Locator generation mismatch |

O(1). Queue entry may remain as tombstone until match or compaction. When cancel reduces level `total_volume` to zero, `level.clear()` removes all queue entries including tombstones.

## Queries

```cpp
uint32_t best_bid_tick() const noexcept;
uint32_t best_ask_tick() const noexcept;
uint64_t bid_volume(uint32_t price_tick) const noexcept;
uint64_t ask_volume(uint32_t price_tick) const noexcept;
```

| Function | Empty side | Out-of-range tick |
| --- | --- | --- |
| `best_bid_tick` | `UINT32_MAX` | — |
| `best_ask_tick` | `UINT32_MAX` | — |
| `bid_volume` | — | `0` |
| `ask_volume` | — | `0` |

## Receipt integration

When `receipt_queue_` is non-null, each fill calls `push`. Return value is not checked; a full ring drops the receipt while the fill remains committed. Consumer must drain the ring via `pop`.

## Thread safety

`LimitOrderBook` is not thread-safe. `RingBuffer` is SPSC: one producer (matching thread), one consumer.

## Config loader

```cpp
raijin::BookConfig config = raijin::load_config("config/settings.json");
```

See [Configuration](config.md).
