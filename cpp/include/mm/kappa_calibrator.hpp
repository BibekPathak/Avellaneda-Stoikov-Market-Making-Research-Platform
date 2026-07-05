#pragma once

#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>

namespace mm {

class KappaCalibrator {
public:
    KappaCalibrator(
        size_t num_buckets = 20,
        double min_kappa = 1.0,
        double max_kappa = 1000.0,
        double base_kappa = 10.0,
        double ewma_alpha = 0.3
    )   : num_buckets_(num_buckets)
        , min_kappa_(min_kappa)
        , max_kappa_(max_kappa)
        , base_kappa_(base_kappa)
        , alpha_(ewma_alpha)
        , bucket_width_(1.0 / static_cast<double>(num_buckets))
        , buckets_(num_buckets, Bucket{0.0, 0ULL, 0ULL})
    {}

    void record_quote(double distance, bool filled) {
        int idx = distance_to_bucket(distance);
        if (idx < 0 || idx >= static_cast<int>(num_buckets_)) return;

        Bucket& b = buckets_[static_cast<size_t>(idx)];
        b.total_quotes++;

        double new_ratio;
        if (filled) {
            b.total_fills++;
            new_ratio = 1.0;
        } else {
            new_ratio = 0.0;
        }

        if (b.fill_ratio_ewma == 0.0 && b.total_quotes > 0) {
            b.fill_ratio_ewma = new_ratio;
        } else if (b.total_quotes > 0) {
            b.fill_ratio_ewma = alpha_ * new_ratio + (1.0 - alpha_) * b.fill_ratio_ewma;
        }
    }

    double kappa(double distance) const {
        int idx = distance_to_bucket(distance);
        if (idx < 0 || idx >= static_cast<int>(num_buckets_)) {
            return base_kappa_;
        }

        const Bucket& b = buckets_[static_cast<size_t>(idx)];
        if (b.total_quotes == 0) return base_kappa_;

        double k = b.fill_ratio_ewma * base_kappa_;
        return std::max(min_kappa_, std::min(max_kappa_, k));
    }

    void reset() {
        for (auto& b : buckets_) {
            b.fill_ratio_ewma = 0.0;
            b.total_fills = 0;
            b.total_quotes = 0;
        }
    }

    struct Bucket {
        double fill_ratio_ewma = 0.0;
        uint64_t total_fills = 0;
        uint64_t total_quotes = 0;
    };

    const Bucket& bucket(size_t i) const { return buckets_[i]; }
    size_t num_buckets() const { return num_buckets_; }
    double min_kappa() const { return min_kappa_; }
    double max_kappa() const { return max_kappa_; }
    double base_kappa() const { return base_kappa_; }
    double alpha() const { return alpha_; }

private:
    int distance_to_bucket(double distance) const {
        if (distance < 0.0 || distance >= 1.0) return -1;
        return static_cast<int>(distance / bucket_width_);
    }

    size_t num_buckets_;
    double min_kappa_;
    double max_kappa_;
    double base_kappa_;
    double alpha_;
    double bucket_width_;
    std::vector<Bucket> buckets_;
};

} // namespace mm
