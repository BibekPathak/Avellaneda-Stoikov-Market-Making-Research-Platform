#pragma once

#include "hft/order.hpp"
#include <cstdint>
#include <cstdlib>

namespace mm {

struct ASRiskLimits {
    int64_t max_position = 10000;
    int64_t max_order_size = 5000;
    double max_drawdown = 0.05;
    int64_t max_daily_loss = 100000;
    double max_exposure_pct = 0.5;
    uint64_t max_quotes_per_second = 20;
    int64_t capital = 10000000;
};

struct RiskCheckResult {
    bool approved = true;
    const char* reason = nullptr;
};

class ASRiskManager {
public:
    explicit ASRiskManager(const ASRiskLimits& limits = ASRiskLimits{})
        : limits_(limits)
        , daily_pnl_(0)
        , peak_pnl_(0)
        , daily_loss_(0)
        , quote_count_(0)
        , last_quote_ts_us_(0)
    {}

    RiskCheckResult check_quote(
        int64_t bid_price, int64_t ask_price,
        uint64_t bid_size, uint64_t ask_size,
        int64_t current_position, int64_t mid_price
    ) {
        RiskCheckResult r;
        r.approved = true;

        if (bid_price <= 0 || ask_price <= 0) {
            r.approved = false;
            r.reason = "negative price";
            return r;
        }
        if (bid_price >= ask_price) {
            r.approved = false;
            r.reason = "bid >= ask";
            return r;
        }

        int64_t new_pos_if_both_fill = current_position
            + static_cast<int64_t>(bid_size)
            - static_cast<int64_t>(ask_size);

        if (std::llabs(new_pos_if_both_fill) > limits_.max_position) {
            r.approved = false;
            r.reason = "max position exceeded";
            return r;
        }

        uint64_t pos_if_buy_fills = static_cast<uint64_t>(
            std::llabs(current_position + static_cast<int64_t>(bid_size)));
        if (pos_if_buy_fills > limits_.max_position) {
            r.approved = false;
            r.reason = "bid would exceed max position";
            return r;
        }

        if (bid_size > limits_.max_order_size || ask_size > limits_.max_order_size) {
            r.approved = false;
            r.reason = "order size exceeds limit";
            return r;
        }

        double max_exposure_val = static_cast<double>(limits_.capital) * limits_.max_exposure_pct;
        double exposure = static_cast<double>(std::llabs(current_position))
                        * static_cast<double>(mid_price);
        if (exposure > max_exposure_val) {
            r.approved = false;
            r.reason = "exposure limit exceeded";
            return r;
        }

        if (daily_loss_ <= -limits_.max_daily_loss) {
            r.approved = false;
            r.reason = "daily loss limit reached";
            return r;
        }

        if (drawdown() >= limits_.max_drawdown) {
            r.approved = false;
            r.reason = "max drawdown reached";
            return r;
        }

        return r;
    }

    RiskCheckResult check_order(
        hft::Side side, int64_t price, uint64_t quantity,
        int64_t current_position, int64_t mid_price
    ) {
        RiskCheckResult r;
        r.approved = true;

        if (price <= 0) {
            r.approved = false;
            r.reason = "negative price";
            return r;
        }
        if (quantity > limits_.max_order_size) {
            r.approved = false;
            r.reason = "order size exceeds limit";
            return r;
        }

        int64_t new_pos = current_position
            + (side == hft::Side::Buy
               ? static_cast<int64_t>(quantity)
               : -static_cast<int64_t>(quantity));

        if (std::llabs(new_pos) > limits_.max_position) {
            r.approved = false;
            r.reason = "max position exceeded";
            return r;
        }

        return r;
    }

    bool check_quote_rate(uint64_t timestamp_us) {
        if (timestamp_us == 0) return true;
        if (last_quote_ts_us_ == 0) {
            last_quote_ts_us_ = timestamp_us;
            quote_count_ = 1;
            return true;
        }

        uint64_t elapsed = timestamp_us - last_quote_ts_us_;
        if (elapsed >= 1000000) {
            quote_count_ = 0;
            last_quote_ts_us_ = timestamp_us;
        }

        if (quote_count_ >= limits_.max_quotes_per_second) {
            return false;
        }

        quote_count_++;
        return true;
    }

    void record_fill(int64_t price, uint64_t quantity, hft::Side side) {
        (void)price;
        (void)quantity;
        (void)side;
    }

    void record_pnl(int64_t realized_pnl, int64_t unrealized_pnl) {
        int64_t total = realized_pnl + unrealized_pnl;
        daily_pnl_ = total;
        if (total > peak_pnl_) {
            peak_pnl_ = total;
        }
        if (total < daily_loss_) {
            daily_loss_ = total;
        }
    }

    void reset_daily() {
        daily_pnl_ = 0;
        peak_pnl_ = 0;
        daily_loss_ = 0;
        quote_count_ = 0;
        last_quote_ts_us_ = 0;
    }

    void reset() {
        reset_daily();
    }

    int64_t daily_pnl() const { return daily_pnl_; }
    int64_t peak_pnl() const { return peak_pnl_; }
    int64_t daily_loss() const { return daily_loss_; }

    double drawdown() const {
        if (peak_pnl_ == 0) return 0.0;
        return static_cast<double>(peak_pnl_ - daily_pnl_) / static_cast<double>(peak_pnl_);
    }

    const ASRiskLimits& limits() const { return limits_; }

private:
    ASRiskLimits limits_;
    int64_t daily_pnl_;
    int64_t peak_pnl_;
    int64_t daily_loss_;
    uint64_t quote_count_;
    uint64_t last_quote_ts_us_;
};

} // namespace mm
