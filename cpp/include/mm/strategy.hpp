#pragma once

#include "hft/strategy.hpp"
#include "mm/inventory.hpp"
#include "mm/volatility.hpp"
#include "mm/fill_model.hpp"
#include "mm/avellaneda_stoikov.hpp"
#include "mm/quote_engine.hpp"
#include "mm/latency_model.hpp"
#include "mm/risk.hpp"
#include "mm/kappa_calibrator.hpp"
#include <memory>
#include <limits>

namespace mm {

class ASMarketMaker : public hft::IStrategy {
public:
    ASMarketMaker(
        double risk_aversion = 0.1,
        double time_horizon = 1.0,
        uint64_t order_size = 1000,
        int64_t tick_size = 1,
        uint64_t quote_interval_us = 100000,
        VolatilityMethod vol_method = VolatilityMethod::RollingStd,
        size_t vol_window = 20,
        int64_t max_position = 10000
    )   : as_model_(risk_aversion, time_horizon)
        , quote_engine_(tick_size, quote_interval_us, order_size)
        , volatility_(vol_method, vol_window)
        , risk_(ASRiskLimits{max_position, static_cast<int64_t>(order_size * 2)})
        , max_position_(max_position)
        , order_size_(order_size)
        , tick_size_(tick_size)
        , quote_interval_us_(quote_interval_us)
        , last_tick_time_us_(std::numeric_limits<uint64_t>::max())
        , tick_count_(0)
        , tick_rate_per_sec_(0.0)
        , cum_bid_size_(0)
        , cum_ask_size_(0)
        , book_update_count_(0)
    {
        fill_model_ = std::make_unique<SimplePoissonFill>();
        latency_ = std::make_unique<FixedLatency>(0);
    }

    void set_fill_model(std::unique_ptr<FillModel> model) {
        fill_model_ = std::move(model);
    }

    void set_latency_model(std::unique_ptr<LatencyModel> model) {
        latency_ = std::move(model);
    }

    template<typename M, typename... Args>
    M& emplace_latency(Args&&... args) {
        auto m = std::make_unique<M>(std::forward<Args>(args)...);
        M& ref = *m;
        latency_ = std::move(m);
        return ref;
    }

    std::vector<hft::Order> on_tick(
        const hft::MarketTick& tick,
        uint64_t order_id,
        uint64_t client_id
    ) override {
        std::vector<hft::Order> orders;

        int64_t mid = tick.mid_price();
        int64_t spread = tick.spread();
        if (mid <= 0 || spread <= 0) return orders;

        last_mid_price_ = mid;
        last_spread_ = spread;

        update_tick_rate(tick.timestamp);
        update_book_stats(tick);

        volatility_.add_price(mid);

        double sigma = volatility_.volatility();

        double avg_dist = 0.25;
        double kappa = kappa_calibrator_.kappa(avg_dist);
        if (kappa <= 0.0) kappa = 10.0;

        double inv_norm = static_cast<double>(inventory_.position)
                        / static_cast<double>(max_position_);
        inv_norm = std::max(-1.0, std::min(1.0, inv_norm));

        ASQuote asq = as_model_.compute_quotes(
            mid, inv_norm, sigma, kappa, as_model_.time_horizon()
        );

        if (!quote_engine_.should_quote(tick.timestamp)) {
            return orders;
        }

        Quote q = quote_engine_.generate_quote(asq, tick.timestamp);

        auto risk_check = risk_.check_quote(
            q.bid_price, q.ask_price, q.bid_size, q.ask_size,
            inventory_.position, mid
        );
        if (!risk_check.approved) {
            return orders;
        }

        double bid_dist = std::max(0.0,
            static_cast<double>(mid - spread / 2 - q.bid_price) / static_cast<double>(spread));
        double ask_dist = std::max(0.0,
            static_cast<double>(q.ask_price - (mid + spread / 2)) / static_cast<double>(spread));
        kappa_calibrator_.record_quote(bid_dist, false);
        kappa_calibrator_.record_quote(ask_dist, false);

        uint64_t latency_us = latency_->delay_us();
        uint64_t delayed_ts = tick.timestamp + latency_us;

        if (q.bid_size > 0) {
            orders.emplace_back(
                order_id, client_id, delayed_ts,
                q.bid_price, q.bid_size, hft::Side::Buy, hft::OrderType::Limit
            );
        }
        if (q.ask_size > 0) {
            orders.emplace_back(
                order_id + 1, client_id, delayed_ts,
                q.ask_price, q.ask_size, hft::Side::Sell, hft::OrderType::Limit
            );
        }

        pending_bid_order_id_ = order_id;
        pending_ask_order_id_ = order_id + 1;
        last_bid_price_ = q.bid_price;
        last_ask_price_ = q.ask_price;

        return orders;
    }

    void on_trade(const hft::Trade& trade) override {
        inventory_.fill(trade.side, trade.price, trade.quantity);
        inventory_.mark_to_market(
            (trade.price > 0) ? trade.price : 10000
        );
        risk_.record_pnl(inventory_.realized_pnl, inventory_.unrealized_pnl);

        if (last_spread_ > 0 && last_mid_price_ > 0) {
            double fill_dist;
            if (trade.side == hft::Side::Buy && last_bid_price_ > 0) {
                fill_dist = std::max(0.0,
                    static_cast<double>(last_mid_price_ - last_spread_ / 2 - last_bid_price_)
                    / static_cast<double>(last_spread_));
            } else if (trade.side == hft::Side::Sell && last_ask_price_ > 0) {
                fill_dist = std::max(0.0,
                    static_cast<double>(last_ask_price_ - (last_mid_price_ + last_spread_ / 2))
                    / static_cast<double>(last_spread_));
            } else {
                fill_dist = 0.0;
            }
            kappa_calibrator_.record_quote(fill_dist, true);
        }
    }

    void reset() override {
        inventory_.reset();
        volatility_.reset();
        quote_engine_.reset();
        risk_.reset();
        last_tick_time_us_ = std::numeric_limits<uint64_t>::max();
        tick_count_ = 0;
        tick_rate_per_sec_ = 0.0;
        cum_bid_size_ = 0;
        cum_ask_size_ = 0;
        book_update_count_ = 0;
        pending_bid_order_id_ = 0;
        pending_ask_order_id_ = 0;
        last_bid_price_ = 0;
        last_ask_price_ = 0;
        last_mid_price_ = 0;
        last_spread_ = 0;
        kappa_calibrator_.reset();
    }

    const Inventory& inventory() const { return inventory_; }
    Inventory& inventory() { return inventory_; }
    double current_volatility() const { return volatility_.volatility(); }
    const AvellanedaStoikov& as_model() const { return as_model_; }
    AvellanedaStoikov& as_model() { return as_model_; }
    const QuoteEngine& quote_engine() const { return quote_engine_; }
    QuoteEngine& quote_engine() { return quote_engine_; }
    const ASRiskManager& risk_manager() const { return risk_; }
    ASRiskManager& risk_manager() { return risk_; }
    const VolatilityEstimator& volatility_estimator() const { return volatility_; }
    VolatilityEstimator& volatility_estimator() { return volatility_; }
    const FillModel& fill_model() const { return *fill_model_; }
    int64_t max_position() const { return max_position_; }
    uint64_t order_size() const { return order_size_; }
    int64_t last_bid_price() const { return last_bid_price_; }
    int64_t last_ask_price() const { return last_ask_price_; }
    double tick_rate() const { return tick_rate_per_sec_; }
    KappaCalibrator& kappa_calibrator() { return kappa_calibrator_; }
    const KappaCalibrator& kappa_calibrator() const { return kappa_calibrator_; }

private:
    void update_tick_rate(uint64_t timestamp_us) {
        if (last_tick_time_us_ == std::numeric_limits<uint64_t>::max()) {
            last_tick_time_us_ = timestamp_us;
            return;
        }
        tick_count_++;
        uint64_t elapsed = timestamp_us - last_tick_time_us_;
        if (elapsed >= 1000000) {
            tick_rate_per_sec_ = static_cast<double>(tick_count_)
                               / (static_cast<double>(elapsed) / 1000000.0);
            tick_count_ = 0;
            last_tick_time_us_ = timestamp_us;
        }
    }

    void update_book_stats(const hft::MarketTick& tick) {
        cum_bid_size_ += tick.bid_size;
        cum_ask_size_ += tick.ask_size;
        book_update_count_++;
        if (book_update_count_ >= 1000) {
            cum_bid_size_ /= 2;
            cum_ask_size_ /= 2;
            book_update_count_ = 500;
        }
    }

    Inventory inventory_;
    AvellanedaStoikov as_model_;
    QuoteEngine quote_engine_;
    VolatilityEstimator volatility_;
    ASRiskManager risk_;
    std::unique_ptr<FillModel> fill_model_;
    std::unique_ptr<LatencyModel> latency_;

    int64_t max_position_;
    uint64_t order_size_;
    int64_t tick_size_;
    uint64_t quote_interval_us_;

    uint64_t last_tick_time_us_;
    uint64_t tick_count_;
    double tick_rate_per_sec_;
    uint64_t cum_bid_size_;
    uint64_t cum_ask_size_;
    uint64_t book_update_count_;

    KappaCalibrator kappa_calibrator_;

    uint64_t pending_bid_order_id_ = 0;
    uint64_t pending_ask_order_id_ = 0;
    int64_t last_bid_price_ = 0;
    int64_t last_ask_price_ = 0;
    int64_t last_mid_price_ = 0;
    int64_t last_spread_ = 0;
};

} // namespace mm
