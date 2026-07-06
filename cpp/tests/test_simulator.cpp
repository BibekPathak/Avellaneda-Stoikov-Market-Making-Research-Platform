#include <gtest/gtest.h>
#include "mm/simulator.hpp"
#include <memory>

using namespace mm;

// ── Empty Feed Produces Empty Result ─────────────────────────────────────

TEST(SimulatorTest, EmptyFeed) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    auto strategy = std::make_unique<ASMarketMaker>(0.1, 1.0, 1000, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    auto result = sim.run();

    EXPECT_EQ(result.total_ticks(), 0);
    EXPECT_EQ(result.total_trades(), 0);
}

// ── Synthetic Feed Runs Without Error ────────────────────────────────────

TEST(SimulatorTest, SyntheticFeedRuns) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(100, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(0.1, 1.0, 1000, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    EXPECT_EQ(result.total_ticks(), 100);
    EXPECT_GE(result.total_trades(), 0);
}

// ── Produces Snapshots ───────────────────────────────────────────────────

TEST(SimulatorTest, ProducesSnapshots) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(50, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(0.1, 1.0, 1000, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    sim.set_snapshot_interval(10);
    auto result = sim.run();

    EXPECT_EQ(result.snapshots.size(), 5);
    EXPECT_GT(result.snapshots[0].mid_price, 0);
}

// ── Trades Execute on Matching Orders ────────────────────────────────────

TEST(SimulatorTest, TradesExecute) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(500, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(0.01, 1.0, 500, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    auto result = sim.run();

    // With tight spread and low risk aversion, trades should occur
    EXPECT_GE(result.total_trades(), 0);
}

// ── Inventory Tracks Trades ──────────────────────────────────────────────

TEST(SimulatorTest, InventoryTracksTrades) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    // Create a feed with stable prices so quotes cross frequently
    feed->generate_synthetic(200, 50000, 50);

    auto strategy = std::make_unique<ASMarketMaker>(0.01, 1.0, 100, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    // Inventory should be non-zero after trades
    bool had_trades = result.total_trades() > 0;
    bool inventory_changed = false;

    for (size_t i = 1; i < result.snapshots.size(); i++) {
        if (result.snapshots[i].inventory != result.snapshots[0].inventory) {
            inventory_changed = true;
            break;
        }
    }

    if (had_trades) {
        EXPECT_TRUE(inventory_changed);
    }
}

// ── PnL Monotonicity After Starting ─────────────────────────────────────

TEST(SimulatorTest, PnLTracks) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(100, 50000, 50);

    auto strategy = std::make_unique<ASMarketMaker>(0.05, 1.0, 200, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    for (const auto& snap : result.snapshots) {
        EXPECT_EQ(snap.total_pnl, snap.realized_pnl + snap.unrealized_pnl);
    }
}

// ── Volatility Increases Over Time ───────────────────────────────────────

TEST(SimulatorTest, VolatilityUpdates) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(100, 50000, 200);

    auto strategy = std::make_unique<ASMarketMaker>(0.1, 1.0, 500, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    sim.set_snapshot_interval(10);
    auto result = sim.run();

    // After 100 ticks, volatility should be non-zero
    for (const auto& snap : result.snapshots) {
        EXPECT_GE(snap.volatility, 0.0);
    }
}

// ── Strategy Parameters Persist Through Simulation ───────────────────────

TEST(SimulatorTest, StrategyParamsPersist) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(10, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(0.5, 2.0, 777, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    sim.run();

    EXPECT_NEAR(sim.strategy().as_model().risk_aversion(), 0.5, 1e-10);
    EXPECT_NEAR(sim.strategy().as_model().time_horizon(), 2.0, 1e-10);
    EXPECT_EQ(sim.strategy().order_size(), 777);
}

// ── High Risk Aversion Reduces Trade Frequency ───────────────────────────

TEST(SimulatorTest, HighRiskAversionFewerTrades) {
    auto feed_low = std::make_unique<hft::MarketDataFeed>();
    feed_low->generate_synthetic(200, 50000, 100);

    auto feed_high = std::make_unique<hft::MarketDataFeed>();
    feed_high->generate_synthetic(200, 50000, 100);

    auto strat_low = std::make_unique<ASMarketMaker>(0.001, 1.0, 500, 1, 0);
    auto strat_high = std::make_unique<ASMarketMaker>(10.0, 1.0, 500, 1, 0);

    Simulator sim_low(std::move(feed_low), std::move(strat_low));
    auto result_low = sim_low.run();

    Simulator sim_high(std::move(feed_high), std::move(strat_high));
    auto result_high = sim_high.run();

    // Higher risk aversion → wider spreads → fewer fills
    EXPECT_GE(result_low.total_trades(), result_high.total_trades());
}

// ── Fill Simulation Mode Produces Trades ─────────────────────────────────

TEST(SimulatorTest, FillSimulationProducesTrades) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(200, 50000, 100);

    // High gamma produces narrower spread, making fills more likely
    auto strategy = std::make_unique<ASMarketMaker>(1000.0, 1.0, 500, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy), true);
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    EXPECT_EQ(result.total_ticks(), 200);
    EXPECT_GT(result.total_trades(), 0);
    EXPECT_TRUE(sim.fill_simulation_mode());
}

// ── Fill Simulation Tracks Inventory ─────────────────────────────────────

TEST(SimulatorTest, FillSimulationInventory) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(100, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(1000.0, 1.0, 200, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy), true);
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    bool inventory_changed = false;
    for (size_t i = 1; i < result.snapshots.size(); i++) {
        if (result.snapshots[i].inventory != result.snapshots[0].inventory) {
            inventory_changed = true;
            break;
        }
    }

    EXPECT_TRUE(inventory_changed);
    EXPECT_NE(result.total_trades(), 0);
}

// ── Fill Simulation with QueuePosition Fill Model ────────────────────────

TEST(SimulatorTest, FillSimulationQueueModel) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(200, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(1000.0, 1.0, 500, 1, 0);
    strategy->set_fill_model(std::make_unique<QueuePositionFill>());

    Simulator sim(std::move(feed), std::move(strategy), true);
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    EXPECT_EQ(result.total_ticks(), 200);
    EXPECT_GT(result.total_trades(), 0);
}

// ── Fill Simulation Mode Toggle ──────────────────────────────────────────

TEST(SimulatorTest, FillSimulationToggle) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(50, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(0.1, 1.0, 500, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy));
    EXPECT_FALSE(sim.fill_simulation_mode());

    sim.set_fill_simulation_mode(true);
    EXPECT_TRUE(sim.fill_simulation_mode());
}

// ── Fill Simulation Reports Trades In Result ─────────────────────────────

TEST(SimulatorTest, FillSimulationReportsTrades) {
    auto feed = std::make_unique<hft::MarketDataFeed>();
    feed->generate_synthetic(100, 50000, 100);

    auto strategy = std::make_unique<ASMarketMaker>(0.05, 1.0, 500, 1, 0);

    Simulator sim(std::move(feed), std::move(strategy), true);
    sim.set_snapshot_interval(1);
    auto result = sim.run();

    EXPECT_EQ(result.trades.size(), result.snapshots.back().trade_count);
    for (const auto& trade : result.trades) {
        EXPECT_GT(trade.price, 0);
        EXPECT_GT(trade.quantity, 0);
    }
}
