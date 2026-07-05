"""
compare_models.py - Compare strategy variants and parameter configurations.

Usage:
    python compare_models.py <results1.csv> <results2.csv> ... [--labels A,B,C]
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import sys
import os

plt.style.use("seaborn-v0_8-darkgrid")


def load_result(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    df["time_s"] = (df["timestamp"] - df["timestamp"].iloc[0]) / 1_000_000
    return df


def extract_metrics(df: pd.DataFrame, capital: float = 1e7) -> dict:
    if df.empty:
        return {}

    returns = df["total_pnl"].diff().dropna().values
    mean_ret = np.mean(returns)
    std_ret = np.std(returns, ddof=1)
    sharpe = (mean_ret / std_ret * np.sqrt(252)) if std_ret > 0 else 0.0

    pnl = df["total_pnl"].values
    peak = np.maximum.accumulate(pnl)
    dd = peak - pnl
    max_dd = np.max(dd)

    final_inv = df["inventory"].iloc[-1]
    total_pnl = df["total_pnl"].iloc[-1]
    trades = int(df["trade_count"].iloc[-1])
    avg_spread = (df["ask_price"] - df["bid_price"]).mean()
    vol = df["volatility"].mean()
    max_pos = df["inventory"].abs().max()

    return {
        "Total PnL": total_pnl,
        "Sharpe": round(sharpe, 3),
        "Max DD": max_dd,
        "Max Position": int(max_pos),
        "Final Inv": int(final_inv),
        "Trades": trades,
        "Avg Spread": round(avg_spread, 1),
        "Avg Vol": round(vol, 4),
    }


def generate_report(paths: list[str], labels: list[str] | None = None):
    if labels is None:
        labels = [os.path.basename(p).replace(".csv", "") for p in paths]

    if len(paths) != len(labels):
        print("Error: number of paths and labels must match")
        return

    rows = {}
    for path, label in zip(paths, labels):
        df = load_result(path)
        rows[label] = extract_metrics(df)

    report_df = pd.DataFrame(rows).T
    print("\n=== Model Comparison ===\n")
    print(report_df.to_string(float_format=lambda x: f"{x:,.2f}"))

    report_df.to_csv("comparison_report.csv")
    print("\nSaved comparison_report.csv")

    # Generate comparison plot
    fig, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

    for path, label in zip(paths, labels):
        df = load_result(path)
        t = df["time_s"]
        axes[0].plot(t, df["total_pnl"], label=label, linewidth=1.2)
        axes[1].plot(t, df["inventory"], label=label, linewidth=1.2, alpha=0.8)

    axes[0].set_ylabel("PnL")
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)
    axes[0].axhline(0, color="gray", ls="--", alpha=0.5)

    axes[1].set_xlabel("Time (s)")
    axes[1].set_ylabel("Inventory")
    axes[1].legend()
    axes[1].grid(True, alpha=0.3)
    axes[1].axhline(0, color="gray", ls="--", alpha=0.5)

    fig.suptitle("Strategy Comparison", fontsize=14)
    plt.tight_layout()
    fig.savefig("comparison_plot.png", dpi=150, bbox_inches="tight")
    print("Saved comparison_plot.png")


if __name__ == "__main__":
    args = sys.argv[1:]

    labels = None
    if "--labels" in args:
        idx = args.index("--labels")
        labels = args[idx + 1].split(",")
        args = args[:idx] + args[idx + 2:]

    if len(args) < 1:
        print(__doc__)
        sys.exit(1)

    generate_report(args, labels)
