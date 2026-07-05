#pragma once

#include "mm/analytics.hpp"
#include <sstream>
#include <iomanip>

namespace mm {

class Dashboard {
public:
    explicit Dashboard(const Analytics& analytics)
        : analytics_(analytics)
        , ss_(analytics.compute_summary())
    {}

    std::string text() const {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2);

        out << "\n==================== RISK DASHBOARD ====================\n\n";

        out << "  PnL\n";
        out << "    Total         " << pad(ss_.pnl.total, 12) << "\n";
        out << "    Realized      " << pad(ss_.pnl.total_realized, 12) << "\n";
        out << "    Unrealized    " << pad(ss_.pnl.total_unrealized, 12) << "\n";
        out << "    Spread Capt.  " << pad(ss_.pnl.spread_capture, 12) << "\n";
        out << "    Inventory     " << pad(ss_.pnl.inventory_pnl, 12) << "\n";
        out << "    Adverse Sel.  " << pad(ss_.pnl.adverse_selection, 12) << "\n\n";

        out << "  Inventory\n";
        out << "    Position      " << pad(ss_.final_inventory, 12) << "\n";
        out << "    Max Position  " << pad(static_cast<int64_t>(0), 12) << "\n\n";

        out << "  Trades\n";
        out << "    Count         " << pad(static_cast<int64_t>(ss_.trades.total_trades), 12) << "\n";
        out << "    Win Rate      " << pad(ss_.trades.win_rate * 100.0, 10) << "%\n";
        out << "    Profit Fact.  " << pad(ss_.trades.profit_factor, 10) << "\n\n";

        out << "  Risk\n";
        out << "    Sharpe        " << pad(ss_.risk.sharpe, 10) << "\n";
        out << "    Sortino       " << pad(ss_.risk.sortino, 10) << "\n";
        out << "    Max DD        " << pad(ss_.risk.max_drawdown, 12) << "\n";
        out << "    Max DD%       " << pad(ss_.risk.max_drawdown_pct * 100.0, 10) << "%\n";
        out << "    Volatility    " << pad(ss_.risk.volatility, 10) << "\n\n";

        out << "  Fills\n";
        out << "    Quotes        " << pad(static_cast<int64_t>(ss_.fills.total_quotes), 12) << "\n";
        out << "    Fills         " << pad(static_cast<int64_t>(ss_.fills.total_fills), 12) << "\n";
        out << "    Fill Rate     " << pad(ss_.fills.fill_rate * 100.0, 10) << "%\n";
        out << "    Avg Price     " << pad(ss_.fills.avg_fill_price, 10) << "\n\n";

        out << "==========================================================\n";
        return out.str();
    }

    std::string json() const {
        std::ostringstream out;
        out << std::fixed << std::setprecision(4);

        out << "{\n";
        out << "  \"pnl\": {\n";
        out << "    \"total\": " << ss_.pnl.total << ",\n";
        out << "    \"realized\": " << ss_.pnl.total_realized << ",\n";
        out << "    \"unrealized\": " << ss_.pnl.total_unrealized << ",\n";
        out << "    \"spread_capture\": " << ss_.pnl.spread_capture << ",\n";
        out << "    \"inventory_pnl\": " << ss_.pnl.inventory_pnl << ",\n";
        out << "    \"adverse_selection\": " << ss_.pnl.adverse_selection << "\n";
        out << "  },\n";
        out << "  \"inventory\": {\n";
        out << "    \"position\": " << ss_.final_inventory << "\n";
        out << "  },\n";
        out << "  \"trades\": {\n";
        out << "    \"count\": " << ss_.trades.total_trades << ",\n";
        out << "    \"win_rate\": " << ss_.trades.win_rate << ",\n";
        out << "    \"profit_factor\": " << ss_.trades.profit_factor << "\n";
        out << "  },\n";
        out << "  \"risk\": {\n";
        out << "    \"sharpe\": " << ss_.risk.sharpe << ",\n";
        out << "    \"sortino\": " << ss_.risk.sortino << ",\n";
        out << "    \"max_drawdown\": " << ss_.risk.max_drawdown << ",\n";
        out << "    \"max_drawdown_pct\": " << ss_.risk.max_drawdown_pct << ",\n";
        out << "    \"volatility\": " << ss_.risk.volatility << "\n";
        out << "  },\n";
        out << "  \"fills\": {\n";
        out << "    \"quotes\": " << ss_.fills.total_quotes << ",\n";
        out << "    \"fills\": " << ss_.fills.total_fills << ",\n";
        out << "    \"fill_rate\": " << ss_.fills.fill_rate << ",\n";
        out << "    \"avg_fill_price\": " << ss_.fills.avg_fill_price << "\n";
        out << "  }\n";
        out << "}\n";
        return out.str();
    }

private:
    static std::string pad(int64_t val, int width) {
        std::ostringstream out;
        out << std::setw(width) << val;
        return out.str();
    }

    static std::string pad(double val, int width) {
        std::ostringstream out;
        out << std::fixed << std::setprecision(2) << std::setw(width) << val;
        return out.str();
    }

    const Analytics& analytics_;
    SummaryStats ss_;
};

} // namespace mm
