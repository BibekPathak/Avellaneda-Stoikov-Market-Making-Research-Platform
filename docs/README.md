# Avellaneda–Stoikov Market Making Research Platform

A closed-loop market-making simulator implementing the Avellaneda–Stoikov framework, integrating a matching engine, LOB reconstruction, and full PnL analytics.

## Architecture

```
Market Data → State Estimator → Avellaneda–Stoikov Model → Quote Engine → Exchange Simulator → PnL Analytics
                    │                    │                       │               │
             (Volatility, Fill      (Reservation Price,     (Tick Rounding,  (Matching Engine,
              Probabilities,         Optimal Spread)         Min Spread,      Inventory Tracking)
              Inventory)                                     Quote Interval)
```

## Components

### C++ Library (`cpp/include/mm/`)

| Component | File | Description |
|---|---|---|
| Inventory | `inventory.hpp` | Position, cash, PnL tracking with long/short flip handling |
| Volatility | `volatility.hpp` | RollingStd, EWMA, Parkinson, Realized estimators |
| Fill Model | `fill_model.hpp` | SimplePoisson, QueuePosition fill probability models |
| AS Model | `avellaneda_stoikov.hpp` | Reservation price + optimal spread equations |
| Quote Engine | `quote_engine.hpp` | Tick rounding, min spread, quote frequency control |
| Latency Model | `latency_model.hpp` | Fixed, Uniform, Normal latency distributions |
| Risk Manager | `risk.hpp` | Position, drawdown, exposure, loss limits |
| Strategy | `strategy.hpp` | ASMarketMaker — full decision loop per tick |
| Simulator | `simulator.hpp` | Feed → Strategy → Exchange → Results pipeline |
| Analytics | `analytics.hpp` | PnL attribution, trade stats, Sharpe, drawdown |
| Dashboard | `dashboard.hpp` | Text + JSON dashboard output |

### Python (`python/`)

| Script | Purpose |
|---|---|
| `analysis.py` | Load CSV, compute metrics |
| `plots.py` | PnL curves, inventory, spread charts |
| `parameter_sweep.py` | Sweep gamma, latency, vol method |
| `compare_models.py` | Compare strategy variants |
| `run_experiments.py` | Run all 6 research experiments |

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
./cpp/tests/test_inventory
./cpp/tests/test_volatility
./cpp/tests/test_fill_model
./cpp/tests/test_avellaneda_stoikov
./cpp/tests/test_quote_engine
./cpp/tests/test_latency_model
./cpp/tests/test_risk
./cpp/tests/test_strategy
./cpp/tests/test_simulator
./cpp/tests/test_analytics
./cpp/tests/test_dashboard

# Run all
for t in build/cpp/tests/test_*; do if [ -x "$t" ]; then $t 2>&1 | tail -1; fi; done
```

## Usage

```bash
# Run simulation with synthetic data
./build/cpp/mm_simulate --ticks 10000 --gamma 0.1 --order-size 200 --output results.csv

# Run with custom parameters
./build/cpp/mm_simulate \
    --ticks 5000 \
    --base-price 50000 \
    --tick-size 100 \
    --gamma 0.05 \
    --order-size 200 \
    --quote-interval 0 \
    --vol-method 0 \
    --vol-window 50 \
    --latency 100 \
    --fixed-spread 0 \
    --fill-model 0 \
    --output results.csv

# Load CSV data instead of synthetic
./build/cpp/mm_simulate --csv data/market_data.csv --output results.csv

# Python analysis
pip install -r python/requirements.txt
python python/analysis.py results.csv
python python/plots.py results.csv plots/
python python/parameter_sweep.py --gamma 0.01,0.05,0.1,0.5

# Run experiments
python python/run_experiments.py
```

## Research Experiments

| # | Experiment | Variables | Metrics |
|---|---|---|---|
| 1 | Inventory Penalty | γ ∈ {0.01, 0.05, 0.1, 0.5} | PnL, Inventory |
| 2 | Latency | {0, 50, 100, 500, 2000} μs | PnL decay |
| 3 | Volatility Method | RollingStd, EWMA, Parkinson, Realized | PnL |
| 4 | Fill Model | SimplePoisson, QueuePosition | Sharpe, PnL |
| 5 | Spread Type | Fixed vs Adaptive | PnL |
| 6 | Inventory Awareness | Static vs Inventory-Aware | PnL, Inventory |

## Reused Projects

- **Project 1** (`hft/`): Matching engine, order book, event store, exchange pipeline
- **Project 2** (`lob/`): LOB reconstruction, feature extraction, queue model

## Test Summary

```
test_inventory           [  PASSED  ] 19 tests.
test_volatility          [  PASSED  ] 23 tests.
test_fill_model          [  PASSED  ] 22 tests.
test_avellaneda_stoikov  [  PASSED  ] 27 tests.
test_quote_engine        [  PASSED  ] 22 tests.
test_latency_model       [  PASSED  ] 15 tests.
test_risk                [  PASSED  ] 26 tests.
test_strategy            [  PASSED  ] 13 tests.
test_simulator           [  PASSED  ] 9 tests.
test_analytics           [  PASSED  ] 8 tests.
test_dashboard           [  PASSED  ] 4 tests.
Total:                   [  PASSED  ] 188 tests.
```
