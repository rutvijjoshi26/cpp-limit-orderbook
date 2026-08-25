// Benchmark harness for the matching engine.
//
// Generates N random limit orders with a configurable price distribution
// and feeds them through the order book, measuring throughput and latency.
//
// Run with: ./build/bench
// Or with custom params: ./build/bench --orders 500000

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <vector>

#include "orderbook.hpp"

struct BenchConfig {
    std::uint64_t num_orders = 500'000;
    std::uint64_t center_price = 10'000;
    std::uint64_t price_range = 50;
    std::uint64_t max_size = 100;
    double bid_ratio = 0.5;
};

static std::vector<Order> generate_orders(const BenchConfig& config) {
    std::mt19937_64 rng(std::random_device{}());
    std::uniform_real_distribution<double> coin(0.0, 1.0);
    std::uniform_int_distribution<std::uint64_t> size_dist(1, config.max_size);

    std::vector<Order> orders;
    orders.reserve(config.num_orders);

    std::uint64_t low = (config.center_price > config.price_range)
                             ? config.center_price - config.price_range
                             : 0;
    std::uniform_int_distribution<std::uint64_t> bid_price_dist(low, config.center_price);
    std::uniform_int_distribution<std::uint64_t> ask_price_dist(config.center_price,
                                                                  config.center_price + config.price_range);

    for (std::uint64_t i = 0; i < config.num_orders; ++i) {
        Side side = (coin(rng) < config.bid_ratio) ? Side::Bid : Side::Ask;
        Price price = (side == Side::Bid) ? bid_price_dist(rng) : ask_price_dist(rng);
        Qty size = size_dist(rng);

        // Unique owner per order avoids self-cross rejections skewing the benchmark.
        orders.push_back(Order{i, i, side, price, size, i});
    }

    return orders;
}

static BenchConfig parse_args(int argc, char** argv) {
    BenchConfig config;
    for (int i = 1; i < argc; ++i) {
        auto next = [&](const char* flag) -> std::uint64_t {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "%s expects a value\n", flag);
                std::exit(1);
            }
            return std::strtoull(argv[++i], nullptr, 10);
        };

        if (std::strcmp(argv[i], "--orders") == 0) {
            config.num_orders = next("--orders");
        } else if (std::strcmp(argv[i], "--price-range") == 0) {
            config.price_range = next("--price-range");
        } else if (std::strcmp(argv[i], "--max-size") == 0) {
            config.max_size = next("--max-size");
        } else if (std::strcmp(argv[i], "--bid-ratio") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "--bid-ratio expects a value\n");
                std::exit(1);
            }
            config.bid_ratio = std::strtod(argv[++i], nullptr);
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::fprintf(stderr,
                          "Usage: bench [OPTIONS]\n"
                          "  --orders N        Number of orders (default: 500000)\n"
                          "  --price-range N   Half-width of price range (default: 50)\n"
                          "  --bid-ratio F     Probability an order is a bid (default: 0.5)\n"
                          "  --max-size N      Max order size (default: 100)\n");
            std::exit(0);
        } else {
            std::fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            std::exit(1);
        }
    }
    return config;
}

int main(int argc, char** argv) {
    BenchConfig config = parse_args(argc, argv);
    std::vector<Order> orders = generate_orders(config);
    std::size_t n = orders.size();

    std::printf("=== Order Book Benchmark ===\n");
    std::printf("Orders:      %zu\n", n);
    std::printf("Price range: %llu +/- %llu\n", (unsigned long long)config.center_price,
                (unsigned long long)config.price_range);
    std::printf("Bid ratio:   %.0f%%\n", config.bid_ratio * 100.0);
    std::printf("Max size:    %llu\n\n", (unsigned long long)config.max_size);

    OrderBook book;
    std::vector<long long> latencies_ns;
    latencies_ns.reserve(n);
    std::uint64_t total_trades = 0;

    auto wall_start = std::chrono::high_resolution_clock::now();

    for (const auto& order : orders) {
        auto t0 = std::chrono::high_resolution_clock::now();
        try {
            auto trades = book.place_limit_order(order);
            total_trades += trades.size();
        } catch (const BookError&) {
            // Self-cross or duplicate — skip silently, same as the reference bench.
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        latencies_ns.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
    }

    auto wall_end = std::chrono::high_resolution_clock::now();
    double wall_ms = std::chrono::duration<double, std::milli>(wall_end - wall_start).count();

    std::sort(latencies_ns.begin(), latencies_ns.end());

    long long total_ns = 0;
    for (auto v : latencies_ns) total_ns += v;
    long long avg_ns = total_ns / static_cast<long long>(n);
    long long p50 = latencies_ns[n * 50 / 100];
    long long p99 = latencies_ns[n * 99 / 100];

    double orders_per_sec = (wall_ms > 0.0) ? (static_cast<double>(n) / (wall_ms / 1000.0)) : 0.0;

    std::printf("%-24s %15s\n", "Metric", "Value");
    std::printf("%-24s %15s\n", "------------------------", "---------------");
    std::printf("%-24s %12.0f /s\n", "Throughput", orders_per_sec);
    std::printf("%-24s %12lld ns\n", "Avg latency", avg_ns);
    std::printf("%-24s %12lld ns\n", "p50 latency", p50);
    std::printf("%-24s %12lld ns\n", "p99 latency", p99);
    std::printf("%-24s %15llu\n", "Total trades", (unsigned long long)total_trades);
    std::printf("%-24s %12.2f ms\n", "Wall clock time", wall_ms);

    return 0;
}
