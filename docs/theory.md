---
title: Matching
layout: default
nav_order: 8
---

# Matching

Discrete-tick central limit order book with price-time priority.

## Tick index

Prices are unsigned integers `0 .. price_level_count - 1`. No currency or tick-size in the core. External mapping:

```text
price_tick = (price - origin) / tick_size
```

| Side | Better tick |
| --- | --- |
| Ask | Lower |
| Bid | Higher |

## Price priority

| Incoming | Walk condition |
| --- | --- |
| Buy | `best_ask <= limit_tick` while asks remain |
| Sell | `limit_tick <= best_bid` while bids remain |

Consumes best opposite tick first, then worse ticks.

## Time priority

Within a tick, FIFO queue ordered by arrival. Matcher reads head after `clean_front`.

## Crossing

| Resting | Incoming limit | Result |
| --- | --- | --- |
| Ask 5010 | Buy 5020 | Match |
| Bid 5000 | Sell 4990 | Match |
| Ask 5010 | Buy 5000 | Rest (no cross) |
| Bid 5000 | Sell 5010 | Rest (no cross) |

Non-crossing orders rest and may update best bid or best ask.

## Partial fills

| Case | Head order | Incoming | Queue |
| --- | --- | --- | --- |
| Incoming smaller | Volume reduced | Fully filled | Head unchanged |
| Incoming larger | Deallocated, popped | Volume reduced | Next head |

## Event effects

| Event | Pool | Queue slot | Locator | Receipt |
| --- | --- | --- | --- | --- |
| Full fill | Deallocate | Pop | Inactive | Push if ring attached |
| Partial fill | Retain | Head retained | Active | Push if ring attached |
| Cancel | Deallocate | Tombstone or level clear | Inactive | None |

## Empty book

| Condition | Return |
| --- | --- |
| No bids | `best_bid_tick() == UINT32_MAX` |
| No asks | `best_ask_tick() == UINT32_MAX` |

`UINT32_MAX` is a sentinel, not a valid tick index.
