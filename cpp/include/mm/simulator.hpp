#pragma once

#include "hft/matching_engine.hpp"
#include "hft/market_data_feed.hpp"
#include "mm/strategy.hpp"
#include <memory>
#include <vector>
#include <string>
#include <random>

namespace mm {

struct SimSnapshot {
    uint64_t tick = 0;
    uint64_t timestamp = 0;
    int64_t mid_price = 0;
    int64_t bid_price = 0;
    int64_t ask_price = 0;
    int64_t inventory = 0;
    int64_t cash = 0;
    int64_t realized_pnl = 0;
    int64_t unrealized_pnl = 0;
    int64_t total_pnl = 0;
    uint64_t trade_count = 0;
    double volatility = 0.0;
    double reservation_price = 0.0;
    double half_spread = 0.0;
};

struct SimResult {
    std::vector<SimSnapshot> snapshots;
    std::vector<hft::Trade> trades;

    void clear() {
        snapshots.clear();
        trades.clear();
    }

    size_t total_ticks() const { return snapshots.size(); }
    size_t total_trades() const { return trades.size(); }
};

class Simulator {
public:
    Simulator(
        std::unique_ptr<hft::MarketDataFeed> feed,
        std::unique_ptr<ASMarketMaker> strategy,
        bool fill_simulation_mode = false
    )   : feed_(std::move(feed))
        , strategy_(std::move(strategy))
        , engine_(hft::LadderConfig{0, 200000, 1}, 1 << 22)
        , fill_simulation_mode_(fill_simulation_mode)
        , snapshot_interval_(1)
        , order_id_(1)
        , client_id_(1)
        , gen_(std::random_device{}())
        , dist_(0.0, 1.0)
    {}

    SimResult run() {
        SimResult result;
        engine_.reset();
        strategy_->reset();
        order_id_ = 1;

        engine_.set_trade_callback([this, &result](const hft::Trade& trade) {
            strategy_->on_trade(trade);
            result.trades.push_back(trade);
        });

        uint64_t trade_seq = 1;
        uint64_t tick_num = 0;

        while (feed_->has_next()) {
            hft::MarketTick tick = feed_->next();
            tick_num++;

            auto orders = strategy_->on_tick(tick, order_id_, client_id_);

            if (fill_simulation_mode_) {
                for (const auto& order : orders) {
                    FillModelState state = build_state(tick, order);
                    double prob = strategy_->fill_model().prob_fill(
                        order.side, order.price, state
                    );
                    if (dist_(gen_) < prob) {
                        hft::Trade trade(
                            trade_seq++,          // trade_id
                            0,                     // sequence
                            order.order_id,        // order_id
                            0,                     // counter_order_id
                            tick.timestamp,        // timestamp
                            order.price,           // price
                            order.quantity,        // quantity
                            order.side             // side
                        );
                        strategy_->on_trade(trade);
                        result.trades.push_back(trade);
                    }
                }
                order_id_ += 2;
            } else {
                for (const auto& order : orders) {
                    auto trades = engine_.process_order(order);
                    order_id_ += 2;
                    (void)trades;
                }
            }

            if (tick_num % snapshot_interval_ == 0) {
                SimSnapshot snap;
                snap.tick = tick_num;
                snap.timestamp = tick.timestamp;
                snap.mid_price = tick.mid_price();
                snap.bid_price = strategy_->last_bid_price();
                snap.ask_price = strategy_->last_ask_price();
                snap.inventory = strategy_->inventory().position;
                snap.cash = strategy_->inventory().cash;
                snap.realized_pnl = strategy_->inventory().realized_pnl;
                snap.unrealized_pnl = strategy_->inventory().unrealized_pnl;
                snap.total_pnl = strategy_->inventory().total_pnl();
                snap.trade_count = strategy_->inventory().trade_count;
                snap.volatility = strategy_->current_volatility();
                snap.reservation_price = strategy_->as_model().reservation_price(
                    tick.mid_price(),
                    static_cast<double>(strategy_->inventory().position)
                        / static_cast<double>(strategy_->max_position()),
                    strategy_->current_volatility(),
                    strategy_->as_model().time_horizon()
                );
                snap.half_spread = strategy_->quote_engine().generate_quote(
                    snap.reservation_price, 0.0, tick.timestamp
                ).ask_price - snap.mid_price;
                result.snapshots.push_back(snap);
            }

            client_id_++;
        }

        return result;
    }

    void set_snapshot_interval(uint64_t interval) {
        snapshot_interval_ = interval > 0 ? interval : 1;
    }

    void set_fill_simulation_mode(bool enabled) {
        fill_simulation_mode_ = enabled;
    }

    bool fill_simulation_mode() const { return fill_simulation_mode_; }

    ASMarketMaker& strategy() { return *strategy_; }
    const ASMarketMaker& strategy() const { return *strategy_; }
    hft::MatchingEngine& engine() { return engine_; }
    const hft::MatchingEngine& engine() const { return engine_; }

private:
    FillModelState build_state(const hft::MarketTick& tick, const hft::Order& order) const {
        FillModelState state;
        state.mid_price = tick.mid_price();
        state.spread = tick.spread();
        double tr = strategy_->tick_rate();
        state.arrival_rate = tr > 0.0 ? tr : 100.0;
        state.cancel_rate = state.arrival_rate * 0.5;
        state.depth_ahead = 0;
        state.level_depth = 1000;
        state.level_price = (order.side == hft::Side::Buy)
            ? tick.bid_price : tick.ask_price;
        state.order_flow_imbalance = 0.0;
        state.queue_imbalance = static_cast<double>(tick.bid_size)
            / static_cast<double>(tick.bid_size + tick.ask_size + 1);
        state.time_horizon = 1.0;
        return state;
    }

    std::unique_ptr<hft::MarketDataFeed> feed_;
    std::unique_ptr<ASMarketMaker> strategy_;
    hft::MatchingEngine engine_;
    bool fill_simulation_mode_;
    uint64_t snapshot_interval_;
    uint64_t order_id_;
    uint64_t client_id_;
    std::mt19937 gen_;
    std::uniform_real_distribution<double> dist_;
};

} // namespace mm
