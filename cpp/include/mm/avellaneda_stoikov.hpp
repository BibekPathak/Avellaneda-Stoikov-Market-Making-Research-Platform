#pragma once

#include <cstdint>
#include <cmath>
#include <limits>

namespace mm {

struct ASQuote {
    int64_t bid_price = 0;
    int64_t ask_price = 0;
    double reservation_price = 0.0;
    double half_spread = 0.0;
};

class AvellanedaStoikov {
public:
    AvellanedaStoikov(
        double risk_aversion = 0.1,
        double time_horizon = 1.0
    )   : gamma_(risk_aversion)
        , T_(time_horizon)
    {}

    ASQuote compute_quotes(
        int64_t mid_price,
        double inventory,
        double volatility,
        double fill_intensity,
        double remaining_time
    ) const {
        ASQuote q;
        double mid = static_cast<double>(mid_price);
        q.reservation_price = reservation_price(mid_price, inventory, volatility, remaining_time);
        double spread_frac = optimal_spread_fraction(volatility, fill_intensity, remaining_time);
        q.half_spread = mid * spread_frac * 0.5;
        q.bid_price = static_cast<int64_t>(std::round(q.reservation_price - q.half_spread));
        q.ask_price = static_cast<int64_t>(std::round(q.reservation_price + q.half_spread));
        if (q.bid_price > q.ask_price) {
            int64_t avg = (q.bid_price + q.ask_price) / 2;
            q.bid_price = avg;
            q.ask_price = avg;
        }
        return q;
    }

    double reservation_price(
        int64_t mid_price,
        double inventory,
        double volatility,
        double remaining_time
    ) const {
        double tau = std::max(remaining_time, 0.0);
        double var = volatility * volatility;
        double shift = static_cast<double>(mid_price) * inventory * gamma_ * var * tau;
        return static_cast<double>(mid_price) - shift;
    }

    double optimal_spread_fraction(
        double volatility,
        double fill_intensity,
        double remaining_time
    ) const {
        double tau = std::max(remaining_time, 0.0);
        double var = volatility * volatility;
        double inventory_term = gamma_ * var * tau;
        double competition_term = 0.0;
        if (fill_intensity > 0.0 && gamma_ > 0.0) {
            competition_term = (2.0 / gamma_) * std::log(1.0 + gamma_ / fill_intensity);
        }
        return inventory_term + competition_term;
    }

    void set_risk_aversion(double gamma) { gamma_ = gamma; }
    void set_time_horizon(double T) { T_ = T; }

    double risk_aversion() const { return gamma_; }
    double time_horizon() const { return T_; }

private:
    double gamma_;
    double T_;
};

} // namespace mm
