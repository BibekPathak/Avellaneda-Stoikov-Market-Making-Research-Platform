#pragma once

#include "hft/order.hpp"
#include <cmath>
#include <cstdint>
#include <limits>

namespace mm {

struct FillModelState {
    int64_t mid_price = 0;
    int64_t spread = 0;
    double arrival_rate = 0.0;
    double cancel_rate = 0.0;

    uint64_t depth_ahead = 0;
    uint64_t level_depth = 0;
    int64_t level_price = 0;

    double order_flow_imbalance = 0.0;
    double queue_imbalance = 0.0;

    double time_horizon = 1.0;
};

class FillModel {
public:
    virtual ~FillModel() = default;
    virtual double prob_fill(hft::Side side, int64_t price, const FillModelState& state) const = 0;
    virtual const char* name() const = 0;
    virtual void reset() {}
};

class SimplePoissonFill : public FillModel {
public:
    explicit SimplePoissonFill(
        double base_intensity = 10.0,
        double decay_factor = 1.5
    )   : base_intensity_(base_intensity)
        , decay_factor_(decay_factor)
    {}

    double prob_fill(hft::Side buy_or_sell, int64_t price, const FillModelState& state) const override {
        if (state.arrival_rate <= 0.0 && state.cancel_rate <= 0.0) return 0.0;
        if (state.spread <= 0) return 0.0;

        double distance = distance_from_inside(buy_or_sell, price, state);
        double lambda = effective_intensity(distance, state);

        if (lambda <= 0.0) return 0.0;
        if (state.time_horizon <= 0.0) return 0.0;

        double prob = 1.0 - std::exp(-lambda * state.time_horizon);
        return prob > 0.0 ? (prob < 1.0 ? prob : 1.0) : 0.0;
    }

    const char* name() const override { return "SimplePoisson"; }

    void reset() override {}

private:
    double distance_from_inside(hft::Side side, int64_t price, const FillModelState& state) const {
        int64_t inside;
        if (side == hft::Side::Buy) {
            inside = state.level_price > 0 ? state.level_price : (state.mid_price - state.spread / 2);
        } else {
            inside = state.level_price > 0 ? state.level_price : (state.mid_price + state.spread / 2);
        }

        if (side == hft::Side::Buy) {
            return static_cast<double>(inside - price) / static_cast<double>(state.spread);
        } else {
            return static_cast<double>(price - inside) / static_cast<double>(state.spread);
        }
    }

    double effective_intensity(double distance, const FillModelState& state) const {
        if (distance <= 0.0) {
            return state.arrival_rate + state.cancel_rate;
        }
        double decay = std::exp(-distance * decay_factor_);
        return (state.arrival_rate + state.cancel_rate) * decay;
    }

    double base_intensity_;
    double decay_factor_;
};

class QueuePositionFill : public FillModel {
public:
    explicit QueuePositionFill(
        double position_decay = 2.0,
        double base_rate = 5.0
    )   : position_decay_(position_decay)
        , base_rate_(base_rate)
    {}

    double prob_fill(hft::Side side, int64_t price, const FillModelState& state) const override {
        if (state.spread <= 0) return 0.0;

        double distance = distance_from_inside(side, price, state);
        double queue_factor = queue_position_factor(state);

        double rate = (state.arrival_rate + state.cancel_rate + base_rate_)
                    * std::exp(-distance * position_decay_)
                    * queue_factor;

        if (rate <= 0.0 || state.time_horizon <= 0.0) return 0.0;

        double prob = 1.0 - std::exp(-rate * state.time_horizon);
        return prob > 0.0 ? (prob < 1.0 ? prob : 1.0) : 0.0;
    }

    const char* name() const override { return "QueuePosition"; }

    void reset() override {}

private:
    double distance_from_inside(hft::Side side, int64_t price, const FillModelState& state) const {
        int64_t inside;
        if (side == hft::Side::Buy) {
            inside = state.level_price > 0 ? state.level_price : (state.mid_price - state.spread / 2);
        } else {
            inside = state.level_price > 0 ? state.level_price : (state.mid_price + state.spread / 2);
        }

        if (side == hft::Side::Buy) {
            return static_cast<double>(inside - price) / static_cast<double>(state.spread);
        } else {
            return static_cast<double>(price - inside) / static_cast<double>(state.spread);
        }
    }

    double queue_position_factor(const FillModelState& state) const {
        if (state.level_depth == 0) return 0.0;
        double pos_ratio = static_cast<double>(state.depth_ahead) / static_cast<double>(state.level_depth);
        return std::exp(-pos_ratio * position_decay_);
    }

    double position_decay_;
    double base_rate_;
};

} // namespace mm
