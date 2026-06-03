---
title: Order pool
layout: default
nav_order: 2
parent: Components
---

# Order pool

Headers: `include/core/order_pool.hpp`  
Implementation: `src/core/order_pool.cpp`

Independent pool per side. Dense `Order` storage with O(1) allocate and deallocate.

## Layout

| Member | Purpose |
| --- | --- |
| `orders_` | `Order` array |
| `free_` | Stack of free indices |
| `generations_` | Per-slot counter; initial value 1 |

Construction fills `free_` with indices `capacity-1 .. 0`.

## Interface

```cpp
PoolIndex allocate_index() noexcept;   // UINT32_MAX on exhaustion
void deallocate(PoolIndex index) noexcept;
Order &get_order(PoolIndex index) noexcept;
uint32_t generation(PoolIndex index) const noexcept;
size_t capacity() const noexcept;
```

`deallocate` increments `generations_[index]` and returns index to `free_`.

## Generational refs

```cpp
struct OrderRef {
    PoolIndex index;
    uint32_t generation;
};
```

Valid ref iff `pool.generation(index) == ref.generation` and `pool.get_order(index).volume != 0`.

Prevents stale FIFO entries from addressing a slot reused after cancel or fill.

## Capacity

`order_pool_capacity` bounds resting orders per side. Pool index and `order_id` are independent; locators map `order_id` → `{ index, generation, side, active }`.

## Allocation

Construction validates capacity and allocates fixed-size vectors for orders, free indices, and generations.
