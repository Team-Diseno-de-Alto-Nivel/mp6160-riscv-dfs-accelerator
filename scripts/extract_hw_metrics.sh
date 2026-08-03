#!/usr/bin/env bash
set -euo pipefail

# Output CSV
OUT=results/hw_metrics.csv
mkdir -p "$(dirname "$OUT")"

timestamp=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

# Default NA values
hls_bram=NA
hls_dsp=NA
hls_ff=NA
hls_lut=NA
worst_ii=NA
worst_ii_loop=NA
cosim_status=NA
cosim_latency_min_cycles=NA
cosim_latency_avg_cycles=NA
cosim_latency_max_cycles=NA
vivado_lut=NA
vivado_ff=NA
vivado_bram=NA
vivado_dsp=NA
vivado_wns_ns=NA
vivado_fmax_mhz=NA

# Try to locate common HLS report (csynth, csim/cosim) and extract numbers.
# This is conservative: if a value cannot be parsed, we leave NA.
# Search heuristically for HLS synthesis report(s)
for rpt in \
  src/hls/solution*/syn/report/*.rpt \
  src/hls/solution*/csynth.rpt \
  dfs_accel_prj/solution*/syn/report/*.rpt \
  ; do
  for f in $rpt; do
    [ -f "$f" ] || continue
    # Attempt to extract BRAM/DSP/FF/LUT from a typical 'Utilization Estimates' table
    # Look for lines like "BRAM_18K" or "DSP48E" or "FF" or "LUT"
    if grep -q -i "BRAM_18K" "$f" 2>/dev/null || grep -q -i "BRAM" "$f" 2>/dev/null; then
      bram=$(grep -iE "BRAM(_18K)?|BRAM_18K" "$f" | head -n1 | awk '{print $NF}')
      [ -n "$bram" ] && hls_bram=${bram}
    fi
    if grep -q -i "DSP48E" "$f" 2>/dev/null || grep -q -i "DSP" "$f" 2>/dev/null; then
      dsp=$(grep -iE "DSP48E|DSP" "$f" | head -n1 | awk '{print $NF}')
      [ -n "$dsp" ] && hls_dsp=${dsp}
    fi
    if grep -q -i "FFs" "$f" 2>/dev/null || grep -q -i "^FF" "$f" 2>/dev/null; then
      ff=$(grep -iE "FFs|^FF" "$f" | head -n1 | awk '{print $NF}')
      [ -n "$ff" ] && hls_ff=${ff}
    fi
    if grep -q -i "LUTs" "$f" 2>/dev/null || grep -q -i "^LUT" "$f" 2>/dev/null; then
      lut=$(grep -iE "LUTs|^LUT" "$f" | head -n1 | awk '{print $NF}')
      [ -n "$lut" ] && hls_lut=${lut}
    fi
    # Try extract II (init interval) / worst loop
    if grep -q -i "worst-case II" "$f" 2>/dev/null || grep -q -i "Latency" "$f" 2>/dev/null; then
      ii=$(grep -iE "worst-?case.*II|worst.*II|II =|II:" "$f" | head -n1 | sed -E 's/.*([0-9]+).*/\1/')
      [ -n "$ii" ] && worst_ii=${ii}
    fi
  done
done

# Try to find cosim result (simple heuristic)
if grep -q -i "cosim" -r src/hls 2>/dev/null || [ -f src/hls/build/hls_host ] ; then
  # If cosim passes in typical output files, set Pass
  cosim_status=NA
  # Search for cosim logs
  cosim_log=$(find . -maxdepth 4 -type f -iname "*cosim*.log" -o -iname "*cosim*.txt" 2>/dev/null | head -n1 || true)
  if [ -n "$cosim_log" ] && grep -q -i "Pass" "$cosim_log" 2>/dev/null; then
    cosim_status=Pass
    # attempt to extract latencies (min/avg/max) if present
    cosim_latency_min_cycles=$(grep -iE "min" "$cosim_log" | head -n1 | sed -E 's/.*([0-9]+).*/\1/' || true)
    cosim_latency_avg_cycles=$(grep -iE "avg|average" "$cosim_log" | head -n1 | sed -E 's/.*([0-9]+).*/\1/' || true)
    cosim_latency_max_cycles=$(grep -iE "max" "$cosim_log" | head -n1 | sed -E 's/.*([0-9]+).*/\1/' || true)
  fi
fi

# Try to parse Vivado post-route utilization/timing reports
for rpt in \
  src/vivado/reports/*.rpt \
  src/vivado/reports/*_utilization.rpt \
  dfs_accel_prj/solution*/impl/reports/*.rpt \
  ; do
  for f in $rpt; do
    [ -f "$f" ] || continue
    if grep -qi "Number of LUTs" "$f" 2>/dev/null || grep -qi "LUT" "$f" 2>/dev/null; then
      lut=$(grep -iE "LUTs|Number of LUTs|LUT" "$f" | head -n1 | sed -E 's/.*[^0-9]([0-9,]+).*/\1/' | tr -d ',')
      [ -n "$lut" ] && vivado_lut=${lut}
    fi
    if grep -qi "Number of Slice Registers" "$f" 2>/dev/null || grep -qi "FF" "$f" 2>/dev/null; then
      ff=$(grep -iE "Slice Registers|FFs|FF" "$f" | head -n1 | sed -E 's/.*[^0-9]([0-9,]+).*/\1/' | tr -d ',')
      [ -n "$ff" ] && vivado_ff=${ff}
    fi
    if grep -qi "BRAM_18K" "$f" 2>/dev/null || grep -qi "BRAM" "$f" 2>/dev/null; then
      bram=$(grep -iE "BRAM_18K|BRAM" "$f" | head -n1 | sed -E 's/.*[^0-9]([0-9,]+).*/\1/' | tr -d ',')
      [ -n "$bram" ] && vivado_bram=${bram}
    fi
    if grep -qi "DSP48E" "$f" 2>/dev/null || grep -qi "DSP" "$f" 2>/dev/null; then
      dsp=$(grep -iE "DSP48E|DSP" "$f" | head -n1 | sed -E 's/.*[^0-9]([0-9,]+).*/\1/' | tr -d ',')
      [ -n "$dsp" ] && vivado_dsp=${dsp}
    fi
    # Extract WNS (worst negative slack) and fmax if present
    if grep -qi "Worst negative slack" "$f" 2>/dev/null || grep -qi "WNS" "$f" 2>/dev/null; then
      wns=$(grep -iE "Worst negative slack|WNS" "$f" | head -n1 | sed -E 's/.*(-?[0-9]+(\.[0-9]+)?).*/\1/')
      [ -n "$wns" ] && vivado_wns_ns=${wns}
    fi
    if grep -qi "FMAX" "$f" 2>/dev/null || grep -qi "fmax" "$f" 2>/dev/null; then
      fmax=$(grep -iE "FMAX|fmax" "$f" | head -n1 | sed -E 's/.*([0-9]+(\.[0-9]+)?).*/\1/')
      [ -n "$fmax" ] && vivado_fmax_mhz=${fmax}
    fi
  done
done

# Append header if file doesn't exist
if [ ! -f "$OUT" ]; then
  echo "timestamp,hls_bram,hls_dsp,hls_ff,hls_lut,worst_ii,worst_ii_loop,cosim_status,cosim_latency_min_cycles,cosim_latency_avg_cycles,cosim_latency_max_cycles,vivado_lut,vivado_ff,vivado_bram,vivado_dsp,vivado_wns_ns,vivado_fmax_mhz" > "$OUT"
fi

# Write a new line (do not duplicate if identical last line)
newline="$timestamp,$hls_bram,$hls_dsp,$hls_ff,$hls_lut,$worst_ii,$worst_ii_loop,$cosim_status,$cosim_latency_min_cycles,$cosim_latency_avg_cycles,$cosim_latency_max_cycles,$vivado_lut,$vivado_ff,$vivado_bram,$vivado_dsp,$vivado_wns_ns,$vivado_fmax_mhz"

# Avoid duplicate consecutive lines
last=$(tail -n1 "$OUT" 2>/dev/null || true)
if [ "$last" != "$newline" ]; then
  echo "$newline" >> "$OUT"
else
  echo "No new changes to append to $OUT"
fi
