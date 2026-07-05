"""
parameter_sweep.py - Run sweeps over gamma, latency, volatility method.

Usage:
    python parameter_sweep.py --gamma 0.01,0.05,0.1,0.5
    python parameter_sweep.py --latency 0,50,100,500,2000
    python parameter_sweep.py --vol-method 0,1,2,3
    python parameter_sweep.py --all
"""

import subprocess
import pandas as pd
import numpy as np
import argparse
import tempfile
import os
import glob
import sys


SIMULATE_BIN = os.path.join(
    os.path.dirname(__file__), "..", "build", "cpp", "mm_simulate"
)


def run_sweep(params: dict, output_dir: str) -> list[dict]:
    os.makedirs(output_dir, exist_ok=True)
    results = []

    keys = list(params.keys())
    values = list(params.values())

    def _recurse(idx: int, current: dict):
        if idx >= len(keys):
            tag = "_".join(f"{k}={v}" for k, v in current.items())
            tag = tag.replace(".", "_")
            out_path = os.path.join(output_dir, f"results_{tag}.csv")

            cmd = [SIMULATE_BIN, "--ticks", "5000", "--output", out_path]

            for k, v in current.items():
                cmd.extend([f"--{k.replace('_', '-')}", str(v)])

            print(f"  Running: {' '.join(cmd)}")
            ret = subprocess.run(cmd, capture_output=True, text=True)

            if ret.returncode == 0:
                summary = parse_summary(ret.stdout)
                summary["tag"] = tag
                summary.update(current)
                results.append(summary)
                print(f"    PnL={summary.get('final_pnl', '?')}")
            else:
                print(f"    ERROR: {ret.stderr[:200]}")
            return

        for v in values[idx]:
            current[keys[idx]] = v
            _recurse(idx + 1, current)

    _recurse(0, {})
    return results


def parse_summary(text: str) -> dict:
    out = {}
    for line in text.split("\n"):
        if "Final PnL:" in line:
            try:
                out["final_pnl"] = int(line.split(":")[-1].strip())
            except ValueError:
                out["final_pnl"] = 0
        if "Final Inventory:" in line:
            try:
                out["final_inventory"] = int(line.split(":")[-1].strip())
            except ValueError:
                out["final_inventory"] = 0
        if "Trades:" in line:
            try:
                out["total_trades"] = int(line.split(":")[-1].strip())
            except ValueError:
                out["total_trades"] = 0
    return out


def analyze_results(results: list[dict], output_dir: str):
    df = pd.DataFrame(results)
    if df.empty:
        print("No results to analyze")
        return

    csv_path = os.path.join(output_dir, "sweep_results.csv")
    df.to_csv(csv_path, index=False)
    print(f"\nSaved {csv_path}")

    print("\nSummary:")
    print(df.to_string())


def main():
    parser = argparse.ArgumentParser(description="Parameter sweep for market maker")
    parser.add_argument("--gamma", type=str, help="Comma-separated gamma values")
    parser.add_argument("--latency", type=str, help="Comma-separated latency (us) values")
    parser.add_argument("--vol-method", type=str, help="Comma-separated volatility methods (0-3)")
    parser.add_argument("--order-size", type=str, help="Comma-separated order sizes")
    parser.add_argument("--all", action="store_true", help="Run all default sweeps")
    parser.add_argument("--output", type=str, default="sweep_results",
                        help="Output directory")

    args = parser.parse_args()

    params = {}

    if args.gamma:
        params["gamma"] = [float(x) for x in args.gamma.split(",")]
    if args.latency:
        params["latency"] = [int(x) for x in args.latency.split(",")]
    if args.vol_method:
        params["vol_method"] = [int(x) for x in args.vol_method.split(",")]
    if args.order_size:
        params["order_size"] = [int(x) for x in args.order_size.split(",")]

    if args.all:
        params = {
            "gamma": [0.01, 0.05, 0.1, 0.5],
            "latency": [0, 50, 100, 500, 2000],
            "vol_method": [0, 1, 2, 3],
        }

    if not params:
        parser.print_help()
        return

    print(f"Parameter sweep over: {list(params.keys())}")
    results = run_sweep(params, args.output)
    analyze_results(results, args.output)


if __name__ == "__main__":
    main()
