#include <gtest/gtest.h>
#include "mm/fill_model.hpp"
#include <cmath>

using namespace mm;

// ── FillModelState Defaults ───────────────────────────────────────────────

FillModelState default_state() {
    FillModelState s;
    s.mid_price = 10000;
    s.spread = 100;
    s.arrival_rate = 10.0;
    s.cancel_rate = 5.0;
    s.depth_ahead = 0;
    s.level_depth = 1000;
    s.level_price = 0;
    s.order_flow_imbalance = 0.0;
    s.queue_imbalance = 0.5;
    s.time_horizon = 1.0;
    return s;
}

// ── SimplePoissonFill — Probability Bounds ────────────────────────────────

TEST(SimplePoissonTest, ProbabilityInBounds) {
    SimplePoissonFill model;
    auto state = default_state();

    for (int p = 9900; p <= 10100; p += 50) {
        double prob_buy = model.prob_fill(hft::Side::Buy, p, state);
        EXPECT_GE(prob_buy, 0.0);
        EXPECT_LE(prob_buy, 1.0);

        double prob_sell = model.prob_fill(hft::Side::Sell, p, state);
        EXPECT_GE(prob_sell, 0.0);
        EXPECT_LE(prob_sell, 1.0);
    }
}

// ── SimplePoissonFill — Aggressive Quotes Fill More ───────────────────────

TEST(SimplePoissonTest, AggressiveQuotesFillMore) {
    SimplePoissonFill model;
    auto state = default_state();

    double prob_bid = model.prob_fill(hft::Side::Buy,  9950, state);
    double prob_mid = model.prob_fill(hft::Side::Buy, 10000, state);
    double prob_ask = model.prob_fill(hft::Side::Buy, 10050, state);

    // At ask (crossing spread) should fill faster than at mid, which fills faster than at bid
    EXPECT_GE(prob_ask, prob_mid);
    EXPECT_GE(prob_mid, prob_bid);
}

// ── SimplePoissonFill — Buy/Sell Symmetry ─────────────────────────────────

TEST(SimplePoissonTest, Symmetry) {
    SimplePoissonFill model;
    auto state = default_state();

    // Buy at mid-50 (aggressive) should be symmetric to Sell at mid+50
    double prob_buy = model.prob_fill(hft::Side::Buy,  9950, state);
    double prob_sell = model.prob_fill(hft::Side::Sell, 10050, state);

    EXPECT_NEAR(prob_buy, prob_sell, 1e-10);
}

// ── SimplePoissonFill — No Arrivals = No Fill ─────────────────────────────

TEST(SimplePoissonTest, NoArrivalsNoCancels) {
    SimplePoissonFill model;
    auto state = default_state();
    state.arrival_rate = 0.0;
    state.cancel_rate = 0.0;

    double prob = model.prob_fill(hft::Side::Buy, 10000, state);
    EXPECT_NEAR(prob, 0.0, 1e-10);
}

// ── SimplePoissonFill — Higher Arrival Rate = Higher Fill ────────────────

TEST(SimplePoissonTest, HigherArrivalRate) {
    SimplePoissonFill model;
    auto state_mid = default_state();
    auto state_high = default_state();
    state_high.arrival_rate = 100.0;

    double prob_mid = model.prob_fill(hft::Side::Buy, 9950, state_mid);
    double prob_high = model.prob_fill(hft::Side::Buy, 9950, state_high);

    EXPECT_GT(prob_high, prob_mid);
}

// ── SimplePoissonFill — Zero Spread = No Fill ─────────────────────────────

TEST(SimplePoissonTest, ZeroSpread) {
    SimplePoissonFill model;
    auto state = default_state();
    state.spread = 0;

    double prob = model.prob_fill(hft::Side::Buy, 10000, state);
    EXPECT_NEAR(prob, 0.0, 1e-10);
}

// ── SimplePoissonFill — Short Time Horizon = Low Fill ─────────────────────

TEST(SimplePoissonTest, ShortTimeHorizon) {
    SimplePoissonFill model;
    auto state = default_state();

    double prob_1s = model.prob_fill(hft::Side::Buy, 9950, state);
    state.time_horizon = 0.01;
    double prob_10ms = model.prob_fill(hft::Side::Buy, 9950, state);

    EXPECT_GT(prob_1s, prob_10ms);
}

// ── SimplePoissonFill — Fill at Competitive Price (distance <= 0) ─────────

TEST(SimplePoissonTest, FillAtInside) {
    SimplePoissonFill model;
    auto state = default_state();

    // Buy at the bid (inside): should have non-trivial fill probability
    int64_t bid = state.mid_price - state.spread / 2;
    double prob = model.prob_fill(hft::Side::Buy, bid, state);
    EXPECT_GT(prob, 0.0);
    EXPECT_LT(prob, 1.0);
}

// ── SimplePoissonFill — Fill Crossing Spread ──────────────────────────────

TEST(SimplePoissonTest, FillCrossingSpread) {
    SimplePoissonFill model;
    auto state = default_state();

    // Buy at the ask (crossing spread): should fill
    int64_t ask = state.mid_price + state.spread / 2;
    double prob = model.prob_fill(hft::Side::Buy, ask, state);
    EXPECT_GT(prob, 0.99);
}

// ── SimplePoissonFill — Far From Inside = Very Low Fill ──────────────────

TEST(SimplePoissonTest, FarFromInside) {
    SimplePoissonFill model;
    auto state = default_state();

    double prob = model.prob_fill(hft::Side::Buy, 9000, state);
    EXPECT_NEAR(prob, 0.0, 1e-4);
}

// ── SimplePoissonFill — Lambda = 0.0 (edge case) ─────────────────────────

TEST(SimplePoissonTest, NegativeTimeHorizon) {
    SimplePoissonFill model;
    auto state = default_state();
    state.time_horizon = -1.0;

    double prob = model.prob_fill(hft::Side::Buy, 9950, state);
    EXPECT_NEAR(prob, 0.0, 1e-10);
}

// ── SimplePoissonFill — Poisson Formula Verification ──────────────────────

TEST(SimplePoissonTest, PoissonFormula) {
    SimplePoissonFill model(10.0, 0.0); // decay_factor = 0, so no distance decay
    auto state = default_state();
    state.arrival_rate = 2.0;
    state.cancel_rate = 0.0;

    // With no distance decay and λ = 2.0, T = 1.0:
    // P = 1 - exp(-2.0 * 1.0) = 1 - exp(-2) ≈ 0.8647
    double prob = model.prob_fill(hft::Side::Buy, 9900, state);
    double expected = 1.0 - std::exp(-2.0);
    EXPECT_NEAR(prob, expected, 1e-6);
}

// ── QueuePositionFill — Probability Bounds ────────────────────────────────

TEST(QueuePositionTest, ProbabilityInBounds) {
    QueuePositionFill model;
    auto state = default_state();

    for (int p = 9900; p <= 10100; p += 50) {
        double prob_buy = model.prob_fill(hft::Side::Buy, p, state);
        EXPECT_GE(prob_buy, 0.0);
        EXPECT_LE(prob_buy, 1.0);
    }
}

// ── QueuePositionFill — Queue Position Reduces Fill Prob ─────────────────

TEST(QueuePositionTest, PositionReducesFillProb) {
    QueuePositionFill model;
    auto state = default_state();

    double prob_front = model.prob_fill(hft::Side::Buy, 9950, state);

    state.depth_ahead = 500;
    state.level_depth = 1000;
    double prob_mid = model.prob_fill(hft::Side::Buy, 9950, state);

    state.depth_ahead = 900;
    double prob_back = model.prob_fill(hft::Side::Buy, 9950, state);

    EXPECT_GE(prob_front, prob_mid);
    EXPECT_GE(prob_mid, prob_back);
}

// ── QueuePositionFill — Empty Level = No Fill ─────────────────────────────

TEST(QueuePositionTest, EmptyLevel) {
    QueuePositionFill model;
    auto state = default_state();
    state.level_depth = 0;

    double prob = model.prob_fill(hft::Side::Buy, 9950, state);
    EXPECT_NEAR(prob, 0.0, 1e-10);
}

// ── QueuePositionFill — Aggressive Quotes Outweigh Position ──────────────

TEST(QueuePositionTest, AggressiveOverridesPosition) {
    QueuePositionFill model;
    auto state = default_state();

    state.depth_ahead = 900;
    state.level_depth = 1000;
    double prob_back = model.prob_fill(hft::Side::Buy, 9950, state);

    // Crossing the spread even from far back should fill
    double prob_cross = model.prob_fill(hft::Side::Buy, 10050, state);
    EXPECT_GT(prob_cross, prob_back);
}

// ── QueuePositionFill — NoArrivals = Lower Fill ───────────────────────────

TEST(QueuePositionTest, NoArrivals) {
    QueuePositionFill model;
    auto state = default_state();
    state.arrival_rate = 0.0;
    state.cancel_rate = 0.0;

    double prob_base = model.prob_fill(hft::Side::Buy, 9950, default_state());
    double prob_zero = model.prob_fill(hft::Side::Buy, 9950, state);

    EXPECT_GE(prob_base, prob_zero);
}

// ── QueuePositionFill — Symmetry ──────────────────────────────────────────

TEST(QueuePositionTest, Symmetry) {
    QueuePositionFill model;
    auto state = default_state();

    double prob_buy = model.prob_fill(hft::Side::Buy,  9950, state);
    double prob_sell = model.prob_fill(hft::Side::Sell, 10050, state);

    EXPECT_NEAR(prob_buy, prob_sell, 1e-10);
}

// ── QueuePositionFill — Short Horizon = Low Fill ──────────────────────────

TEST(QueuePositionTest, ShortHorizon) {
    QueuePositionFill model;
    auto state = default_state();

    double prob_long = model.prob_fill(hft::Side::Buy, 9950, state);
    state.time_horizon = 0.001;
    double prob_short = model.prob_fill(hft::Side::Buy, 9950, state);

    EXPECT_GT(prob_long, prob_short);
}

// ── Both Models: Zero Spread Edge Cases ────────────────────────────────────

TEST(QueuePositionTest, ZeroSpread) {
    QueuePositionFill model;
    auto state = default_state();
    state.spread = 0;

    double prob = model.prob_fill(hft::Side::Buy, 10000, state);
    EXPECT_NEAR(prob, 0.0, 1e-10);
}

// ── FillModel Interface: Polymorphism ──────────────────────────────────────

TEST(FillModelTest, Polymorphism) {
    SimplePoissonFill poisson;
    QueuePositionFill queue;

    FillModel& m1 = poisson;
    FillModel& m2 = queue;

    auto state = default_state();

    EXPECT_STREQ(m1.name(), "SimplePoisson");
    EXPECT_STREQ(m2.name(), "QueuePosition");

    double p1 = m1.prob_fill(hft::Side::Buy, 9950, state);
    double p2 = m2.prob_fill(hft::Side::Buy, 9950, state);

    EXPECT_GE(p1, 0.0);
    EXPECT_GE(p2, 0.0);
}

// ── Different Decay Factors ───────────────────────────────────────────────

TEST(SimplePoissonTest, DifferentDecayFactors) {
    auto state = default_state();

    SimplePoissonFill slow_decay(10.0, 0.5);  // gentle decay
    SimplePoissonFill fast_decay(10.0, 5.0);  // aggressive decay

    double prob_slow = slow_decay.prob_fill(hft::Side::Buy, 9900, state);
    double prob_fast = fast_decay.prob_fill(hft::Side::Buy, 9900, state);

    // Slower decay means further-away quotes have higher fill prob
    EXPECT_GE(prob_slow, prob_fast);
}
