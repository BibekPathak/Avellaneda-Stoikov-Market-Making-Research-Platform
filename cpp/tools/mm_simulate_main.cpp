#include "mm/simulator.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

struct Config {
    int num_ticks = 1000;
    int64_t base_price = 50000;
    int64_t tick_size = 100;
    std::string csv_path;
    double gamma = 0.1;
    double horizon = 1.0;
    uint64_t order_size = 1000;
    uint64_t quote_interval_us = 100000;
    int64_t max_position = 10000;
    int vol_method = 0;
    size_t vol_window = 20;
    uint64_t latency_us = 0;
    std::string output_path = "results.csv";
    uint64_t snapshot_interval = 1;
    int fixed_spread = 0;
    int fill_model = 0;
    bool fill_simulation = false;
    bool show_help = false;
};

Config parse_args(int argc, char** argv) {
    Config cfg;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        auto next = [&]() { return (i + 1 < argc) ? std::string(argv[++i]) : std::string(); };

        if (arg == "--ticks") cfg.num_ticks = std::stoi(next());
        else if (arg == "--base-price") cfg.base_price = std::stoll(next());
        else if (arg == "--tick-size") cfg.tick_size = std::stoll(next());
        else if (arg == "--csv") cfg.csv_path = next();
        else if (arg == "--gamma") cfg.gamma = std::stod(next());
        else if (arg == "--horizon") cfg.horizon = std::stod(next());
        else if (arg == "--order-size") cfg.order_size = std::stoull(next());
        else if (arg == "--quote-interval") cfg.quote_interval_us = std::stoull(next());
        else if (arg == "--max-position") cfg.max_position = std::stoll(next());
        else if (arg == "--vol-method") cfg.vol_method = std::stoi(next());
        else if (arg == "--vol-window") cfg.vol_window = static_cast<size_t>(std::stoul(next()));
        else if (arg == "--latency") cfg.latency_us = std::stoull(next());
        else if (arg == "--output") cfg.output_path = next();
        else if (arg == "--snapshot-interval") cfg.snapshot_interval = std::stoull(next());
        else if (arg == "--fixed-spread") cfg.fixed_spread = std::stoi(next());
        else if (arg == "--fill-model") cfg.fill_model = std::stoi(next());
        else if (arg == "--fill-simulation") cfg.fill_simulation = true;
        else if (arg == "--help" || arg == "-h") cfg.show_help = true;
    }
    return cfg;
}

void print_help(const char* prog) {
    std::cout << "Usage: " << prog << " [options]\n"
              << "Options:\n"
              << "  --ticks <N>              Number of synthetic ticks (default: 1000)\n"
              << "  --base-price <P>         Base price (default: 50000)\n"
              << "  --tick-size <S>          Tick size (default: 100)\n"
              << "  --csv <file>             Use CSV file instead of synthetic data\n"
              << "  --gamma <G>              Risk aversion (default: 0.1)\n"
              << "  --horizon <H>            Time horizon in years (default: 1.0)\n"
              << "  --order-size <N>         Order size (default: 1000)\n"
              << "  --quote-interval <US>    Quote interval in us (default: 100000)\n"
              << "  --max-position <N>       Max position (default: 10000)\n"
              << "  --vol-method <M>         Vol method 0=RStd 1=EWMA 2=Park 3=Real (default: 0)\n"
              << "  --vol-window <N>         Volatility window (default: 20)\n"
              << "  --latency <US>           Fixed latency in us (default: 0)\n"
              << "  --fixed-spread <N>       Fixed spread (0=adaptive AS, >0=static spread)\n"
              << "  --fill-model <M>         0=SimplePoisson 1=QueuePosition (default: 0)\n"
              << "  --fill-simulation        Use FillModel instead of matching engine for fills\n"
              << "  --output <file>          Output CSV path (default: results.csv)\n"
              << "  --snapshot-interval <N>  Snapshot every N ticks (default: 1)\n"
              << "  --help                   Show this help\n";
}

mm::VolatilityMethod parse_vol_method(int m) {
    switch (m) {
        case 0: return mm::VolatilityMethod::RollingStd;
        case 1: return mm::VolatilityMethod::EWMA;
        case 2: return mm::VolatilityMethod::Parkinson;
        case 3: return mm::VolatilityMethod::Realized;
        default: return mm::VolatilityMethod::RollingStd;
    }
}

void write_csv(const std::string& path, const mm::SimResult& result) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "Error: cannot write to " << path << "\n";
        return;
    }

    out << "tick,timestamp,mid_price,bid_price,ask_price,"
        << "inventory,cash,realized_pnl,unrealized_pnl,total_pnl,"
        << "trade_count,volatility,reservation_price,half_spread\n";

    for (const auto& s : result.snapshots) {
        out << s.tick << ","
            << s.timestamp << ","
            << s.mid_price << ","
            << s.bid_price << ","
            << s.ask_price << ","
            << s.inventory << ","
            << s.cash << ","
            << s.realized_pnl << ","
            << s.unrealized_pnl << ","
            << s.total_pnl << ","
            << s.trade_count << ","
            << s.volatility << ","
            << s.reservation_price << ","
            << s.half_spread << "\n";
    }

    std::cout << "Wrote " << result.snapshots.size() << " rows to " << path << "\n";
}

int main(int argc, char** argv) {
    Config cfg = parse_args(argc, argv);

    if (cfg.show_help) {
        print_help(argv[0]);
        return 0;
    }

    auto feed = std::make_unique<hft::MarketDataFeed>();
    if (!cfg.csv_path.empty()) {
        if (!feed->load_csv(cfg.csv_path)) {
            std::cerr << "Error: cannot load " << cfg.csv_path << "\n";
            return 1;
        }
        std::cout << "Loaded " << feed->size() << " ticks from " << cfg.csv_path << "\n";
    } else {
        feed->generate_synthetic(
            static_cast<size_t>(cfg.num_ticks),
            cfg.base_price,
            cfg.tick_size
        );
        std::cout << "Generated " << cfg.num_ticks << " synthetic ticks\n";
    }

    if (cfg.fixed_spread > 0) {
        auto strategy = std::make_unique<mm::ASMarketMaker>(
            cfg.gamma, cfg.horizon, cfg.order_size, cfg.tick_size,
            cfg.quote_interval_us, parse_vol_method(cfg.vol_method),
            cfg.vol_window, cfg.max_position
        );
        strategy->emplace_latency<mm::FixedLatency>(cfg.latency_us);
        mm::Simulator sim(std::move(feed), std::move(strategy), cfg.fill_simulation);
        sim.set_snapshot_interval(cfg.snapshot_interval);
        std::cout << "Running fixed-spread simulation...\n";
        auto result = sim.run();
        std::cout << "Simulation complete:\n"
                  << "  Ticks:  " << result.total_ticks() << "\n"
                  << "  Trades: " << result.total_trades() << "\n"
                  << "  Final PnL: " << (result.snapshots.empty() ? 0 : result.snapshots.back().total_pnl) << "\n";
        write_csv(cfg.output_path, result);
        return 0;
    }

    auto strategy = std::make_unique<mm::ASMarketMaker>(
        cfg.gamma,
        cfg.horizon,
        cfg.order_size,
        cfg.tick_size,
        cfg.quote_interval_us,
        parse_vol_method(cfg.vol_method),
        cfg.vol_window,
        cfg.max_position
    );

    if (cfg.fill_model == 1) {
        strategy->set_fill_model(std::make_unique<mm::QueuePositionFill>());
    }

    if (cfg.latency_us > 0) {
        strategy->emplace_latency<mm::FixedLatency>(cfg.latency_us);
    }

    mm::Simulator sim(std::move(feed), std::move(strategy), cfg.fill_simulation);
    sim.set_snapshot_interval(cfg.snapshot_interval);

    std::cout << "Running simulation...\n";
    auto result = sim.run();

    std::cout << "Simulation complete:\n"
              << "  Ticks:  " << result.total_ticks() << "\n"
              << "  Trades: " << result.total_trades() << "\n"
              << "  Final PnL: " << (result.snapshots.empty() ? 0 : result.snapshots.back().total_pnl) << "\n"
              << "  Final Inventory: " << (result.snapshots.empty() ? 0 : result.snapshots.back().inventory) << "\n";

    write_csv(cfg.output_path, result);

    return 0;
}
