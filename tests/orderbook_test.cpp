#include "orderbook.hpp"
#include "test_harness.hpp"

static Order make_order(OrderId id, OwnerId owner, Side side, Price price, Qty size, Timestamp ts) {
    return Order{id, owner, side, price, size, ts};
}

// -- Empty book ---------------------------------------------------------

TEST(empty_book_has_no_best_prices) {
    OrderBook book;
    CHECK(!book.best_bid().has_value());
    CHECK(!book.best_ask().has_value());
    CHECK_EQ(book.depth_at_price(Side::Bid, 100), Qty{0});
    CHECK_EQ(book.depth_at_price(Side::Ask, 100), Qty{0});
}

// -- Resting orders (no cross) -------------------------------------------

TEST(resting_bid_updates_best_bid) {
    OrderBook book;
    auto trades = book.place_limit_order(make_order(1, 1, Side::Bid, 100, 10, 0));
    CHECK(trades.empty());
    CHECK_EQ(book.best_bid().value(), Price{100});
    CHECK(!book.best_ask().has_value());
    CHECK_EQ(book.depth_at_price(Side::Bid, 100), Qty{10});
}

TEST(resting_ask_updates_best_ask) {
    OrderBook book;
    auto trades = book.place_limit_order(make_order(1, 1, Side::Ask, 200, 5, 0));
    CHECK(trades.empty());
    CHECK_EQ(book.best_ask().value(), Price{200});
    CHECK(!book.best_bid().has_value());
}

// -- Simple cross ---------------------------------------------------------

TEST(simple_cross_full_fill) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 100, 10, 0));
    auto trades = book.place_limit_order(make_order(2, 2, Side::Bid, 100, 10, 1));

    CHECK_EQ(trades.size(), size_t{1});
    CHECK(trades[0] == (Trade{1, 2, 100, 10}));
    CHECK(!book.best_bid().has_value());
    CHECK(!book.best_ask().has_value());
}

TEST(bid_crosses_at_better_price) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 100, 10, 0));
    auto trades = book.place_limit_order(make_order(2, 2, Side::Bid, 105, 10, 1));
    CHECK_EQ(trades.size(), size_t{1});
    CHECK_EQ(trades[0].price, Price{100});
}

// -- Partial fills ---------------------------------------------------------

TEST(partial_fill_taker_remainder_rests) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 100, 5, 0));
    auto trades = book.place_limit_order(make_order(2, 2, Side::Bid, 100, 10, 1));

    CHECK_EQ(trades.size(), size_t{1});
    CHECK_EQ(trades[0].size, Qty{5});
    CHECK(!book.best_ask().has_value());
    CHECK_EQ(book.best_bid().value(), Price{100});
    CHECK_EQ(book.depth_at_price(Side::Bid, 100), Qty{5});
}

TEST(partial_fill_maker_remains) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 100, 10, 0));
    auto trades = book.place_limit_order(make_order(2, 2, Side::Bid, 100, 3, 1));

    CHECK_EQ(trades.size(), size_t{1});
    CHECK_EQ(trades[0].size, Qty{3});
    CHECK_EQ(book.best_ask().value(), Price{100});
    CHECK_EQ(book.depth_at_price(Side::Ask, 100), Qty{7});
    CHECK(!book.best_bid().has_value());
}

// -- Multiple fills walking the book ---------------------------------------

TEST(multiple_fills_walk_the_book) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 100, 5, 0));
    book.place_limit_order(make_order(2, 2, Side::Ask, 101, 5, 1));
    book.place_limit_order(make_order(3, 3, Side::Ask, 102, 5, 2));

    auto trades = book.place_limit_order(make_order(4, 4, Side::Bid, 102, 12, 3));

    CHECK_EQ(trades.size(), size_t{3});
    CHECK(trades[0] == (Trade{1, 4, 100, 5}));
    CHECK(trades[1] == (Trade{2, 4, 101, 5}));
    CHECK(trades[2] == (Trade{3, 4, 102, 2}));

    CHECK_EQ(book.best_ask().value(), Price{102});
    CHECK_EQ(book.depth_at_price(Side::Ask, 102), Qty{3});
    CHECK(!book.best_bid().has_value());
}

// -- Cancellation -----------------------------------------------------------

TEST(cancel_removes_order) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Bid, 100, 10, 0));
    CHECK_EQ(book.best_bid().value(), Price{100});

    Order cancelled = book.cancel_order(1);
    CHECK_EQ(cancelled.id, OrderId{1});
    CHECK_EQ(cancelled.size, Qty{10});
    CHECK(!book.best_bid().has_value());
}

TEST(cancel_nonexistent_order_fails) {
    OrderBook book;
    bool threw = false;
    try {
        book.cancel_order(999);
    } catch (const BookError& e) {
        threw = true;
        CHECK(e.kind == BookErrorKind::OrderNotFound);
        CHECK_EQ(e.order_id, OrderId{999});
    }
    CHECK(threw);
}

// -- Self-cross protection ----------------------------------------------

TEST(self_cross_rejected) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 100, 10, 0));
    bool threw = false;
    try {
        book.place_limit_order(make_order(2, 1, Side::Bid, 100, 5, 1));
    } catch (const BookError& e) {
        threw = true;
        CHECK(e.kind == BookErrorKind::SelfCross);
        CHECK_EQ(e.order_id, OrderId{2});
    }
    CHECK(threw);
    CHECK_EQ(book.best_ask().value(), Price{100});
    CHECK_EQ(book.depth_at_price(Side::Ask, 100), Qty{10});
}

TEST(self_cross_not_triggered_when_no_overlap) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 105, 10, 0));
    auto trades = book.place_limit_order(make_order(2, 1, Side::Bid, 100, 5, 1));
    CHECK(trades.empty());
    CHECK_EQ(book.best_bid().value(), Price{100});
}

// -- Price-time priority -----------------------------------------------

TEST(time_priority_at_same_price) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 100, 5, 0));
    book.place_limit_order(make_order(2, 2, Side::Ask, 100, 5, 1));

    auto trades = book.place_limit_order(make_order(3, 3, Side::Bid, 100, 3, 2));
    CHECK_EQ(trades.size(), size_t{1});
    CHECK_EQ(trades[0].maker_id, OrderId{1});
    CHECK_EQ(trades[0].size, Qty{3});
    CHECK_EQ(book.depth_at_price(Side::Ask, 100), Qty{7});
}

TEST(price_priority_over_time) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Ask, 101, 5, 0));
    book.place_limit_order(make_order(2, 2, Side::Ask, 100, 5, 1));

    auto trades = book.place_limit_order(make_order(3, 3, Side::Bid, 101, 8, 2));
    CHECK_EQ(trades.size(), size_t{2});
    CHECK_EQ(trades[0].price, Price{100});
    CHECK_EQ(trades[1].price, Price{101});
}

// -- Edge cases -----------------------------------------------------------

TEST(zero_size_rejected) {
    OrderBook book;
    bool threw = false;
    try {
        book.place_limit_order(make_order(1, 1, Side::Bid, 100, 0, 0));
    } catch (const BookError& e) {
        threw = true;
        CHECK(e.kind == BookErrorKind::ZeroSize);
    }
    CHECK(threw);
}

TEST(duplicate_order_id_rejected) {
    OrderBook book;
    book.place_limit_order(make_order(1, 1, Side::Bid, 100, 10, 0));
    bool threw = false;
    try {
        book.place_limit_order(make_order(1, 2, Side::Ask, 200, 5, 1));
    } catch (const BookError& e) {
        threw = true;
        CHECK(e.kind == BookErrorKind::DuplicateOrderId);
        CHECK_EQ(e.order_id, OrderId{1});
    }
    CHECK(threw);
}

int main() {
    return testkit::run_all();
}
