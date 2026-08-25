#pragma once

#include <deque>
#include <map>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

#include "types.hpp"

// A price-time-priority limit order book.
//
// Internals:
// - `bids_`/`asks_`: std::map sorted ascending by price. Bids read from the
//   back (highest price = best bid); asks read from the front (lowest price
//   = best ask).
// - `order_index_`: maps OrderId -> (Side, Price) for O(1) lookup of an
//   order's location by ID during cancellation.
class OrderBook {
public:
    std::optional<Price> best_bid() const;
    std::optional<Price> best_ask() const;
    Qty depth_at_price(Side side, Price price) const;

    std::vector<Trade> place_limit_order(const Order& order);
    Order cancel_order(OrderId order_id);

private:
    std::map<Price, std::deque<Order>> bids_;
    std::map<Price, std::deque<Order>> asks_;
    std::unordered_map<OrderId, std::pair<Side, Price>> order_index_;

    bool would_self_cross(const Order& order) const;
    Qty match_at_level(Side resting_side, Price price, const Order& incoming, Qty remaining,
                        std::vector<Trade>& trades);
};
