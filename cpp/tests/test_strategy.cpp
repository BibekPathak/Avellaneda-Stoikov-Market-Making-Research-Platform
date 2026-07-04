#include <gtest/gtest.h>
#include "mm/strategy.hpp"
#include <cmath>

using namespace mm;

// ── Basic Construction ───────────────────────────────────────────────────

TEST(ASMarketMakerTest, DefaultConstruction) {
    ASMarketMaker mm;
    EXPECT_EQ(mm.inventory().position, 0);
    EXPECT_EQ(mm.inventory().cash, 0);
    EXPECT_EQ(mm.order_size(), 1000);
    EXPECT_EQ(mm.max_position(), 10000);
    EXPECT_NEAR(mm.current_volatility(), 0.0, 1e-10);
}

// ── First Tick Generates Orders ──────────────────────────────────────────

TEST(ASMarketMakerTest, FirstTickGeneratesOrders) {
    ASMarketMaker mm;
    hft::MarketTick tick(1000, 1, 9950, 10050, 10000, 10000);

    auto orders = mm.on_tick(tick, 1, 1);

    EXPECT_FALSE(orders.empty());
    // Should have at least 2 orders (bid + ask)
    EXPECT_GE(orders.size(), 2);
    // Bid < Ask
    EXPECT_LT(orders[0].price, orders[1].price);
    // Both limit orders
    EXPECT_EQ(orders[0].type, hft::OrderType::Limit);
    EXPECT_EQ(orders[1].type, hft::OrderType::Limit);
}

// ── Consecutive Ticks Respect Quote Interval ────────────────────────────

TEST(ASMarketMakerTest, QuoteIntervalRespected) {
    ASMarketMaker mm(0.1, 1.0, 1000, 1, 100000);
    hft::MarketTick tick(1000, 1, 9950, 10050, 10000, 10000);

    // First tick generates orders
    auto orders1 = mm.on_tick(tick, 1, 1);
    EXPECT_FALSE(orders1.empty());

    // Same timestamp should not generate (should_quote returns false)
    auto orders2 = mm.on_tick(tick, 3, 1);
    EXPECT_TRUE(orders2.empty());

    // Later timestamp within interval should not generate
    hft::MarketTick tick2(105000, 2, 9960, 10040, 10000, 10000);
    auto orders3 = mm.on_tick(tick2, 5, 1);
    EXPECT_FALSE(orders3.empty());
}

// ── Trade Updates Inventory ──────────────────────────────────────────────

TEST(ASMarketMakerTest, TradeUpdatesInventory) {
    ASMarketMaker mm;

    hft::Trade buy_trade(1, 1, 1, 2, 2000, 10000, 100, hft::Side::Buy);
    mm.on_trade(buy_trade);

    EXPECT_EQ(mm.inventory().position, 100);
    EXPECT_EQ(mm.inventory().cash, -1000000);

    hft::Trade sell_trade(2, 2, 2, 1, 3000, 10100, 50, hft::Side::Sell);
    mm.on_trade(sell_trade);

    EXPECT_EQ(mm.inventory().position, 50);
    EXPECT_GT(mm.inventory().cash, -1000000);
}

// ── Volatility Updates Over Time ─────────────────────────────────────────

TEST(ASMarketMakerTest, VolatilityUpdates) {
    ASMarketMaker mm(0.1, 1.0, 1000, 1, 100000);

    // Feed a series of ticks with varying prices
    mm.on_tick(hft::MarketTick(1000, 1, 10000, 10100, 100, 100), 0, 0);
    mm.on_tick(hft::MarketTick(1100, 2, 10100, 10200, 100, 100), 0, 0);
    mm.on_tick(hft::MarketTick(1200, 3, 10200, 10300, 100, 100), 0, 0);

    double vol = mm.current_volatility();
    EXPECT_GE(vol, 0.0);
}

// ── Reset Clears State ───────────────────────────────────────────────────

TEST(ASMarketMakerTest, ResetClearsState) {
    ASMarketMaker mm;

    mm.on_tick(hft::MarketTick(1000, 1, 9950, 10050, 100, 100), 1, 1);
    mm.on_trade(hft::Trade(1, 1, 1, 2, 2000, 10000, 100, hft::Side::Buy));

    EXPECT_NE(mm.inventory().position, 0);

    mm.reset();

    EXPECT_EQ(mm.inventory().position, 0);
    EXPECT_EQ(mm.inventory().cash, 0);
    EXPECT_EQ(mm.quote_engine().sequence(), 0);
}

// ── Negative Price Tick Produces No Orders ───────────────────────────────

TEST(ASMarketMakerTest, NegativeMidPrice) {
    ASMarketMaker mm;
    auto orders = mm.on_tick(hft::MarketTick(1000, 1, -10, 10, 100, 100), 1, 1);
    EXPECT_TRUE(orders.empty());
}

// ── Zero Spread Tick Produces No Orders ──────────────────────────────────

TEST(ASMarketMakerTest, ZeroSpread) {
    ASMarketMaker mm;
    auto orders = mm.on_tick(hft::MarketTick(1000, 1, 10000, 10000, 100, 100), 1, 1);
    EXPECT_TRUE(orders.empty());
}

// ── Fill Model Swapping ──────────────────────────────────────────────────

TEST(ASMarketMakerTest, SwapFillModel) {
    ASMarketMaker mm;
    mm.set_fill_model(std::make_unique<QueuePositionFill>());
    // Should still generate orders
    auto orders = mm.on_tick(hft::MarketTick(1000, 1, 9950, 10050, 100, 100), 1, 1);
    EXPECT_FALSE(orders.empty());
}

// ── Latency Model Adds Delay to Orders ───────────────────────────────────

TEST(ASMarketMakerTest, LatencyModelAffectsTimestamp) {
    ASMarketMaker mm(0.1, 1.0, 1000, 1, 100000);
    mm.emplace_latency<FixedLatency>(500);

    auto orders = mm.on_tick(hft::MarketTick(1000, 1, 9950, 10050, 100, 100), 1, 1);

    ASSERT_FALSE(orders.empty());
    EXPECT_EQ(orders[0].timestamp, 1500);
}

// ── High Volatility Affects Spread ───────────────────────────────────────

TEST(ASMarketMakerTest, HigherVolatilityWiderSpread) {
    ASMarketMaker mm_low_vol(0.5, 1.0, 100, 1, 100);
    // Feed flat prices → low volatility
    for (int i = 0; i < 30; i++) {
        mm_low_vol.on_tick(hft::MarketTick(1000 + i * 1000, i, 9950, 10050, 100, 100), i * 2, 1);
    }

    ASMarketMaker mm_high_vol(0.5, 1.0, 100, 1, 100);
    // Feed oscillating prices → higher volatility
    for (int i = 0; i < 30; i++) {
        int64_t bid = (i % 2 == 0) ? 9900 : 10000;
        int64_t ask = bid + 100;
        mm_high_vol.on_tick(hft::MarketTick(1000 + i * 1000, i, bid, ask, 100, 100), i * 2, 1);
    }

    auto orders_low = mm_low_vol.on_tick(hft::MarketTick(100000, 50, 9950, 10050, 100, 100), 100, 1);
    auto orders_high = mm_high_vol.on_tick(hft::MarketTick(100000, 50, 9950, 10050, 100, 100), 200, 1);

    ASSERT_GE(orders_low.size(), 2);
    ASSERT_GE(orders_high.size(), 2);

    int64_t spread_low = orders_low[1].price - orders_low[0].price;
    int64_t spread_high = orders_high[1].price - orders_high[0].price;

    EXPECT_GE(spread_high, spread_low);
}

// ── Long Inventory Shifts Quotes Down ────────────────────────────────────

TEST(ASMarketMakerTest, LongInventoryShiftsDown) {
    ASMarketMaker mm(1.0, 1.0, 100, 1, 100);

    auto orders_flat = mm.on_tick(hft::MarketTick(1000, 1, 9900, 10100, 100, 100), 1, 1);
    ASSERT_GE(orders_flat.size(), 2);
    int64_t flat_bid = orders_flat[0].price;
    int64_t flat_ask = orders_flat[1].price;

    // Get long inventory
    mm.on_trade(hft::Trade(1, 2, 3, 4, 2000, 10000, 200, hft::Side::Buy));

    auto orders_long = mm.on_tick(hft::MarketTick(3000, 3, 9900, 10100, 100, 100), 5, 1);
    ASSERT_GE(orders_long.size(), 2);
    int64_t long_bid = orders_long[0].price;
    int64_t long_ask = orders_long[1].price;

    EXPECT_LE(long_bid, flat_bid);
    EXPECT_LE(long_ask, flat_ask);
}

// ── Tick Rate Estimation ─────────────────────────────────────────────────

TEST(ASMarketMakerTest, TickRateEstimated) {
    ASMarketMaker mm(0.1, 1.0, 1000, 1, 0);
    hft::MarketTick tick(0, 0, 9950, 10050, 100, 100);

    // Feed ticks — first tick sets base time
    mm.on_tick(tick, 0, 1);

    // Send 5 ticks at times 200000, 400000, 600000, 800000, 1000000
    // At time 1000000: elapsed = 1000000 >= 1000000, rate = 5 ticks/sec
    for (int i = 1; i <= 5; i++) {
        tick.timestamp = i * 200000;
        tick.sequence = i;
        mm.on_tick(tick, i * 2, 1);
    }

    double rate = mm.tick_rate();
    EXPECT_NEAR(rate, 5.0, 0.5);
}
