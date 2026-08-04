#!/usr/bin/env python3
"""FPGA-10 (#92): correlate the simulation cost model against real hardware.

Reads results/metrics.csv (cost model, see docs/guides/metrics-schema.md) and
results/onboard_metrics.csv (real KV260 measurements, #91) and writes:

    results/tables/cost_model_correlation.md   per-problem summary + per-case detail
    results/tables/cost_model_correlation.tex  LaTeX booktabs table (per-problem)
    results/plots/cost_model_error.png         latency relative error per case (matplotlib only)

"Modelled" speedup is OFF.latency_ns / ON.latency_ns, both from the cost
model (results/metrics.csv) -- the same figure already published in the
paper. "Measured" speedup keeps that same OFF (the software baseline was
only ever run in simulation, see the paper's Experiment Plan; the on-board
benchmark only exercises accelerator_on=1) and swaps ON for the real median
latency measured on the KV260. Per-problem aggregates use the geometric
mean of the per-case ratios -- matches the "Speedup (modelled)" figures
already hand-written in README.md, back-verified from results/metrics.csv.

noi_all_water has no DFS work to charge in the cost model (modelled ON is 0
ns), so it has no meaningful speedup ratio on either side and is excluded
from the per-problem aggregates -- same convention the README's per-case
table already uses ("--"). Its latency gap (0 ns modelled vs. real cycles
measured) is the headline model-vs-hardware finding, reported per-case.
"""
import csv
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

PROBLEM_LABELS = {
    "number_of_islands": "Number of Islands",
    "unique_paths_iii": "Unique Paths III",
    "word_search_ii": "Word Search II",
    "word_search_ii_nopruning": "Word Search II (no pruning)",
    "pacific_atlantic": "Pacific Atlantic",
    "longest_increasing_path": "Longest Increasing Path",
    "longest_increasing_path_nomemo": "Longest Increasing Path (no memo)",
}


def load_metrics(csv_path):
    """(algorithm, case) -> {0: off_row, 1: on_row} from the cost-model CSV."""
    rows = {}
    with open(csv_path, newline="") as fh:
        for r in csv.DictReader(fh):
            key = (r["algorithm"], r["case"])
            rows.setdefault(key, {})[int(r["accelerator_on"])] = r
    return rows


def load_onboard(csv_path):
    """(algorithm, case) -> onboard row (latency_ns_median etc.)."""
    rows = {}
    with open(csv_path, newline="") as fh:
        for r in csv.DictReader(fh):
            rows[(r["algorithm"], r["case"])] = r
    return rows


def build_cases(metrics, onboard):
    cases = []
    for (algo, case), bymode in sorted(metrics.items()):
        off, on = bymode.get(0), bymode.get(1)
        board = onboard.get((algo, case))
        if not off or not on or not board:
            continue
        off_ns = float(off["latency_ns"])
        modeled_on_ns = float(on["latency_ns"])
        measured_on_ns = float(board["latency_ns_median"])

        latency_rel_error = (
            (measured_on_ns - modeled_on_ns) / measured_on_ns if measured_on_ns > 0 else 0.0
        )
        valid = modeled_on_ns > 0 and measured_on_ns > 0
        cases.append(
            {
                "algorithm": algo,
                "case": case,
                "off_ns": off_ns,
                "modeled_on_ns": modeled_on_ns,
                "measured_on_ns": measured_on_ns,
                "latency_rel_error": latency_rel_error,
                "modeled_speedup": off_ns / modeled_on_ns if valid else None,
                "measured_speedup": off_ns / measured_on_ns if valid else None,
                "valid": valid,
            }
        )
    return cases


def geomean(values):
    values = [v for v in values if v is not None and v > 0]
    if not values:
        return None
    return math.exp(sum(math.log(v) for v in values) / len(values))


def build_summary(cases):
    by_algo = {}
    for c in cases:
        by_algo.setdefault(c["algorithm"], []).append(c)

    def rel_error_fields(group):
        rel_errors = [c["latency_rel_error"] for c in group if c["valid"]]
        return {
            "latency_rel_error_mean": sum(rel_errors) / len(rel_errors) if rel_errors else None,
            "latency_rel_error_max": max(rel_errors, key=abs) if rel_errors else None,
        }

    def speedup_rel_error(modeled, measured):
        if modeled is None or measured in (None, 0):
            return None
        return (measured - modeled) / measured

    # Per-problem rows use the geometric mean of the per-case speedup ratios
    # -- verified by back-computing from the numbers already published in
    # README.md (e.g. Number of Islands' 1.65x only matches the geomean of
    # its 5 valid cases, not their arithmetic mean of 1.67x).
    summary = []
    for algo, label in PROBLEM_LABELS.items():
        group = by_algo.get(algo, [])
        modeled = geomean([c["modeled_speedup"] for c in group])
        measured = geomean([c["measured_speedup"] for c in group])
        summary.append(
            {
                "algorithm": algo,
                "label": label,
                "modeled_speedup": modeled,
                "measured_speedup": measured,
                "speedup_rel_error": speedup_rel_error(modeled, measured),
                **rel_error_fields(group),
            }
        )

    # The Aggregate row instead uses total-OFF-time / total-ON-time (a
    # throughput-weighted speedup, not a mean of per-problem ratios) --
    # same back-computation check: only sum(off_ns)/sum(on_ns) over the
    # cost model reproduces the published 1.56x aggregate (a geomean of
    # either the per-case or per-problem ratios lands near 2.0x instead).
    valid = [c for c in cases if c["valid"]]
    off_sum = sum(c["off_ns"] for c in valid)
    modeled_on_sum = sum(c["modeled_on_ns"] for c in valid)
    measured_on_sum = sum(c["measured_on_ns"] for c in valid)
    modeled_agg = off_sum / modeled_on_sum if modeled_on_sum else None
    measured_agg = off_sum / measured_on_sum if measured_on_sum else None
    aggregate = {
        "algorithm": "aggregate",
        "label": "Aggregate",
        "modeled_speedup": modeled_agg,
        "measured_speedup": measured_agg,
        "speedup_rel_error": speedup_rel_error(modeled_agg, measured_agg),
        **rel_error_fields(cases),
    }
    return summary, aggregate


def fmt_x(v):
    return "{:.2f}x".format(v) if v is not None else "--"


def fmt_pct(v):
    return "{:+.1f}%".format(v * 100) if v is not None else "--"


def write_markdown(cases, summary, aggregate, path):
    lines = ["## Per problem", ""]
    lines.append(
        "| Problem (variant) | Speedup (modelled) | Speedup (measured) | "
        "Relative error (latency, mean) |"
    )
    lines.append("|---|---:|---:|---:|")
    for s in summary:
        lines.append(
            "| {label} | {mod} | {meas} | {err} |".format(
                label=s["label"], mod=fmt_x(s["modeled_speedup"]),
                meas=fmt_x(s["measured_speedup"]), err=fmt_pct(s["latency_rel_error_mean"]),
            )
        )
    lines.append(
        "| **{label}** | **{mod}** | **{meas}** | **{err}** |".format(
            label=aggregate["label"], mod=fmt_x(aggregate["modeled_speedup"]),
            meas=fmt_x(aggregate["measured_speedup"]), err=fmt_pct(aggregate["latency_rel_error_mean"]),
        )
    )

    lines += ["", "## Per case", ""]
    lines.append(
        "| Problem (variant) | Case | Modelled ON (ns) | Measured ON (ns, median) | Latency relative error |"
    )
    lines.append("|---|---|---:|---:|---:|")
    for c in cases:
        lines.append(
            "| {algo} | `{case}` | {mod:.0f} | {meas:.0f} | {err} |".format(
                algo=PROBLEM_LABELS.get(c["algorithm"], c["algorithm"]), case=c["case"],
                mod=c["modeled_on_ns"], meas=c["measured_on_ns"], err=fmt_pct(c["latency_rel_error"]),
            )
        )
    with open(path, "w") as fh:
        fh.write("\n".join(lines) + "\n")


def write_latex(summary, aggregate, path):
    def esc(s):
        return s.replace("_", r"\_")

    lines = [
        r"\begin{table}[t]",
        r"\centering",
        r"\caption{Simulated (modelled) vs.\ on-board (measured) speedup, DFS accelerator on Kria KV260.}",
        r"\label{tab:cost_model_correlation}",
        r"\begin{tabular}{lrrr}",
        r"\toprule",
        r"Problem (variant) & Speedup (modelled) & Speedup (measured) & Rel.\ error (latency) \\",
        r"\midrule",
    ]
    for s in summary:
        lines.append(
            "{} & {} & {} & {} \\\\".format(
                esc(s["label"]), fmt_x(s["modeled_speedup"]), fmt_x(s["measured_speedup"]),
                fmt_pct(s["latency_rel_error_mean"]),
            )
        )
    lines.append(r"\midrule")
    lines.append(
        r"\textbf{{{}}} & \textbf{{{}}} & \textbf{{{}}} & \textbf{{{}}} \\".format(
            aggregate["label"], fmt_x(aggregate["modeled_speedup"]),
            fmt_x(aggregate["measured_speedup"]), fmt_pct(aggregate["latency_rel_error_mean"]),
        )
    )
    lines += [r"\bottomrule", r"\end{tabular}", r"\end{table}"]
    with open(path, "w") as fh:
        fh.write("\n".join(lines) + "\n")


def write_plot(cases, plots_dir):
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print(">> matplotlib not available -- skipping PNG plot.")
        return

    labels = ["{}/{}".format(c["algorithm"], c["case"]) for c in cases]
    errors = [c["latency_rel_error"] * 100 for c in cases]
    x = range(len(cases))

    fig, ax = plt.subplots(figsize=(max(8, len(cases) * 0.5), 5))
    ax.bar(x, errors, color="#c44e52")
    ax.axhline(0.0, color="gray", linewidth=0.8, linestyle="--")
    ax.set_xticks(list(x))
    ax.set_xticklabels(labels, rotation=90, fontsize=7)
    ax.set_ylabel("Latency relative error (%), (measured - modelled) / measured")
    ax.set_title("Cost model vs. on-board latency, per case")
    fig.tight_layout()
    fig.savefig(os.path.join(plots_dir, "cost_model_error.png"), dpi=150)
    plt.close(fig)
    print(">> Wrote plot to", plots_dir)


def main():
    metrics_path = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "results", "metrics.csv")
    onboard_path = sys.argv[2] if len(sys.argv) > 2 else os.path.join(ROOT, "results", "onboard_metrics.csv")

    if not os.path.exists(metrics_path):
        sys.exit("metrics CSV not found: {} (run `make experiments` first)".format(metrics_path))
    if not os.path.exists(onboard_path):
        sys.exit(
            "onboard metrics CSV not found: {} (run `make onboard-deploy && "
            "make onboard-fetch-metrics KV260_HOST=<alias>` first)".format(onboard_path)
        )

    cases = build_cases(load_metrics(metrics_path), load_onboard(onboard_path))
    if not cases:
        sys.exit("no (algorithm, case) pairs matched between {} and {}".format(metrics_path, onboard_path))

    summary, aggregate = build_summary(cases)

    tables_dir = os.path.join(ROOT, "results", "tables")
    plots_dir = os.path.join(ROOT, "results", "plots")
    os.makedirs(tables_dir, exist_ok=True)
    os.makedirs(plots_dir, exist_ok=True)

    write_markdown(cases, summary, aggregate, os.path.join(tables_dir, "cost_model_correlation.md"))
    write_latex(summary, aggregate, os.path.join(tables_dir, "cost_model_correlation.tex"))
    print(">> Wrote tables to", tables_dir)
    write_plot(cases, plots_dir)


if __name__ == "__main__":
    main()
