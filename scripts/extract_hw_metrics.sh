#!/usr/bin/env bash
# FPGA-7 (#68): automate capture of hardware metrics from HLS/Vivado reports
# into results/hw_metrics.csv, so DOC-7 (#93) can populate the README/paper
# without manual copy-paste out of .rpt files.
#
# Usage: scripts/extract_hw_metrics.sh
# Safe to run at any point in the flow: each source degrades to "NA" if its
# report doesn't exist yet. Run again after each stage to fill in more rows:
#   - after `make hls-synth`  -> csynth (#90) + cosim (#95) rows populate
#   - after `make vivado-impl` -> utilization (#65) + timing (#66) rows populate
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
csynth_rpt="$repo_root/src/hls/reports/csynth.rpt"
cosim_rpt="$repo_root/src/hls/reports/dfs_accel_cosim.rpt"
util_rpt="$repo_root/src/vivado/reports/utilization.rpt"
timing_rpt="$repo_root/src/vivado/reports/timing_summary.rpt"
out_dir="$repo_root/results"
out_csv="$out_dir/hw_metrics.csv"

# Clock period (ns) synthesis was targeted against. Must match
# create_clock in src/hls/scripts/run_hls.tcl -- update both together.
target_clock_ns="4.0"

mkdir -p "$out_dir"

# ---------------------------------------------------------------------------
# #90: HLS estimates from csynth.rpt (top-level `dfs_accel` module row +
# worst-case II across VITIS_LOOP rows marked with Issue Type "II").
# ---------------------------------------------------------------------------
hls_bram="NA"; hls_dsp="NA"; hls_ff="NA"; hls_lut="NA"; worst_ii="NA"; worst_ii_loop="NA"

if [[ -f "$csynth_rpt" ]]; then
    read -r hls_bram hls_dsp hls_ff hls_lut < <(awk -F'|' '
        {
            name = $2; gsub(/^[ \t]+|[ \t]+$/, "", name)
            if (name == "+ dfs_accel") {
                bram = $11; dsp = $12; ff = $13; lut = $14
                gsub(/[ \t]/, "", bram); gsub(/\(.*/, "", bram)
                gsub(/[ \t]/, "", dsp);  gsub(/\(.*/, "", dsp)
                gsub(/[ \t]/, "", ff);   gsub(/\(.*/, "", ff)
                gsub(/[ \t]/, "", lut);  gsub(/\(.*/, "", lut)
                print bram, dsp, ff, lut
                exit
            }
        }
    ' "$csynth_rpt")

    read -r worst_ii worst_ii_loop < <(awk -F'|' '
        {
            issue = $3; gsub(/[ \t]/, "", issue)
            if (issue == "II") {
                name = $2; gsub(/^[ \t]*o[ \t]+/, "", name); gsub(/[ \t]+$/, "", name)
                interval = $8; gsub(/[ \t]/, "", interval)
                if (interval ~ /^[0-9]+$/ && interval+0 > max+0) { max = interval+0; loop = name }
            }
        }
        END { print (max == "" ? "NA" : max), (loop == "" ? "NA" : loop) }
    ' "$csynth_rpt")
fi

# ---------------------------------------------------------------------------
# #95: cosim latency (RTL, real cycles) from dfs_accel_cosim.rpt.
# ---------------------------------------------------------------------------
cosim_status="NA"; cosim_lat_min="NA"; cosim_lat_avg="NA"; cosim_lat_max="NA"

if [[ -f "$cosim_rpt" ]]; then
    read -r cosim_status cosim_lat_min cosim_lat_avg cosim_lat_max < <(awk -F'|' '
        $2 ~ /Verilog/ {
            status = $3; gsub(/[ \t]/, "", status)
            lmin = $4; gsub(/[ \t]/, "", lmin)
            lavg = $5; gsub(/[ \t]/, "", lavg)
            lmax = $6; gsub(/[ \t]/, "", lmax)
            print status, lmin, lavg, lmax
            exit
        }
    ' "$cosim_rpt")
fi

# ---------------------------------------------------------------------------
# #65: real post-route utilization from Vivado's report_utilization text
# report. BEST-EFFORT / UNVERIFIED until a real utilization.rpt exists --
# standard Vivado section names, but confirm once #65 actually runs.
# ---------------------------------------------------------------------------
vivado_lut="NA"; vivado_ff="NA"; vivado_bram="NA"; vivado_dsp="NA"

if [[ -f "$util_rpt" ]]; then
    vivado_lut=$(awk -F'|' '$2 ~ /CLB LUTs/  { v=$3; gsub(/[ \t]/,"",v); print v; exit }' "$util_rpt")
    vivado_ff=$(awk  -F'|' '$2 ~ /CLB Registers/ { v=$3; gsub(/[ \t]/,"",v); print v; exit }' "$util_rpt")
    vivado_bram=$(awk -F'|' '$2 ~ /Block RAM Tile/ { v=$3; gsub(/[ \t]/,"",v); print v; exit }' "$util_rpt")
    vivado_dsp=$(awk -F'|' '$2 ~ /^[ \t]*DSPs[ \t]*$/ { v=$3; gsub(/[ \t]/,"",v); print v; exit }' "$util_rpt")
    : "${vivado_lut:=NA}"; : "${vivado_ff:=NA}"; : "${vivado_bram:=NA}"; : "${vivado_dsp:=NA}"
fi

# ---------------------------------------------------------------------------
# #66: real Fmax derived from Vivado's report_timing_summary WNS. Also
# BEST-EFFORT / UNVERIFIED until a real timing_summary.rpt exists.
# ---------------------------------------------------------------------------
vivado_wns="NA"; vivado_fmax="NA"

if [[ -f "$timing_rpt" ]]; then
    vivado_wns=$(awk '
        /WNS\(ns\)/ { getline; gsub(/^[ \t]+/, ""); print $1; exit }
    ' "$timing_rpt")
    if [[ "$vivado_wns" =~ ^-?[0-9.]+$ ]]; then
        vivado_fmax=$(awk -v period="$target_clock_ns" -v wns="$vivado_wns" \
            'BEGIN { achieved = period - wns; if (achieved > 0) printf "%.2f", 1000.0/achieved; else print "NA" }')
    else
        vivado_wns="NA"
    fi
fi

# ---------------------------------------------------------------------------
# Write consolidated CSV (one row, appended with a timestamp so re-runs keep
# history instead of clobbering earlier captures).
# ---------------------------------------------------------------------------
header="timestamp,hls_bram,hls_dsp,hls_ff,hls_lut,worst_ii,worst_ii_loop,cosim_status,cosim_latency_min_cycles,cosim_latency_avg_cycles,cosim_latency_max_cycles,vivado_lut,vivado_ff,vivado_bram,vivado_dsp,vivado_wns_ns,vivado_fmax_mhz"

if [[ ! -f "$out_csv" ]]; then
    echo "$header" > "$out_csv"
fi

echo "$(date -u +%Y-%m-%dT%H:%M:%SZ),$hls_bram,$hls_dsp,$hls_ff,$hls_lut,$worst_ii,$worst_ii_loop,$cosim_status,$cosim_lat_min,$cosim_lat_avg,$cosim_lat_max,$vivado_lut,$vivado_ff,$vivado_bram,$vivado_dsp,$vivado_wns,$vivado_fmax" >> "$out_csv"

echo "extract_hw_metrics.sh: wrote $out_csv"
column -s, -t "$out_csv" 2>/dev/null || cat "$out_csv"
