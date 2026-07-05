#include <gtest/gtest.h>
#include "mm/kappa_calibrator.hpp"

using namespace mm;

// ── Default construction ─────────────────────────────────────────────────

TEST(KappaCalibratorTest, DefaultConstruction) {
    KappaCalibrator cal;
    EXPECT_EQ(cal.num_buckets(), 20);
    EXPECT_EQ(cal.base_kappa(), 10.0);
    EXPECT_EQ(cal.min_kappa(), 1.0);
    EXPECT_EQ(cal.max_kappa(), 1000.0);
}

// ── No data returns base_kappa ──────────────────────────────────────────

TEST(KappaCalibratorTest, DefaultKappa) {
    KappaCalibrator cal;
    EXPECT_EQ(cal.kappa(0.1), 10.0);
    EXPECT_EQ(cal.kappa(0.5), 10.0);
    EXPECT_EQ(cal.kappa(0.9), 10.0);
}

// ── Distance out of range returns base_kappa ────────────────────────────

TEST(KappaCalibratorTest, DistanceOutOfRange) {
    KappaCalibrator cal;
    EXPECT_EQ(cal.kappa(-0.1), 10.0);
    EXPECT_EQ(cal.kappa(1.0), 10.0);
    EXPECT_EQ(cal.kappa(2.0), 10.0);
}

// ── Fills at distance increase kappa ─────────────────────────────────────

TEST(KappaCalibratorTest, FillsAtDistanceIncreaseKappa) {
    KappaCalibrator cal(20, 1.0, 1000.0, 10.0, 1.0);

    // First call initializes with the observed value
    cal.record_quote(0.1, true);  // ratio = 1.0
    EXPECT_NEAR(cal.kappa(0.1), 10.0, 0.1);

    cal.record_quote(0.1, true);  // ewma = 1.0 * 1.0 + 0.0 * 1.0 = 1.0
    EXPECT_NEAR(cal.kappa(0.1), 10.0, 0.1);

    // Higher fill rate than bucket 0.5
    cal.record_quote(0.5, false);
    cal.record_quote(0.5, false);
    double k_near = cal.kappa(0.1);
    double k_far = cal.kappa(0.5);
    EXPECT_GT(k_near, k_far);
}

// ── No fills at distance returns min_kappa ───────────────────────────────

TEST(KappaCalibratorTest, NoFillsAtDistance) {
    KappaCalibrator cal(20, 1.0, 1000.0, 10.0, 0.3);

    for (int i = 0; i < 10; i++) {
        cal.record_quote(0.3, false);
    }

    double k = cal.kappa(0.3);
    EXPECT_EQ(k, 1.0);
}

// ── Different distances use different buckets ────────────────────────────

TEST(KappaCalibratorTest, DifferentDistances) {
    KappaCalibrator cal(10, 1.0, 1000.0, 10.0, 1.0);

    // All fills at 0.05 (bucket 0)
    cal.record_quote(0.05, true);
    cal.record_quote(0.05, true);
    cal.record_quote(0.05, true);

    // No fills at 0.15 (bucket 1)
    cal.record_quote(0.15, false);
    cal.record_quote(0.15, false);

    double k_near = cal.kappa(0.05);
    double k_far = cal.kappa(0.15);

    EXPECT_GT(k_near, k_far);
}

// ── Reset clears state ──────────────────────────────────────────────────

TEST(KappaCalibratorTest, Reset) {
    KappaCalibrator cal(10, 1.0, 1000.0, 10.0, 0.5);

    cal.record_quote(0.1, true);
    cal.record_quote(0.1, true);
    EXPECT_GT(cal.kappa(0.1), 1.0);

    cal.reset();

    EXPECT_EQ(cal.kappa(0.1), 10.0);
    EXPECT_EQ(cal.bucket(0).total_quotes, 0);
    EXPECT_EQ(cal.bucket(0).total_fills, 0);
}

// ── Bucket boundaries ────────────────────────────────────────────────────

TEST(KappaCalibratorTest, BucketBoundaries) {
    KappaCalibrator cal(10, 1.0, 1000.0, 10.0, 1.0); // 10 buckets of width 0.1

    // distance 0.0 → bucket 0
    cal.record_quote(0.0, true);
    EXPECT_EQ(cal.bucket(0).total_quotes, 1);

    // distance 0.09 → bucket 0 (floor(0.09/0.1) = 0)
    cal.record_quote(0.09, true);
    EXPECT_EQ(cal.bucket(0).total_quotes, 2);

    // distance 0.10 → bucket 1 (floor(0.10/0.1) = 1)
    cal.record_quote(0.10, true);
    EXPECT_EQ(cal.bucket(0).total_quotes, 2);
    EXPECT_EQ(cal.bucket(1).total_quotes, 1);
}

// ── Max kappa clamping ──────────────────────────────────────────────────

TEST(KappaCalibratorTest, MaxKappaClamping) {
    KappaCalibrator cal(10, 1.0, 50.0, 100.0, 1.0);

    cal.record_quote(0.1, true);
    // fill_ratio_ewma = 1.0, kappa = 1.0 * 100.0 = 100.0, but max=50.0
    EXPECT_EQ(cal.kappa(0.1), 50.0);
}

// ── Min kappa clamping ──────────────────────────────────────────────────

TEST(KappaCalibratorTest, MinKappaClamping) {
    KappaCalibrator cal(10, 2.0, 1000.0, 50.0, 1.0);
    // fill_ratio = 0 but min_kappa = 2.0

    cal.record_quote(0.1, false);
    EXPECT_EQ(cal.kappa(0.1), 2.0);
}

// ── EWMA smoothing ──────────────────────────────────────────────────────

TEST(KappaCalibratorTest, EwmaSmoothing) {
    KappaCalibrator cal(10, 1.0, 1000.0, 10.0, 0.5);

    const double d = 0.05;

    // First fill → initializes ratio = 1.0
    cal.record_quote(d, true);
    double k1 = cal.kappa(d);
    EXPECT_NEAR(k1, 10.0, 0.1);

    // No fill → ewma = 0.5 * 0.0 + 0.5 * 1.0 = 0.5
    cal.record_quote(d, false);
    double k2 = cal.kappa(d);
    EXPECT_NEAR(k2, 5.0, 0.5);

    // Fill → ewma = 0.5 * 1.0 + 0.5 * 0.5 = 0.75
    cal.record_quote(d, true);
    double k3 = cal.kappa(d);
    EXPECT_NEAR(k3, 7.5, 0.5);

    // Kappa should recover after a fill
    EXPECT_GT(k3, k2);
}

// ── Custom parameters ───────────────────────────────────────────────────

TEST(KappaCalibratorTest, CustomParameters) {
    KappaCalibrator cal(30, 0.5, 500.0, 20.0, 0.1);

    EXPECT_EQ(cal.num_buckets(), 30);
    EXPECT_EQ(cal.min_kappa(), 0.5);
    EXPECT_EQ(cal.max_kappa(), 500.0);
    EXPECT_EQ(cal.base_kappa(), 20.0);
    EXPECT_EQ(cal.alpha(), 0.1);
}

// ── Many quotes at various distances ────────────────────────────────────

TEST(KappaCalibratorTest, ManyQuotes) {
    KappaCalibrator cal(10, 1.0, 100.0, 10.0, 0.2);

    // Bucket 0 (0.0-0.1): 8 fills / 10 quotes
    for (int i = 0; i < 8; i++) cal.record_quote(0.05, true);
    for (int i = 0; i < 2; i++) cal.record_quote(0.05, false);

    // Bucket 4 (0.4-0.5): 1 fill / 10 quotes
    for (int i = 0; i < 1; i++) cal.record_quote(0.45, true);
    for (int i = 0; i < 9; i++) cal.record_quote(0.45, false);

    double k_aggressive = cal.kappa(0.05);
    double k_passive = cal.kappa(0.45);

    EXPECT_GT(k_aggressive, k_passive);
}
