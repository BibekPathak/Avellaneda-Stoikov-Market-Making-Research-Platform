#include <gtest/gtest.h>
#include "mm/quote_engine.hpp"
#include <cmath>

using namespace mm;

// ── Default Construction ─────────────────────────────────────────────────

TEST(QuoteEngineTest, DefaultConstruction) {
    QuoteEngine qe;
    EXPECT_EQ(qe.tick_size(), 1);
    EXPECT_EQ(qe.quote_interval(), 100000);
    EXPECT_EQ(qe.order_size(), 1000);
    EXPECT_EQ(qe.sequence(), 0);
    EXPECT_EQ(qe.last_quote_time(), 0);
}

// ── Custom Construction ──────────────────────────────────────────────────

TEST(QuoteEngineTest, CustomConstruction) {
    QuoteEngine qe(10, 50000, 500);
    EXPECT_EQ(qe.tick_size(), 10);
    EXPECT_EQ(qe.quote_interval(), 50000);
    EXPECT_EQ(qe.order_size(), 500);
}

// ── Zero Tick Size Clamped to 1 ──────────────────────────────────────────

TEST(QuoteEngineTest, ZeroTickSizeClamped) {
    QuoteEngine qe(0, 100000, 1000);
    EXPECT_EQ(qe.tick_size(), 1);
}

// ── Basic Quote Generation ───────────────────────────────────────────────

TEST(QuoteEngineTest, BasicQuote) {
    QuoteEngine qe(1, 100000, 1000);
    Quote q = qe.generate_quote(10000.0, 50.0, 1000);

    EXPECT_EQ(q.bid_price, 9950);
    EXPECT_EQ(q.ask_price, 10050);
    EXPECT_EQ(q.bid_size, 1000);
    EXPECT_EQ(q.ask_size, 1000);
    EXPECT_EQ(q.timestamp, 1000);
    EXPECT_EQ(q.sequence, 1);
}

// ── Quote Sequence Increments ────────────────────────────────────────────

TEST(QuoteEngineTest, SequenceIncrements) {
    QuoteEngine qe;
    Quote q1 = qe.generate_quote(10000.0, 50.0, 1000);
    Quote q2 = qe.generate_quote(10000.0, 50.0, 2000);
    Quote q3 = qe.generate_quote(10000.0, 50.0, 3000);

    EXPECT_EQ(q1.sequence, 1);
    EXPECT_EQ(q2.sequence, 2);
    EXPECT_EQ(q3.sequence, 3);
}

// ── Tick Size Rounding ───────────────────────────────────────────────────

TEST(QuoteEngineTest, TickSizeRounding) {
    QuoteEngine qe(10, 100000, 1000);
    Quote q = qe.generate_quote(10000.0, 53.0, 1000);

    // reservation = 10000, half_spread = 53
    // raw_bid = 10000 - 53 = 9947 → snap to 10 → 9950
    // raw_ask = 10000 + 53 = 10053 → snap to 10 → 10050
    EXPECT_EQ(q.bid_price, 9950);
    EXPECT_EQ(q.ask_price, 10050);
}

// ── Tick Size Rounding: Fractional Prices ────────────────────────────────

TEST(QuoteEngineTest, TickSizeRoundingFractional) {
    QuoteEngine qe(5, 100000, 1000);
    Quote q = qe.generate_quote(10001.5, 52.3, 1000);

    // raw_bid = 10001.5 - 52.3 = 9949.2 → snap to 5 → 9949.2/5 = 1989.84 → 1990 * 5 = 9950
    // raw_ask = 10001.5 + 52.3 = 10053.8 → snap to 5 → 10053.8/5 = 2010.76 → 2011 * 5 = 10055
    EXPECT_TRUE(q.bid_price % 5 == 0);
    EXPECT_TRUE(q.ask_price % 5 == 0);
    EXPECT_LT(q.bid_price, q.ask_price);
}

// ── Min Spread Enforcement: Spread Too Narrow ────────────────────────────

TEST(QuoteEngineTest, MinSpreadEnforcement) {
    QuoteEngine qe(1, 100000, 1000);
    qe.set_min_spread(10);

    // reservation = 10000, half_spread = 1
    // raw_bid = 9999, raw_ask = 10001, spread = 2 < 10
    Quote q = qe.generate_quote(10000.0, 1.0, 1000);

    EXPECT_GE(q.ask_price - q.bid_price, 10);
    EXPECT_TRUE(q.bid_price < q.ask_price);
}

// ── Min Spread: Symmetric Expansion Around Reservation ───────────────────

TEST(QuoteEngineTest, MinSpreadSymmetric) {
    QuoteEngine qe(1, 100000, 1000);
    qe.set_min_spread(20);

    Quote q = qe.generate_quote(10000.0, 2.0, 1000);
    // reservation = 10000, raw_bid = 9998, raw_ask = 10002, spread = 4
    // adjustment = (20-4)/2 = 8, bid -= 8, ask += 8 → 9990, 10010
    // remainder = 20 - (10010-9990) = 0

    EXPECT_GE(q.ask_price - q.bid_price, 20);
    EXPECT_EQ((q.bid_price + q.ask_price) / 2, 10000);
}

// ── Min Spread: Odd Adjustment ───────────────────────────────────────────

TEST(QuoteEngineTest, MinSpreadOddAdjustment) {
    QuoteEngine qe(1, 100000, 1000);
    qe.set_min_spread(11);

    Quote q = qe.generate_quote(10000.0, 2.0, 1000);
    // raw_bid = 9998, raw_ask = 10002, spread = 4
    // adjustment = (11-4)/2 = 3 (integer), bid -= 3, ask += 3 → 9995, 10005
    // spread = 10, remain = 11 - 10 = 1, ask += 1 → 10006
    // final: 9995, 10006, spread = 11

    EXPECT_GE(q.ask_price - q.bid_price, 11);
}

// ── should_quote: Initially Should Quote ─────────────────────────────────

TEST(QuoteEngineTest, ShouldQuoteInitially) {
    QuoteEngine qe(1, 100000, 1000);
    EXPECT_TRUE(qe.should_quote(0));
    EXPECT_TRUE(qe.should_quote(50000));
}

// ── should_quote: After Quote, Respects Interval ─────────────────────────

TEST(QuoteEngineTest, ShouldQuoteInterval) {
    QuoteEngine qe(1, 100000, 1000);

    qe.generate_quote(10000.0, 50.0, 100000);

    EXPECT_FALSE(qe.should_quote(100000));
    EXPECT_FALSE(qe.should_quote(150000));
    EXPECT_FALSE(qe.should_quote(199999));
    EXPECT_TRUE(qe.should_quote(200000));
}

// ── should_quote: Out of Order Timestamps ────────────────────────────────

TEST(QuoteEngineTest, ShouldQuoteOutOfOrder) {
    QuoteEngine qe(1, 100000, 1000);

    qe.generate_quote(10000.0, 50.0, 200000);

    EXPECT_TRUE(qe.should_quote(100000)); // earlier than last quote time
}

// ── Quote from ASQuote ───────────────────────────────────────────────────

TEST(QuoteEngineTest, QuoteFromASQuote) {
    QuoteEngine qe(1, 100000, 500);

    ASQuote asq;
    asq.reservation_price = 10000.0;
    asq.half_spread = 50.0;

    Quote q = qe.generate_quote(asq, 1000);

    EXPECT_EQ(q.bid_price, 9950);
    EXPECT_EQ(q.ask_price, 10050);
    EXPECT_EQ(q.bid_size, 500);
    EXPECT_EQ(q.ask_size, 500);
}

// ── Large Tick Size ──────────────────────────────────────────────────────

TEST(QuoteEngineTest, LargeTickSize) {
    QuoteEngine qe(100, 100000, 1000);

    Quote q = qe.generate_quote(10000.0, 250.0, 1000);

    EXPECT_TRUE(q.bid_price % 100 == 0);
    EXPECT_TRUE(q.ask_price % 100 == 0);
    EXPECT_LT(q.bid_price, q.ask_price);
}

// ── Min Spread with Large Tick Size ──────────────────────────────────────

TEST(QuoteEngineTest, MinSpreadWithLargeTick) {
    QuoteEngine qe(50, 100000, 1000);
    // Default min_spread = 50 * 2 = 100

    Quote q = qe.generate_quote(10000.0, 10.0, 1000);

    EXPECT_GE(q.ask_price - q.bid_price, 100);
}

// ── Reset ──────────────────────────────────────────────────────────────────

TEST(QuoteEngineTest, Reset) {
    QuoteEngine qe(1, 100000, 1000);

    qe.generate_quote(10000.0, 50.0, 1000);
    EXPECT_EQ(qe.sequence(), 1);
    EXPECT_GT(qe.last_quote_time(), 0);

    qe.reset();
    EXPECT_EQ(qe.sequence(), 0);
    EXPECT_EQ(qe.last_quote_time(), 0);
}

// ── Set Methods ──────────────────────────────────────────────────────────

TEST(QuoteEngineTest, SetMethods) {
    QuoteEngine qe;

    qe.set_tick_size(25);
    EXPECT_EQ(qe.tick_size(), 25);

    qe.set_quote_interval(250000);
    EXPECT_EQ(qe.quote_interval(), 250000);

    qe.set_order_size(2500);
    EXPECT_EQ(qe.order_size(), 2500);

    qe.set_min_spread(50);
    EXPECT_EQ(qe.min_spread(), 50);
}

// ── Set Zero Tick Size Clamped ─────────────────────────────────────────

TEST(QuoteEngineTest, SetZeroTickSizeClamped) {
    QuoteEngine qe;
    qe.set_tick_size(0);
    EXPECT_EQ(qe.tick_size(), 1);
}

// ── Set Zero Min Spread Clamped ─────────────────────────────────────────

TEST(QuoteEngineTest, SetZeroMinSpreadClamped) {
    QuoteEngine qe;
    qe.set_min_spread(0);
    EXPECT_GT(qe.min_spread(), 0);
}

// ── Negative Reservation Price (Edge Case) ───────────────────────────────

TEST(QuoteEngineTest, NegativeReservationPrice) {
    QuoteEngine qe(1, 100000, 1000);
    Quote q = qe.generate_quote(-100.0, 50.0, 1000);

    EXPECT_LT(q.ask_price, 0);
    EXPECT_LT(q.bid_price, q.ask_price);
}

// ── Zero Spread Quote (Edge Case) ────────────────────────────────────────

TEST(QuoteEngineTest, ZeroSpread) {
    QuoteEngine qe(1, 100000, 1000);

    Quote q = qe.generate_quote(10000.0, 0.0, 1000);
    // raw_bid = raw_ask = 10000, spread = 0
    // min_spread_ = 2, adjustment = 1, bid = 9999, ask = 10001
    EXPECT_EQ(q.bid_price, 9999);
    EXPECT_EQ(q.ask_price, 10001);
    EXPECT_LT(q.bid_price, q.ask_price);
}
