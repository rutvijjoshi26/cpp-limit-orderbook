# cpp-limit-orderbook

A price-time-priority limit order book and matching engine, written in modern C++17.

## What it does

- Maintains separate bid and ask books (`std::map<Price, std::deque<Order>>`), each price level ordered by arrival time.
- Matches incoming limit orders against resting orders on the opposite side, walking price levels best-first and orders within a level oldest-first (price-time priority).
- Rejects self-crossing orders (an incoming order that would match against a resting order from the same owner) instead of silently trading with yourself.
- Cancels resting orders in O(1) average time via a secondary `OrderId -> (Side, Price)` index, avoiding a full book scan.
- Reports partial fills correctly: an order that only partially matches rests on the book with its remaining size.

## Build

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Test

```
./build/orderbook_test
```

16 unit tests cover empty-book behavior, resting orders, full and partial fills, multi-level fills, cancellation, self-cross rejection, price-time priority, and edge cases (zero size, duplicate order IDs).

## Benchmark

```
./build/bench --orders 2000000
```

Generates synthetic orders around a center price with a configurable bid/ask split and feeds them through the book one at a time, timing each `place_limit_order` call individually.

Measured on a single core (Apple Silicon, `-O3`, no external dependencies):

| Metric      | Value          |
|-------------|----------------|
| Throughput  | ~10-13M orders/sec |
| p50 latency | ~42 ns         |
| p99 latency | < 600 ns       |

## Design notes

- Prices, sizes, and timestamps are all `uint64_t` to avoid floating-point precision issues in matching logic.
- Cancellation within a price level is currently O(n) in the number of resting orders at that level (linear scan of the `std::deque`); a production engine would use an intrusive list or arena allocator for O(1) removal within a level too.
- The benchmark measures per-order latency with `std::chrono::high_resolution_clock`, sorting all samples to report p50/p99 rather than just an average.
