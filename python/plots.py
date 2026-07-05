"""
plots.py - Generate plots from simulation results.

Usage:
    python plots.py results.csv [output_dir]
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
import sys
import os

plt.style.use("seaborn-v0_8-darkgrid")


def load_data(path: str) -> pd.DataFrame:
    df = pd.read_csv(path)
    df["time_s"] = (df["timestamp"] - df["timestamp"].iloc[0]) / 1_000_000
    return df


def plot_dashboard(df: pd.DataFrame, output_dir: str = "."):
    fig = plt.figure(figsize=(14, 10))
    gs = gridspec.GridSpec(3, 3, figure=fig, hspace=0.35, wspace=0.3)

    ax_pnl = fig.add_subplot(gs[0, :])
    ax_inv = fig.add_subplot(gs[1, 0])
    ax_spread = fig.add_subplot(gs[1, 1])
    ax_price = fig.add_subplot(gs[1, 2])
    ax_vol = fig.add_subplot(gs[2, 0])
    ax_exposure = fig.add_subplot(gs[2, 1])
    ax_returns = fig.add_subplot(gs[2, 2])

    # PnL
    ax_pnl.plot(df["time_s"], df["total_pnl"], label="Total PnL", linewidth=1.5)
    ax_pnl.plot(df["time_s"], df["realized_pnl"], label="Realized PnL", alpha=0.7)
    ax_pnl.plot(df["time_s"], df["unrealized_pnl"], label="Unrealized PnL", alpha=0.7)
    ax_pnl.set_xlabel("Time (s)")
    ax_pnl.set_ylabel("PnL")
    ax_pnl.set_title("PnL Over Time")
    ax_pnl.legend()
    ax_pnl.axhline(y=0, color="gray", linestyle="--", alpha=0.5)

    # Inventory
    ax_inv.plot(df["time_s"], df["inventory"], color="green", linewidth=1)
    ax_inv.fill_between(df["time_s"], 0, df["inventory"], alpha=0.15, color="green")
    ax_inv.set_xlabel("Time (s)")
    ax_inv.set_ylabel("Position")
    ax_inv.set_title("Inventory")
    ax_inv.axhline(y=0, color="gray", linestyle="--", alpha=0.5)

    # Spread
    spread = df["ask_price"] - df["bid_price"]
    ax_spread.plot(df["time_s"], spread, color="purple", linewidth=0.8)
    ax_spread.set_xlabel("Time (s)")
    ax_spread.set_ylabel("Spread (ticks)")
    ax_spread.set_title("Bid-Ask Spread")

    # Price
    ax_price.plot(df["time_s"], df["mid_price"], color="blue", linewidth=1)
    ax_price.plot(df["time_s"], df["bid_price"], color="red", alpha=0.5, linewidth=0.5)
    ax_price.plot(df["time_s"], df["ask_price"], color="green", alpha=0.5, linewidth=0.5)
    ax_price.set_xlabel("Time (s)")
    ax_price.set_ylabel("Price")
    ax_price.set_title("Quotes & Mid Price")

    # Volatility
    ax_vol.plot(df["time_s"], df["volatility"], color="orange", linewidth=1)
    ax_vol.set_xlabel("Time (s)")
    ax_vol.set_ylabel("Volatility")
    ax_vol.set_title("Estimated Volatility")

    # Exposure
    exposure = np.abs(df["inventory"]) * df["mid_price"]
    ax_exposure.plot(df["time_s"], exposure, color="red", linewidth=1)
    ax_exposure.fill_between(df["time_s"], 0, exposure, alpha=0.1, color="red")
    ax_exposure.set_xlabel("Time (s)")
    ax_exposure.set_ylabel("Exposure")
    ax_exposure.set_title("Inventory Exposure")

    # Return distribution
    returns = df["total_pnl"].diff().dropna().values
    ax_returns.hist(returns, bins=50, alpha=0.7, color="steelblue", edgecolor="white")
    ax_returns.axvline(x=0, color="red", linestyle="--", alpha=0.5)
    ax_returns.set_xlabel("Per-Tick PnL")
    ax_returns.set_ylabel("Frequency")
    ax_returns.set_title("PnL Distribution")

    fig.suptitle("Avellaneda-Stoikov Market Maker — Simulation Results", fontsize=14, y=1.02)
    fig.savefig(os.path.join(output_dir, "dashboard.png"), dpi=150, bbox_inches="tight")
    print(f"Saved dashboard.png")
    plt.close(fig)


def plot_single_chart(df: pd.DataFrame, output_dir: str = "."):
    fig, axes = plt.subplots(3, 1, figsize=(12, 10), sharex=True)

    t = df["time_s"]

    axes[0].plot(t, df["total_pnl"], label="Total PnL", color="blue")
    axes[0].plot(t, df["realized_pnl"], label="Realized", color="green")
    axes[0].set_ylabel("PnL")
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(t, df["inventory"], color="purple")
    axes[1].fill_between(t, 0, df["inventory"], alpha=0.2, color="purple")
    axes[1].set_ylabel("Inventory")
    axes[1].axhline(0, color="gray", ls="--", alpha=0.5)
    axes[1].grid(True, alpha=0.3)

    axes[2].plot(t, df["bid_price"], label="Bid", color="red", alpha=0.7)
    axes[2].plot(t, df["ask_price"], label="Ask", color="green", alpha=0.7)
    axes[2].plot(t, df["mid_price"], label="Mid", color="blue", linewidth=1)
    axes[2].set_xlabel("Time (s)")
    axes[2].set_ylabel("Price")
    axes[2].legend()
    axes[2].grid(True, alpha=0.3)

    plt.tight_layout()
    fig.savefig(os.path.join(output_dir, "timeseries.png"), dpi=150)
    print(f"Saved timeseries.png")
    plt.close(fig)


def compare_runs(files: list[str], labels: list[str], output_dir: str = "."):
    fig, axes = plt.subplots(2, 1, figsize=(12, 8), sharex=True)

    for f, label in zip(files, labels):
        df = load_data(f)
        t = df["time_s"]
        axes[0].plot(t, df["total_pnl"], label=label, alpha=0.8)
        axes[1].plot(t, df["inventory"], label=label, alpha=0.8)

    axes[0].set_ylabel("PnL")
    axes[0].legend()
    axes[0].grid(True, alpha=0.3)
    axes[0].axhline(0, color="gray", ls="--", alpha=0.5)

    axes[1].set_xlabel("Time (s)")
    axes[1].set_ylabel("Inventory")
    axes[1].legend()
    axes[1].grid(True, alpha=0.3)
    axes[1].axhline(0, color="gray", ls="--", alpha=0.5)

    plt.tight_layout()
    fig.savefig(os.path.join(output_dir, "compare.png"), dpi=150)
    print(f"Saved compare.png")
    plt.close(fig)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    path = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else "."
    os.makedirs(out, exist_ok=True)

    df = load_data(path)
    plot_dashboard(df, out)
    plot_single_chart(df, out)
    print("Done.")
