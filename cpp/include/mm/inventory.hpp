#pragma once

#include "hft/order.hpp"
#include <cstdint>
#include <cstdlib>

namespace mm {

struct Inventory {
    int64_t cash = 0;
    int64_t position = 0;
    int64_t realized_pnl = 0;
    int64_t unrealized_pnl = 0;
    int64_t avg_entry_price = 0;
    uint64_t trade_count = 0;
    uint64_t buy_volume = 0;
    uint64_t sell_volume = 0;

    void fill(hft::Side side, int64_t price, uint64_t quantity) {
        trade_count++;
        int64_t qty = static_cast<int64_t>(quantity);

        if (side == hft::Side::Buy) {
            cash -= price * qty;
            buy_volume += quantity;

            if (position >= 0) {
                int64_t total_qty = position + qty;
                avg_entry_price = total_qty > 0
                    ? (avg_entry_price * position + price * qty) / total_qty
                    : 0;
            } else {
                int64_t covered = std::min(qty, -position);
                realized_pnl += (avg_entry_price - price) * covered;
                int64_t new_pos = position + qty;

                if (new_pos > 0) {
                    avg_entry_price = price;
                } else if (new_pos == 0) {
                    avg_entry_price = 0;
                }
            }
            position += qty;

        } else {
            cash += price * qty;
            sell_volume += quantity;

            if (position <= 0) {
                int64_t abs_pos = -position;
                int64_t total_qty = abs_pos + qty;
                avg_entry_price = total_qty > 0
                    ? (avg_entry_price * abs_pos + price * qty) / total_qty
                    : 0;
            } else {
                int64_t covered = std::min(qty, position);
                realized_pnl += (price - avg_entry_price) * covered;
                int64_t new_pos = position - qty;

                if (new_pos < 0) {
                    avg_entry_price = price;
                } else if (new_pos == 0) {
                    avg_entry_price = 0;
                }
            }
            position -= qty;
        }
    }

    void mark_to_market(int64_t mid_price) {
        if (position == 0) {
            unrealized_pnl = 0;
        } else {
            unrealized_pnl = position * (mid_price - avg_entry_price);
        }
    }

    int64_t total_pnl() const {
        return realized_pnl + unrealized_pnl;
    }

    uint64_t exposure() const {
        return static_cast<uint64_t>(std::llabs(position));
    }

    void reset() {
        cash = 0;
        position = 0;
        realized_pnl = 0;
        unrealized_pnl = 0;
        avg_entry_price = 0;
        trade_count = 0;
        buy_volume = 0;
        sell_volume = 0;
    }
};

} // namespace mm
