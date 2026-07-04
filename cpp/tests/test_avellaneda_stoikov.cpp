#include <gtest/gtest.h>
#include "mm/avellaneda_stoikov.hpp"
#include <cmath>

using namespace mm;

// ── Reservation Price: Zero Inventory = Mid ───────────────────────────────

TEST(ASTest, ReservationPriceZeroInventory) {
    AvellanedaStoikov as(0.1, 1.0);
    double r = as.reservation_price(10000, 0.0, 0.2, 1.0);
    EXPECT_NEAR(r, 10000.0, 1e-6);
}

// ── Reservation Price: Long Inventory Shifts Down ─────────────────────────

TEST(ASTest, ReservationPriceLongInventory) {
    AvellanedaStoikov as(0.1, 1.0);
    double r = as.reservation_price(10000, 0.5, 0.2, 1.0);
    EXPECT_NEAR(r, 9980.0, 1e-6);
}

// ── Reservation Price: Short Inventory Shifts Up ──────────────────────────

TEST(ASTest, ReservationPriceShortInventory) {
    AvellanedaStoikov as(0.1, 1.0);
    double r = as.reservation_price(10000, -0.5, 0.2, 1.0);
    EXPECT_NEAR(r, 10020.0, 1e-6);
}

// ── Reservation Price: Higher Volatility = Larger Shift ───────────────────

TEST(ASTest, ReservationPriceHigherVolatility) {
    AvellanedaStoikov as(0.1, 1.0);
    double r_low = as.reservation_price(10000, 0.5, 0.1, 1.0);
    double r_high = as.reservation_price(10000, 0.5, 0.5, 1.0);
    EXPECT_LT(r_high, r_low);
}

// ── Reservation Price: Higher Risk Aversion = Larger Shift ────────────────

TEST(ASTest, ReservationPriceHigherRiskAversion) {
    AvellanedaStoikov as_low(0.05, 1.0);
    AvellanedaStoikov as_high(0.5, 1.0);
    double r_low = as_low.reservation_price(10000, 0.5, 0.2, 1.0);
    double r_high = as_high.reservation_price(10000, 0.5, 0.2, 1.0);
    EXPECT_LT(r_high, r_low);
}

// ── Reservation Price: Zero Remaining Time = No Shift ─────────────────────

TEST(ASTest, ReservationPriceZeroTime) {
    AvellanedaStoikov as(0.1, 1.0);
    double r = as.reservation_price(10000, 0.5, 0.2, 0.0);
    EXPECT_NEAR(r, 10000.0, 1e-6);
}

// ── Reservation Price: Negative Time Clamped to Zero ──────────────────────

TEST(ASTest, ReservationPriceNegativeTime) {
    AvellanedaStoikov as(0.1, 1.0);
    double r = as.reservation_price(10000, 0.5, 0.2, -1.0);
    EXPECT_NEAR(r, 10000.0, 1e-6);
}

// ── Reservation Price: Zero Volatility = No Shift ─────────────────────────

TEST(ASTest, ReservationPriceZeroVolatility) {
    AvellanedaStoikov as(0.1, 1.0);
    double r = as.reservation_price(10000, 0.5, 0.0, 1.0);
    EXPECT_NEAR(r, 10000.0, 1e-6);
}

// ── Reservation Price: Zero Risk Aversion = No Shift ──────────────────────

TEST(ASTest, ReservationPriceZeroRiskAversion) {
    AvellanedaStoikov as(0.0, 1.0);
    double r = as.reservation_price(10000, 0.5, 0.2, 1.0);
    EXPECT_NEAR(r, 10000.0, 1e-6);
}

// ── Reservation Price: Max Inventory = Large Shift ────────────────────────

TEST(ASTest, ReservationPriceMaxInventory) {
    AvellanedaStoikov as(0.1, 1.0);
    double r = as.reservation_price(10000, 1.0, 0.2, 1.0);
    EXPECT_NEAR(r, 9960.0, 1e-6);
}

// ── Spread Fraction: Higher Volatility = Wider ────────────────────────────

TEST(ASTest, SpreadFracHigherVolatility) {
    AvellanedaStoikov as(0.1, 1.0);
    double s_low = as.optimal_spread_fraction(0.1, 10.0, 1.0);
    double s_high = as.optimal_spread_fraction(0.5, 10.0, 1.0);
    EXPECT_GT(s_high, s_low);
}

// ── Spread Fraction: Higher Fill Intensity = Narrower ─────────────────────

TEST(ASTest, SpreadFracHigherFillIntensity) {
    AvellanedaStoikov as(0.1, 1.0);
    double s_low = as.optimal_spread_fraction(0.2, 5.0, 1.0);
    double s_high = as.optimal_spread_fraction(0.2, 50.0, 1.0);
    EXPECT_LT(s_high, s_low);
}

// ── Spread Fraction: Zero Time = Only Competition Term ────────────────────

TEST(ASTest, SpreadFracZeroTime) {
    AvellanedaStoikov as(0.1, 1.0);
    double s = as.optimal_spread_fraction(0.2, 10.0, 0.0);
    double expected = (2.0 / 0.1) * std::log(1.0 + 0.1 / 10.0);
    EXPECT_NEAR(s, expected, 1e-10);
}

// ── Spread Fraction: Zero Gamma = Zero Spread ─────────────────────────────

TEST(ASTest, SpreadFracZeroGamma) {
    AvellanedaStoikov as(0.0, 1.0);
    double s = as.optimal_spread_fraction(0.2, 10.0, 1.0);
    EXPECT_NEAR(s, 0.0, 1e-10);
}

// ── Spread Fraction: Low Fill Intensity = Wide ────────────────────────────

TEST(ASTest, SpreadFracLowFillIntensity) {
    AvellanedaStoikov as(0.1, 1.0);
    double s = as.optimal_spread_fraction(0.2, 0.01, 1.0);
    EXPECT_GT(s, 0.0);
}

// ── Spread Fraction: Very High Fill Intensity = Lower Bound ───────────────

TEST(ASTest, SpreadFracHighFillIntensity) {
    AvellanedaStoikov as(0.1, 1.0);
    double s = as.optimal_spread_fraction(0.2, 1e6, 1.0);
    EXPECT_NEAR(s, 0.004, 1e-5);
}

// ── Spread Fraction: Formula Verification ─────────────────────────────────

TEST(ASTest, SpreadFracFormula) {
    AvellanedaStoikov as(0.1, 1.0);
    double gamma = 0.1;
    double kappa = 10.0;
    double sigma = 0.2;
    double tau = 1.0;

    double s = as.optimal_spread_fraction(sigma, kappa, tau);
    double expected = gamma * sigma * sigma * tau + (2.0 / gamma) * std::log(1.0 + gamma / kappa);
    EXPECT_NEAR(s, expected, 1e-10);
}

// ── Full Quote: Bid < Ask ─────────────────────────────────────────────────

TEST(ASTest, FullQuoteBidLessThanAsk) {
    AvellanedaStoikov as(0.1, 1.0);
    ASQuote q = as.compute_quotes(10000, 0.3, 0.2, 10.0, 1.0);
    EXPECT_LT(q.bid_price, q.ask_price);
    EXPECT_GT(q.bid_price, 0);
    EXPECT_GT(q.ask_price, 0);
}

// ── Full Quote: Long Inventory Shifts Both Down ───────────────────────────

TEST(ASTest, FullQuoteLongInventoryShiftsDown) {
    AvellanedaStoikov as(0.1, 1.0);

    ASQuote q_flat = as.compute_quotes(10000, 0.0, 0.2, 1000.0, 1.0);
    ASQuote q_long = as.compute_quotes(10000, 0.5, 0.2, 1000.0, 1.0);

    EXPECT_LT(q_long.bid_price, q_flat.bid_price);
    EXPECT_LT(q_long.ask_price, q_flat.ask_price);
}

// ── Full Quote: Short Inventory Shifts Both Up ────────────────────────────

TEST(ASTest, FullQuoteShortInventoryShiftsUp) {
    AvellanedaStoikov as(0.1, 1.0);

    ASQuote q_flat = as.compute_quotes(10000, 0.0, 0.2, 1000.0, 1.0);
    ASQuote q_short = as.compute_quotes(10000, -0.5, 0.2, 1000.0, 1.0);

    EXPECT_GT(q_short.bid_price, q_flat.bid_price);
    EXPECT_GT(q_short.ask_price, q_flat.ask_price);
}

// ── Full Quote: Wider Spread With Higher Volatility ───────────────────────

TEST(ASTest, FullQuoteWiderSpreadWithHigherVol) {
    AvellanedaStoikov as(0.1, 1.0);

    ASQuote q_low = as.compute_quotes(10000, 0.0, 0.1, 1000.0, 1.0);
    ASQuote q_high = as.compute_quotes(10000, 0.0, 0.5, 1000.0, 1.0);

    EXPECT_GE(q_high.ask_price - q_high.bid_price, q_low.ask_price - q_low.bid_price);
}

// ── Full Quote: Zero Volatility ───────────────────────────────────────────

TEST(ASTest, FullQuoteZeroVolatility) {
    AvellanedaStoikov as(0.1, 1.0);
    ASQuote q = as.compute_quotes(10000, 0.5, 0.0, 1000.0, 1.0);

    EXPECT_NEAR(q.reservation_price, 10000.0, 1e-6);
    EXPECT_LT(q.bid_price, q.ask_price);
}

// ── Full Quote: Getters / Setters ─────────────────────────────────────────

TEST(ASTest, GetSetRiskAversion) {
    AvellanedaStoikov as;
    EXPECT_NEAR(as.risk_aversion(), 0.1, 1e-10);
    as.set_risk_aversion(0.5);
    EXPECT_NEAR(as.risk_aversion(), 0.5, 1e-10);
}

TEST(ASTest, GetSetTimeHorizon) {
    AvellanedaStoikov as;
    EXPECT_NEAR(as.time_horizon(), 1.0, 1e-10);
    as.set_time_horizon(5.0);
    EXPECT_NEAR(as.time_horizon(), 5.0, 1e-10);
}

// ── Full Quote: Default Constructor Values ────────────────────────────────

TEST(ASTest, DefaultConstructor) {
    AvellanedaStoikov as;
    EXPECT_NEAR(as.risk_aversion(), 0.1, 1e-10);
    EXPECT_NEAR(as.time_horizon(), 1.0, 1e-10);
}

// ── Full Quote: Rounding to Integer Ticks ─────────────────────────────────

TEST(ASTest, FullQuoteIntegerRounding) {
    AvellanedaStoikov as(0.1, 1.0);
    ASQuote q = as.compute_quotes(10000, 0.1, 0.2, 1000.0, 1.0);

    EXPECT_TRUE(q.bid_price >= 9900);
    EXPECT_TRUE(q.ask_price <= 10100);
    EXPECT_LT(q.bid_price, q.ask_price);
}

// ── Full Quote: Extreme Inventory Produces Sensible Prices ────────────────

TEST(ASTest, FullQuoteExtremeInventory) {
    AvellanedaStoikov as(1.0, 1.0);
    ASQuote q = as.compute_quotes(10000, 1.0, 0.5, 5.0, 1.0);

    EXPECT_GT(q.bid_price, 0);
    EXPECT_GT(q.ask_price, 0);
    EXPECT_LT(q.bid_price, q.ask_price);
}
