#include <gtest/gtest.h>
#include "mm/risk.hpp"

using namespace mm;

// ── Default Limits ───────────────────────────────────────────────────────

TEST(RiskTest, DefaultLimits) {
    ASRiskManager rm;
    EXPECT_EQ(rm.limits().max_position, 10000);
    EXPECT_EQ(rm.limits().max_order_size, 5000);
    EXPECT_EQ(rm.limits().capital, 10000000);
}

// ── Check Quote: Basic Valid Quote ───────────────────────────────────────

TEST(RiskTest, CheckQuoteValid) {
    ASRiskManager rm;
    auto r = rm.check_quote(9950, 10050, 1000, 1000, 0, 10000);
    EXPECT_TRUE(r.approved);
    EXPECT_EQ(r.reason, nullptr);
}

// ── Check Quote: Bid >= Ask ──────────────────────────────────────────────

TEST(RiskTest, CheckQuoteBidGeAsk) {
    ASRiskManager rm;
    auto r = rm.check_quote(10050, 10050, 1000, 1000, 0, 10000);
    EXPECT_FALSE(r.approved);
}

TEST(RiskTest, CheckQuoteBidGtAsk) {
    ASRiskManager rm;
    auto r = rm.check_quote(10050, 9950, 1000, 1000, 0, 10000);
    EXPECT_FALSE(r.approved);
}

// ── Check Quote: Negative Price ──────────────────────────────────────────

TEST(RiskTest, CheckQuoteNegativeBid) {
    ASRiskManager rm;
    auto r = rm.check_quote(-1, 10050, 1000, 1000, 0, 10000);
    EXPECT_FALSE(r.approved);
}

TEST(RiskTest, CheckQuoteNegativeAsk) {
    ASRiskManager rm;
    auto r = rm.check_quote(9950, 0, 1000, 1000, 0, 10000);
    EXPECT_FALSE(r.approved);
}

// ── Check Quote: Max Position ────────────────────────────────────────────

TEST(RiskTest, CheckQuoteExceedsMaxPosition) {
    ASRiskLimits limits;
    limits.max_position = 1000;
    ASRiskManager rm(limits);

    // Current position 900, bid size 200 → new pos = 1100 > 1000
    auto r = rm.check_quote(9950, 10050, 200, 100, 900, 10000);
    EXPECT_FALSE(r.approved);
}

TEST(RiskTest, CheckQuoteAtMaxPosition) {
    ASRiskLimits limits;
    limits.max_position = 1000;
    limits.capital = 10000000;
    limits.max_exposure_pct = 1.0;
    ASRiskManager rm(limits);

    auto r = rm.check_quote(9950, 10050, 100, 100, 900, 10000);
    EXPECT_TRUE(r.approved);
}

// ── Check Quote: Order Size ──────────────────────────────────────────────

TEST(RiskTest, CheckQuoteOrderSizeExceeds) {
    ASRiskLimits limits;
    limits.max_order_size = 1000;
    ASRiskManager rm(limits);

    auto r = rm.check_quote(9950, 10050, 1001, 500, 0, 10000);
    EXPECT_FALSE(r.approved);

    r = rm.check_quote(9950, 10050, 500, 1001, 0, 10000);
    EXPECT_FALSE(r.approved);
}

// ── Check Quote: Exposure Limit ──────────────────────────────────────────

TEST(RiskTest, CheckQuoteExposureExceeds) {
    ASRiskLimits limits;
    limits.capital = 1000000;
    limits.max_exposure_pct = 0.5;  // max exposure = 500000
    ASRiskManager rm(limits);

    // position = 100, mid = 10000, exposure = 1,000,000 > 500,000
    auto r = rm.check_quote(9950, 10050, 100, 100, 100, 10000);
    EXPECT_FALSE(r.approved);
}

TEST(RiskTest, CheckQuoteExposureWithin) {
    ASRiskLimits limits;
    limits.capital = 10000000;
    limits.max_exposure_pct = 0.5;  // max exposure = 5,000,000
    ASRiskManager rm(limits);

    // position = 100, mid = 10000, exposure = 1,000,000 < 5,000,000
    auto r = rm.check_quote(9950, 10050, 100, 100, 100, 10000);
    EXPECT_TRUE(r.approved);
}

// ── Check Quote: Daily Loss Limit ────────────────────────────────────────

TEST(RiskTest, CheckQuoteDailyLossReached) {
    ASRiskLimits limits;
    limits.max_daily_loss = 1000;
    ASRiskManager rm(limits);

    rm.record_pnl(-1000, 0);
    auto r = rm.check_quote(9950, 10050, 100, 100, 0, 10000);
    EXPECT_FALSE(r.approved);
}

TEST(RiskTest, CheckQuoteDailyLossNotReached) {
    ASRiskLimits limits;
    limits.max_daily_loss = 1000;
    ASRiskManager rm(limits);

    rm.record_pnl(-999, 0);
    auto r = rm.check_quote(9950, 10050, 100, 100, 0, 10000);
    EXPECT_TRUE(r.approved);
}

// ── Check Quote: Drawdown Limit ──────────────────────────────────────────

TEST(RiskTest, CheckQuoteDrawdownReached) {
    ASRiskLimits limits;
    limits.max_drawdown = 0.1;
    ASRiskManager rm(limits);

    rm.record_pnl(1000, 0);   // peak = 1000
    rm.record_pnl(-100, 0);   // drawdown = (1000 - (-100)) / 1001 ≈ 1.099 > 0.1
    // Wait, drawdown = (peak - current) / peak, peak=1000, current=-100
    // drawdown = (1000 - (-100)) / 1000 = 1.1 > 0.1
    auto r = rm.check_quote(9950, 10050, 100, 100, 0, 10000);
    EXPECT_FALSE(r.approved);
}

// ── Check Order ──────────────────────────────────────────────────────────

TEST(RiskTest, CheckOrderValid) {
    ASRiskManager rm;
    auto r = rm.check_order(hft::Side::Buy, 10000, 1000, 0, 10000);
    EXPECT_TRUE(r.approved);
}

TEST(RiskTest, CheckOrderNegativePrice) {
    ASRiskManager rm;
    auto r = rm.check_order(hft::Side::Buy, -1, 1000, 0, 10000);
    EXPECT_FALSE(r.approved);
}

TEST(RiskTest, CheckOrderExceedsPosition) {
    ASRiskLimits limits;
    limits.max_position = 1000;
    ASRiskManager rm(limits);

    auto r = rm.check_order(hft::Side::Buy, 10000, 500, 800, 10000);
    EXPECT_FALSE(r.approved);
}

TEST(RiskTest, CheckOrderExceedsSize) {
    ASRiskLimits limits;
    limits.max_order_size = 1000;
    ASRiskManager rm(limits);

    auto r = rm.check_order(hft::Side::Buy, 10000, 1001, 0, 10000);
    EXPECT_FALSE(r.approved);
}

// ── Rate Limiting ────────────────────────────────────────────────────────

TEST(RiskTest, QuoteRateLimitRespected) {
    ASRiskLimits limits;
    limits.max_quotes_per_second = 5;
    ASRiskManager rm(limits);

    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(rm.check_quote_rate(1000000));
    }
    EXPECT_FALSE(rm.check_quote_rate(1000000));
}

TEST(RiskTest, QuoteRateLimitResetsAfterSecond) {
    ASRiskLimits limits;
    limits.max_quotes_per_second = 5;
    ASRiskManager rm(limits);

    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(rm.check_quote_rate(1000000));
    }
    EXPECT_FALSE(rm.check_quote_rate(1000000));

    // Next second should reset
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(rm.check_quote_rate(2000000));
    }
    EXPECT_FALSE(rm.check_quote_rate(2000000));
}

TEST(RiskTest, QuoteRateFirstCallAlwaysAllowed) {
    ASRiskManager rm;
    EXPECT_TRUE(rm.check_quote_rate(0));
    EXPECT_TRUE(rm.check_quote_rate(1000));
}

// ── PnL Recording ────────────────────────────────────────────────────────

TEST(RiskTest, RecordPnl) {
    ASRiskManager rm;
    rm.record_pnl(100, 50);
    EXPECT_EQ(rm.daily_pnl(), 150);
    EXPECT_EQ(rm.peak_pnl(), 150);
}

TEST(RiskTest, RecordPnlPeakTracking) {
    ASRiskManager rm;
    rm.record_pnl(100, 0);
    EXPECT_EQ(rm.peak_pnl(), 100);
    rm.record_pnl(-50, 0);
    EXPECT_EQ(rm.peak_pnl(), 100);
}

// ── Drawdown ─────────────────────────────────────────────────────────────

TEST(RiskTest, DrawdownZeroAtPeak) {
    ASRiskManager rm;
    rm.record_pnl(500, 0);
    EXPECT_DOUBLE_EQ(rm.drawdown(), 0.0);
}

TEST(RiskTest, DrawdownPositive) {
    ASRiskManager rm;
    rm.record_pnl(1000, 0);
    rm.record_pnl(800, 0);
    // drawdown = (1000 - 800) / 1000 = 0.2
    EXPECT_NEAR(rm.drawdown(), 0.2, 1e-10);
}

// ── Reset ──────────────────────────────────────────────────────────────────

TEST(RiskTest, Reset) {
    ASRiskManager rm;
    rm.record_pnl(500, 0);
    rm.check_quote_rate(1000);

    rm.reset();

    EXPECT_EQ(rm.daily_pnl(), 0);
    EXPECT_EQ(rm.peak_pnl(), 0);
}
