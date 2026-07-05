"""
run_experiments.py - Run all 6 research experiments and produce results.

Usage:
    python run_experiments.py [--quick] [--output experiments_output]
"""

import subprocess
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
import argparse
import os
import sys
import json
import glob

SIMULATE = os.path.join(
    os.path.dirname(__file__), "..", "build", "cpp", "mm_simulate"
)
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), "..", "research")
NUM_TICKS = 5000 if "--quick" not in sys.argv else 2000
BASE_ARGS = ["--order-size", "200", "--quote-interval", "0", "--vol-window", "50"]

plt.style.use("seaborn-v0_8-darkgrid")


def run_sim(label: str, extra_args: list[str], subdir: str = "") -> str:
    dirpath = os.path.join(OUTPUT_DIR, subdir) if subdir else OUTPUT_DIR
    os.makedirs(dirpath, exist_ok=True)
    out = os.path.join(dirpath, f"{label}.csv")
    cmd = [SIMULATE, "--ticks", str(NUM_TICKS), "--output", out] + extra_args
    print(f"  {' '.join(cmd)}")
    ret = subprocess.run(cmd, capture_output=True, text=True)
    if ret.returncode != 0:
        print(f"    ERROR: {ret.stderr[:200]}")
    return out


def parse_pnl(line: str) -> int:
    try:
        return int(line.split(":")[-1].strip())
    except (ValueError, IndexError):
        return 0


def parse_trades(line: str) -> int:
    try:
        return int(line.split(":")[-1].strip())
    except (ValueError, IndexError):
        return 0


def load_csv(path: str) -> pd.DataFrame:
    return pd.read_csv(path)


def experiment_1_gamma(subdir: str):
    """Sweep risk aversion gamma: 0.01, 0.05, 0.1, 0.5"""
    print("\n=== Experiment 1: Inventory Penalty (Gamma) Sweep ===")
    gammas = [0.01, 0.05, 0.1, 0.5]
    results = []

    for g in gammas:
        out = run_sim(f"gamma_{g}", [
            "--gamma", str(g), "--order-size", "200",
            "--quote-interval", "0", "--vol-window", "50"
        ], subdir)
        df = load_csv(out)
        results.append({
            "gamma": g,
            "final_pnl": df["total_pnl"].iloc[-1] if not df.empty else 0,
            "max_inv": df["inventory"].abs().max() if not df.empty else 0,
            "trades": df["trade_count"].iloc[-1] if not df.empty else 0,
            "final_inv": df["inventory"].iloc[-1] if not df.empty else 0,
        })

    df = pd.DataFrame(results)
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))
    axes[0].plot(df["gamma"], df["final_pnl"], "o-", color="blue")
    axes[0].set_xlabel("Gamma")
    axes[0].set_ylabel("Final PnL")
    axes[0].set_title("PnL vs Risk Aversion")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(df["gamma"], df["max_inv"], "o-", color="red")
    axes[1].set_xlabel("Gamma")
    axes[1].set_ylabel("Max |Inventory|")
    axes[1].set_title("Inventory vs Risk Aversion")
    axes[1].grid(True, alpha=0.3)

    plt.tight_layout()
    fig.savefig(os.path.join(OUTPUT_DIR, "exp1_gamma.png"), dpi=150)
    plt.close(fig)
    print(f"  Saved exp1_gamma.png")
    print(df.to_string())

    # Load all CSVs for combined PnL plot
    plt.figure(figsize=(10, 5))
    for g in gammas:
        path = os.path.join(OUTPUT_DIR, subdir, f"gamma_{g}.csv")
        df = load_csv(path)
        t = (df["timestamp"] - df["timestamp"].iloc[0]) / 1_000_000
        plt.plot(t, df["total_pnl"], label=f"γ={g}", linewidth=1)
    plt.xlabel("Time (s)")
    plt.ylabel("PnL")
    plt.title("PnL Comparison Across Gamma Values")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "exp1_gamma_pnl.png"), dpi=150)
    plt.close()
    print(f"  Saved exp1_gamma_pnl.png")


def experiment_2_latency(subdir: str):
    """Sweep latency: 10, 50, 100, 500, 2000 us"""
    print("\n=== Experiment 2: Latency Sweep ===")
    latencies = [0, 50, 100, 500, 2000]
    results = []

    for lat in latencies:
        out = run_sim(f"latency_{lat}", BASE_ARGS + ["--latency", str(lat)], subdir)
        df = load_csv(out)
        results.append({
            "latency_us": lat,
            "final_pnl": df["total_pnl"].iloc[-1] if not df.empty else 0,
            "trades": df["trade_count"].iloc[-1] if not df.empty else 0,
        })

    df = pd.DataFrame(results)
    fig, axes = plt.subplots(1, 2, figsize=(10, 4))
    axes[0].plot(df["latency_us"], df["final_pnl"], "o-", color="blue")
    axes[0].set_xlabel("Latency (us)")
    axes[0].set_ylabel("Final PnL")
    axes[0].set_title("PnL vs Latency")
    axes[0].grid(True, alpha=0.3)

    axes[1].plot(df["latency_us"], df["trades"], "o-", color="green")
    axes[1].set_xlabel("Latency (us)")
    axes[1].set_ylabel("Trade Count")
    axes[1].set_title("Trades vs Latency")
    axes[1].grid(True, alpha=0.3)

    plt.tight_layout()
    fig.savefig(os.path.join(OUTPUT_DIR, "exp2_latency.png"), dpi=150)
    plt.close(fig)
    print(f"  Saved exp2_latency.png")
    print(df.to_string())


def experiment_3_volatility(subdir: str):
    """Compare volatility estimation methods"""
    print("\n=== Experiment 3: Volatility Method Comparison ===")
    methods = {0: "RollingStd", 1: "EWMA", 2: "Parkinson", 3: "Realized"}
    results = []

    for m, name in methods.items():
        out = run_sim(f"vol_{name}", BASE_ARGS + ["--vol-method", str(m)], subdir)
        df = load_csv(out)
        results.append({
            "method": name,
            "final_pnl": df["total_pnl"].iloc[-1] if not df.empty else 0,
            "avg_vol": df["volatility"].mean() if not df.empty else 0,
            "trades": df["trade_count"].iloc[-1] if not df.empty else 0,
        })

    df = pd.DataFrame(results)
    print(df.to_string())

    fig, ax = plt.subplots(figsize=(8, 4))
    bars = ax.bar(df["method"], df["final_pnl"], color=["blue", "orange", "green", "red"])
    ax.set_ylabel("Final PnL")
    ax.set_title("PnL by Volatility Estimation Method")
    ax.grid(True, alpha=0.3, axis="y")
    plt.tight_layout()
    fig.savefig(os.path.join(OUTPUT_DIR, "exp3_volatility.png"), dpi=150)
    plt.close(fig)
    print(f"  Saved exp3_volatility.png")


def experiment_4_fill_model(subdir: str):
    """Compare Simple Poisson vs Queue Position fill models"""
    print("\n=== Experiment 4: Fill Model Comparison ===")
    models = {0: "SimplePoisson", 1: "QueuePosition"}
    results = []

    for m, name in models.items():
        out = run_sim(f"fill_{name}", BASE_ARGS + ["--fill-model", str(m)], subdir)
        df = load_csv(out)
        results.append({
            "model": name,
            "final_pnl": df["total_pnl"].iloc[-1] if not df.empty else 0,
            "trades": df["trade_count"].iloc[-1] if not df.empty else 0,
            "final_inv": df["inventory"].iloc[-1] if not df.empty else 0,
        })

    df = pd.DataFrame(results)
    print(df.to_string())
    print(f"  Note: QueuePosition requires LOB queue data; "
          f"results may be similar with synthetic data")


def experiment_5_spread(subdir: str):
    """Compare fixed spread vs adaptive spread"""
    print("\n=== Experiment 5: Fixed vs Adaptive Spread ===")
    results = []

    # Adaptive spread (AS model, default)
    out_adaptive = run_sim("adaptive_spread", BASE_ARGS + ["--gamma", "0.5", "--order-size", "200"], subdir)
    df_a = load_csv(out_adaptive)
    results.append({
        "type": "Adaptive (AS)",
        "final_pnl": df_a["total_pnl"].iloc[-1] if not df_a.empty else 0,
        "trades": df_a["trade_count"].iloc[-1] if not df_a.empty else 0,
        "avg_spread": (df_a["ask_price"] - df_a["bid_price"]).mean() if not df_a.empty else 0,
    })

    # Fixed spread
    out_fixed = run_sim("fixed_spread", BASE_ARGS + ["--fixed-spread", "200"], subdir)
    df_f = load_csv(out_fixed)
    results.append({
        "type": f"Fixed (200 ticks)",
        "final_pnl": df_f["total_pnl"].iloc[-1] if not df_f.empty else 0,
        "trades": df_f["trade_count"].iloc[-1] if not df_f.empty else 0,
        "avg_spread": (df_f["ask_price"] - df_f["bid_price"]).mean() if not df_f.empty else 0,
    })

    df = pd.DataFrame(results)
    print(df.to_string())

    # Overlay PnL
    plt.figure(figsize=(10, 5))
    for label, path in [("Adaptive", out_adaptive), ("Fixed", out_fixed)]:
        d = load_csv(path)
        t = (d["timestamp"] - d["timestamp"].iloc[0]) / 1_000_000
        plt.plot(t, d["total_pnl"], label=label, linewidth=1.2)
    plt.xlabel("Time (s)")
    plt.ylabel("PnL")
    plt.title("Fixed vs Adaptive Spread")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "exp5_spread.png"), dpi=150)
    plt.close()
    print(f"  Saved exp5_spread.png")


def experiment_6_inventory(subdir: str):
    """Compare static quotes (no inventory awareness) vs inventory-aware"""
    print("\n=== Experiment 6: Static vs Inventory-Aware Quotes ===")
    results = []

    # Inventory-aware (gamma > 0)
    out_aware = run_sim("inv_aware", BASE_ARGS + ["--gamma", "0.5"], subdir)
    df_a = load_csv(out_aware)
    results.append({
        "type": "Inventory-Aware (γ=0.5)",
        "final_pnl": df_a["total_pnl"].iloc[-1] if not df_a.empty else 0,
        "max_inv": df_a["inventory"].abs().max() if not df_a.empty else 0,
        "trades": df_a["trade_count"].iloc[-1] if not df_a.empty else 0,
    })

    # Static quotes (gamma ~0, no inventory adjustment)
    out_static = run_sim("inv_static", BASE_ARGS + ["--gamma", "0.001"], subdir)
    df_s = load_csv(out_static)
    results.append({
        "type": "Static (γ≈0)",
        "final_pnl": df_s["total_pnl"].iloc[-1] if not df_s.empty else 0,
        "max_inv": df_s["inventory"].abs().max() if not df_s.empty else 0,
        "trades": df_s["trade_count"].iloc[-1] if not df_s.empty else 0,
    })

    df = pd.DataFrame(results)
    print(df.to_string())

    # PnL overlay
    plt.figure(figsize=(10, 5))
    for label, path in [("Inventory-Aware", out_aware), ("Static", out_static)]:
        d = load_csv(path)
        t = (d["timestamp"] - d["timestamp"].iloc[0]) / 1_000_000
        plt.plot(t, d["total_pnl"], label=label, linewidth=1.2)
    plt.xlabel("Time (s)")
    plt.ylabel("PnL")
    plt.title("Static vs Inventory-Aware Quotes")
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig(os.path.join(OUTPUT_DIR, "exp6_inventory.png"), dpi=150)
    plt.close()
    print(f"  Saved exp6_inventory.png")


def run_all():
    subdir = "experiments"
    os.makedirs(os.path.join(OUTPUT_DIR, subdir), exist_ok=True)

    experiment_1_gamma(subdir)
    experiment_2_latency(subdir)
    experiment_3_volatility(subdir)
    experiment_4_fill_model(subdir)
    experiment_5_spread(subdir)
    experiment_6_inventory(subdir)

    print("\n=== All experiments complete ===")
    print(f"Results in: {OUTPUT_DIR}")


if __name__ == "__main__":
    run_all()
