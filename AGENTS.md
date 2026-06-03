# Raijin-LOB

Production-grade limit order book core feeding a JEPA world model for market microstructure.

## Operator

User is Loner: solo operator, no safety net, high-competition target.

Role of the agent:

- long-horizon strategic mentor
- ruthless technical reviewer
- immediate capital generation plus frontier capability
- no corporate filler
- no emotes

## Workflow

- Read `AGENTS.md`, `Readme.md`, and relevant `docs/` before code changes.
- Research with web search before significant engineering decisions.
- Explain syntax when giving code to the user.
- Keep code minimal.
- No code comments unless the user explicitly asks.
- Put implementation docs in `docs/`, not in `Readme.md`.
- `Readme.md` is the end-goal manifesto. Do not replace it with build notes.

## Build

```bash
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20
make -j$(nproc)
```

Target:

- Linux
- GCC >= 10 or Clang >= 12
- C++20
- CMake >= 3.10
- strict flags: `-Wall -Wextra -Werror`
- release flags: `-O3 -march=native -DNDEBUG`

## Current Status

The LOB core rewrite is done.

Do not rewrite the LOB again unless:

- a correctness bug is found
- a benchmark proves a regression
- tests expose an edge case
- the user explicitly asks for another LOB pass

Shipped:

- execution receipt SPSC ring buffer (header-only, LOB-integrated)
- receipt writes unconditionally; full ring silently drops
- structured `AddResult` return (matched/rested/dropped volumes)
- `ModifyResult` for cancel-and-re-add semantics
- `OrderType` enum (`Limit`, `Market`), `TimeInForce` enum (`GTC`, `IOC`, `FOK`)
- Market orders: cross entire book, no rest
- IOC: fill what you can, cancel remainder
- FOK: pre-scan availability, reject or fill-all
- `modify_order` cancel-and-re-add (preserves side from locator)
- inline fast path for Limit+GTC in header; cold path for Market/IOC/FOK in .cpp
- JSON config loader (`load_config`, `config/settings.json`)
- Google Test suite (76 tests in `tests/`)
- Google Benchmark suite (11 benches in `benchmarks/`)
- CI (build, `ctest`, benchmark summary)
- `src/main.cpp` uses `BookConfig` via JSON
- GitHub Pages docs in `docs/` (deploy via `.github/workflows/pages.yml`)
- known-issues ledger: resolved

Agent, simulator, and gateway remain stubs.

Next high-priority work:

1. simulator v0 (feed, receipt drain, replay)
2. optional CI benchmark regression guard

## LOB Core

Files:

- `include/core/types.hpp`
- `include/core/order_pool.hpp`
- `include/core/price_level.hpp`
- `include/core/limit_order_book.hpp`
- `src/core/order_pool.cpp`
- `src/core/limit_order_book.cpp`

Invariants:

- `Order` is exactly 16 bytes.
- `Order` has no pointers and no side field.
- side is implied by `bid_pool` or `ask_pool`.
- order lookup is flat array locator by `order_id`.
- queue entries are `{ index, generation }`.
- generation protects reused pool slots from stale FIFO refs.
- cancellation is lazy: volume becomes zero, generation increments.
- deep tombstone buildup is handled by level compaction on failed push.
- best-price discovery uses active price words and bit scans.
- hot path has no logging, I/O, string work, hash map, linked list, or per-order heap allocation.

## LOB API

Production construction:

```cpp
raijin::BookConfig config{
    order_pool_capacity,
    price_level_count,
    level_queue_capacity,
    max_order_id
};

raijin::RingBuffer<raijin::ExecutionReceipt> receipts(65536);
raijin::LimitOrderBook book(config, &receipts);
```

`add_order` returns `AddResult`:

```cpp
raijin::AddResult result = book.add_order(order_id, price_tick, volume, is_buy);
// result.valid       - input passed validation
// result.accepted    - any volume matched or rested
// result.matched_volume  - total filled
// result.rested_volume   - successfully rested
// result.dropped_volume  - unmatched remainder that failed to rest
```

The old `LimitOrderBook(size_t)` constructor is intentionally gone. Do not restore hardcoded LOB dimensions.

## Design Rules

- No hardcoded tunables in live LOB paths.
- Use config-owned dimensions.
- Prefer contiguous arrays and integer indices over pointers.
- Prefer power-of-two rings and masks over modulo in queue paths.
- Keep matching price-time priority.
- Keep cancellation O(1) except rare level-local compaction.
- Avoid changing non-LOB directories unless the user asks.

## Docs

Technical reference: `docs/` (Jekyll, just-the-docs). Deploy via `.github/workflows/pages.yml`.

| File | Subject |
| --- | --- |
| `docs/index.md` | Overview, build |
| `docs/architecture.md` | Structure, invariants |
| `docs/api.md` | Public interface |
| `docs/config.md` | `BookConfig`, JSON |
| `docs/components.md` | Component index |
| `docs/price-level.md` | FIFO levels |
| `docs/order-pool.md` | Order pool |
| `docs/ring-buffer.md` | SPSC receipts |
| `docs/theory.md` | Matching semantics |
| `docs/files.md` | Repository layout |
| `docs/build-and-test.md` | Build, CI, tests |
| `docs/benchmarks.md` | Benchmark definitions |
| `docs/known-issues.md` | Known issues ledger |

If build direction changes, update this file and the relevant doc page.
