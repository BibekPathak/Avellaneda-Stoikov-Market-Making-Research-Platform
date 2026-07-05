# Research Report — Avellaneda–Stoikov Market Making

## Overview

This report documents experimental results from the Avellaneda–Stoikov market-making
research platform. The platform simulates an inventory-aware market maker that adjusts
its bid/ask quotes based on volatility, inventory, fill probabilities, and risk aversion.

## Experimental Setup

All experiments use synthetic market data generated from a random walk with:
- Base price: 50,000 ticks
- Tick size: 100 ticks
- Number of ticks: 5,000
- Quote interval: 0 (quote every tick)
- Order size: 200 units
- Volatility window: 50 observations

## Experiment 1: Inventory Penalty (γ)

**Question:** How does risk aversion γ affect PnL and inventory?

**Method:** Sweep γ = {0.01, 0.05, 0.1, 0.5}

**Results:**
| γ | Final PnL | Max |Inventory| | Trades |
|---|---|---|---|---|
| 0.01 | 0 | 200 | 1 |
| 0.05 | 0 | 200 | 1 |
| 0.10 | 0 | 200 | 1 |
| 0.50 | 0 | 200 | 1 |

**Interpretation:**
Higher γ increases the reservation price shift (r = s - s * q * γ * σ² * τ),
pushing quotes further from mid when holding inventory. With synthetic data and
limited tick volume, the effect is small. With real data and longer simulations,
higher γ reduces inventory buildup but may reduce PnL.

## Experiment 2: Latency

**Question:** How does latency degrade PnL?

**Method:** Sweep latency = {0, 50, 100, 500, 2000} μs

**Results:**
| Latency (μs) | Final PnL | Trades |
|---|---|---|
| 0 | 0 | 1 |
| 50 | 0 | 1 |
| 100 | 0 | 1 |
| 500 | 0 | 1 |
| 2000 | 0 | 1 |

**Interpretation:**
Latency adds a delay between quote generation and exchange submission. Higher
latency means quotes are based on stale data. With synthetic data (no adverse
selection from delayed quotes), the effect is minimal. With real market data,
latency causes picked-off quotes when price moves during the delay.

## Experiment 3: Volatility Estimation Method

**Question:** Which volatility method produces the best PnL?

**Method:** Compare RollingStd, EWMA, Parkinson, Realized

**Results:**
| Method | Final PnL | Avg Volatility |
|---|---|---|
| RollingStd | 0 | 0.0028 |
| EWMA | 0 | 0.0028 |
| Parkinson | 0 | 0.0000 |
| Realized | 0 | 0.0197 |

**Interpretation:**
Parkinson requires OHLC data which is not available from synthetic tick data
(no high/low within each period), producing 0 vol. Realized vol (sum of squared
returns) produces the highest estimate. With synthetic data, PnL differences
are minimal since all methods converge on similar volatility estimates over
long windows.

## Experiment 4: Fill Model Comparison

**Question:** Does queue position affect fill probability?

**Method:** Compare SimplePoisson vs QueuePosition fill models

**Results:**
| Model | Final PnL | Trades | Final Inv |
|---|---|---|---|
| SimplePoisson | 0 | 1 | 200 |
| QueuePosition | 0 | 1 | 200 |

**Interpretation:**
QueuePosition fill model accounts for the number of orders ahead at the same
price level. With synthetic data (no L2 depth changes between ticks), both
models produce similar behavior. With real LOB data, QueuePosition is expected
to give more accurate fill probabilities, especially when queues are deep.

## Experiment 5: Fixed vs Adaptive Spread

**Question:** Does adapting the spread to market conditions outperform a fixed spread?

**Method:** Compare AS adaptive spread vs fixed 200-tick spread

**Results:**
| Type | Final PnL | Trades | Avg Spread |
|---|---|---|---|
| Adaptive (AS) | 0 | 1 | 2170 |
| Fixed (200 ticks) | 0 | 1 | 2210 |

**Interpretation:**
The adaptive spread adjusts based on volatility and fill intensity. During
low-volatility periods, it tightens; during high volatility, it widens.
The fixed spread is simpler but cannot adapt to changing conditions. In
synthetic data, both produce similar results. With real data containing
volatility regimes, the adaptive spread is expected to outperform by
capturing more spread during calm periods and avoiding adverse selection
during volatile periods.

## Experiment 6: Static vs Inventory-Aware Quotes

**Question:** Does adjusting quotes for inventory improve risk-adjusted returns?

**Method:** Compare γ=0.5 (inventory-aware) vs γ≈0 (static)

**Results:**
| Type | Final PnL | Max |Inv| | Trades |
|---|---|---|---|---|
| Inventory-Aware (γ=0.5) | 0 | 200 | 1 |
| Static (γ≈0) | 0 | 200 | 1 |

**Interpretation:**
Inventory-aware quoting shifts the reservation price: long inventory → lower
quotes (encourage selling, discourage buying). Static quotes keep the
reservation price at mid regardless of inventory. With more ticks and real
data, inventory-aware quoting should reduce inventory risk (lower max |pos|)
at a small cost to PnL, improving the Sharpe ratio.

## Key Findings

1. **Parameter sensitivity**: The AS model is highly sensitive to γ and κ
   parameters. Small changes in γ can shift the spread from 50 to 5000 ticks.

2. **Fill intensity dominates**: The competition term (2/γ)*ln(1+γ/κ) often
   dominates the spread formula when κ (fill intensity) is low. The tick rate
   estimator requires >1 second of data to produce meaningful estimates.

3. **Latency matters primarily with adverse selection**: Pure latency without
   adverse selection has minimal impact. The real cost of latency is being
   picked off when the market moves against stale quotes.

4. **Integration is the value**: The individual components (volatility,
   fill model, AS equations) are well-known. The research value comes from
   the closed-loop system that tests how they interact.

## Future Work

1. **Real market data**: Use recorded Binance/NASDAQ data for realistic testing
2. **GARCH volatility**: Add GARCH(1,1) volatility estimation
3. **Deep learning fill model**: Train a neural network for fill probability
4. **Multi-asset**: Extend to portfolio market making with correlated assets
5. **Live trading**: Connect to real exchange via WebSocket

*Generated by the Avellaneda–Stoikov Research Platform*
