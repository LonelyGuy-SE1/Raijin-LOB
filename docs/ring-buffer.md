---
title: Ring buffer
layout: default
nav_order: 3
parent: Components
---

# Ring buffer

Header: `include/core/ring_buffer.hpp` (header-only)

Lock-free SPSC ring. Used for `ExecutionReceipt` handoff from matching thread to consumer.

## `ExecutionReceipt`

| Field | Type |
| --- | --- |
| `maker_order_id` | `uint64_t` |
| `taker_order_id` | `uint64_t` |
| `price_tick` | `uint32_t` |
| `executed_volume` | `uint32_t` |

## Interface

```cpp
explicit RingBuffer(size_t capacity);  // throws if capacity not power of two
bool push(const T &item) noexcept;
bool pop(T &item) noexcept;
```

Non-copyable. Indexing: `index & (capacity - 1)`.

## Memory ordering

| Atomic | Alignment | Role |
| --- | --- | --- |
| `write_pos_` | 64 bytes | Producer index |
| `read_pos_` | 64 bytes | Consumer index |

Producer: `memory_order_relaxed` load on write, `acquire` on read check, `release` on write commit.  
Consumer: symmetric pattern on read side.

## LOB integration

```cpp
RingBuffer<ExecutionReceipt> rb(65536);
LimitOrderBook book(config, &rb);
```

Matching calls `push` on each fill when pointer is non-null. LOB never calls `pop`. Full ring: `push` returns `false`; fill is still committed.

## Concurrency

Single producer, single consumer only.
