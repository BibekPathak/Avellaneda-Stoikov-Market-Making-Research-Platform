#pragma once

#include <cstdint>
#include <cmath>
#include <vector>
#include <deque>
#include <numeric>

namespace mm {

enum class VolatilityMethod {
    RollingStd,
    EWMA,
    Parkinson,
    Realized
};

class VolatilityEstimator {
public:
    VolatilityEstimator(
        VolatilityMethod method = VolatilityMethod::RollingStd,
        size_t window = 20,
        double lambda = 0.94
    )   : method_(method)
        , window_(window)
        , lambda_(lambda)
        , ewma_variance_(0.0)
        , ewma_initialized_(false)
        , count_(0)
    {}

    void add_price(int64_t price) {
        double p = static_cast<double>(price);
        if (!prices_.empty()) {
            double ret = std::log(p / prices_.back());
            if (std::isfinite(ret)) {
                returns_.push_back(ret);
                if (returns_.size() > window_) {
                    returns_.pop_front();
                }
                update_ewma(ret);
            }
        }
        prices_.push_back(p);
        if (prices_.size() > window_ + 1) {
            prices_.pop_front();
        }
        count_++;
    }

    void add_ohlc(int64_t high, int64_t low) {
        double h = static_cast<double>(high);
        double l = static_cast<double>(low);
        if (l > 0.0 && h >= l) {
            double parkinson_var = std::log(h / l);
            parkinson_var = parkinson_var * parkinson_var / (4.0 * std::log(2.0));
            parkinson_estimates_.push_back(std::sqrt(parkinson_var));
            if (parkinson_estimates_.size() > window_) {
                parkinson_estimates_.pop_front();
            }
        }
        count_++;
    }

    double volatility() const {
        switch (method_) {
            case VolatilityMethod::RollingStd:
                return compute_rolling_std();
            case VolatilityMethod::EWMA:
                return compute_ewma();
            case VolatilityMethod::Parkinson:
                return compute_parkinson();
            case VolatilityMethod::Realized:
                return compute_realized();
        }
        return 0.0;
    }

    double price_volatility(int64_t price) const {
        return volatility() * static_cast<double>(price);
    }

    void reset() {
        prices_.clear();
        returns_.clear();
        parkinson_estimates_.clear();
        ewma_variance_ = 0.0;
        ewma_initialized_ = false;
        count_ = 0;
    }

    void set_method(VolatilityMethod method) { method_ = method; }
    void set_window(size_t window) {
        window_ = window;
        while (returns_.size() > window_) returns_.pop_front();
        while (parkinson_estimates_.size() > window_) parkinson_estimates_.pop_front();
        while (prices_.size() > window_ + 1) prices_.pop_front();
    }
    void set_lambda(double lambda) { lambda_ = lambda; }

    VolatilityMethod method() const { return method_; }
    size_t window() const { return window_; }
    double lambda() const { return lambda_; }
    size_t samples() const { return count_; }

private:
    double compute_rolling_std() const {
        if (returns_.size() < 2) return 0.0;
        size_t n = std::min(returns_.size(), window_);
        if (n < 2) return 0.0;
        double sum = 0.0;
        double sum_sq = 0.0;
        auto it = returns_.end();
        for (size_t i = 0; i < n; i++) {
            --it;
            sum += *it;
            sum_sq += *it * *it;
        }
        double mean = sum / static_cast<double>(n);
        double variance = (sum_sq / static_cast<double>(n)) - (mean * mean);
        return variance > 0.0 ? std::sqrt(variance) : 0.0;
    }

    double compute_ewma() const {
        if (!ewma_initialized_) return 0.0;
        return std::sqrt(ewma_variance_);
    }

    double compute_parkinson() const {
        if (parkinson_estimates_.empty()) return 0.0;
        size_t n = std::min(parkinson_estimates_.size(), window_);
        if (n == 0) return 0.0;
        double sum = 0.0;
        auto it = parkinson_estimates_.end();
        for (size_t i = 0; i < n; i++) {
            --it;
            sum += *it;
        }
        return sum / static_cast<double>(n);
    }

    double compute_realized() const {
        if (returns_.empty()) return 0.0;
        size_t n = std::min(returns_.size(), window_);
        if (n == 0) return 0.0;
        double sum_sq = 0.0;
        auto it = returns_.end();
        for (size_t i = 0; i < n; i++) {
            --it;
            sum_sq += *it * *it;
        }
        return std::sqrt(sum_sq);
    }

    void update_ewma(double ret) {
        if (!ewma_initialized_) {
            ewma_variance_ = ret * ret;
            ewma_initialized_ = true;
        } else {
            ewma_variance_ = lambda_ * ewma_variance_ + (1.0 - lambda_) * ret * ret;
        }
    }

    VolatilityMethod method_;
    size_t window_;
    double lambda_;

    std::deque<double> prices_;
    std::deque<double> returns_;
    std::deque<double> parkinson_estimates_;

    double ewma_variance_;
    bool ewma_initialized_;
    size_t count_;
};

} // namespace mm
