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

jobs="$( (nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null) || echo 2)"

# Extract the CSV block (header + rows) from a program report on stdin.
extract_csv() {
    awk '/^algorithm,case,/{f=1} f{ if ($0 == "") exit; print }'
}

echo ">> Configuring and building (native) ..."
cmake -S "$PROG_DIR" -B "$BUILD_DIR" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 >/dev/null
cmake --build "$BUILD_DIR" -j"$jobs" >/dev/null

echo ">> Running baseline vs accelerated experiment (OFF/ON) ..."
"$BUILD_DIR/program" | tee "$RESULTS_DIR/run.log"
extract_csv < "$RESULTS_DIR/run.log" > "$RESULTS_DIR/metrics.csv"
echo ">> Wrote $RESULTS_DIR/metrics.csv ($(($(wc -l < "$RESULTS_DIR/metrics.csv") - 1)) rows)"

if [ -x "$BUILD_DIR/integration_sim" ]; then
    echo ">> Running program<->model co-simulation over TLM ..."
    "$BUILD_DIR/integration_sim" | tee "$RESULTS_DIR/integration.log"
    extract_csv < "$RESULTS_DIR/integration.log" > "$RESULTS_DIR/integration_metrics.csv"
    echo ">> Wrote $RESULTS_DIR/integration_metrics.csv"
else
    echo ">> integration_sim not built (SystemC unavailable) — skipping co-simulation."
fi

echo ">> Done."
