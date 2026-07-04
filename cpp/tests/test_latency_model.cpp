#include <gtest/gtest.h>
#include "mm/latency_model.hpp"
#include <algorithm>
#include <set>

using namespace mm;

// ── FixedLatency ─────────────────────────────────────────────────────────

TEST(FixedLatencyTest, DefaultConstruction) {
    FixedLatency model;
    EXPECT_EQ(model.delay_us(), 100);
    EXPECT_STREQ(model.name(), "FixedLatency");
}

TEST(FixedLatencyTest, CustomLatency) {
    FixedLatency model(500);
    EXPECT_EQ(model.delay_us(), 500);
}

TEST(FixedLatencyTest, AlwaysReturnsSame) {
    FixedLatency model(250);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(model.delay_us(), 250);
    }
}

TEST(FixedLatencyTest, ZeroLatency) {
    FixedLatency model(0);
    EXPECT_EQ(model.delay_us(), 0);
}

TEST(FixedLatencyTest, LargeLatency) {
    FixedLatency model(999999);
    EXPECT_EQ(model.delay_us(), 999999);
}

// ── UniformLatency ───────────────────────────────────────────────────────

TEST(UniformLatencyTest, DefaultConstruction) {
    UniformLatency model;
    uint64_t d = model.delay_us();
    EXPECT_GE(d, 50);
    EXPECT_LE(d, 150);
    EXPECT_STREQ(model.name(), "UniformLatency");
}

TEST(UniformLatencyTest, CustomRange) {
    UniformLatency model(200, 300);
    for (int i = 0; i < 100; i++) {
        uint64_t d = model.delay_us();
        EXPECT_GE(d, 200);
        EXPECT_LE(d, 300);
    }
}

TEST(UniformLatencyTest, ProducesVariedValues) {
    UniformLatency model(10, 100);
    std::set<uint64_t> values;
    for (int i = 0; i < 500; i++) {
        values.insert(model.delay_us());
    }
    EXPECT_GT(values.size(), 1);
}

TEST(UniformLatencyTest, MinEqualsMax) {
    UniformLatency model(777, 777);
    for (int i = 0; i < 10; i++) {
        EXPECT_EQ(model.delay_us(), 777);
    }
}

TEST(UniformLatencyTest, Reset) {
    UniformLatency model(100, 200);
    model.delay_us();
    model.delay_us();
    model.reset();
    // After reset, should still produce valid values
    uint64_t d = model.delay_us();
    EXPECT_GE(d, 100);
    EXPECT_LE(d, 200);
}

// ── NormalLatency ────────────────────────────────────────────────────────

TEST(NormalLatencyTest, DefaultConstruction) {
    NormalLatency model;
    uint64_t d = model.delay_us();
    EXPECT_GE(d, 0);
    EXPECT_STREQ(model.name(), "NormalLatency");
}

TEST(NormalLatencyTest, MeanApproximation) {
    NormalLatency model(500, 50);
    uint64_t sum = 0;
    int n = 10000;
    for (int i = 0; i < n; i++) {
        sum += model.delay_us();
    }
    double mean = static_cast<double>(sum) / n;
    EXPECT_NEAR(mean, 500.0, 50.0);
}

TEST(NormalLatencyTest, NonNegative) {
    NormalLatency model(10, 100);
    model.set_min_us(0);
    for (int i = 0; i < 1000; i++) {
        uint64_t d = model.delay_us();
        EXPECT_GE(d, 0);
    }
}

TEST(NormalLatencyTest, Reset) {
    NormalLatency model(100, 10);
    model.delay_us();
    model.reset();
    uint64_t d = model.delay_us();
    EXPECT_GE(d, 0);
}

// ── Polymorphism ─────────────────────────────────────────────────────────

TEST(LatencyModelTest, Polymorphism) {
    FixedLatency fixed(123);
    UniformLatency uniform(200, 300);
    NormalLatency normal(500, 50);

    LatencyModel& m1 = fixed;
    LatencyModel& m2 = uniform;
    LatencyModel& m3 = normal;

    EXPECT_STREQ(m1.name(), "FixedLatency");
    EXPECT_STREQ(m2.name(), "UniformLatency");
    EXPECT_STREQ(m3.name(), "NormalLatency");

    EXPECT_EQ(m1.delay_us(), 123);

    uint64_t u = m2.delay_us();
    EXPECT_GE(u, 200);
    EXPECT_LE(u, 300);

    uint64_t n = m3.delay_us();
    EXPECT_GE(n, 0);
}
