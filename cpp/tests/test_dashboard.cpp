#include <gtest/gtest.h>
#include "mm/dashboard.hpp"
#include "mm/simulator.hpp"
#include <memory>

using namespace mm;

// ── Dashboard from empty result ──────────────────────────────────────────

TEST(DashboardTest, EmptyResult) {
    SimResult result;
    Analytics analytics(result);
    Dashboard db(analytics);

    std::string text = db.text();
    EXPECT_TRUE(text.size() > 0);
    EXPECT_NE(text.find("PnL"), std::string::npos);
    EXPECT_NE(text.find("Sharpe"), std::string::npos);

    std::string json = db.json();
    EXPECT_TRUE(json.size() > 0);
    EXPECT_NE(json.find("\"sharpe\""), std::string::npos);
}

// ── Dashboard from populated result ──────────────────────────────────────

TEST(DashboardTest, FromSimulator) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(100, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(0.1, 1.0, 500, 1, 0);
    Simulator sim(std::move(feed), std::move(strategy));
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    Analytics analytics(result);
    Dashboard db(analytics);

    std::string text = db.text();
    EXPECT_NE(text.find("Total"), std::string::npos);
    EXPECT_NE(text.find("Position"), std::string::npos);

    std::string json = db.json();
    EXPECT_NE(json.find("\"total\""), std::string::npos);
}

// ── JSON is valid format ─────────────────────────────────────────────────

TEST(DashboardTest, JsonFormat) {
    SimResult result;

    SimSnapshot snap;
    snap.tick = 1; snap.timestamp = 1000; snap.mid_price = 10000;
    snap.total_pnl = 5000; snap.realized_pnl = 3000; snap.unrealized_pnl = 2000;
    result.snapshots.push_back(snap);

    Analytics analytics(result);
    Dashboard db(analytics);

    std::string json = db.json();
    EXPECT_EQ(json.front(), '{');
    EXPECT_NE(json.find("pnl"), std::string::npos);
    EXPECT_NE(json.find("inventory"), std::string::npos);
    EXPECT_NE(json.find("trades"), std::string::npos);
    EXPECT_NE(json.find("risk"), std::string::npos);
    EXPECT_NE(json.find("fills"), std::string::npos);
}

// ── Text contains all expected sections ──────────────────────────────────

TEST(DashboardTest, TextSections) {
    SimResult result;

    for (int i = 0; i < 3; i++) {
        SimSnapshot snap;
        snap.tick = i + 1; snap.timestamp = (i + 1) * 1000;
        snap.mid_price = 10000; snap.total_pnl = i * 100;
        result.snapshots.push_back(snap);
    }

    Analytics analytics(result);
    Dashboard db(analytics);
    std::string text = db.text();

    EXPECT_NE(text.find("PnL"), std::string::npos);
    EXPECT_NE(text.find("Inventory"), std::string::npos);
    EXPECT_NE(text.find("Trades"), std::string::npos);
    EXPECT_NE(text.find("Risk"), std::string::npos);
    EXPECT_NE(text.find("Fills"), std::string::npos);
    EXPECT_NE(text.find("DASHBOARD"), std::string::npos);
}
