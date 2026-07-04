#pragma once

#include <cstdint>
#include <random>
#include <limits>

namespace mm {

class LatencyModel {
public:
    virtual ~LatencyModel() = default;
    virtual uint64_t delay_us() = 0;
    virtual const char* name() const = 0;
    virtual void reset() {}
};

class FixedLatency : public LatencyModel {
public:
    explicit FixedLatency(uint64_t latency_us = 100)
        : latency_us_(latency_us)
    {}

    uint64_t delay_us() override {
        return latency_us_;
    }

    const char* name() const override {
        return "FixedLatency";
    }

private:
    uint64_t latency_us_;
};

class UniformLatency : public LatencyModel {
public:
    UniformLatency(uint64_t min_us = 50, uint64_t max_us = 150)
        : gen_(std::random_device{}())
        , dist_(static_cast<double>(min_us), static_cast<double>(max_us))
        , min_us_(min_us)
        , max_us_(max_us)
    {}

    uint64_t delay_us() override {
        double d = dist_(gen_);
        if (d < static_cast<double>(min_us_)) d = static_cast<double>(min_us_);
        if (d > static_cast<double>(max_us_)) d = static_cast<double>(max_us_);
        return static_cast<uint64_t>(d);
    }

    const char* name() const override {
        return "UniformLatency";
    }

    void reset() override {
        gen_.seed(std::random_device{}());
    }

private:
    std::mt19937 gen_;
    std::uniform_real_distribution<double> dist_;
    uint64_t min_us_;
    uint64_t max_us_;
};

class NormalLatency : public LatencyModel {
public:
    NormalLatency(uint64_t mean_us = 100, uint64_t std_us = 20)
        : gen_(std::random_device{}())
        , dist_(static_cast<double>(mean_us), static_cast<double>(std_us))
        , min_us_(0)
    {}

    uint64_t delay_us() override {
        double d = dist_(gen_);
        if (d < static_cast<double>(min_us_)) d = static_cast<double>(min_us_);
        if (d > static_cast<double>(std::numeric_limits<uint64_t>::max())) {
            d = static_cast<double>(std::numeric_limits<uint64_t>::max());
        }
        return static_cast<uint64_t>(d);
    }

    const char* name() const override {
        return "NormalLatency";
    }

    void reset() override {
        gen_.seed(std::random_device{}());
    }

    void set_min_us(uint64_t min_us) { min_us_ = min_us; }

private:
    std::mt19937 gen_;
    std::normal_distribution<double> dist_;
    uint64_t min_us_;
};

} // namespace mm
