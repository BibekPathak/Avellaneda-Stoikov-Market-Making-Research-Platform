#include <gtest/gtest.h>
#include "mm/analytics.hpp"
#include "mm/simulator.hpp"
#include <memory>
#include <sstream>

using namespace mm;

// ── Empty Result ─────────────────────────────────────────────────────────

TEST(AnalyticsTest, EmptyResult) {
    SimResult empty;
    Analytics analytics(empty);

    auto pnl = analytics.compute_pnl_attribution();
    EXPECT_EQ(pnl.total, 0);

    auto ts = analytics.compute_trade_stats();
    EXPECT_EQ(ts.total_trades, 0);

    auto rs = analytics.compute_risk_stats();
    EXPECT_NEAR(rs.sharpe, 0.0, 1e-10);

    auto fs = analytics.compute_fill_stats();
    EXPECT_EQ(fs.total_fills, 0);
}

// ── Single Snapshot ──────────────────────────────────────────────────────

TEST(AnalyticsTest, SingleSnapshot) {
    SimResult result;
    SimSnapshot snap;
    snap.tick = 1;
    snap.timestamp = 1000;
    snap.mid_price = 10000;
    snap.inventory = 0;
    snap.cash = 0;
    snap.total_pnl = 0;
    result.snapshots.push_back(snap);

    Analytics analytics(result);
    auto rs = analytics.compute_risk_stats();
    EXPECT_NEAR(rs.sharpe, 0.0, 1e-10);
}

// ── With Trades ──────────────────────────────────────────────────────────

TEST(AnalyticsTest, WithTrades) {
    SimResult result;

    SimSnapshot s1;
    s1.tick = 1; s1.timestamp = 1000; s1.mid_price = 10000;
    s1.inventory = 0; s1.total_pnl = 0;
    result.snapshots.push_back(s1);

    SimSnapshot s2;
    s2.tick = 2; s2.timestamp = 2000; s2.mid_price = 10100;
    s2.inventory = 0; s2.total_pnl = 10000;
    result.snapshots.push_back(s2);

    SimSnapshot s3;
    s3.tick = 3; s3.timestamp = 3000; s3.mid_price = 10050;
    s3.inventory = 0; s3.total_pnl = 5000;
    result.snapshots.push_back(s3);

    Analytics analytics(result);
    auto rs = analytics.compute_risk_stats();
    EXPECT_GT(rs.sharpe, -10.0);
    EXPECT_LT(rs.sharpe, 10.0);
}

// ── Adverse Selection Detection ──────────────────────────────────────────

TEST(AnalyticsTest, AdverseSelection) {
    SimResult result;

    for (int i = 0; i < 10; i++) {
        SimSnapshot snap;
        snap.tick = i + 1;
        snap.timestamp = (i + 1) * 1000;
        snap.mid_price = 10000 + i * 100;
        result.snapshots.push_back(snap);
    }

    // Buy fill at tick 3 (mid=10200), then price rises → no adverse selection (good)
    hft::Trade buy_trade(1, 1, 1, 2, 3000, 10150, 100, hft::Side::Buy);
    result.trades.push_back(buy_trade);

    // Sell fill at tick 6 (mid=10500), then price rises → adverse selection (sold too low)
    // Actually code checks: sell side, if mid_later > mid_at_fill → adverse
    // mid_at_fill (tick 6) = 10000 + 5*100 = 10500
    // mid_later (tick 6+5=11, but max is 10) = tick 10 = 10000 + 9*100 = 10900
    // 10900 > 10500 → adverse! loss = 50 * (10900 - 10500) = 20000

    // The actual trade was sold at 10500, but price went up to 10900
    // Wait, trade price is 10550 which is close to 10500
    // Sell was at ~10550 but if we sold when we should have held, we missed profit
    hft::Trade sell_trade(2, 2, 2, 1, 6000, 10550, 50, hft::Side::Sell);
    result.trades.push_back(sell_trade);

    Analytics analytics(result);
    int64_t adverse = analytics.compute_adverse_selection(5);

    EXPECT_GT(adverse, 0);
}

// ── Fill Stats ───────────────────────────────────────────────────────────

TEST(AnalyticsTest, FillStats) {
    SimResult result;

    for (int i = 0; i < 10; i++) {
        SimSnapshot snap;
        snap.tick = i + 1;
        snap.timestamp = (i + 1) * 1000;
        snap.mid_price = 10000;
        result.snapshots.push_back(snap);
    }

    // 3 fills among 10 quotes
    result.trades.emplace_back(1, 1, 1, 2, 1000, 10000, 100, hft::Side::Buy);
    result.trades.emplace_back(2, 2, 3, 4, 2000, 10050, 100, hft::Side::Sell);
    result.trades.emplace_back(3, 3, 5, 6, 3000, 10025, 100, hft::Side::Buy);

    Analytics analytics(result);
    auto fs = analytics.compute_fill_stats();

    EXPECT_EQ(fs.total_fills, 3);
    EXPECT_EQ(fs.total_quotes, 10);
    EXPECT_NEAR(fs.fill_rate, 0.3, 1e-10);
    EXPECT_NEAR(fs.avg_fill_price, (10000 + 10050 + 10025) / 3.0, 1e-6);
    EXPECT_EQ(fs.max_fill_price, 10050);
    EXPECT_EQ(fs.min_fill_price, 10000);
}

// ── Print Summary Does Not Crash ─────────────────────────────────────────

TEST(AnalyticsTest, PrintSummary) {
    SimResult result;

    // Add some data
    for (int i = 0; i < 5; i++) {
        SimSnapshot snap;
        snap.tick = i + 1;
        snap.timestamp = (i + 1) * 1000;
        snap.mid_price = 10000 + i * 50;
        snap.inventory = (i % 2 == 0) ? 100 : -50;
        snap.cash = (i % 2 == 0) ? -1000000 : 500000;
        snap.realized_pnl = i * 1000;
        snap.unrealized_pnl = 0;
        snap.total_pnl = i * 1000;
        snap.volatility = 0.01 * i;
        result.snapshots.push_back(snap);
    }

    result.trades.emplace_back(1, 1, 1, 2, 2000, 10000, 100, hft::Side::Buy);

    Analytics analytics(result);
    std::ostringstream oss;
    analytics.print_summary(oss);

    EXPECT_TRUE(oss.str().size() > 0);
    EXPECT_NE(oss.str().find("Total ticks"), std::string::npos);
    EXPECT_NE(oss.str().find("PnL"), std::string::npos);
    EXPECT_NE(oss.str().find("Sharpe"), std::string::npos);
}

// ── PnL Attribution ──────────────────────────────────────────────────────

TEST(AnalyticsTest, PnLAttributionBasic) {
    SimResult result;

    SimSnapshot snap;
    snap.tick = 1; snap.timestamp = 1000; snap.mid_price = 10000;
    snap.inventory = 0; snap.cash = 0;
    snap.realized_pnl = 10000; snap.unrealized_pnl = 2000;
    snap.total_pnl = 12000;
    result.snapshots.push_back(snap);

    snap.tick = 2; snap.timestamp = 2000; snap.mid_price = 10100;
    snap.inventory = 0; snap.realized_pnl = 10000; snap.unrealized_pnl = 2000;
    snap.total_pnl = 12000;
    result.snapshots.push_back(snap);

    Analytics analytics(result);
    auto pnl = analytics.compute_pnl_attribution();

    EXPECT_EQ(pnl.total, 12000);
    EXPECT_EQ(pnl.total_realized, 10000);
    EXPECT_EQ(pnl.total_unrealized, 2000);
}

// ── Simulator Integration Test ───────────────────────────────────────────

TEST(AnalyticsTest, FromSimulatorRun) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(100, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(0.1, 1.0, 500, 1, 0);
    Simulator sim(std::move(feed), std::move(strategy));
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    Analytics analytics(result);
    auto ss = analytics.compute_summary();

    EXPECT_EQ(ss.total_ticks, 100);
    EXPECT_GE(ss.risk.sharpe, -10.0);
    EXPECT_LE(ss.risk.sharpe, 10.0);

    // Print to verify it's well-formed
    std::ostringstream oss;
    analytics.print_summary(oss);
    EXPECT_TRUE(oss.str().size() > 50);
}
