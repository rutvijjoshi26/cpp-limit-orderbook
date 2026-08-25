#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

// Core types for the matching engine.

using OrderId = std::uint64_t;
using Price = std::uint64_t;
using Qty = std::uint64_t;
using Timestamp = std::uint64_t;
using OwnerId = std::uint64_t;

enum class Side { Bid, Ask };

struct Order {
    OrderId id;
    OwnerId owner;
    Side side;
    Price price;
    Qty size;
    Timestamp timestamp;

    bool operator==(const Order& other) const {
        return id == other.id && owner == other.owner && side == other.side &&
               price == other.price && size == other.size && timestamp == other.timestamp;
    }
};

struct Trade {
    OrderId maker_id;
    OrderId taker_id;
    Price price;
    Qty size;

    bool operator==(const Trade& other) const {
        return maker_id == other.maker_id && taker_id == other.taker_id &&
               price == other.price && size == other.size;
    }
};

enum class BookErrorKind { ZeroSize, DuplicateOrderId, SelfCross, OrderNotFound };

class BookError : public std::runtime_error {
public:
    BookErrorKind kind;
    OrderId order_id;

    BookError(BookErrorKind k, OrderId id, const std::string& message)
        : std::runtime_error(message), kind(k), order_id(id) {}
};
