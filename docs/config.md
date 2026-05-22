---
title: Configuration
layout: default
nav_order: 4
---

# Configuration

## `BookConfig`

| Field | Type | Constraint |
| --- | --- | --- |
| `order_pool_capacity` | `size_t` | `> 0` |
| `price_level_count` | `uint32_t` | `> 0` |
| `level_queue_capacity` | `uint32_t` | `> 0`, power of two |
| `max_order_id` | `uint64_t` | `< UINT64_MAX`, allocatable as `max_order_id + 1` locators |

Construction throws `std::invalid_argument` when:

- Any dimension is zero
- `level_queue_capacity` is not a power of two
- `price_level_count × level_queue_capacity` overflows internal limits
- `max_order_id` is invalid for locator allocation

## JSON loader

Implementation: `src/core/config.cpp`  
Declaration: `include/core/config.hpp`

```cpp
raijin::BookConfig config = raijin::load_config(path);
```

### Schema

```json
{
    "order_pool_capacity": 1000000,
    "price_level_count": 100000,
    "level_queue_capacity": 256,
    "max_order_id": 1000000
}
```

All keys required. Missing key: `nlohmann::json::at()` exception. Unopenable file: `std::runtime_error`.

Default path in `src/main.cpp`: `config/settings.json`.

## Dimension sizing

| Resource | Rule |
| --- | --- |
| Pool | `order_pool_capacity` per side; peak concurrent resting bids and asks separately |
| Locators | `(max_order_id + 1)` entries allocated regardless of active count |
| Level queue | Max distinct `OrderRef` slots per tick = `level_queue_capacity` |
| Ticks | External price maps to index: `tick = (price - origin) / tick_size`; require `tick < price_level_count` |

### Reference profiles

| Profile | `order_pool_capacity` | `price_level_count` | `level_queue_capacity` | `max_order_id` |
| --- | --- | --- | --- | --- |
| `test::tiny_config()` | 8 | 256 | 4 | 64 |
| `test::small_config()` | 256 | 8192 | 64 | 4096 |
| Benchmark default | 100000 | 8192 | 512 | 500000 |
| `config/settings.json` | 1000000 | 100000 | 256 | 1000000 |

## Receipt ring

Independent of `BookConfig`. Capacity must be a power of two.

```cpp
RingBuffer<ExecutionReceipt> rb(65536);
LimitOrderBook book(config, &rb);
```

Size to peak fill burst between consumer drains. Undrained overflow drops receipts without affecting book state.
