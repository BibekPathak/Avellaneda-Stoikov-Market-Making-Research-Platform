#include <gtest/gtest.h>
#include "mm/inventory.hpp"

using namespace mm;

// ── Initial State ─────────────────────────────────────────────────────────

TEST(InventoryTest, InitialState) {
    Inventory inv;
    EXPECT_EQ(inv.cash, 0);
    EXPECT_EQ(inv.position, 0);
    EXPECT_EQ(inv.realized_pnl, 0);
    EXPECT_EQ(inv.unrealized_pnl, 0);
    EXPECT_EQ(inv.avg_entry_price, 0);
    EXPECT_EQ(inv.trade_count, 0);
    EXPECT_EQ(inv.total_pnl(), 0);
    EXPECT_EQ(inv.exposure(), 0);
}

// ── Single Buy ────────────────────────────────────────────────────────────

TEST(InventoryTest, SingleBuy) {
    Inventory inv;
    inv.fill(hft::Side::Buy, 10000, 100);

    EXPECT_EQ(inv.cash, -1000000);
    EXPECT_EQ(inv.position, 100);
    EXPECT_EQ(inv.avg_entry_price, 10000);
    EXPECT_EQ(inv.trade_count, 1);
    EXPECT_EQ(inv.buy_volume, 100);
    EXPECT_EQ(inv.sell_volume, 0);
    EXPECT_EQ(inv.realized_pnl, 0);
    EXPECT_EQ(inv.total_pnl(), 0);
}

// ── Single Sell ───────────────────────────────────────────────────────────

TEST(InventoryTest, SingleSell) {
    Inventory inv;
    inv.fill(hft::Side::Sell, 10100, 50);

    EXPECT_EQ(inv.cash, 505000);
    EXPECT_EQ(inv.position, -50);
    EXPECT_EQ(inv.avg_entry_price, 10100);
    EXPECT_EQ(inv.trade_count, 1);
    EXPECT_EQ(inv.sell_volume, 50);
}

// ── Buy then Sell at Profit ───────────────────────────────────────────────

TEST(InventoryTest, BuyThenSellProfit) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 100);
    EXPECT_EQ(inv.position, 100);
    EXPECT_EQ(inv.realized_pnl, 0);

    inv.fill(hft::Side::Sell, 10100, 100);

    EXPECT_EQ(inv.position, 0);
    EXPECT_EQ(inv.cash, 10000);  // -1,000,000 + 1,010,000 = 10,000
    EXPECT_EQ(inv.realized_pnl, 10000);
    EXPECT_EQ(inv.total_pnl(), 10000);
    EXPECT_EQ(inv.avg_entry_price, 0);
}

// ── Buy then Sell Partial (Partial Close) ─────────────────────────────────

TEST(InventoryTest, BuyThenSellPartial) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 200);
    inv.fill(hft::Side::Sell, 10200, 50);

    EXPECT_EQ(inv.position, 150);
    EXPECT_EQ(inv.cash, -2000000 + 510000);
    EXPECT_EQ(inv.realized_pnl, (10200 - 10000) * 50);
    EXPECT_EQ(inv.avg_entry_price, 10000);
}

// ── Multiple Buys (Averaging In) ──────────────────────────────────────────

TEST(InventoryTest, MultipleBuysAveraging) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 100);
    inv.fill(hft::Side::Buy, 10200, 100);

    EXPECT_EQ(inv.position, 200);
    int64_t expected_avg = (10000 * 100 + 10200 * 100) / 200;
    EXPECT_EQ(inv.avg_entry_price, expected_avg);
    EXPECT_EQ(inv.cash, -(10000 * 100 + 10200 * 100));
}

// ── Multiple Sells (Averaging In Short) ───────────────────────────────────

TEST(InventoryTest, MultipleSellsAveraging) {
    Inventory inv;

    inv.fill(hft::Side::Sell, 10000, 50);
    inv.fill(hft::Side::Sell, 9800, 50);

    EXPECT_EQ(inv.position, -100);
    int64_t expected_avg = (10000 * 50 + 9800 * 50) / 100;
    EXPECT_EQ(inv.avg_entry_price, expected_avg);
}

// ── Sell then Buy (Cover Short at Profit) ─────────────────────────────────

TEST(InventoryTest, SellThenBuyCover) {
    Inventory inv;

    inv.fill(hft::Side::Sell, 10000, 100);
    EXPECT_EQ(inv.position, -100);
    EXPECT_EQ(inv.realized_pnl, 0);

    inv.fill(hft::Side::Buy, 9900, 100);

    EXPECT_EQ(inv.position, 0);
    EXPECT_EQ(inv.realized_pnl, (10000 - 9900) * 100);
    EXPECT_EQ(inv.total_pnl(), 10000);
}

// ── Flip from Long to Short ───────────────────────────────────────────────

TEST(InventoryTest, FlipLongToShort) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 100);
    EXPECT_EQ(inv.position, 100);

    inv.fill(hft::Side::Sell, 10100, 150);

    EXPECT_EQ(inv.position, -50);
    int64_t realized = (10100 - 10000) * 100;
    EXPECT_EQ(inv.realized_pnl, realized);
    EXPECT_EQ(inv.avg_entry_price, 10100);
}

// ── Flip from Short to Long ───────────────────────────────────────────────

TEST(InventoryTest, FlipShortToLong) {
    Inventory inv;

    inv.fill(hft::Side::Sell, 10000, 100);
    EXPECT_EQ(inv.position, -100);

    inv.fill(hft::Side::Buy, 9900, 150);

    EXPECT_EQ(inv.position, 50);
    int64_t realized = (10000 - 9900) * 100;
    EXPECT_EQ(inv.realized_pnl, realized);
    EXPECT_EQ(inv.avg_entry_price, 9900);
}

// ── Mark to Market (Long) ─────────────────────────────────────────────────

TEST(InventoryTest, MarkToMarketLong) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 100);

    inv.mark_to_market(10200);
    EXPECT_EQ(inv.unrealized_pnl, 100 * (10200 - 10000));

    inv.mark_to_market(9800);
    EXPECT_EQ(inv.unrealized_pnl, 100 * (9800 - 10000));
}

// ── Mark to Market (Short) ────────────────────────────────────────────────

TEST(InventoryTest, MarkToMarketShort) {
    Inventory inv;

    inv.fill(hft::Side::Sell, 10000, 100);

    inv.mark_to_market(9800);
    EXPECT_EQ(inv.unrealized_pnl, -100 * (9800 - 10000));

    inv.mark_to_market(10200);
    EXPECT_EQ(inv.unrealized_pnl, -100 * (10200 - 10000));
}

// ── Mark to Market (Flat) ─────────────────────────────────────────────────

TEST(InventoryTest, MarkToMarketFlat) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 100);
    inv.fill(hft::Side::Sell, 10100, 100);

    inv.mark_to_market(99999);
    EXPECT_EQ(inv.unrealized_pnl, 0);
}

// ── Total PnL (Realized + Unrealized) ─────────────────────────────────────

TEST(InventoryTest, TotalPnL) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 100);
    inv.fill(hft::Side::Sell, 10100, 50);
    inv.mark_to_market(10200);

    int64_t expected_realized = (10100 - 10000) * 50;
    int64_t expected_unrealized = 50 * (10200 - 10000);

    EXPECT_EQ(inv.realized_pnl, expected_realized);
    EXPECT_EQ(inv.unrealized_pnl, expected_unrealized);
    EXPECT_EQ(inv.total_pnl(), expected_realized + expected_unrealized);
}

// ── Exposure ──────────────────────────────────────────────────────────────

TEST(InventoryTest, Exposure) {
    Inventory inv;

    EXPECT_EQ(inv.exposure(), 0);

    inv.fill(hft::Side::Buy, 10000, 100);
    EXPECT_EQ(inv.exposure(), 100);

    inv.fill(hft::Side::Sell, 10100, 200);
    EXPECT_EQ(inv.exposure(), 100);

    inv.fill(hft::Side::Buy, 9900, 100);
    EXPECT_EQ(inv.exposure(), 0);
}

// ── Reset ──────────────────────────────────────────────────────────────────

TEST(InventoryTest, Reset) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 100);
    inv.fill(hft::Side::Sell, 10100, 100);
    inv.mark_to_market(10050);

    EXPECT_NE(inv.cash, 0);
    EXPECT_NE(inv.total_pnl(), 0);

    inv.reset();

    EXPECT_EQ(inv.cash, 0);
    EXPECT_EQ(inv.position, 0);
    EXPECT_EQ(inv.realized_pnl, 0);
    EXPECT_EQ(inv.unrealized_pnl, 0);
    EXPECT_EQ(inv.avg_entry_price, 0);
    EXPECT_EQ(inv.trade_count, 0);
    EXPECT_EQ(inv.buy_volume, 0);
    EXPECT_EQ(inv.sell_volume, 0);
}

// ── Round Trip (Buy then Sell back to flat) ───────────────────────────────

TEST(InventoryTest, RoundTripFlat) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 50);
    inv.fill(hft::Side::Buy, 10020, 50);
    inv.fill(hft::Side::Sell, 10050, 50);
    inv.fill(hft::Side::Sell, 10040, 50);

    EXPECT_EQ(inv.position, 0);
    EXPECT_EQ(inv.avg_entry_price, 0);

    int64_t expected_pnl = (10050 - 10000) * 50 + (10040 - 10020) * 50;
    EXPECT_EQ(inv.realized_pnl, expected_pnl);
}

// ── Sequence of Trades with Interleaved Mark-to-Market ────────────────────

TEST(InventoryTest, SequenceWithMarkToMarket) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 100);
    inv.mark_to_market(10100);
    EXPECT_EQ(inv.total_pnl(), 100 * 100);

    inv.fill(hft::Side::Sell, 10100, 50);
    EXPECT_EQ(inv.realized_pnl, (10100 - 10000) * 50);

    inv.mark_to_market(10100);
    EXPECT_EQ(inv.unrealized_pnl, 50 * (10100 - 10000));
    EXPECT_EQ(inv.total_pnl(), 100 * 100);

    inv.mark_to_market(10050);
    EXPECT_EQ(inv.unrealized_pnl, 50 * (10050 - 10000));
    EXPECT_EQ(inv.total_pnl(), (10100 - 10000) * 50 + 50 * (10050 - 10000));
}

// ── Zero Quantity Fill (No-Op) ────────────────────────────────────────────

TEST(InventoryTest, ZeroQuantityFill) {
    Inventory inv;

    inv.fill(hft::Side::Buy, 10000, 0);
    EXPECT_EQ(inv.cash, 0);
    EXPECT_EQ(inv.position, 0);
    EXPECT_EQ(inv.trade_count, 1);
}
