---
title: Price levels
layout: default
nav_order: 1
parent: Components
---

# Price levels

Header: `include/core/price_level.hpp`

Fixed-capacity FIFO ring of `OrderRef` per tick, plus cached `total_volume_`.

## Ring buffer

| Field | Role |
| --- | --- |
| `orders_` | Slice of flat `OrderRef` array, bound at LOB construction |
| `head_`, `tail_`, `count_` | Ring indices |
| `mask_` | `capacity - 1` |

| Operation | Behavior |
| --- | --- |
| `push` | Tail insert; returns `false` when `count == capacity` |
| `front` | Head element |
| `pop` | Advance head, decrement count |
| `empty` | `count == 0` |

Capacity must be power of two.

## Volume

`add_volume` / `remove_volume` track aggregate level quantity. Matching and cancellation update order volume and level total. LOB calls `clear()` when `total_volume()` reaches zero after cancel or fill.

## Tombstones

Cancel deallocates the pool slot and marks the locator inactive. The `OrderRef` remains in the ring until popped or compacted. Valid head requires matching `generation` and non-zero order volume.

## Compaction

`compact(Predicate)` packs active refs to the front and discards inactive entries in O(n) for current queue depth.

| Trigger | Location |
| --- | --- |
| Failed `push` during `rest_order` | Compacts, retries push |
| 8 consecutive stale pops in `clean_front` | Compacts during match |

Single stale head: O(1) pop. Batch threshold limits per-iteration tombstone walk at the touch.

## Best price interaction

When level volume reaches zero, LOB clears the level and calls `erase_best_ask` or `erase_best_bid`. If the erased tick was best, `next_ask` / `next_bid` scans the side bit radar.

## Level clear on solo cancel

When cancel removes the last volume at a tick, `level.clear()` resets head, tail, count, and volume. No tombstones remain. Tombstone chains require at least one other live order at the tick during cancel.
