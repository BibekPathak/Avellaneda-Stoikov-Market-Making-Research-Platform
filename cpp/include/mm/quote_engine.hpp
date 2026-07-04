#pragma once

#include "mm/avellaneda_stoikov.hpp"
#include <cstdint>
#include <cmath>

namespace mm {

struct Quote {
    int64_t bid_price = 0;
    int64_t ask_price = 0;
    uint64_t bid_size = 0;
    uint64_t ask_size = 0;
    uint64_t timestamp = 0;
    uint64_t sequence = 0;
};

class QuoteEngine {
public:
    QuoteEngine(
        int64_t tick_size = 1,
        uint64_t quote_interval_us = 100000,
        uint64_t order_size = 1000
    )   : tick_size_(tick_size > 0 ? tick_size : 1)
        , quote_interval_us_(quote_interval_us)
        , order_size_(order_size)
        , min_spread_(tick_size_ * 2)
        , last_quote_time_us_(0)
        , sequence_(0)
    {}

    Quote generate_quote(
        double reservation_price,
        double half_spread,
        uint64_t timestamp_us
    ) {
        if (timestamp_us > last_quote_time_us_) {
            last_quote_time_us_ = timestamp_us;
        }
        sequence_++;

        int64_t raw_bid = snap_to_tick(reservation_price - half_spread);
        int64_t raw_ask = snap_to_tick(reservation_price + half_spread);

        int64_t spread = raw_ask - raw_bid;
        if (spread < min_spread_) {
            int64_t adjustment = (min_spread_ - spread) / 2;
            raw_bid -= adjustment;
            raw_ask += adjustment;
            int64_t remain = (min_spread_ - (raw_ask - raw_bid));
            raw_ask += remain;
        }

        Quote q;
        q.bid_price = raw_bid;
        q.ask_price = raw_ask;
        q.bid_size = order_size_;
        q.ask_size = order_size_;
        q.timestamp = timestamp_us;
        q.sequence = sequence_;
        return q;
    }

    Quote generate_quote(const ASQuote& as_quote, uint64_t timestamp_us) {
        return generate_quote(as_quote.reservation_price, as_quote.half_spread, timestamp_us);
    }

    bool should_quote(uint64_t timestamp_us) const {
        if (last_quote_time_us_ == 0) return true;
        if (timestamp_us < last_quote_time_us_) return true;
        return timestamp_us >= last_quote_time_us_ + quote_interval_us_;
    }

    void set_tick_size(int64_t tick_size) {
        tick_size_ = tick_size > 0 ? tick_size : 1;
    }

    void set_quote_interval(uint64_t interval_us) {
        quote_interval_us_ = interval_us;
    }

    void set_order_size(uint64_t size) {
        order_size_ = size;
    }

    void set_min_spread(int64_t min_spread) {
        min_spread_ = min_spread > 0 ? min_spread : tick_size_;
    }

    int64_t tick_size() const { return tick_size_; }
    uint64_t quote_interval() const { return quote_interval_us_; }
    uint64_t order_size() const { return order_size_; }
    int64_t min_spread() const { return min_spread_; }
    uint64_t last_quote_time() const { return last_quote_time_us_; }
    uint64_t sequence() const { return sequence_; }

    void reset() {
        last_quote_time_us_ = 0;
        sequence_ = 0;
    }

private:
    int64_t snap_to_tick(double price) const {
        double snapped = std::round(price / static_cast<double>(tick_size_)) * tick_size_;
        return static_cast<int64_t>(snapped);
    }

    int64_t tick_size_;
    uint64_t quote_interval_us_;
    uint64_t order_size_;
    int64_t min_spread_;
    uint64_t last_quote_time_us_;
    uint64_t sequence_;
};

} // namespace mm
