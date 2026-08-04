## Per problem

| Problem (variant) | Speedup (modelled) | Speedup (measured) | Relative error (latency, mean) |
|---|---:|---:|---:|
| Number of Islands | 1.65x | 0.07x | +96.0% |
| Unique Paths III | 1.48x | 0.09x | +90.3% |
| Word Search II | 1.54x | 0.04x | +97.4% |
| Word Search II (no pruning) | 1.61x | 0.04x | +97.7% |
| Pacific Atlantic | 1.41x | 0.06x | +90.9% |
| Longest Increasing Path | 4.00x | 0.01x | +99.6% |
| Longest Increasing Path (no memo) | 4.00x | 0.02x | +99.2% |
| **Aggregate** | **1.56x** | **0.10x** | **+96.0%** |

## Per case

| Problem (variant) | Case | Modelled ON (ns) | Measured ON (ns, median) | Latency relative error |
|---|---|---:|---:|---:|
| Longest Increasing Path | `lip_desc` | 90 | 15845 | +99.4% |
| Longest Increasing Path | `lip_single` | 10 | 12320 | +99.9% |
| Longest Increasing Path | `lip_zigzag` | 90 | 15850 | +99.4% |
| Longest Increasing Path (no memo) | `lip_desc` | 230 | 19406 | +98.8% |
| Longest Increasing Path (no memo) | `lip_single` | 10 | 12275 | +99.9% |
| Longest Increasing Path (no memo) | `lip_zigzag` | 230 | 19405 | +98.8% |
| Number of Islands | `noi_all_land_4c` | 690 | 13196 | +94.8% |
| Number of Islands | `noi_all_water` | 0 | 12260 | +100.0% |
| Number of Islands | `noi_checker_4c` | 400 | 13250 | +97.0% |
| Number of Islands | `noi_checker_8c` | 580 | 13170 | +95.6% |
| Number of Islands | `noi_classic_4c` | 450 | 13160 | +96.6% |
| Number of Islands | `noi_classic_8c` | 530 | 13160 | +96.0% |
| Pacific Atlantic | `pa_classic` | 2890 | 16710 | +82.7% |
| Pacific Atlantic | `pa_single` | 120 | 12250 | +99.0% |
| Unique Paths III | `up3_no_path` | 180 | 13185 | +98.6% |
| Unique Paths III | `up3_one_obstacle` | 4820 | 38901 | +87.6% |
| Unique Paths III | `up3_open` | 10040 | 65540 | +84.7% |
| Word Search II | `ws2_classic` | 570 | 18530 | +96.9% |
| Word Search II | `ws2_small` | 350 | 16726 | +97.9% |
| Word Search II (no pruning) | `ws2_classic` | 380 | 16775 | +97.7% |
| Word Search II (no pruning) | `ws2_small` | 360 | 14920 | +97.6% |
