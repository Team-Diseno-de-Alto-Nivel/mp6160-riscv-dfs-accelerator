#!/usr/bin/env python3
"""INT-2 (#46): turn the consolidated metrics CSV into paper-ready artifacts.

Reads results/metrics.csv (see docs/guides/metrics-schema.md) and writes:

    results/tables/speedup.md    markdown table (ON vs OFF latency + speedup)
    results/tables/speedup.tex   LaTeX booktabs table
    results/plots/speedup.png    bar chart of speedup per case (matplotlib only)
    results/plots/latency.png    grouped OFF/ON latency bars (matplotlib only)

The tables use only the standard library, so they are always produced. The plots
are emitted only when matplotlib is importable, so the script degrades gracefully
in a minimal environment.
"""
import csv
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def load_pairs(csv_path):
    rows = {}
    with open(csv_path, newline="") as fh:
        for r in csv.DictReader(fh):
            key = (r["algorithm"], r["case"])
            rows.setdefault(key, {})[int(r["accelerator_on"])] = r
    pairs = []
    for (algo, case), bymode in sorted(rows.items()):
        off, on = bymode.get(0), bymode.get(1)
        if not off or not on:
            continue
        off_ns, on_ns = float(off["latency_ns"]), float(on["latency_ns"])
        speedup = off_ns / on_ns if on_ns > 0 else 0.0
        pairs.append(
            {
                "algorithm": algo,
                "case": case,
                "off_ns": off_ns,
                "on_ns": on_ns,
                "speedup": speedup,
                "result": off["result"],
                "match": off["result"] == on["result"],
            }
        )
    return pairs


def write_markdown(pairs, path):
    lines = [
        "| Algorithm | Case | OFF (ns) | ON (ns) | Speedup | Result | Match |",
        "|---|---|---:|---:|---:|---:|:---:|",
    ]
    for p in pairs:
        lines.append(
            "| {algorithm} | {case} | {off_ns:.0f} | {on_ns:.0f} | "
            "{speedup:.2f}x | {result} | {mark} |".format(
                mark="✓" if p["match"] else "✗", **p
            )
        )
    with open(path, "w") as fh:
        fh.write("\n".join(lines) + "\n")


def write_latex(pairs, path):
    def esc(s):
        return s.replace("_", r"\_")

    lines = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{DFS accelerator: baseline (OFF) vs accelerated (ON) latency.}",
        r"\label{tab:speedup}",
        r"\begin{tabular}{llrrr}",
        r"\toprule",
        r"Algorithm & Case & OFF (ns) & ON (ns) & Speedup \\",
        r"\midrule",
    ]
    for p in pairs:
        lines.append(
            "{} & {} & {:.0f} & {:.0f} & {:.2f}$\\times$ \\\\".format(
                esc(p["algorithm"]), esc(p["case"]), p["off_ns"], p["on_ns"], p["speedup"]
            )
        )
    lines += [r"\bottomrule", r"\end{tabular}", r"\end{table}"]
    with open(path, "w") as fh:
        fh.write("\n".join(lines) + "\n")


def write_plots(pairs, plots_dir):
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(">> matplotlib not available — skipping PNG plots.")
        return

    labels = ["{}/{}".format(p["algorithm"], p["case"]) for p in pairs]
    x = range(len(pairs))

    fig, ax = plt.subplots(figsize=(max(8, len(pairs) * 0.5), 5))
    ax.bar(x, [p["speedup"] for p in pairs], color="#4c72b0")
    ax.axhline(1.0, color="gray", linewidth=0.8, linestyle="--")
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, rotation=90, fontsize=7)
    ax.set_ylabel("Speedup (OFF / ON)")
    ax.set_title("DFS accelerator speedup per case")
    fig.tight_layout()
    fig.savefig(os.path.join(plots_dir, "speedup.png"), dpi=150)
    plt.close(fig)

    fig, ax = plt.subplots(figsize=(max(8, len(pairs) * 0.5), 5))
    w = 0.4
    ax.bar([i - w / 2 for i in x], [p["off_ns"] for p in pairs], w, label="OFF", color="#c44e52")
    ax.bar([i + w / 2 for i in x], [p["on_ns"] for p in pairs], w, label="ON", color="#55a868")
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, rotation=90, fontsize=7)
    ax.set_ylabel("Simulated latency (ns)")
    ax.set_title("DFS latency: baseline vs accelerated")
    ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(plots_dir, "latency.png"), dpi=150)
    plt.close(fig)
    print(">> Wrote plots to", plots_dir)


def main():
    csv_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "results", "metrics.csv")
    if not os.path.exists(csv_path):
        sys.exit("metrics CSV not found: {} (run scripts/run_experiments.sh first)".format(csv_path))

    pairs = load_pairs(csv_path)
    if not pairs:
        sys.exit("no ON/OFF pairs found in {}".format(csv_path))

    tables_dir = os.path.join(ROOT, "results", "tables")
    plots_dir = os.path.join(ROOT, "results", "plots")
    os.makedirs(tables_dir, exist_ok=True)
    os.makedirs(plots_dir, exist_ok=True)

    write_markdown(pairs, os.path.join(tables_dir, "speedup.md"))
    write_latex(pairs, os.path.join(tables_dir, "speedup.tex"))
    print(">> Wrote tables to", tables_dir)
    write_plots(pairs, plots_dir)


if __name__ == "__main__":
    main()
