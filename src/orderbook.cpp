#include "orderbook.hpp"

#include <algorithm>

std::optional<Price> OrderBook::best_bid() const {
    if (bids_.empty()) return std::nullopt;
    return bids_.rbegin()->first;
}

std::optional<Price> OrderBook::best_ask() const {
    if (asks_.empty()) return std::nullopt;
    return asks_.begin()->first;
}

Qty OrderBook::depth_at_price(Side side, Price price) const {
    const auto& book = (side == Side::Bid) ? bids_ : asks_;
    auto it = book.find(price);
    if (it == book.end()) return 0;
    Qty total = 0;
    for (const auto& o : it->second) total += o.size;
    return total;
}

bool OrderBook::would_self_cross(const Order& order) const {
    const auto& opposite = (order.side == Side::Bid) ? asks_ : bids_;

    if (order.side == Side::Bid) {
        // Ask levels with price <= bid price would cross.
        for (auto it = opposite.begin(); it != opposite.end() && it->first <= order.price; ++it) {
            for (const auto& resting : it->second) {
                if (resting.owner == order.owner) return true;
            }
        }
    } else {
        // Bid levels with price >= ask price would cross.
        for (auto it = opposite.lower_bound(order.price); it != opposite.end(); ++it) {
            for (const auto& resting : it->second) {
                if (resting.owner == order.owner) return true;
            }
        }
    }
    return false;
}

Qty OrderBook::match_at_level(Side resting_side, Price price, const Order& incoming, Qty remaining,
                               std::vector<Trade>& trades) {
    auto& book = (resting_side == Side::Bid) ? bids_ : asks_;

    auto level_it = book.find(price);
    if (level_it == book.end()) return remaining;

    auto& queue = level_it->second;

    while (!queue.empty() && remaining > 0) {
        Order& maker = queue.front();
        Qty fill_size = std::min(remaining, maker.size);

        trades.push_back(Trade{maker.id, incoming.id, price, fill_size});

        remaining -= fill_size;
        maker.size -= fill_size;

        if (maker.size == 0) {
            order_index_.erase(maker.id);
            queue.pop_front();
        }
    }

    if (queue.empty()) {
        book.erase(level_it);
    }

    return remaining;
}

std::vector<Trade> OrderBook::place_limit_order(const Order& order) {
    if (order.size == 0) {
        throw BookError(BookErrorKind::ZeroSize, order.id, "order size must be > 0");
    }
    if (order_index_.find(order.id) != order_index_.end()) {
        throw BookError(BookErrorKind::DuplicateOrderId, order.id, "duplicate order id");
    }
    if (would_self_cross(order)) {
        throw BookError(BookErrorKind::SelfCross, order.id, "order would self-cross");
    }

    std::vector<Trade> trades;
    Qty remaining = order.size;

    if (order.side == Side::Bid) {
        // A bid crosses if bid_price >= ask_price. Walk asks ascending
        // (best/lowest first) up to and including the bid price.
        std::vector<Price> matchable;
        for (auto it = asks_.begin(); it != asks_.end() && it->first <= order.price; ++it) {
            matchable.push_back(it->first);
        }
        for (Price ask_price : matchable) {
            if (remaining == 0) break;
            remaining = match_at_level(Side::Ask, ask_price, order, remaining, trades);
        }
    } else {
        // An ask crosses if ask_price <= bid_price. Walk bids descending
        // (best/highest first) down to the ask price.
        std::vector<Price> matchable;
        for (auto it = bids_.lower_bound(order.price); it != bids_.end(); ++it) {
            matchable.push_back(it->first);
        }
        std::reverse(matchable.begin(), matchable.end());
        for (Price bid_price : matchable) {
            if (remaining == 0) break;
            remaining = match_at_level(Side::Bid, bid_price, order, remaining, trades);
        }
    }

    if (remaining > 0) {
        Order resting = order;
        resting.size = remaining;
        auto& book = (order.side == Side::Bid) ? bids_ : asks_;
        book[order.price].push_back(resting);
        order_index_[order.id] = {order.side, order.price};
    }

    return trades;
}

Order OrderBook::cancel_order(OrderId order_id) {
    auto idx_it = order_index_.find(order_id);
    if (idx_it == order_index_.end()) {
        throw BookError(BookErrorKind::OrderNotFound, order_id, "order not found");
    }

    Side side = idx_it->second.first;
    Price price = idx_it->second.second;
    order_index_.erase(idx_it);

    auto& book = (side == Side::Bid) ? bids_ : asks_;
    auto level_it = book.find(price);

    auto& queue = level_it->second;
    auto pos = std::find_if(queue.begin(), queue.end(),
                             [order_id](const Order& o) { return o.id == order_id; });

    Order cancelled = *pos;
    queue.erase(pos);

    if (queue.empty()) {
        book.erase(level_it);
    }

    return cancelled;
}
