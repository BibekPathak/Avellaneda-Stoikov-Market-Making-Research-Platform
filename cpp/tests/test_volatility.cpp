#include <gtest/gtest.h>
#include "mm/volatility.hpp"
#include <cmath>

using namespace mm;

// ── Initial State ─────────────────────────────────────────────────────────

TEST(VolatilityTest, InitialState) {
    VolatilityEstimator est;
    EXPECT_EQ(est.volatility(), 0.0);
    EXPECT_EQ(est.samples(), 0);
    EXPECT_EQ(est.window(), 20);
    EXPECT_EQ(est.method(), VolatilityMethod::RollingStd);
}

// ── Not Enough Data ───────────────────────────────────────────────────────

TEST(VolatilityTest, NotEnoughData) {
    VolatilityEstimator est;
    est.add_price(10000);
    EXPECT_EQ(est.volatility(), 0.0);
    EXPECT_EQ(est.samples(), 1);
}

TEST(VolatilityTest, OneReturnNotEnoughForStd) {
    VolatilityEstimator est;
    est.add_price(10000);
    est.add_price(10100);
    EXPECT_EQ(est.volatility(), 0.0);
}

// ── RollingStd — Constant Returns ─────────────────────────────────────────

TEST(VolatilityTest, RollingStdConstantReturns) {
    VolatilityEstimator est(VolatilityMethod::RollingStd, 5);

    // Use exact powers of 1.01 for perfectly constant log returns
    est.add_price(10000);
    est.add_price(10100);
    est.add_price(10201);
    est.add_price(10303);
    est.add_price(10406);

    double vol = est.volatility();
    // Integer rounding gives tiny variation (~1e-6)
    EXPECT_LT(vol, 1e-4);

    // Add a different return
    est.add_price(11000);
    vol = est.volatility();
    EXPECT_GT(vol, 0.005);
}

// ── RollingStd — Known Values ─────────────────────────────────────────────

TEST(VolatilityTest, RollingStdKnownValues) {
    VolatilityEstimator est(VolatilityMethod::RollingStd, 10);

    int64_t price = 10000;
    for (int i = 0; i < 11; i++) {
        est.add_price(price);
        price += 100; // constant 1% additions
    }

    // Returns: ln(10100/10000), ln(10200/10100), etc.
    // Each return should be roughly ln(1.01) ≈ 0.00995
    // With zero variance and >2 returns, we need at least some variation
    // Actually the returns are not perfectly constant because
    // 100/10000 = 1% but 100/10100 ≈ 0.99% in log terms
    // So there will be small variance
    double vol = est.volatility();
    EXPECT_GT(vol, 0.0);
    EXPECT_LT(vol, 0.001); // small variation in returns
}

// ── RollingStd — Bounded Window ───────────────────────────────────────────

TEST(VolatilityTest, RollingStdBoundedWindow) {
    VolatilityEstimator est(VolatilityMethod::RollingStd, 3);

    // Add 100 constant prices → returns = 0, vol = 0
    for (int i = 0; i < 10; i++) {
        est.add_price(10000);
    }

    // After the first price, all subsequent identical prices give 0 return
    // With 3 return window, we need at least 2 returns for std
    // All returns are 0, so vol = 0
    EXPECT_NEAR(est.volatility(), 0.0, 1e-10);

    // Now add a jump
    est.add_price(11000); // ~10% jump
    // Window of 3 returns: the last 3 returns include at least one 0 and one ~0.095
    double vol = est.volatility();
    EXPECT_GT(vol, 0.01);
}

// ── EWMA — Lambda = 0 (Only Latest Return Matters) ────────────────────────

TEST(VolatilityTest, EwmaLambdaZero) {
    VolatilityEstimator est(VolatilityMethod::EWMA, 20, 0.0);

    est.add_price(10000);
    est.add_price(11000);
    // First return ~0.0953, variance initialized to 0.0953² = 0.00909
    double vol = est.volatility();
    EXPECT_NEAR(vol, std::abs(std::log(11000.0 / 10000.0)), 1e-8);

    // Add another price with a different return
    est.add_price(10000);
    // With λ=0, variance = 0 * prev + 1.0 * r² = r²
    double r2 = std::log(10000.0 / 11000.0);
    EXPECT_NEAR(est.volatility(), std::abs(r2), 1e-8);
}

// ── EWMA — Lambda = 1 (Never Updates After Init) ─────────────────────────

TEST(VolatilityTest, EwmaLambdaOne) {
    VolatilityEstimator est(VolatilityMethod::EWMA, 20, 1.0);

    est.add_price(10000);
    est.add_price(11000);
    double vol_first = est.volatility();

    // With λ=1, variance stays at initial value regardless of new returns
    est.add_price(1); // extreme return
    EXPECT_NEAR(est.volatility(), vol_first, 1e-8);
}

// ── EWMA — Typical Lambda = 0.94 ──────────────────────────────────────────

TEST(VolatilityTest, EwmaTypicalLambda) {
    VolatilityEstimator est(VolatilityMethod::EWMA, 20, 0.94);

    // Add a series of prices with known returns
    est.add_price(10000);
    double r1 = std::log(10500.0 / 10000.0);
    est.add_price(10500); // return ≈ 0.0488
    // Initialized: σ² = r1²

    double r2 = std::log(10200.0 / 10500.0);
    est.add_price(10200); // return ≈ -0.0290
    // σ² = 0.94 * r1² + 0.06 * r2²

    double expected_var = 0.94 * r1 * r1 + 0.06 * r2 * r2;
    EXPECT_NEAR(est.volatility(), std::sqrt(expected_var), 1e-10);
}

// ── Parkinson — Single Period ─────────────────────────────────────────────

TEST(VolatilityTest, ParkinsonSinglePeriod) {
    VolatilityEstimator est(VolatilityMethod::Parkinson, 10);

    est.add_ohlc(10500, 9500);
    // σ = sqrt(ln(10500/9500)² / (4*ln(2)))
    double expected = std::sqrt(
        std::pow(std::log(10500.0 / 9500.0), 2) / (4.0 * std::log(2.0))
    );
    EXPECT_NEAR(est.volatility(), expected, 1e-10);
}

// ── Parkinson — Multiple Periods (Averaging) ──────────────────────────────

TEST(VolatilityTest, ParkinsonMultiplePeriods) {
    VolatilityEstimator est(VolatilityMethod::Parkinson, 5);

    est.add_ohlc(10200, 9800);
    est.add_ohlc(10300, 9700);
    est.add_ohlc(10100, 9900);

    double v1 = std::sqrt(std::pow(std::log(10200.0 / 9800.0), 2) / (4.0 * std::log(2.0)));
    double v2 = std::sqrt(std::pow(std::log(10300.0 / 9700.0), 2) / (4.0 * std::log(2.0)));
    double v3 = std::sqrt(std::pow(std::log(10100.0 / 9900.0), 2) / (4.0 * std::log(2.0)));

    EXPECT_NEAR(est.volatility(), (v1 + v2 + v3) / 3.0, 1e-10);
}

// ── Parkinson — Bounded Window ────────────────────────────────────────────

TEST(VolatilityTest, ParkinsonBoundedWindow) {
    VolatilityEstimator est(VolatilityMethod::Parkinson, 2);

    for (int i = 0; i < 10; i++) {
        est.add_ohlc(10100, 9900);
    }
    // Window size 2, so only last 2 estimates are kept
    EXPECT_EQ(est.samples(), 10);
    EXPECT_NEAR(est.volatility(), std::sqrt(std::pow(std::log(10100.0 / 9900.0), 2) / (4.0 * std::log(2.0))), 1e-10);
}

// ── Realized — Single Return ──────────────────────────────────────────────

TEST(VolatilityTest, RealizedSingleReturn) {
    VolatilityEstimator est(VolatilityMethod::Realized);

    est.add_price(10000);
    est.add_price(10500);
    double r = std::log(10500.0 / 10000.0);

    EXPECT_NEAR(est.volatility(), std::sqrt(r * r), 1e-10);
}

// ── Realized — Multiple Returns ───────────────────────────────────────────

TEST(VolatilityTest, RealizedMultipleReturns) {
    VolatilityEstimator est(VolatilityMethod::Realized);

    est.add_price(10000);
    est.add_price(10500);
    est.add_price(10200);

    double r1 = std::log(10500.0 / 10000.0);
    double r2 = std::log(10200.0 / 10500.0);

    EXPECT_NEAR(est.volatility(), std::sqrt(r1 * r1 + r2 * r2), 1e-10);
}

// ── Realized — Bounded Window ─────────────────────────────────────────────

TEST(VolatilityTest, RealizedBoundedWindow) {
    VolatilityEstimator est(VolatilityMethod::Realized, 3);

    // Add many prices to fill and overflow the window
    for (int i = 0; i < 10; i++) {
        est.add_price(10000);
    }
    // All returns are 0 (flat prices), plus the first add_price doesn't produce a return
    // The first return is between price 0 and price 1 both at 10000 → 0
    // After many identical prices, all returns are 0
    EXPECT_NEAR(est.volatility(), 0.0, 1e-10);

    // Now add a jump
    est.add_price(11000);
    // Window of 3 returns: last 3 returns include one non-zero (~0.0953)
    double vol = est.volatility();
    EXPECT_GT(vol, 0.0);
    EXPECT_LT(vol, 0.1);
}

// ── Price Volatility ──────────────────────────────────────────────────────

TEST(VolatilityTest, PriceVolatility) {
    VolatilityEstimator est(VolatilityMethod::RollingStd, 5);

    est.add_price(10000);
    est.add_price(11000);
    est.add_price(12000);
    est.add_price(13000);
    est.add_price(14000);
    est.add_price(15000);

    double vol = est.volatility();
    double price_vol = est.price_volatility(10000);

    EXPECT_NEAR(price_vol, vol * 10000.0, 1e-6);
}

// ── Reset ──────────────────────────────────────────────────────────────────

TEST(VolatilityTest, Reset) {
    VolatilityEstimator est;

    est.add_price(10000);
    est.add_price(11000);
    est.add_price(12000);
    EXPECT_GT(est.samples(), 0);

    est.reset();

    EXPECT_EQ(est.samples(), 0);
    EXPECT_EQ(est.volatility(), 0.0);
}

// ── Method Switching ──────────────────────────────────────────────────────

TEST(VolatilityTest, MethodSwitching) {
    VolatilityEstimator est(VolatilityMethod::RollingStd);

    est.add_price(10000);
    est.add_price(11000);
    est.add_price(12000);
    double rolling_vol = est.volatility();

    est.set_method(VolatilityMethod::EWMA);
    double ewma_vol = est.volatility();

    // Different methods should give different results with same data
    EXPECT_NE(rolling_vol, ewma_vol);
}

// ── Window Size Change ────────────────────────────────────────────────────

TEST(VolatilityTest, WindowChange) {
    VolatilityEstimator est(VolatilityMethod::RollingStd, 10);

    est.add_price(10000);
    for (int i = 0; i < 10; i++) {
        est.add_price(10000 + (i % 2 == 0 ? 500 : -500));
    }

    double vol_wide = est.volatility();

    est.set_window(3);  // Changes window but doesn't affect existing data
    // Note: setting window doesn't retroactively change stored data
    // The stored returns buffer might exceed new window until next add_price
    double vol_narrow = est.volatility();

    // Should be different
    EXPECT_NE(vol_wide, vol_narrow);
}

// ── Zero Volatility (Flat Prices) ─────────────────────────────────────────

TEST(VolatilityTest, ZeroVolatilityFlatPrices) {
    VolatilityEstimator est(VolatilityMethod::RollingStd);

    for (int i = 0; i < 100; i++) {
        est.add_price(10000);
    }
    EXPECT_NEAR(est.volatility(), 0.0, 1e-10);
}

// ── Negative Prices (Edge Case) ──────────────────────────────────────────

TEST(VolatilityTest, NegativePriceProducesNoReturn) {
    VolatilityEstimator est;

    est.add_price(-10000);
    est.add_price(10000);
    // Log of negative / positive would be -inf or nan
    EXPECT_EQ(est.volatility(), 0.0);
}

// ── Zero Price (Edge Case) ────────────────────────────────────────────────

TEST(VolatilityTest, ZeroPriceProducesNoReturn) {
    VolatilityEstimator est;

    est.add_price(0);
    est.add_price(10000);
    // Log of 10000/0 = inf
    EXPECT_EQ(est.volatility(), 0.0);
}

// ── Parkinson with Invalid OHLC ───────────────────────────────────────────

TEST(VolatilityTest, ParkinsonInvalidOhlc) {
    VolatilityEstimator est(VolatilityMethod::Parkinson);

    est.add_ohlc(0, 10000);  // low > high should be rejected
    EXPECT_EQ(est.volatility(), 0.0);

    est.add_ohlc(-100, 10000);  // negative high
    EXPECT_EQ(est.volatility(), 0.0);
}
