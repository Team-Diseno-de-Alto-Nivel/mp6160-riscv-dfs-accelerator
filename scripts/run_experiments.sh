#!/usr/bin/env bash
# INT-2 (#30): build and run the five algorithms twice (accelerator OFF/ON),
# collect the consolidated metrics CSV, and — when the SystemC co-simulation is
# available — also run the program<->model integration over TLM.
#
# Outputs (under results/):
#   run.log                 full program report (per-run table, speedup, CSV)
#   metrics.csv             consolidated metrics, one row per (case x mode)
#   integration.log         co-simulation report (if integration_sim was built)
#   integration_metrics.csv co-simulation metrics CSV (if available)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROG_DIR="$ROOT/src/program"
BUILD_DIR="$PROG_DIR/build-native"
RESULTS_DIR="$ROOT/results"
mkdir -p "$RESULTS_DIR"

# Dataset tier: legacy (hand-written cases only, the CI default) or
# small/medium/large to add the generated datasets up to that size. See
# docs/guides/synthetic-datasets.md.
TIER="${1:-legacy}"
case "$TIER" in
    legacy) RUN_LOG="$RESULTS_DIR/run.log";        CSV="$RESULTS_DIR/metrics.csv" ;;
    small|medium|large)
            RUN_LOG="$RESULTS_DIR/run_$TIER.log";  CSV="$RESULTS_DIR/metrics_$TIER.csv" ;;
    *) echo "unknown tier: $TIER (use legacy|small|medium|large)" >&2; exit 2 ;;
esac

jobs="$( (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null) || echo 2)"

# Extract the CSV block (header + rows) from a program report on stdin.
extract_csv() {
    awk '/^algorithm,case,/{f=1} f{ if ($0 == "") exit; print }'
}

echo ">> Configuring and building (native) ..."
cmake -S "$PROG_DIR" -B "$BUILD_DIR" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
cmake --build "$BUILD_DIR" -j"$jobs" >/dev/null

echo ">> Running baseline vs accelerated experiment (OFF/ON, tier=$TIER) ..."
"$BUILD_DIR/program" --tier="$TIER" | tee "$RUN_LOG"
extract_csv < "$RUN_LOG" > "$CSV"
echo ">> Wrote $CSV ($(($(wc -l < "$CSV") - 1)) rows)"

if [ "$TIER" != "legacy" ]; then
    echo ">> Tier $TIER is software-only (grids exceed the SystemC/HLS capacity) — skipping co-simulation."
    echo ">> Done."
    exit 0
fi

if [ -x "$BUILD_DIR/integration_sim" ]; then
    echo ">> Running program<->model co-simulation over TLM ..."
    "$BUILD_DIR/integration_sim" | tee "$RESULTS_DIR/integration.log"
    extract_csv < "$RESULTS_DIR/integration.log" > "$RESULTS_DIR/integration_metrics.csv"
    echo ">> Wrote $RESULTS_DIR/integration_metrics.csv"
else
    echo ">> integration_sim not built (SystemC unavailable) — skipping co-simulation."
fi

echo ">> Done."
