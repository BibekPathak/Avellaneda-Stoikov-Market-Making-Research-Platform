"""
analysis.py - Load simulation CSV, compute metrics, and compare strategies.

Usage:
    python analysis.py results.csv
    python analysis.py --compare sweep_results/*.csv
"""

import pandas as pd
import numpy as np
import sys
import glob


def load_results(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    df["price_impact"] = df["total_pnl"].diff().fillna(0)
    df["return"] = df["total_pnl"].pct_change().fillna(0)
    return df


def compute_metrics(df: pd.DataFrame, capital: float = 1e7) -> dict:
    if df.empty:
        return {}

    total_pnl = df["total_pnl"].iloc[-1]
    realized = df["realized_pnl"].iloc[-1]
    unrealized = df["unrealized_pnl"].iloc[-1]
    final_inv = df["inventory"].iloc[-1]
    total_trades = df["trade_count"].iloc[-1]
    total_ticks = len(df)

    returns = df["return"].values[1:]
    mean_ret = np.mean(returns)
    std_ret = np.std(returns, ddof=1)

    sharpe = (mean_ret / std_ret * np.sqrt(252)) if std_ret > 0 else 0.0

    downside = returns[returns < 0]
    semi_std = np.std(downside, ddof=1) if len(downside) > 1 else 0.0
    sortino = (mean_ret / semi_std * np.sqrt(252)) if semi_std > 0 else 0.0

    pnl_series = df["total_pnl"].values
    peak = np.maximum.accumulate(pnl_series)
    drawdown = peak - pnl_series
    max_dd = np.max(drawdown)
    max_dd_pct = max_dd / peak[np.argmax(drawdown)] if peak[np.argmax(drawdown)] > 0 else 0.0

    spread = df["ask_price"] - df["bid_price"]
    avg_spread = spread.mean()
    avg_bid = df["bid_price"].mean()
    avg_ask = df["ask_price"].mean()

    return {
        "total_pnl": int(total_pnl),
        "realized_pnl": int(realized),
        "unrealized_pnl": int(unrealized),
        "final_inventory": int(final_inv),
        "total_trades": int(total_trades),
        "total_ticks": total_ticks,
        "sharpe": round(sharpe, 4),
        "sortino": round(sortino, 4),
        "max_drawdown": int(max_dd),
        "max_drawdown_pct": round(float(max_dd_pct), 4),
        "avg_spread": round(float(avg_spread), 2),
        "volatility": round(float(std_ret), 6),
        "fill_rate": round(total_trades / total_ticks, 4) if total_ticks > 0 else 0.0,
    }


def print_metrics(metrics: dict):
    if not metrics:
        print("No data")
        return
    print(f"  PnL:          {metrics['total_pnl']:>12,}")
    print(f"  Realized:     {metrics['realized_pnl']:>12,}")
    print(f"  Unrealized:   {metrics['unrealized_pnl']:>12,}")
    print(f"  Final Inv:    {metrics['final_inventory']:>12,}")
    print(f"  Trades:       {metrics['total_trades']:>12,}")
    print(f"  Sharpe:       {metrics['sharpe']:>12.4f}")
    print(f"  Sortino:      {metrics['sortino']:>12.4f}")
    print(f"  Max DD:       {metrics['max_drawdown']:>12,}")
    print(f"  Max DD%:      {metrics['max_drawdown_pct']:>12.2%}")
    print(f"  Avg Spread:   {metrics['avg_spread']:>12.2f}")
    print(f"  Fill Rate:    {metrics['fill_rate']:>12.2%}")


def compare_results(paths: list[str]) -> pd.DataFrame:
    rows = []
    for path in paths:
        df = load_results(path)
        m = compute_metrics(df)
        m["file"] = path
        rows.append(m)
    return pd.DataFrame(rows).set_index("file")


if __name__ == "__main__":
    if "--compare" in sys.argv:
        idx = sys.argv.index("--compare")
        patterns = sys.argv[idx + 1:]
        files = []
        for p in patterns:
            files.extend(glob.glob(p))
        comp = compare_results(files)
        print(comp.to_string())
    elif len(sys.argv) > 1:
        df = load_results(sys.argv[1])
        m = compute_metrics(df)
        print_metrics(m)
    else:
        print(__doc__)
