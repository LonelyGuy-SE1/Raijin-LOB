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

### `AddResult`

| Field | Type | Description |
| --- | --- | --- |
| `valid` | `bool` | Input passed validation |
| `accepted` | `bool` | Any volume matched or rested |
| `matched_volume` | `uint32_t` | Total volume matched |
| `rested_volume` | `uint32_t` | Volume successfully rested |
| `dropped_volume` | `uint32_t_t` | Unmatched remainder that failed to rest |

## `add_order`

```cpp
AddResult add_order(uint64_t order_id, uint32_t price_tick, uint32_t volume, bool is_buy,
                    OrderType type = OrderType::Limit, TimeInForce tif = TimeInForce::GTC);
```

The fast path (Limit + GTC) is inlined in the header. Market, IOC, and FOK forward to a cold path in the .cpp.

### Order types

| Type | Behavior |
| --- | --- |
| `Limit` | Cross as much as possible, rest remainder |
| `Market` | Cross entire book; no rest; unmatched = dropped |

### Time-in-force

| TIF | Behavior |
| --- | --- |
| `GTC` | Good-till-cancelled; rest unmatched |
| `IOC` | Immediate-or-cancel; fill what you can, cancel rest |
| `FOK` | Fill-or-kill; reject if insufficient aggregate volume |

### Preconditions (returns `AddResult{valid=false}`, no state change)

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

Rest fails when pool is exhausted or level queue is full after compaction. The unmatched remainder is reported as `dropped_volume`.

### Return value semantics

| Outcome | Result |
| --- | --- |
| Fully matched, nothing to rest | `accepted=true`, `matched_volume=volume` |
| Rest succeeds | `accepted=true`, `rested_volume>0` |
| Match commits fills, rest fails | `accepted=true`, `dropped_volume>0` |
| Validation failure | `valid=false` |
| Unmatched and rest fails | `accepted=false`, `dropped_volume=volume` |

Matching precedes rest. `accepted` is true when any volume matched or rested. Residual volume that cannot rest is reported as `dropped_volume`.

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
bool has_best_bid() const noexcept;
bool has_best_ask() const noexcept;
uint64_t bid_volume(uint32_t price_tick) const noexcept;
uint64_t ask_volume(uint32_t price_tick) const noexcept;
```

| Function | Empty side | Out-of-range tick |
| --- | --- | --- |
| `best_bid_tick` | `UINT32_MAX` | — |
| `best_ask_tick` | `UINT32_MAX` | — |
| `has_best_bid` | `false` | — |
| `has_best_ask` | `false` | — |
| `bid_volume` | — | `0` |
| `ask_volume` | — | `0` |

## Receipt integration

When `receipt_queue_` is non-null, each fill calls `push`. When the ring is full, the receipt is silently dropped. Consumer must drain the ring via `pop`.

## `modify_order`

```cpp
ModifyResult modify_order(uint64_t order_id, uint32_t new_price_tick, uint32_t new_volume);
```

Cancel-and-re-add. Returns `ModifyResult`:

| Field | Type | Description |
| --- | --- | --- |
| `valid` | `bool` | Input passed validation |
| `cancelled` | `bool` | Original order was cancelled |
| `reested` | `bool` | New order was successfully rested |
| `dropped_volume` | `uint32_t` | Volume that failed to rest |

## Thread safety

`LimitOrderBook` is not thread-safe. `RingBuffer` is SPSC: one producer (matching thread), one consumer.

## Config loader

```cpp
raijin::BookConfig config = raijin::load_config("config/settings.json");
```

See [Configuration](config.md).
