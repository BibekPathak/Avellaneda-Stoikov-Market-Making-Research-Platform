#pragma once

#include "mm/simulator.hpp"
#include <cmath>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <iomanip>

namespace mm {

struct PnLAttribution {
    int64_t spread_capture = 0;
    int64_t inventory_pnl = 0;
    int64_t adverse_selection = 0;
    int64_t total_realized = 0;
    int64_t total_unrealized = 0;
    int64_t total = 0;
};

struct TradeStats {
    uint64_t total_trades = 0;
    uint64_t buy_trades = 0;
    uint64_t sell_trades = 0;
    uint64_t winning_trades = 0;
    uint64_t losing_trades = 0;
    double win_rate = 0.0;
    int64_t gross_profit = 0;
    int64_t gross_loss = 0;
    double profit_factor = 0.0;
    double avg_profit = 0.0;
    double avg_loss = 0.0;
    double avg_holding_ticks = 0.0;
};

struct RiskStats {
    double sharpe = 0.0;
    double sortino = 0.0;
    double max_drawdown = 0.0;
    double max_drawdown_pct = 0.0;
    double volatility = 0.0;
    double avg_volatility = 0.0;
};

struct FillStats {
    uint64_t total_quotes = 0;
    uint64_t total_fills = 0;
    double fill_rate = 0.0;
    double avg_fill_price = 0.0;
    int64_t max_fill_price = 0;
    int64_t min_fill_price = 0;
};

struct SummaryStats {
    PnLAttribution pnl;
    TradeStats trades;
    RiskStats risk;
    FillStats fills;
    int64_t final_inventory = 0;
    uint64_t total_ticks = 0;
    double final_mid_price = 0.0;
};

class Analytics {
public:
    Analytics(const SimResult& result, int64_t capital = 10000000)
        : result_(result)
        , capital_(capital)
    {}

    PnLAttribution compute_pnl_attribution() const {
        PnLAttribution pnl;
        if (result_.snapshots.empty()) return pnl;

        const auto& last = result_.snapshots.back();
        pnl.total_realized = last.realized_pnl;
        pnl.total_unrealized = last.unrealized_pnl;
        pnl.total = last.total_pnl;

        pnl.inventory_pnl = compute_inventory_pnl();
        pnl.adverse_selection = compute_adverse_selection();
        pnl.spread_capture = pnl.total_realized - pnl.inventory_pnl;

        return pnl;
    }

    TradeStats compute_trade_stats() const {
        TradeStats ts;
        if (result_.trades.empty()) return ts;

        ts.total_trades = result_.trades.size();
        std::vector<double> pnl_per_trade;
        pnl_per_trade.reserve(ts.total_trades);

        for (size_t i = 0; i < result_.trades.size(); i++) {
            const auto& t = result_.trades[i];
            if (t.side == hft::Side::Buy) {
                ts.buy_trades++;
            } else {
                ts.sell_trades++;
            }

            int64_t trade_pnl = (t.side == hft::Side::Sell ? 1 : -1)
                              * static_cast<int64_t>(t.quantity)
                              * t.price;
            if (trade_pnl > 0) {
                ts.winning_trades++;
                ts.gross_profit += trade_pnl;
            } else if (trade_pnl < 0) {
                ts.losing_trades++;
                ts.gross_loss += trade_pnl;
            }
            pnl_per_trade.push_back(static_cast<double>(trade_pnl));
        }

        ts.win_rate = ts.total_trades > 0
            ? static_cast<double>(ts.winning_trades) / static_cast<double>(ts.total_trades)
            : 0.0;

        ts.profit_factor = ts.gross_loss != 0
            ? static_cast<double>(ts.gross_profit) / static_cast<double>(-ts.gross_loss)
            : (ts.gross_profit > 0 ? 999.0 : 0.0);

        ts.avg_profit = ts.winning_trades > 0
            ? static_cast<double>(ts.gross_profit) / static_cast<double>(ts.winning_trades)
            : 0.0;

        ts.avg_loss = ts.losing_trades > 0
            ? static_cast<double>(-ts.gross_loss) / static_cast<double>(ts.losing_trades)
            : 0.0;

        return ts;
    }

    RiskStats compute_risk_stats() const {
        RiskStats rs;
        if (result_.snapshots.size() < 2) return rs;

        std::vector<double> returns;
        returns.reserve(result_.snapshots.size());

        std::vector<double> pnl_series;
        pnl_series.reserve(result_.snapshots.size());

        double peak_pnl = -1e18;
        double max_dd = 0.0;

        for (const auto& snap : result_.snapshots) {
            double cur = static_cast<double>(snap.total_pnl);
            pnl_series.push_back(cur);

            if (cur > peak_pnl) peak_pnl = cur;
            double dd = peak_pnl - cur;
            if (dd > max_dd) max_dd = dd;

            if (returns.empty()) {
                returns.push_back(0.0);
            } else {
                returns.push_back(cur - pnl_series[pnl_series.size() - 2]);
            }

            if (snap.volatility > rs.avg_volatility) {
                rs.avg_volatility = snap.volatility;
            }
        }

        size_t n = returns.size();
        if (n < 2) return rs;

        double sum = std::accumulate(returns.begin(), returns.end(), 0.0);
        double mean = sum / static_cast<double>(n);

        double sum_sq = 0.0;
        double sum_sq_neg = 0.0;
        for (size_t i = 1; i < n; i++) {
            double diff = returns[i] - mean;
            sum_sq += diff * diff;
            if (returns[i] < 0) {
                sum_sq_neg += returns[i] * returns[i];
            }
        }

        double stddev = std::sqrt(sum_sq / static_cast<double>(n - 1));
        rs.volatility = stddev;

        if (stddev > 0) {
            rs.sharpe = mean / stddev * std::sqrt(252.0);
        }

        double semi_stddev = n > 1 ? std::sqrt(sum_sq_neg / static_cast<double>(n - 1)) : 0.0;
        if (semi_stddev > 0) {
            rs.sortino = mean / semi_stddev * std::sqrt(252.0);
        }

        rs.max_drawdown = max_dd;
        if (peak_pnl > 0) {
            rs.max_drawdown_pct = max_dd / peak_pnl;
        }

        return rs;
    }

    FillStats compute_fill_stats() const {
        FillStats fs;
        fs.total_quotes = result_.snapshots.size();
        fs.total_fills = result_.trades.size();

        if (fs.total_quotes > 0) {
            fs.fill_rate = static_cast<double>(fs.total_fills)
                         / static_cast<double>(fs.total_quotes);
        }

        int64_t sum_price = 0;
        int64_t max_p = 0;
        int64_t min_p = std::numeric_limits<int64_t>::max();

        for (const auto& t : result_.trades) {
            sum_price += t.price;
            if (t.price > max_p) max_p = t.price;
            if (t.price < min_p) min_p = t.price;
        }

        if (fs.total_fills > 0) {
            fs.avg_fill_price = static_cast<double>(sum_price) / static_cast<double>(fs.total_fills);
            fs.max_fill_price = max_p;
            fs.min_fill_price = min_p;
        } else {
            fs.min_fill_price = 0;
        }

        return fs;
    }

    int64_t compute_adverse_selection(size_t lookahead = 5) const {
        if (result_.trades.empty() || result_.snapshots.size() < lookahead + 1) {
            return 0;
        }

        int64_t adverse = 0;

        for (const auto& trade : result_.trades) {
            uint64_t trade_seq = trade.sequence;
            size_t fill_idx = 0;
            for (size_t i = 0; i < result_.snapshots.size(); i++) {
                if (result_.snapshots[i].timestamp >= trade.timestamp) {
                    fill_idx = i;
                    break;
                }
            }

            size_t later_idx = std::min(fill_idx + lookahead, result_.snapshots.size() - 1);
            int64_t mid_at_fill = result_.snapshots[fill_idx].mid_price;
            int64_t mid_later = result_.snapshots[later_idx].mid_price;

            if (trade.side == hft::Side::Buy) {
                if (mid_later < mid_at_fill) {
                    int64_t loss = static_cast<int64_t>(trade.quantity) * (mid_at_fill - mid_later);
                    adverse += loss;
                }
            } else {
                if (mid_later > mid_at_fill) {
                    int64_t loss = static_cast<int64_t>(trade.quantity) * (mid_later - mid_at_fill);
                    adverse += loss;
                }
            }
        }

        return adverse;
    }

    int64_t compute_inventory_pnl() const {
        if (result_.snapshots.empty()) return 0;
        const auto& first = result_.snapshots.front();
        const auto& last = result_.snapshots.back();

        int64_t avg_pos = (first.inventory + last.inventory) / 2;
        int64_t price_change = last.mid_price - first.mid_price;
        return avg_pos * price_change;
    }

    SummaryStats compute_summary() const {
        SummaryStats ss;
        ss.pnl = compute_pnl_attribution();
        ss.trades = compute_trade_stats();
        ss.risk = compute_risk_stats();
        ss.fills = compute_fill_stats();

        if (!result_.snapshots.empty()) {
            const auto& last = result_.snapshots.back();
            ss.final_inventory = last.inventory;
            ss.total_ticks = last.tick;
            ss.final_mid_price = static_cast<double>(last.mid_price);
        }

        return ss;
    }

    void print_summary(std::ostream& os = std::cout) const {
        auto ss = compute_summary();

        os << "\n=== Simulation Summary ===\n";
        os << "Total ticks:  " << ss.total_ticks << "\n";
        os << "Final mid:    " << ss.final_mid_price << "\n";
        os << "Final inv:    " << ss.final_inventory << "\n\n";

        os << "--- PnL ---\n";
        os << "  Total:        " << ss.pnl.total << "\n";
        os << "  Realized:     " << ss.pnl.total_realized << "\n";
        os << "  Unrealized:   " << ss.pnl.total_unrealized << "\n";
        os << "  Spread Cap:   " << ss.pnl.spread_capture << "\n";
        os << "  Inventory:    " << ss.pnl.inventory_pnl << "\n";
        os << "  Adverse Sel:  " << ss.pnl.adverse_selection << "\n\n";

        os << "--- Trades ---\n";
        os << "  Count:        " << ss.trades.total_trades << "\n";
        os << "  Win Rate:     " << (ss.trades.win_rate * 100.0) << "%\n";
        os << "  Profit Fact:  " << ss.trades.profit_factor << "\n";
        os << "  Avg Win:      " << ss.trades.avg_profit << "\n";
        os << "  Avg Loss:     " << ss.trades.avg_loss << "\n\n";

        os << "--- Risk ---\n";
        os << "  Sharpe:       " << ss.risk.sharpe << "\n";
        os << "  Sortino:      " << ss.risk.sortino << "\n";
        os << "  Max DD:       " << ss.risk.max_drawdown << "\n";
        os << "  Max DD%:      " << (ss.risk.max_drawdown_pct * 100.0) << "%\n";
        os << "  Volatility:   " << ss.risk.volatility << "\n\n";

        os << "--- Fills ---\n";
        os << "  Quotes:       " << ss.fills.total_quotes << "\n";
        os << "  Fills:        " << ss.fills.total_fills << "\n";
        os << "  Fill Rate:    " << (ss.fills.fill_rate * 100.0) << "%\n";
        os << "  Avg Price:    " << ss.fills.avg_fill_price << "\n";
        os << "  Max Price:    " << ss.fills.max_fill_price << "\n";
        os << "  Min Price:    " << ss.fills.min_fill_price << "\n";
    }

private:
    const SimResult& result_;
    int64_t capital_;
};

} // namespace mm
